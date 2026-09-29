; OffWork per-user installer (Inno Setup 6).
;
; Per-user on purpose: PrivilegesRequired=lowest means no UAC prompt and no
; administrator, and {autopf} then resolves to {localappdata}\Programs.
;
; Chinese wizard text is provided inline in [Messages] rather than by shipping a
; third-party ChineseSimplified.isl: Inno Setup does not bundle one, and vendoring
; a translation would add a file whose licensing and CI availability we would
; have to guarantee. Everything the user actually reads is covered here.

#ifndef SourceExe
  #define SourceExe "..\build\Release\OffWork.exe"
#endif
#ifndef AppVersion
  #define AppVersion "0.3.0"
#endif

#define AppName "OffWork"
#define AppPublisher "alertform"
#define AppExeName "OffWork.exe"
#define RunKey "Software\Microsoft\Windows\CurrentVersion\Run"
#define RunValueName "OffWork"

[Setup]
; Never change AppId: it is what lets a future version upgrade this one in place.
AppId={{8F3A1C2E-5B47-4D9A-9E61-7C0D2A6F4B83}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=no
AllowNoIcons=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\build\installer
OutputBaseFilename=OffWork-Setup
SetupIconFile=..\resources\offwork.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; No AppMutex: since 0.3.0 the title-bar close button hides OffWork to the tray,
; so "please close all instances" became a prompt the user cannot satisfy by
; closing the window -- with the widget hidden there is nothing on screen to
; close, and OK just re-prompts. Instead [Code] below asks the running app to
; exit (both at install and at uninstall), and Restart Manager stays on as a
; safety net for anything else holding the files.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "default"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=安装程序
SetupWindowTitle=安装 - %1
ButtonBack=< 上一步(&B)
ButtonNext=下一步(&N) >
ButtonInstall=安装(&I)
ButtonCancel=取消
ButtonFinish=完成(&F)
ButtonBrowse=浏览(&R)...
ButtonWizardBrowse=浏览(&R)...
ButtonNewFolder=新建文件夹(&M)
ButtonYes=是(&Y)
ButtonNo=否(&N)
ButtonOK=确定
ExitSetupTitle=退出安装程序
ExitSetupMessage=安装尚未完成。现在退出的话, OffWork 不会被安装。确定要退出吗?
WizardSelectDir=选择安装位置
SelectDirDesc=您想把 OffWork 安装到哪里?
SelectDirLabel3=安装程序会把 OffWork 安装到下面的文件夹。
SelectDirBrowseLabel=点「下一步」继续。要换一个位置, 点「浏览」。
DiskSpaceGBLabel=至少需要 [gb] GB 可用磁盘空间。
DiskSpaceMBLabel=至少需要 [mb] MB 可用磁盘空间。
InvalidPath=请输入完整路径, 含盘符, 例如 C:\APP
DirExistsTitle=文件夹已存在
DirExists=文件夹%n%n%1%n%n已经存在。仍要安装到这个文件夹吗?
WizardSelectTasks=选择附加任务
SelectTasksDesc=您想让安装程序额外做些什么?
SelectTasksLabel2=选择安装 OffWork 时要执行的附加任务, 然后点「下一步」。
WizardReady=准备安装
ReadyLabel1=安装程序已准备好把 OffWork 安装到您的电脑。
ReadyLabel2a=点「安装」开始安装。要检查或修改设置, 点「上一步」。
ReadyLabel2b=点「安装」开始安装。
ReadyMemoDir=安装位置:
ReadyMemoTasks=附加任务:
WizardInstalling=正在安装
InstallingLabel=正在把 OffWork 安装到您的电脑, 请稍候。
FinishedHeadingLabel=OffWork 安装完成
FinishedLabel=OffWork 已经安装到您的电脑, 可以从开始菜单启动。
ClickFinish=点「完成」结束安装。
ConfirmUninstall=确定要完全卸载 OffWork 及其组件吗?
UninstallStatusLabel=正在从您的电脑移除 OffWork, 请稍候。
StatusExtractFiles=正在解压文件...
StatusUninstalling=正在卸载 OffWork...
WizardPreparing=准备安装
PreparingDesc=安装程序正在准备把 OffWork 安装到您的电脑。
ApplicationsFound=下列程序正在使用需要更新的文件。建议让安装程序自动关闭它们。
ApplicationsFound2=下列程序正在使用需要更新的文件。建议让安装程序自动关闭它们, 安装完成后会尝试重新启动。
CloseApplications=自动关闭这些程序(&A)
DontCloseApplications=不关闭(&D)
ErrorCloseApplications=安装程序无法自动关闭所有程序。建议先关闭正在使用待更新文件的程序再继续。
SetupAppRunningError=安装程序检测到 %1 正在运行。%n%n请先退出它 (托盘图标右键 → 退出), 然后点「确定」继续, 或点「取消」退出安装。
UninstallAppRunningError=卸载程序检测到 %1 正在运行。%n%n请先退出它 (托盘图标右键 → 退出), 然后点「确定」继续, 或点「取消」退出卸载。
UninstalledMost=%1 已卸载。%n%n有些文件未能删除, 可以手动删除。
UninstalledAll=%1 已从您的电脑移除。
UninstallAppTitle=卸载
UninstallAppFullTitle=卸载 %1

[CustomMessages]
default.CreateDesktopIcon=创建桌面快捷方式
default.EnableAutostart=开机自启 (登录时自动运行 OffWork)
default.LaunchAfterInstall=安装完成后运行 OffWork
default.AdditionalIcons=快捷方式:
default.StartupOptions=启动选项:

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "{cm:EnableAutostart}"; GroupDescription: "{cm:StartupOptions}"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; The app reads and writes this exact key and value name (src/autostart.h), and
; stores the path quoted so a directory with spaces still launches.
Root: HKCU; Subkey: "{#RunKey}"; ValueType: string; ValueName: "{#RunValueName}"; \
    ValueData: """{app}\{#AppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart
; Without the task, make sure no stale value survives an install, and still make
; uninstall remove one the user later switched on from inside the app.
Root: HKCU; Subkey: "{#RunKey}"; ValueType: none; ValueName: "{#RunValueName}"; \
    Flags: deletevalue uninsdeletevalue; Tasks: not autostart

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchAfterInstall}"; \
    Flags: nowait postinstall skipifsilent

[Code]
// Ask a running OffWork to exit before its files are replaced or removed.
//
// WM_ENDSESSION(TRUE) is the exit path 0.3.0+ implements for exactly this (its
// WM_CLOSE only hides to the tray). WM_CLOSE is still sent for 0.2.0, which
// exits on it and ignores WM_ENDSESSION. Both go to OffWork's own window only.
const
  WM_CLOSE = $0010;
  WM_ENDSESSION = $0016;
  OffWorkWindowClass = 'OffWork.Native.Widget';

function CloseRunningOffWork(): Boolean;
var
  Wnd: HWND;
  Waited: Integer;
begin
  Wnd := FindWindowByClassName(OffWorkWindowClass);
  if Wnd = 0 then
  begin
    Result := True;
    Exit;
  end;
  Log('OffWork is running; asking it to exit.');
  PostMessage(Wnd, WM_ENDSESSION, 1, 0);
  PostMessage(Wnd, WM_CLOSE, 0, 0);
  Waited := 0;
  while (FindWindowByClassName(OffWorkWindowClass) <> 0) and (Waited < 8000) do
  begin
    Sleep(200);
    Waited := Waited + 200;
  end;
  // The window disappears slightly before the process lets go of its image.
  Sleep(500);
  Result := FindWindowByClassName(OffWorkWindowClass) = 0;
  if Result then
    Log('OffWork exited.')
  else
    Log('OffWork did not exit within 8 seconds.');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not CloseRunningOffWork() then
    Result := 'OffWork 仍在运行, 无法更新。请在托盘图标上右键选择「退出」, 然后重新运行安装程序。';
end;

// usAppMutexCheck runs after the user has confirmed the uninstall (so cancelling
// never closes OffWork as a side effect) and before Inno's own running-app
// check. That check still fires on machines first installed by 0.2.0: the
// uninstall log keeps the AppMutex that installer recorded, even after later
// upgrades by an installer that no longer sets one.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usAppMutexCheck then
    if not CloseRunningOffWork() then
      MsgBox('OffWork 仍在运行, 程序文件可能无法删除。卸载完成后请在托盘图标上右键选择「退出」, 再手动删除安装目录。',
        mbError, MB_OK);
end;
