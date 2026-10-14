# FT6336U V03 升级

本目录固件仅用于 LT0470GF0430M_V1.1、684×1216、FT6336U 面板。
来自供应商 `LT0470GF0430M_V1.1 _FT6336U_684x1216_FT6336U_V03_20261010_app.bin`。

- 文件大小：47176 字节；按头部长度烧写前 47096 字节。
- 版本：0x03；应用 XOR：0xF6；整个文件 CRC32：0x857C2A9C。
- SHA256：`e9da2e5453cd53710e6b1d1f0b2785a69888691fbffd1ef7f005e3ccd5c690e8`。
- 不使用 `all.bin`，不把触摸固件直接交给 esptool。

## 使用

在 `idf.py menuconfig` 中启用
`Upgrade LT0470GF0430M FT6336U touch firmware to V03 on boot`
（`CONFIG_METALIO_TOUCH_UPGRADE_ON_BOOT=y`），编译 ESP32 固件并烧录。
整个升级期间保持电源连接。升级在 EPD/I2C 初始化之后、触摸采样、
LVGL 和后台监控启动之前执行，独占 I2C1。

串口日志标签为 `touch_upgrade`。成功日志为 `updated and verified V03`；
已有 V03 或更新版本时输出 `already current; skipped`。成功后可关闭该选项重新编译。
默认关闭，不会因为更新 ESP32 应用而自行擦除触摸固件。

升级先检查文件 CRC、头部、芯片 ID 64/26 和当前版本；进入 Bootloader 后
确认 ID 79/1C 才擦除应用区。逐包检查状态，校验 XOR，并在重启后确认版本。
任一步失败即停止，尝试退出 Bootloader；同一次启动不重新擦除。
擦除后的失败不保证触摸可用，当前实现不包含损坏固件的强制恢复。
需使用供应商工具或修复升级条件后恢复，不能把失败日志当作升级成功。

协议依据供应商 `Focaltech_Touch_FT6336U_I2C_V4.1_20230424(1).zip` 中
`focaltech_upgrade_ft6336u.c` 及 `focaltech_flash.c`；实现没有直接复制 Linux 驱动代码。

离线检查：`python tools/test_ft6336u_upgrade_host.py`。
