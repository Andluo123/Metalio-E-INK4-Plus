#!/usr/bin/env python3
"""把 build/flash_args 里的各段合成一个从 0x0 烧录的全量包。

产线烧录工具通常只支持「一个文件 + 首地址 0x0」，因此这个脚本是产线
交付物的唯一来源：

    build/ 里的 bootloader / 分区表 / otadata / app / 资源 / 字体包
        └─► <project>-full-0x0.bin

关键点（都是踩过的坑）：

* 首地址必须是 0x0，且 bootloader 必须在 0x2000（esp32s31），
  不是 0x1000。写错就是 "invalid header: 0x..." 死循环。
* 段间空隙由 esptool 填 0xFF，因此 NVS（0x9000）等落进空隙的分区会被清空。
* 合成完必须校验三处 magic，否则等于把一块砖交给产线。

用法：
    python scripts/merge_full_image.py --build-dir build \
        --output firmware/metalio-e-ink4-plus-full-0x0.bin
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

BOOTLOADER_OFFSET = 0x2000
PARTITION_OFFSET = 0x8000
PARTITION_TABLE_MAGIC = b"\xaa\x50"
ESP_IMAGE_MAGIC = 0xE9


def fail(message: str) -> NoReturn:
    print(f"错误: {message}", file=sys.stderr)
    raise SystemExit(1)


def parse_flash_args(build_dir: Path) -> tuple[list[tuple[int, Path]], dict[str, str]]:
    flash_args = build_dir / "flash_args"
    if not flash_args.is_file():
        fail(f"缺少 {flash_args}，请先完成编译")

    entries: list[tuple[int, Path]] = []
    options: dict[str, str] = {}
    for line in flash_args.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("--"):
            parts = line.split()
            for key, value in zip(parts[0::2], parts[1::2]):
                # --flash-mode dio -> flash_mode
                options[key.lstrip("-").replace("-", "_")] = value
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        try:
            offset = int(parts[0], 0)
        except ValueError:
            continue
        path = Path(parts[1])
        if not path.is_absolute():
            path = build_dir / path
        entries.append((offset, path.resolve()))

    if not entries:
        fail(f"{flash_args} 里没有可合并的段")
    return entries, options


def project_name(build_dir: Path) -> str:
    desc = build_dir / "project_description.json"
    if desc.is_file():
        try:
            return json.loads(desc.read_text(encoding="utf-8")).get("project_name", "")
        except (OSError, ValueError):
            pass
    return ""


def byte_at(image: Path, offset: int, length: int) -> bytes:
    with image.open("rb") as fp:
        fp.seek(offset)
        return fp.read(length)


def merge(build_dir: Path, output: Path, chip: str,
          flash_mode: str, flash_freq: str, flash_size: str) -> None:
    entries, options = parse_flash_args(build_dir)

    missing = [str(path) for _, path in entries if not path.is_file()]
    if missing:
        fail("以下固件段不存在：\n  " + "\n  ".join(missing))

    # 识别 app 段，用于合成后的布局校验
    proj = project_name(build_dir)
    app_offset = 0x80000
    for offset, path in entries:
        if proj and path.name == f"{proj}.bin":
            app_offset = offset
            break

    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable, "-m", "esptool", "--chip", chip, "merge-bin",
        "-o", str(output),
        "--flash-mode", flash_mode,
        "--flash-freq", flash_freq,
        "--flash-size", flash_size,
    ]
    for offset, path in entries:
        cmd += [f"0x{offset:x}", str(path)]

    print(f"[merge] {len(entries)} 段 -> {output}")
    subprocess.run(cmd, check=True)

    # ---- 布局校验：错了就是砖 ----
    if byte_at(output, BOOTLOADER_OFFSET, 1) != bytes([ESP_IMAGE_MAGIC]):
        fail(f"0x{BOOTLOADER_OFFSET:x} 处不是 bootloader（首字节应为 0xE9）")
    if byte_at(output, PARTITION_OFFSET, 2) != PARTITION_TABLE_MAGIC:
        fail(f"0x{PARTITION_OFFSET:x} 处不是分区表（magic 应为 0xAA50）")
    if byte_at(output, app_offset, 1) != bytes([ESP_IMAGE_MAGIC]):
        fail(f"0x{app_offset:x} 处不是 app 镜像（首字节应为 0xE9）")

    size = output.stat().st_size
    print(
        f"[verify] 0x{BOOTLOADER_OFFSET:x} bootloader / "
        f"0x{PARTITION_OFFSET:x} partition-table / "
        f"0x{app_offset:x} app 布局正确"
    )
    print(f"[done] {output} ({size} bytes) —— 烧录首地址 0x0")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--chip", default="esp32s31")
    parser.add_argument("--flash-mode", default="")
    parser.add_argument("--flash-freq", default="")
    parser.add_argument("--flash-size", default="")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if not build_dir.is_dir():
        fail(f"构建目录不存在：{build_dir}")

    _, options = parse_flash_args(build_dir)
    flash_mode = args.flash_mode or options.get("flash_mode", "dio")
    flash_freq = args.flash_freq or options.get("flash_freq", "80m")
    flash_size = args.flash_size or options.get("flash_size", "16MB")

    merge(build_dir, args.output.resolve(), args.chip, flash_mode, flash_freq, flash_size)


if __name__ == "__main__":
    main()
