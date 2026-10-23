"""压缩 AP 设备页面并内嵌标识图片，仅使用 Python 标准库。"""

import base64
import zlib
from pathlib import Path
import sys


def pack(source: Path, output: Path) -> None:
    page = source.read_text(encoding="utf-8")
    logo = base64.b64encode((source.parent / "cloudzao-logo.png").read_bytes()).decode("ascii")
    before, after = page.split('src="cloudzao-logo.png"', 1)
    image = 'src="data:image/png;base64,' + logo + '"'
    # 图片与页面分别结束 DEFLATE 块，避免大段 base64 干扰页面的压缩编码表。
    compressor = zlib.compressobj(9, zlib.DEFLATED, 31)
    packed = compressor.compress(before.encode("utf-8")) + compressor.flush(zlib.Z_BLOCK)
    packed += compressor.compress(image.encode("ascii")) + compressor.flush(zlib.Z_BLOCK)
    packed += compressor.compress(after.encode("utf-8")) + compressor.flush()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(packed)


if __name__ == "__main__":
    pack(Path(sys.argv[1]), Path(sys.argv[2]))
