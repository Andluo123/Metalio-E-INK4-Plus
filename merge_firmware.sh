#!/usr/bin/env bash
# 编译并导出 metalio-e-ink4-plus 产线全量包：
#   合成与布局校验由 scripts/merge_full_image.py 负责，
#   idf.py build 会自动生成 build/metalio-e-ink4-plus-full-0x0.bin；
#   本脚本负责编译、复制到 firmware/ 并归档。
# 输出：firmware/metalio-e-ink4-plus-{PROJECT_VER}-full-0x0.bin
#       + 根目录 metalio-e-ink4-plus.bin
#       + daily-builds/metalio-e-ink4-YYYYMMDDHHMMSS.bin（本地归档，不提交 git）
#
# 用法:
#   ./merge_firmware.sh           # 编译+合并（默认不上传）
#   ./merge_firmware.sh --full      # 全量包：从 0x0 烧录（给只支持 0x0 的工具）
#   ./merge_firmware.sh --app-only  # 仅 app：必须烧到 0x80000，不能烧 0x0
#   ./merge_firmware.sh -h
#   IDF_PATH=~/esp/v6.1 ./merge_firmware.sh
#
# 上传预留：日后可加 -u/--upload，当前未实现。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

FIRMWARE_DIR="firmware"
DAILY_BUILD_DIR="daily-builds"
PRODUCT_NAME="metalio-e-ink4-plus"
# CMake project() 的名字（决定 build/ 里产物的文件名大小写）
BUILD_PROJECT_NAME="Metalio-E-Ink4-Plus"
APP_FLASH_ADDR="0x80000"

# full = 整片合并镜像（0x0 起，危险）；app = 只出 app 镜像（0x80000）
MODE="full"

# Windows 的 Git Bash 自带一个 "python3" 的 App Execution Alias 存根，
# 它会以非 0 退出，导致 set -e 下静默中止。这里显式挑一个真正能用的解释器。
resolve_python() {
  local c
  for c in "${PYTHON:-}" python python3; do
    [[ -z "$c" ]] && continue
    if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys' >/dev/null 2>&1; then
      echo "$c"
      return 0
    fi
  done
  return 1
}
PYTHON_BIN="$(resolve_python || true)"
if [[ -z "$PYTHON_BIN" ]]; then
  echo "错误: 找不到可用的 Python（python / python3）" >&2
  exit 1
fi

usage() {
  cat <<'EOF'
用法: ./merge_firmware.sh [选项]

  默认编译并合并整片固件（含 font_data 分区的 font.fontpack），不上传。

选项:
  --full         产出全量包（从 0x0 烧录；默认）
  --app-only     只产出 app 镜像（必须烧到 0x80000），不动 NVS/分区表
  -h, --help     显示帮助

说明:
  全量包由构建自动生成（CMake 目标 merge_full_image），本脚本负责
  复制到 firmware/ 并归档；产物与 build/ 里的完全一致。

  两个产物互不通用，文件名里已写明烧录地址，请对照工具的首地址：

    *-full-0x0.bin        全量包，烧录首地址 0x0（含 bootloader/分区表/app/
                           资源/字体包）。注意：段间空隙会被填 0xFF，
                           NVS（设置/配网/阅读进度）会被清空。
    *-app-only-0x80000.bin 仅 app 分区。**绝对不能烧到 0x0**，
                           否则会覆盖 bootloader，开机停在
                           "invalid header" 循环里。

  优先使用 build/flash_args（含 CMake 挂载的 fontpack）；若缺失再按分区表
  Name=font_data 的 Offset 补写 use_font/font.fontpack，避免地址重叠。
  上传功能尚未接入，后续再加 -u/--upload。
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        usage
        exit 0
        ;;
      --app-only)
        MODE="app"
        shift
        ;;
      --full)
        MODE="full"
        shift
        ;;
      -u|--upload)
        echo "错误: 上传尚未实现，请先不加 -u 仅做本地合并" >&2
        exit 1
        ;;
      *)
        echo "未知参数: $1" >&2
        usage >&2
        exit 1
        ;;
    esac
  done
}

# ---------- 解析 app 镜像路径（flash_args 里 0x80000 那一段） ----------
resolve_app_image() {
  local flash_args="build/flash_args"
  local found=""
  if [[ -f "$flash_args" ]]; then
    while read -r addr file; do
      [[ -z "${addr:-}" ]] && continue
      [[ "$addr" == --* ]] && continue
      if [[ "$(norm_hex_addr "$addr")" == "$APP_FLASH_ADDR" ]]; then
        found="$(resolve_flash_path "$file")"
        break
      fi
    done < <(grep -E '^[0-9a-fxA-FX]+[[:space:]]' "$flash_args" || true)
  fi
  if [[ -z "$found" ]]; then
    found="build/Metalio-E-Ink4-Plus.bin"
  fi
  if [[ ! -f "$found" ]]; then
    echo "错误: 找不到 app 镜像 ${found}，请先完成编译" >&2
    exit 1
  fi
  echo "$found"
}

# ---------- IDF 环境 ----------
setup_idf() {
  if command -v idf.py >/dev/null 2>&1 && [[ -n "${IDF_PATH:-}" ]]; then
    echo "[idf] 已激活: IDF_PATH=${IDF_PATH}"
    return 0
  fi

  if [[ -z "${IDF_PATH:-}" ]] || [[ ! -f "${IDF_PATH}/export.sh" ]]; then
    local candidates=(
      "${HOME}/esp/v6.1"
      "${HOME}/esp/esp-idf"
      "${HOME}/esp-idf"
      "${HOME}/.espressif/v6.1/esp-idf"
      "/e/esp-idf-v6.1"
      "/mnt/e/esp-idf-v6.1"
      "/opt/esp/idf"
    )
    local c
    for c in "${candidates[@]}"; do
      if [[ -f "${c}/export.sh" ]]; then
        IDF_PATH="$c"
        break
      fi
    done
  fi

  if [[ -z "${IDF_PATH:-}" ]] || [[ ! -f "${IDF_PATH}/export.sh" ]]; then
    echo "错误: 未找到 ESP-IDF export.sh。请设置 IDF_PATH 后重试，例如:" >&2
    echo "  export IDF_PATH=~/esp/v6.1 && ./merge_firmware.sh" >&2
    exit 1
  fi

  echo "[idf] source ${IDF_PATH}/export.sh"
  # shellcheck disable=SC1091
  source "${IDF_PATH}/export.sh"

  if ! command -v idf.py >/dev/null 2>&1; then
    echo "错误: source 后仍找不到 idf.py" >&2
    exit 1
  fi
}

# ---------- 读版本 ----------
get_project_ver() {
  local ver
  ver="$(sed -n 's/^set(PROJECT_VER "\([^"]*\)").*/\1/p' CMakeLists.txt | head -1)"
  if [[ -z "$ver" ]]; then
    echo "错误: 无法从 CMakeLists.txt 解析 PROJECT_VER" >&2
    exit 1
  fi
  echo "$ver"
}

# ---------- flash_args 路径相对 build/；可能是 bootloader/... 或 ../use_font/... ----------
resolve_flash_path() {
  local file="$1"
  if [[ "$file" == /* ]]; then
    echo "$file"
  else
    echo "build/${file}"
  fi
}

# 十六进制地址规范化为小写 0x...
norm_hex_addr() {
  printf '0x%x' "$(( $1 ))"
}

# ---------- 主流程 ----------
main() {
  setup_idf

  local ver
  ver="$(get_project_ver)"
  mkdir -p "$FIRMWARE_DIR"
  echo "[ver] PROJECT_VER=${ver} MODE=${MODE}"

  echo "[build] idf.py build ..."
  idf.py build

  # ---------- 只出 app 镜像 ----------
  if [[ "$MODE" == "app" ]]; then
    local app_image
    app_image="$(resolve_app_image)"
    local app_out="${FIRMWARE_DIR}/${PRODUCT_NAME}-${ver}-app-only-0x80000.bin"
    cp -f "$app_image" "$app_out"
    local app_size
    app_size="$(wc -c < "$app_out" | tr -d ' ')"
    # app 镜像必须以 0xE9 开头（esp_image magic），否则说明产物本身有问题
    local magic
    magic="$(od -An -tx1 -N1 "$app_out" | tr -d ' \n')"
    if [[ "$magic" != "e9" ]]; then
      echo "错误: ${app_out} 不是合法 app 镜像（首字节 0x${magic:-??}，应为 0xe9）" >&2
      exit 1
    fi
    echo "[done] ${SCRIPT_DIR}/${app_out} (${app_size} bytes)"
    echo "[WARN] 这是「仅 app」镜像，烧录首地址必须是 ${APP_FLASH_ADDR}，禁止烧 0x0"
    echo "[flash] idf.py -p <PORT> app-flash   # 自动写当前运行槽（推荐）"
    echo "[flash] 手工: esptool --chip esp32s31 write_flash ${APP_FLASH_ADDR} ${app_out}"
    echo "[flash] 注意: 手工写 ${APP_FLASH_ADDR} 只更新 ota_0；若设备当前跑在 ota_1，需改地址或改用 app-flash"
    echo "[note] 只覆盖 app 分区：NVS / 分区表 / 字体包保持不变"
    return 0
  fi

  echo "[info] 全量包：烧录首地址 0x0（bootloader 在 0x2000，app 在 0x80000）"
  echo "[warn] 段间空隙填 0xFF，NVS（设置/配网/阅读进度）会被清空"

  local out="${FIRMWARE_DIR}/${PRODUCT_NAME}-${ver}-full-0x0.bin"
  local build_image="build/${BUILD_PROJECT_NAME}-full-0x0.bin"

  # 正常路径：idf.py build 里的 merge_full_image 目标已经生成并校验过。
  # 手工跑脚本时如果没生成，就用同一个 Python 脚本补一次（单一实现来源）。
  if [[ ! -f "$build_image" ]]; then
    echo "[merge] ${build_image} 缺失，调用 scripts/merge_full_image.py 生成"
    "$PYTHON_BIN" scripts/merge_full_image.py \
      --build-dir build --output "$build_image"
  fi
  cp -f "$build_image" "$out"

  local latest="${PRODUCT_NAME}.bin"
  cp -f "$out" "$latest"

  mkdir -p "$DAILY_BUILD_DIR"
  local daily_name="${PRODUCT_NAME}-$(date '+%Y%m%d%H%M%S').bin"
  local daily_out="${DAILY_BUILD_DIR}/${daily_name}"
  cp -f "$out" "$daily_out"

  local size
  size="$(wc -c < "$out" | tr -d ' ')"
  echo "[done] ${SCRIPT_DIR}/${out} (${size} bytes)"
  echo "[flash] 全量包烧录首地址 0x0（工具里填 0x00000000）"
  echo "[done] ${SCRIPT_DIR}/${latest}"
  echo "[done] ${SCRIPT_DIR}/${daily_out}"
  echo "[upload] 已跳过（上传功能尚未接入）"
}

parse_args "$@"
main
