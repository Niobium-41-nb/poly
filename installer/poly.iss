; poly 出题工作台 —— Inno Setup 6 安装脚本
;
; 编译： "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer\poly.iss
; 产物： dist\poly-<版本>-setup.exe
;
; 本文件与 installer\ChineseSimplified.isl 都必须保存为 UTF-8 带 BOM，
; 否则 Inno 会按 ANSI 读取，中文向导会变成乱码。
;
; 安装行为：用户级安装（免管理员），默认装到 %LOCALAPPDATA%\Programs\poly，
; 可选把该目录加入用户 PATH，卸载时会把 PATH 还原。

#define AppName        "poly 出题工作台"
#define AppShortName   "poly"
#define AppVersion     "0.1.0"
#define AppPublisher   "Niobium-41-nb"
#define AppURL         "https://github.com/Niobium-41-nb/poly"
#define ExeName        "poly.exe"
#define GuiExeName     "poly-gui.exe"

[Setup]
AppId={{6083BA86-8384-4B26-973C-5F3E5A6E9ADA}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription={#AppName} 安装程序
VersionInfoCompany={#AppPublisher}
VersionInfoCopyright=Copyright (C) 2026 {#AppPublisher}
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
DefaultDirName={autopf}\{#AppShortName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
SourceDir=..
OutputDir=dist
OutputBaseFilename={#AppShortName}-{#AppVersion}-setup
SetupIconFile=assets\icon.ico
UninstallDisplayIcon={app}\{#GuiExeName}
UninstallDisplayName={#AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes
ShowLanguageDialog=no

[Languages]
Name: "chinese"; MessagesFile: "installer\ChineseSimplified.isl"

[Tasks]
Name: "addtopath"; Description: "把 poly 加入 PATH（推荐：之后在任意目录都能直接用 poly 命令）"
Name: "desktopicon"; Description: "创建桌面快捷方式（窗口界面）"; Flags: unchecked

[Files]
Source: "bin\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\{#GuiExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "testlib\testlib.h"; DestDir: "{app}\testlib"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#GuiExeName}"; Comment: "窗口界面：导入题目、造数据、评测、打包导出"
Name: "{group}\使用说明（README）"; Filename: "{app}\README.md"
Name: "{group}\卸载 {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#GuiExeName}"; Tasks: desktopicon

[Registry]
; {olddata} 会保留用户已有的 PATH；ChangesEnvironment 会让系统广播环境变量变更
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; \
    ValueData: "{olddata};{app}"; Tasks: addtopath; \
    Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
Filename: "{app}\README.md"; Description: "查看使用说明"; \
    Flags: postinstall shellexec skipifsilent
Filename: "{app}\{#GuiExeName}"; Description: "运行 {#AppName}（窗口界面）"; \
    Flags: postinstall nowait skipifsilent unchecked

[Code]
const
  EnvSubKey = 'Environment';

// PATH 里是否已经有这个目录
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath) then
  begin
    Result := True;
    Exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

// 去掉 PATH 里的某个目录（顺带把重复的空段一并规整）
function PathWithoutSegment(const Path, Segment: string): string;
var
  Rest, Item: string;
  P: Integer;
begin
  Result := '';
  Rest := Path + ';';
  while Rest <> '' do
  begin
    P := Pos(';', Rest);
    Item := Trim(Copy(Rest, 1, P - 1));
    Delete(Rest, 1, P);
    if (Item = '') or (CompareText(Item, Segment) = 0) then
      Continue;
    if Result <> '' then
      Result := Result + ';';
    Result := Result + Item;
  end;
end;

// 卸载时把安装目录从用户 PATH 里摘掉（只动我们自己加的那一段）
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  OrigPath, NewPath: string;
begin
  if CurUninstallStep <> usUninstall then
    Exit;
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath) then
    Exit;
  NewPath := PathWithoutSegment(OrigPath, ExpandConstant('{app}'));
  if NewPath <> OrigPath then
    RegWriteExpandStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', NewPath);
end;
