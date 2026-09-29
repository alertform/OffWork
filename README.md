# OffWork 🐱

一只陪你等下班的 Windows 原生悬浮窗。

填写上下班时间和日薪后，OffWork 会实时显示距离下班还有多久、今天的工作进度，以及此刻已经赚到的金额。它不需要安装运行时、不需要登录，也不会联网。

## 下载

前往 [Releases](https://github.com/alertform/OffWork/releases/latest) 下载 `OffWork.exe`。目前提供 Windows x64 单文件版本，支持 Windows 10 和 Windows 11。

## 使用方法

1. 双击运行 `OffWork.exe`，悬浮窗会出现在屏幕右上角。
2. 展开“设置”，以 `HH:MM` 格式填写上班和下班时间，再填写日薪。
3. 点击“保存设置”，倒计时和已赚金额会立即更新并自动收起设置。
4. 点击标题栏图钉，可以开启或取消窗口置顶。
5. 拖动窗口边缘或右下角可以等比例调整大小；在设置里可选开启 85% 半透明。

应用采用单实例设计：再次运行 exe 会唤起已有窗口，不会叠出多个悬浮窗。

设置保存在 `%LOCALAPPDATA%\OffWork\settings.ini`。删除这个文件即可恢复默认值。

## 功能

- 每秒更新当前时间和下班倒计时
- 根据有效工作时长线性计算今日已赚金额
- 支持跨午夜班次
- 默认置顶，并可随时取消
- 支持 75%–150% 等比例缩放并自动记住大小
- 可选 85% 半透明显示，默认关闭
- 自动记住上下班时间和日薪
- 原生双缓冲绘制，设置收起只执行一次尺寸更新
- 内嵌多尺寸小猫程序图标
- 单个原生 Win32 exe，无 .NET/WinUI/Electron 运行时

## 金额计算

工作期间：

```text
已赚金额 = 日薪 × 已工作时长 ÷ 当日总工作时长
```

上班前显示 ¥0.00；下班后封顶为完整日薪。当前版本不单独扣除午休时间。

## 开发与构建

项目使用 C++20、Win32 API、GDI 和 CMake。Release 使用静态 MSVC 运行时，因此最终交付只有一个 exe。

在 Visual Studio Developer PowerShell 中执行：

```powershell
cmake -S . -B build -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

构建结果位于 `build\Release\OffWork.exe`。仓库的 **Build OffWork Windows EXE** workflow 会运行测试、检查包体并上传构建产物。

## 隐私

OffWork 不收集任何数据。上下班时间和日薪只写入当前 Windows 用户的本地配置目录。
