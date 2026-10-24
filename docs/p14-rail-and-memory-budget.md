# P14 共享电源状态机 与 内部 RAM 预算

本文记录两处结构性改动的原因、边界与验证方法。

## 1. P14 共享电源（4G + 蓝牙 + 功放）

### 硬件事实

- P14 = TCA9555 P1.4，高有效，同时给 **4G 模组、BH1098G 蓝牙、功放** 供电。
- 拉低 P14 等于同时切断这三者；不存在“只重启蓝牙”的硬件能力。
- 因此任何“顺手把锁存写高”的代码都无法回答：现在到底有没有电、能不能发 AT？

### 状态机

```
                 ensure_ready()
  UNKNOWN ─────────────────────► SETTLING ──(等满 settle_ms)──► READY
     ▲                              ▲                             │
     │ write failed                 │ sleep_restore / reset-high   │ ensure_ready 幂等返回
     └──────────────────────────────┴─────────────────────────────┘

  OFF ◄── sleep_cut_rails()
  READY ──bt_power_reset()──► RESETTING ──(P14 低 3 s → 高 → 80 ms)──► READY
                                 │
                                 └── 期间 ensure_ready() 返回 ESP_ERR_INVALID_STATE
```

实现位置：

- `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c`
  - `epd_board_metalio_eink4_plus_p14_state()`
  - `epd_board_metalio_eink4_plus_p14_ensure_ready(settle_ms)`
  - `epd_board_metalio_eink4_plus_bt_power_reset()`
- 唯一读入口 + 唯一写入口，状态变化都会打一行：
  `P14 SETTLING -> READY (settled)`

### 不变量

1. **幂等**：`READY` 状态下 `ensure_ready()` 不写 I2C，因此不会顺带重启 4G。
2. **互斥**：`RESETTING` 期间拒绝其他子系统的上电请求（返回 `ESP_ERR_INVALID_STATE`）。
3. **唯一写者**：只有 `p14_ensure_ready()` / `bt_power_reset()` / 睡眠路径会改 P14 锁存。
4. **串行化**：蓝牙复位走 `power_hw_bt_power_reset()`：
   `4G PreparePowerOff → 静音 PA → P14 低 3 s → P14 高 → 等待稳定 → 4G 异步恢复`。
5. **版本查询是纯读操作**：`MetalioAudio_GetBtVersion()` 只做 `ensure_ready()`，
   不再调用 `MetalioAudio_RequestModeInit()`（后者可能带一次 P14 复位）。

### 判定逻辑可测试

`main/boards/metalio-e-ink4-plus/periph/bt_version_policy.h` 把
“能不能发 AT”“结果能不能写缓存”抽成纯函数，
由 `tests/bt_version_test.cc` 在 PC 上回归。

## 2. 内部 RAM 预算

### 约束

- 内部 SRAM 总量 503,744 B（`sram_seg`），静态占用之外才是堆。
- 项目自定下限：**internal8 历史最小 ≥ 51,200 B（50 KiB）**，
  由 `main/firmware_memory.h` 的 `MemBudget` 日志判定
  （`within-floor` / `BELOW-FLOOR`）。
- PSRAM 有 12 MB 以上空闲，凡是**不经 DMA、不要求低延迟**的缓冲区都应优先放 PSRAM。

### 已做的两处回收

| 位置 | 原占用 | 处理 | 收益 |
|:--|--:|:--|--:|
| `waveforms/my_waveform_trim.c` | 17,408 B `.bss` | 删掉 `static trimmed_seq_t seqs[16][16]`，改成“先算长度（256 B）再逐行重算” | ≈17 KB |
| `CONFIG_TINYUSB_MSC_BUFSIZE` | 8,192 B `.data` | 8192 → 4096（USB 全速 12 Mbps 下 4 KB 已够） | 4 KB |

实测 `idf.py size`：

```
           DIRAM        .bss       .data
改动前     232,058     87,024     41,592
改动后     210,674     69,728     37,504
```

合计回收 **21,384 B** 内部 RAM。按运行日志的曲线平移，
internal8 历史最小从 ~36 KB 提升到 ~57 KB，回到 50 KiB 下限之上。

### 为什么没动 line queue

`render.c` 的行队列是 `2 × 128 × 176 B ≈ 45 KB` 内部 RAM，是最诱人的目标，
但它由 LCD EOF **中断**里的 `lq_read()` 逐行读取（`fill_bounce_buffer` 是
`IRAM_ATTR`）。把它放到 PSRAM 会让每行 171 B 的 memcpy 承担 cache miss，
在 16 µs/行 的时序窗口里有欠载风险，因此保持 INTERNAL。

### 观测点

启动阶段新增 checkpoints：`board-epd-init`、`board-display-created`、
`board-sd-mounted`、`board-audio-init`；运行期还有 `system-monitor`、
`chat-protocol-*`、`chat-channel-open`、`boot-complete`。

若某阶段 `BELOW-FLOOR`，先看该阶段新增的缓冲区是否本可以走 PSRAM：

```c
void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (p == nullptr) p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
```
