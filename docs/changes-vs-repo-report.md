# 变更对比报告：当前工作区 vs 仓库 HEAD

| 项 | 值 |
|:--|:--|
| 仓库 | Metalio-E-INK4-Plus |
| 对比基线 | `ad8edf4`（"修正一些过时的描述"） |
| 工作区状态 | 未提交（26 个文件修改，14 项新增） |
| 构建环境 | ESP-IDF 6.1 / target `esp32s31` |
| 报告日期 | 2026-10-24 |
| 构建结果 | 编译通过，**0 warning**；app 4,743,088 B |

---

## 1. 一页摘要

工作区包含**两轮**改动：

| 轮次 | 主题 | 性质 |
|:--|:--|:--|
| A | 设置页显示蓝牙芯片版本 | 功能补齐（此前完全缺失） |
| B | 内部 RAM / 电源状态机 / 构建可复现 / 产线全量包 / 测试 / 告警 | 结构与工程化 |

四项可量化收益：

| 指标 | 基线 | 当前 | 变化 |
|:--|--:|--:|:--|
| 内部 RAM 静态占用（DIRAM） | 232,058 B | 210,674 B | **−21,384 B** |
| internal8 历史最低水位 | 约 36 KB（低于 50 KiB 下限） | 预计约 57 KB | **回到下限之上** |
| 编译告警 | 6 类 | 0 | **清零** |
| PC 侧自动化测试 | 1 个 | 2 个 + CI | **+CI** |
| 产线全量包 | 手工拼 | `idf.py build` 自动产出 | **自动化** |

---

## 2. 变更总览

### 2.1 修改文件（26）

| 文件 | ±行数 | 轮次 | 说明 |
|:--|--:|:--:|:--|
| `merge_firmware.sh` | +126 / −148 | A→B | 重写：全量包默认、委托 Python 合成、解释器探测、产物名带烧录地址 |
| `main/.../periph/metalio_audio.cc` | +161 / −0 | A→B | 蓝牙版本查询/缓存；B 轮改为「查询不触发模式初始化」 |
| `main/.../display/epd_board_metalio_eink4_plus.c` | +156 / −2 | A→B | A 加 `bt_power_on()`；B 改为 P14 状态机 |
| `main/display/screen/settings_screen/settings_about_tab.cc` | +128 / −3 | A | 关于页蓝牙版本项；页面 generation + 请求 ID |
| `main/CMakeLists.txt` | +56 / −18 | B | `lang_config.h` 构建时生成；15 处 GLOB 加 `CONFIGURE_DEPENDS` |
| `README_zn.md` | +65 / −5 | B | 环境准备、host 测试、全量包与烧录地址 |
| `CMakeLists.txt` | +41 / −0 | B | 新增 `merge_full_image`（挂在 `ALL`） |
| `main/.../display/epd_board_metalio_eink4_plus.h` | +37 / −0 | A→B | P14 状态枚举与 API 声明 |
| `main/.../display/waveforms/my_waveform_trim.c` | +25 / −5 | B | 删 17 KB 静态暂存区，改两遍扫描 |
| `main/.../periph/metalio_audio.h` | +23 / −0 | A | 蓝牙版本查询/缓存 API |
| `main/.../power_policy/power_policy_standby_lp.cc` | +13 / −7 | B | 唤醒源位图 API |
| `main/.../power_policy/power_hw.cc` | +8 / −7 | A→B | 去掉外挂 80 ms（B）；唤醒源位图（B） |
| `main/.../power_policy/s31_sleep/s31_sleep.cc` | +11 / −4 | B | volatile 链式赋值拆分；唤醒源位图 |
| `main/assets/lang_home.cc` | +3 / −0 | A | 3 条蓝牙版本文案 |
| `main/idf_component.yml` | +4 / −1 | A | IDF ≥6.1；`78/uart-uhci`、`espressif/iot_eth` |
| `main/.../metalio_e_ink4_plus_board.cc` | +5 / −0 | B | 4 个启动阶段 heap 水位点 |
| `main/display/font/ttf_convert.cc` | +5 / −0 | B | `noreturn` 兜底 |
| `sdkconfig.defaults` | +4 / −0 | B | MSC 缓冲区 4096 |
| `.gitignore` | +6 / −2 | A→B | lock 入库、忽略本地产物 |
| `main/assets/lang_haptic.h` | +2 / −1 | B | 注释同步 |
| `main/display/a2ui/a2ui_image.cc` | +2 / −1 | B | volatile `++` |
| `main/display/font/fontpack_lvgl.cc` | +3 / −1 | B | `memset` → 值初始化 |
| `main/display/screen/book_screen/settings/book_sheet_icons.h` | +2 / −2 | B | `[[maybe_unused]]` |
| `main/display/epd_content_policy.cc` | +1 / −1 | B | 枚举/整型三元 |
| `main/boards/metalio-e-ink4-plus/config.h` | +1 / −1 | A | 仅注释（实测 39=TX/38=RX） |
| `sdkconfig` | +1 / −1 | B | MSC 8192 → 4096 |

### 2.2 新增文件（12 个源码/配置 + 1 产物目录 + 1 用户参考件）

| 文件 | 作用 |
|:--|:--|
| `scripts/merge_full_image.py` | 全量包合成 + 三处 magic 校验（唯一实现来源） |
| `scripts/bootstrap.ps1` | 一键准备 IDF 6.1 环境（幂等） |
| `scripts/requirements-host.txt` | 额外 Python 依赖固定版本 |
| `scripts/gen_lang_config_from_home.py` | 从 `lang_home.cc` 生成头文件（A 轮新增，B 轮接入 CMake） |
| `main/.../power_policy/power_wake_cause.h` | 唤醒源位图判断（替换弃用 API） |
| `main/.../periph/bt_at_parser.h` | AT 应答解析（无 IDF 依赖，可单测） |
| `main/.../periph/bt_version_policy.h` | 查询状态机判定（纯函数） |
| `tests/bt_version_test.cc` | AT 解析 + 状态机单测 |
| `tools/tests/run_host_tests.ps1` | MSVC/g++ 一键跑 host 测试 |
| `.github/workflows/ci.yml` | host 测试 + ESP-IDF 交叉编译 |
| `docs/p14-rail-and-memory-budget.md` | P14 状态机与内存预算设计说明 |
| `dependencies.lock` | 组件版本锁定（原被忽略，现应入库） |
| `firmware/` | 产物目录（全量包） |

> `main/assets/lang_config.h` 已从源码树移除，改为构建时生成到
> `build/esp-idf/main/assets/`，因此不再出现在版本控制里。

---

## 3. 分项详解

### 3.1 蓝牙版本显示（轮次 A）

基线状态：设置页没有「蓝牙版本」这一项，代码里也没有任何 `AT+GVER` 查询。

当前实现：

- 协议：`AT+GVER=1` → 应答 `AT+GVER=<version>`，解析时过滤命令回显
- 缓存：`MetalioAudio_GetCachedBtVersion()`；P14 掉电/恢复时失效
- UI：关于页异步查询（页面 generation + 请求 ID，旧任务不会写到新页面）
- 失败显示「未获取到」，不再停留在「…」
- UART 事务串行化（`s_uart_cmd_mutex`），避免与模式初始化交织

### 3.2 P14 共享电源状态机（轮次 B）

P14 = 4G + 蓝牙 + 功放共用电源，拉低即三者同时掉电。基线只有一个锁存字节，
没有「是否就绪」的概念。

```
UNKNOWN → SETTLING → READY
READY   → RESETTING（低 3 s → 高 → 80 ms）→ READY
OFF     ← sleep_cut_rails()
```

四条不变量：

1. READY 下 `ensure_ready()` 不写 I2C → 幂等，不打扰 4G
2. RESETTING 期间其他请求返回 `ESP_ERR_INVALID_STATE`
3. 只有 ensure / reset / sleep 三条路径改锁存
4. 版本查询不再调用 `RequestModeInit()`（该路径可能带一次 P14 复位）

### 3.3 内部 RAM（轮次 B）

| 位置 | 处理 | 收益 |
|:--|:--|--:|
| `my_waveform_trim.c` | 删 `static trimmed_seq_t seqs[16][16]`，改「先算长度→逐行重算」 | 17,408 B |
| `CONFIG_TINYUSB_MSC_BUFSIZE` | 8192 → 4096 | 4,096 B |

`idf.py size` 实测：

```
           DIRAM        .bss       .data
改动前     232,058     87,024     41,592
改动后     210,674     69,728     37,504
```

**刻意未动**：`render.c` 的行队列（2 × 128 × 176 B ≈ 45 KB）。它由 LCD EOF
中断里的 `lq_read()` 逐行读取，放 PSRAM 会让每行 171 B 的 memcpy 承担
cache miss，在 16 µs/行 窗口内有欠载风险。

新增观测点：`board-epd-init`、`board-display-created`、`board-sd-mounted`、
`board-audio-init`（另有既有的 `system-monitor`、`chat-*`）。

### 3.4 构建可复现（轮次 B）

- `dependencies.lock` 不再被忽略
- `lang_config.h` 由 CMake 生成（改文案只需改 `lang_home.cc`）
- `scripts/bootstrap.ps1` + `requirements-host.txt`
- 15 处 `file(GLOB)` 加 `CONFIGURE_DEPENDS`，避免「新增源文件静默不编译」

> 该改动过程中真实复现了一次事故：GLOB 参数位置写错 → `libmain.a` 从 249 个
> 对象掉到 51 个 → `Protocol::OnNetworkError` 未定义。已在报告中留存，作为
> 「GLOB 必须显式声明依赖」的证据。

### 3.5 产线全量包（轮次 B）

基线：`merge_firmware.sh` 手工拼 esptool 参数，产物名不含烧录地址。

当前：

```
idf.py build
  └─ merge_full_image (ALL)
       └─ scripts/merge_full_image.py
            ├─ 读 build/flash_args（6 段）
            ├─ esptool merge-bin → build/Metalio-E-Ink4-Plus-full-0x0.bin
            └─ 校验 0x2000 / 0x8000 / 0x80000 三处 magic
```

产物命名直接带地址，防止拿错：

| 文件 | 首地址 |
|:--|:--|
| `*-full-0x0.bin` | 0x0（产线） |
| `*-app-only-0x80000.bin` | 0x80000（调试） |

### 3.6 测试与 CI（轮次 B）

| 项 | 内容 |
|:--|:--|
| 单测 | `tests/bt_version_test.cc`：AT 解析 16 例 + 查询判定 6 例 + 结果接受 5 例 |
| 运行器 | `tools/tests/run_host_tests.ps1`（自动导入 MSVC 环境） |
| CI | host 测试 + `espressif/idf:release-v6.1` 交叉编译 |
| 实测 | 2/2 通过 |

### 3.7 编译告警（轮次 B）

| 告警 | 处理 |
|:--|:--|
| `fontpack_lvgl.cc` 对非平凡类型 `memset` | 改为 `*slot = FontpackLvFont{}` |
| `ttf_convert.cc` `noreturn` 仍返回 | 末尾加兜底循环 |
| `a2ui_image.cc` volatile `++` | 改为显式读改写 |
| `epd_content_policy.cc` 枚举/整型三元 | 加 `lv_opa_t` 转换 |
| `book_sheet_icons.h` 未使用函数 | `[[maybe_unused]]` |
| `s31_sleep.cc` volatile 链式赋值 | 拆成独立语句 |
| `esp_sleep_get_wakeup_cause()` 弃用（3 处） | 统一改用位图 API |

---

## 4. 运行期行为差异（同一硬件）

| 场景 | 基线 | 当前 |
|:--|:--|:--|
| 关于页蓝牙版本 | 无此项 | 显示固件版本 / 未获取到 |
| 版本查询时的 P14 | —（无此功能） | 只确认供电，幂等，不重启 4G |
| 蓝牙复位期间发 AT | — | 被拒绝并重试，不插队 |
| 内部 RAM 最低水位 | 低于 50 KiB 下限 | 预计高于下限（待真机确认） |
| `idf.py build` 产物 | app bin | app bin + **全量包** |
| 新增源文件 | 需手工 reconfigure | 自动触发重新配置 |

---

## 5. 风险与未完成项

| 项 | 状态 | 说明 |
|:--|:--|:--|
| 真机验证 P14 状态日志 | 待做 | 关注 `P14 ... -> READY (settled)` |
| 真机验证 RAM 水位 | 待做 | 关注 `MemBudget ... within-floor` |
| 主组件拆分（`main/` 249 个编译单元） | **未做** | 需重排全部 include/REQUIRES，风险远高于收益；建议按 `power_policy → audio_service → reader` 分块、每块单独回归 |
| 仓库瘦身 | **未做**（按用户要求排除） | 建议 Git LFS 或发布包分离 |
| 安全加固 | **未做**（按用户要求排除） | Secure Boot / Flash 加密 / OTA 签名 |
| 全量包清空 NVS | 已知代价 | 段间空隙填 `0xFF`，设置/配网需重做 |

---

## 6. 验证记录

| 命令 | 结果 |
|:--|:--|
| `idf.py build` | 通过，0 warning |
| `idf.py size` | DIRAM 210,674 / 503,744（41.82%），app 4,743,088 B |
| 全量包布局 | 0x2000 `e9 03 02 4f` / 0x8000 `aa 50` / 0x80000 `e9 06 02 4f` |
| 全量包大小 | 16,405,079 B（与已验证参考镜像同尺寸） |
| `tools/tests/run_host_tests.ps1` | 2/2 通过 |
| `bash -n merge_firmware.sh` | 通过 |

---

## 7. 自行复核

```bash
git status --short                       # 26 改 + 14 新增
git diff --stat                          # 行数统计
git diff -- main/CMakeLists.txt          # 逐文件查看
git diff -- <path>

# 构建与校验
idf.py build
idf.py size
python tools/tests/run_host_tests.ps1    # 或 pwsh
```
