# ebook Studio（`tools/ebook`）

ESP32 友好的 `.ebook` **转换 + Web 阅读**工具集。

## 目录

| 路径 | 说明 |
|------|------|
| `epdbook/` | 格式库（writer / reader / extractors） |
| `convert_ebook.py` | CLI 转换 |
| `web/` | Web：上传转换 + 在线阅读（亮/暗主题） |
| `SPEC.md` | 二进制规范 |
| `test-book/` | 测试样书 |
| `book/` | 额外样书（可选） |
| `data/library/` | Web 转换产物 |
| `emulator/` | LVGL 墨水屏 WASM/SDL 模拟器（改阅读布局后在此 `./build_wasm.sh`） |

## Web（推荐）

```bash
cd tools/ebook
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
./run_web.sh
# 打开 http://127.0.0.1:8765
# 端口被占用时会自动顺延；或: EBOOK_PORT=9000 ./run_web.sh
# 强制结束旧实例: ./run_web.sh --kill
```

功能：上传 PDF/EPUB/MOBI/TXT → `.ebook`；书库（可直接打开已有 `.ebook` 阅读）；在线阅读（封面/目录/图文）；下载。

### Windows 单文件 exe（双击即用）

在 Windows（或 WSL 调用 Windows Python）打包：

```bat
cd tools\ebook
build_exe.bat
```

或 WSL：

```bash
cd tools/ebook
chmod +x build_exe.sh
./build_exe.sh
```

产物：`dist/ebook-web.exe`（约 90MB+，含 31 份 MiSans-Light `.ef` 字体）。双击后会启动本地服务并自动打开浏览器；**关闭黑色控制台窗口即停止服务**。书库写在 exe 同目录的 `data/` 下。

打包前会自动把 `tools/epdfont/MiSans-Light/*.ef` 复制到 `web/static/fonts/ef/` 一并打进 exe。

WSL 下执行 `./build_exe.sh` 会先同步到 `C:\Users\<你>\ebook-web-build` 再打包，完成后拷回本目录 `dist/`。

可选环境变量：`EBOOK_PORT`、`EBOOK_HOST`、`EBOOK_NO_BROWSER=1`（禁止自动开浏览器）。

### TXT 切章

转换时按行识别章节标题（与固件 `txt_chapter` 对齐并扩展），例如：

- `第N章/回/节/集/话/篇`、`第N卷/部`、`卷N`
- `序章` / `楔子` / `番外` / `前言` / `后记` / `尾声`…
- `Chapter 1` / `Part 2` / `Volume 3`
- 数字短章名（如 `11清和宫上`，对齐固件）

识别到 ≥2 章时置 `HAS_TOC`。可用 `--chapter-regex` 覆盖默认规则，`--no-split` 整书单章。

超大章（默认单章解压合计 >256KB，含图）会再按体积拆成 `原名 (1/N)`…，**不**因此置 `HAS_TOC`；设备可跨章翻页。`--no-chapter-size-split` 可关闭（不推荐）。新固件按物理块流式读取，不再因单章超过 384KiB 拒绝打开；仍建议保留拆章，以减少章节切换等待并兼容旧固件。

```bash
.venv/bin/python convert_ebook.py novel.txt -o novel.ebook --inspect
```

### PDF 扫描件

Web 勾选「整页转图片」，或 CLI：

```bash
.venv/bin/python convert_ebook.py scan.pdf -o scan.ebook \
  --pdf-pages-as-images \
  --pdf-page-scale 1.5 \
  --pdf-page-max 480x720 \
  --binarize otsu
```

- `--pdf-page-scale`：渲染倍率（相对 72dpi）
- `--pdf-page-max`：编码前最大框（默认 **480×720**，阅读区全宽）；写出 Header `PAGE_IMAGES`，设备从显示区 `(0,0)` 铺满
- `--image-max`：插图最大框；`orig` 表示保留原始尺寸/比例（仍受 chunk 预算缩小）
- 有插图/扫描页时会自动抬高 `chunk_max`（≤49152，与固件一致）
- Web 书库可「清空书库」一键删除全部产物

### 二值化（A2I1）

墨水屏插图/页图默认 **Otsu** 自适应阈值；可选：

| 方法 | CLI | 适用 |
|------|-----|------|
| Otsu | `--binarize otsu` | 漫画线稿、多数彩插（默认） |
| Bayer | `--binarize bayer` | **与设备原生 EPUB 同款**（4×4 有序抖动） |
| 抖动 | `--binarize dither` | 照片、水彩、渐变（Floyd–Steinberg） |
| Sauvola | `--binarize sauvola` | 底色不匀的扫描件 |
| 固定阈值 | `--binarize fixed --threshold 128` | 手动微调 |

另可调 `--contrast`、`--sauvola-window`、`--sauvola-k`。Web：选 **PDF / EPUB** 后展开「二值化预览」，调参预览满意再转换（PDF 按页，EPUB 按插图顺序）。

### 保留彩色（不二值化）

勾选 Web「保留彩色」，或 CLI `--keep-color`：

```bash
.venv/bin/python convert_ebook.py book.epub -o out.ebook --keep-color
```

- 插图/封面以 **JPEG** 写入（非 A2I1），体积较大，仍受 chunk / 封面预算约束
- **设备阅读**时按原生 EPUB 同款路径：`DecodeImageToL8` → RGB 加权灰度 + Bayer 4×4 抖动
- 与 `--binarize` 互斥；需要转换侧控制二值化效果时请用默认 A2I1 流程

## CLI

```bash
.venv/bin/python convert_ebook.py "test-book/Node即学即用.pdf" -o out.ebook --inspect
```

## 设备端大文件阅读

TXT 首次打开先显示当前位置，复用现有低优先级排版任务建立 SD 行索引和页索引，不再按 16MiB 拒绝打开；POS3 进度文件追加源字节锚点，旧进度缺少锚点时按百分比定位附近段落。`.ebook` 按物理块解压，EPUB 按 ZIP 解压流解析 HTML，分别取消旧的单章 384KiB、1MiB 读取限制。

TXT 索引在 PSRAM 中保留两个 4KiB 窗口；命中永久 `.idx` 时直接按需读取行表和页表，不再全量复制到临时工作文件，目录页码按页表顺序计算。横竖屏或字号改变使缓存失效时仍走单任务重建。章节分页正文存于 SD，内存最多保留两页正文及一个页偏移窗口。工作文件位于书籍旁，后缀为 `.rlines.tmp`、`.rindex.tmp`、`.rpages-N.tmp` 及其 `.idx`，正常退出或失败时关闭并删除，浅睡卸卡前关闭句柄。书籍目录需可写并有足够空间；意外断电遗留的这些工作文件可在关闭阅读器后删除。永久 `.idx`、`.cpg` 和 `.pos` 缓存保留。

打开、排版、全书计页和插图加载由后台任务调度：排版与插图、计页串行执行，当前页插图优先。排版结果在 UI 任务发布，字号变化先取消旧任务再应用参数。文件读取失败、内容损坏、不支持的格式或边界、内存不足、SD 缓存失败分别提示；EPUB 读取失败不再替换为“本章无正文”。

上述改动不解除格式本身、单个压缩物理块、ZIP 元数据或平台文件偏移的安全边界，也不保证损坏或加密书籍可读。图片解码仍使用已有内存预算。

后台编排现在分阶段发布结果：TXT 按扫描前缀批量提供已识别目录、章节源位置及页数估计，UI 复用现有定时器约每 2 秒接收，暂估总页数标记 `~`。编排期间可跳转已识别章节或按源字节位置调整阅读进度，进度不再按单页预览计算为 100%。章节书的全书计页也逐章更新。完整结果与持久索引仅在扫描完成后发布。横竖屏切换会取消旧编排并用新视口重排，不等待旧扫描结束；仍保持单个编排任务执行。

翻页期间后台扫描继续运行，沿用现有单任务编排和分批让出 CPU 的逻辑。结果接收采用非阻塞锁，计页缓存由后台写入 SD；沉浸阅读期间的周期结果接收只更新状态，页数和进度随正常翻页显示，不再每 2 秒额外刷底栏。目录和设置仍在交互、待绘制及刷屏结束后按需更新。

进入待机时先取消编排和插图任务，复用 UI Tick 在任务完成通知后关闭字库及 SD 句柄；SD 现有排空流程等待这次资源收尾，不阻塞 UI。待机期间不启动阅读任务、不刷新隐藏正文；快速唤醒仍先等旧任务结束，再恢复资源并重新调度未完成的编排。
