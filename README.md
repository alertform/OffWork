# 下班小助手（DwhOffWork）

一个 Windows 桌面悬浮窗：填写上下班时间和日薪后，实时显示距离下班还有多久，以及今天已经赚了多少钱。

## 功能

- 每秒更新下班倒计时与当前时间
- 按工作时长线性计算今日已赚金额
- 支持跨午夜班次
- 默认置顶，可随时取消置顶
- 自动记住上下班时间与日薪
- 原生 WinUI 3 界面，支持 Windows 深色/浅色主题
- 可发布为单个 `DwhOffWork.exe`（首次运行会把 WinUI 运行依赖解压到临时目录）

## 获取 exe

GitHub Actions 会在推送到 `main`、推送 `v*` 标签或手动触发时构建。进入仓库的 **Actions → Build DwhOffWork Windows EXE → Run workflow**，完成后在页面底部下载 `DwhOffWork-win-x64` artifact。

## Windows 本地构建

需要 Windows 10 1809 及以上、.NET 10 SDK，以及包含 Windows 应用开发组件的 Visual Studio 2026。

```powershell
dotnet test tests\DwhOffWork.Core.Tests\DwhOffWork.Core.Tests.csproj -c Release

dotnet publish src\DwhOffWork\DwhOffWork.csproj `
  -c Release `
  -r win-x64 `
  -p:Platform=x64 `
  -o artifacts\win-x64
```

构建结果位于 `artifacts\win-x64\DwhOffWork.exe`，可以直接复制给 DWH 使用。

## 开发运行

在 Windows 上打开 `DwhOffWork.slnx`，或执行：

```powershell
dotnet build src\DwhOffWork\DwhOffWork.csproj -c Debug -p:Platform=x64
```

设置保存在 `%LOCALAPPDATA%\DwhOffWork\settings.json`。
