# ZStreamEye

[![Windows MSYS2](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml/badge.svg)](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml)

[English](README.md) | 简体中文

ZStreamEye 是一个跨平台桌面 H.264 码流分析工具，基于 C++17、Qt 6 Widgets、`QOpenGLWidget`、FFmpeg 和 CMake 构建。

项目目前支持视频加载/解码、可控制播放、H.264 语法解析、宏块级 overlay、导出工具、可停靠的分析器 UI 和基于 OpenGL 的视频渲染。

## 功能特性

- 通过 `File -> Open` 或拖放打开本地 H.264/视频文件。
- 使用 FFmpeg 在后台线程中解码视频流，避免阻塞 UI。
- 发现容器流，包括基本的视频/音频元数据，同时默认解码最佳视频流。
- 支持播放、暂停、停止和逐帧播放。
- 从索引的关键帧/IDR 检查点重新缓冲被驱逐的帧（如果可用）。
- 通过 `QOpenGLWidget` 渲染解码帧。
- 通过编解码器中性的解析器接口路由码流解析，以便添加额外的编解码器而无需与 `FFmpegDecoder` 耦合。
- 提供 HEVC/H.265 解析器骨架，用于识别 NAL 单元、VPS/SPS/PPS、VCL 访问单元，并提供优雅的不支持切片诊断。当前 HEVC 帧列表里的 `Type` 仍是粗粒度标签：`IRAP` 表示 IDR/CRA/BLA 等随机访问点，`VCL` 表示尚未深入解析 slice type 的视频编码层访问单元。
- 提供 AAC ADTS 解析器骨架，用于音频访问单元，包括头部位字段和畸形包的优雅诊断。
- 提供 MP3 帧头解析器骨架，用于 MPEG 音频帧访问单元。
- 在帧列表中显示从发现的 AAC/MP3 音频流解析的音频访问单元，并在属性树中显示其通用字段。
- 按发现的容器流、视频/音频媒体类型或携带诊断信息的访问单元过滤访问单元列表。
- 在解析的访问单元上保留包元数据和原始包字节，以便后续的十六进制/位视图可以追溯字段到包证据。
- 为选定访问单元的包字节显示只读的位流十六进制面板，并通过语法位字段选择驱动字节范围高亮。
- 显示分析统计面板，包含访问单元计数、帧类型分布、宏块/QP 汇总、QP 分桶分布、运动矢量幅度汇总和诊断分布；这些统计都来自内部 `FrameAnalysis` 数据。
- 使用自研 `H264Parser` 解析 H.264 NALU、SPS、PPS、Slice Header、选定的 CAVLC 宏块字段、残差块、QP、P-slice L0 运动矢量，以及经真实编码器码流验证的窄范围 CABAC P-slice 路径。
- 在可停靠的属性树中显示帧语法信息。
- 在属性树中显示 overlay 可用性，包括当前帧的 QP 范围/常量注释和运动矢量支持诊断。
- 显示帧列表信息，如帧类型、POC 和 `frame_num`。
- 在视频画布上叠加分析数据：
  - 16x16 宏块网格
  - 宏块 QP 热力图
  - 解析的 P-slice 运动矢量
- 独立开关 grid/QP/MV overlay，并调整 overlay 透明度。
- 持久化窗口布局、面板位置、overlay 开关、透明度和最近打开/导出目录。
- 导出选定访问单元的语法 JSON、访问单元列表 CSV、带 overlay 的截图，以及所有解码访问单元的语法 JSON（包含 schema/版本和流/包元数据）。
- 生成包含 Qt、FFmpeg 和运行时 DLL 的 Windows 便携包。
- 从 `Help -> Check for Updates` 检查 GitHub Releases 更新。

## 当前限制

- CAVLC 残差解析已公开包含扫描位置、level 和 run-before 的残差块及非零系数摘要，但仍不是完整的反扫描、反量化或变换可视化。
- CABAC 解析目前仍是窄范围路径：真实 x264 P-slice 已覆盖 P_Skip、P_L0_16x16、部分 P_8x8 语法、完整 `mvd_l0`、luma4x4 残差和 4:2:0 chroma DC 残差。非零 CABAC `mb_qp_delta`、chroma AC、更广的 P 分区、CABAC intra/B 宏块、B_Direct/B_8x8 运动矢量以及 MBAFF/FMO 仍是不支持或仅诊断路径。
- 属性树限制显示的宏块数量以保持播放响应性；overlay 数据仍使用解析的宏块列表。
- 由于某些 Windows/OpenGL 部署渲染 QPainter 文本不正确，OpenGL 画布文本故意避免用于分析提示；请使用属性树和状态栏获取分析解释。

## 项目结构

```text
ZStreamEye/
+-- docs/
+-- scripts/
+-- src/
|   +-- app/
|   |   +-- CMakeLists.txt
|   +-- core/
|   |   +-- CMakeLists.txt
|   |   +-- analysis/
|   |   +-- decode/
|   |   +-- export/
|   |   +-- model/
|   |   +-- parser/
|   |   |   +-- audio/
|   |   |   +-- video/
|   |   |   |   +-- h264/
|   |   |   |   +-- hevc/
|   |   +-- util/
|   |   +-- buffering/
|   +-- platform/
|   |   +-- windows/
|   +-- ui/
|-- tests/
|   +-- CMakeLists.txt
+-- CMakeLists.txt
+-- README.md
+-- README.zh-CN.md
+-- vcpkg.json
```

文件夹职责：

- `src/app`：应用程序窗口、菜单、工具栏、面板布局、文件打开、导出/更新控制器和工作流连接。
- `src/core/model`：流元数据、媒体类型、帧分析数据和文档状态。
- `src/core/parser`：编解码器中性解析器接口，以及 audio/video 解析器模块。
- `src/core/util`：编解码器中性的底层工具，如位读取、字节流和 RBSP/包位范围映射。
- `src/core/decode`：FFmpeg 解码器包装器和解码工作器。
- `src/core/export`：分析结果导出序列化。
- `src/core/buffering`：缓冲和 seek 重新缓冲状态辅助逻辑。
- `src/platform`：平台相关 launcher 和集成代码。
- `src/ui`：可复用的 Qt 部件，如帧列表、属性树、日志面板和视频画布。
- `scripts`：构建/部署辅助脚本。
- `docs`：开发者笔记、部署笔记和 AI 续接路线图。

## 需求要求

- CMake 3.21 或更高版本。
- Qt 6，包含 `Core`、`Widgets`、`Network`、`OpenGL` 和 `OpenGLWidgets`。
- FFmpeg 开发库：
  - `avformat`
  - `avcodec`
  - `avutil`
  - `swscale`
- C++17 编译器：
  - Windows：MSVC 2022、MinGW-w64 或 MSYS2 UCRT64。
  - macOS：Apple Clang / Xcode Command Line Tools。
  - Linux：GCC 或 Clang。

官方参考：

- Qt 安装：https://doc.qt.io/qt-6/get-and-install-qt.html
- Qt with CMake：https://doc.qt.io/qt-6/cmake-get-started.html
- CMake 下载：https://cmake.org/download/
- FFmpeg API 文档：https://ffmpeg.org/doxygen/trunk/
- vcpkg FFmpeg 包：https://vcpkg.io/en/package/ffmpeg.html

## Windows 开发环境

当前验证的 Windows 开发环境使用 MSYS2 UCRT64：

- GCC/G++ 16.1.0
- CMake 4.3.2
- Ninja 1.13.2
- Qt 6.11.0
- FFmpeg 8.1.1

安装日志和验证结果请参见 [installEnv.md](installEnv.md)。

配置并构建：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
```

从开发环境运行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

不要单独分发 `build-msys2-ucrt/ZStreamEye.exe`，它依赖 MSYS2 UCRT64 环境中的 DLL。

## Windows 便携包

生成便携包：

```powershell
.\scripts\deploy-windows-msys2.ps1
```

部署脚本会自行配置并构建专用发布目录 `build-deploy-msys2-ucrt`，再收集运行时 DLL；它不依赖开发用的 `build-msys2-ucrt` 目录。

如果当前 PowerShell 禁止执行脚本，可以只对当前进程绕过策略：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy-windows-msys2.ps1
```

输出：

```text
dist/ZStreamEye-windows-ucrt64/
dist/ZStreamEye-windows-ucrt64.zip
```

便携包包含：

- `ZStreamEye.exe`
- Qt 运行时 DLL
- Qt 插件，如 `platforms/qwindows.dll`
- FFmpeg DLL，如 `avcodec-62.dll`
- MSYS2 UCRT64/GCC 运行时 DLL

终端用户只需解压包并运行 `ZStreamEye.exe` 即可。运行时 DLL 和 Qt 插件分组在 `runtime/` 目录下。

## Windows 安装程序

在生成便携包后创建 Windows 安装程序：

```powershell
.\scripts\deploy-windows-msys2.ps1
.\scripts\package-windows-installer.ps1 -Version "0.1.8"
```

输出：

```text
dist/ZStreamEye-0.1.8-windows-ucrt64-setup.exe
```

安装程序使用 Inno Setup 6 构建，并安装与便携包相同的自包含 Qt、FFmpeg 和 MSYS2 UCRT64 运行时文件。GitHub 发布构建同时发布便携 `.zip` 和安装程序 `.exe`。将发布标记为 `vX.Y.Z`；发布工作流在发布 GitHub Release 资产之前验证标记是否与 CMake 项目版本匹配。

更多细节请参见 [docs/windows-deployment.md](docs/windows-deployment.md)。

## macOS

安装依赖：

```bash
xcode-select --install
brew install cmake qt ffmpeg pkg-config
```

如果 CMake 找不到 Qt：

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行测试：

```bash
ctest --test-dir build --output-on-failure
```

运行：

```bash
open build/ZStreamEye.app
```

## Linux

Ubuntu / Debian：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Fedora：

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel mesa-libGL-devel ffmpeg-devel
```

构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行：

```bash
./build/ZStreamEye
```

## 开发笔记

关键模块：

- `FFmpegDecoder`：封装 `AVFormatContext`、`AVCodecContext`、`AVPacket` 和 `AVFrame`；通过 `IBitstreamParser` 拥有编解码器特定的位流解析器。
- `MediaTypes`：媒体/访问单元标识符，用于防止未来的视频和音频分析路径被强制转换为仅视频字段。
- `BitstreamParser`：编解码器中性的解析器接口和编解码器类型标识符。
- `AacAdtsParser`：用于 ADTS 头字段和诊断的轻量音频访问单元骨架。
- `Mp3FrameParser`：用于 MPEG 音频帧头、比特率/采样率字段和畸形头诊断的轻量音频访问单元骨架。
- `DecodeWorker`：在后台 `QThread` 中运行解码。
- `H264Parser`：直接解析 H.264 语法，不依赖 FFmpeg 的解析器。
- `VideoCanvas`：渲染视频帧和分析 overlay。
- `FrameListView` 和 `PropertyTreeView`：显示解析的访问单元、包元数据和语法信息。
- `BitstreamHexView`：在有限页面中显示选定访问单元的包字节，高亮选定位字段的字节范围，显示选定范围的位级掩码预览，并在用户点击十六进制字节时选择覆盖的语法字段。位字段携带 `offset_basis`；包字节高亮直接跟随包相对字段，并使用标准化的包位范围用于 H.264 RBSP 相对的 SPS/PPS/切片头和选定的宏块语法字段。`PropertyTreeView` 中的 H.264 摘要行直接驱动十六进制选择，因此用户无需为常见字段打开单独的位位置子树。

推荐的下一步工作：

1. 完整实现 CABAC 非零 `mb_qp_delta` 解码、QP 状态更新和真实 fixture 断言；这是 `x264_cabac_residual.h264` 当前遇到的第一个边界。
2. 继续推进真实 CABAC fixture，直到遇到下一个实际不支持的语法，再按实际顺序扩展窄范围 4:2:0 chroma AC 和 P_16x8/P_8x16 路径。
3. 对最新的 Windows 安装程序/便携包和升级路径进行冒烟测试。
4. 在 AAC/MP3 骨架之上添加更丰富的包/访问单元浏览；保持音频分析与 `VideoCanvas` 分离。当前的流选择器过滤解码的访问单元列表；尚未切换 FFmpeg 播放流或提供仅音频分析。
5. 使用图形化子字节装饰和更广泛的偏移标准化扩展位流十六进制面板，用于宏块字段和容器/基本流包装器。
6. 在开始完整 CABAC intra/B-slice 宏块族之前，先扩充真实编码器 CABAC P-slice fixtures。

有关未来 AI/编码代理的信息，请参见 [docs/ai-continuation-notes.md](docs/ai-continuation-notes.md)。有关更长期的 StreamEye 类路线图，请参见 [docs/ai-streameye-roadmap.md](docs/ai-streameye-roadmap.md)。

## 仓库维护

不要提交生成的构建或发布输出：

```text
build*/
dist/
```

这些路径已被 `.gitignore` 忽略。
