# OffWork 🐱

一只陪你等下班的小猫悬浮窗。

填写上下班时间和日薪后，OffWork 会实时显示距离下班还有多久、今天的工作进度，以及此刻已经赚到的金额。数据只保存在本机，不需要登录，也不会联网。

## 下载

前往 [Releases](https://github.com/alertform/OffWork/releases/latest) 下载 `OffWork.exe`。目前提供 Windows x64 单文件版本，支持 Windows 10 1809 及以上系统。

第一次启动时，Windows 可能需要几秒钟解压自带的 .NET 与 WinUI 运行组件。

## 使用方法

1. 双击运行 `OffWork.exe`，悬浮窗会出现在屏幕右上角。
2. 展开“设置”，填写上班时间、下班时间和日薪。
3. 点击“保存设置”，倒计时与已赚金额会立即开始更新。
4. 点击标题栏上的图钉，可以开启或取消窗口置顶。

设置会保存在 `%LOCALAPPDATA%\OffWork\settings.json`。删除这个文件即可恢复默认值。

## 功能

- 每秒更新当前时间和下班倒计时
- 根据有效工作时长线性计算今日已赚金额
- 支持跨午夜班次
- 默认置顶，并可随时取消
- 自动记住上下班时间与日薪
- 原生 WinUI 3 界面，跟随 Windows 深色或浅色主题
- 单个 `OffWork.exe`，无需安装

## 金额计算

工作期间：

```text
已赚金额 = 日薪 × 已工作时长 ÷ 当日总工作时长
```

上班前显示 ¥0.00；下班后封顶为完整日薪。当前版本不单独扣除午休时间。

## 开发与构建

项目使用 C#、.NET 10、WinUI 3 和 Windows App SDK。计算逻辑与界面工程分离，并包含上班前、工作中、下班后和跨午夜班次的单元测试。

在 Windows 上执行：

```powershell
dotnet test tests\DwhOffWork.Core.Tests\DwhOffWork.Core.Tests.csproj -c Release

dotnet publish src\DwhOffWork\DwhOffWork.csproj `
  -c Release `
  -r win-x64 `
  -p:Platform=x64 `
  -o artifacts\win-x64
```

构建结果位于 `artifacts\win-x64\OffWork.exe`。也可以打开 `DwhOffWork.slnx`，或通过仓库的 **Build OffWork Windows EXE** workflow 构建。

## 隐私

OffWork 不收集任何数据。上下班时间和日薪只写入当前 Windows 用户的本地配置目录。
