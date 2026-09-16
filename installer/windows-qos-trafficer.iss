; ---------------------------------------------------------------------------
; Windows QoS Trafficer — Inno Setup installer (desktop edition, x64)
;
; Bundles the GUI app plus the signed WinDivert runtime. The WinDivert
; kernel driver installs itself silently on first run (WinDivertOpen),
; so the installer only needs to place WinDivert.dll + WinDivert64.sys
; next to the executable and require Administrator rights.
; ---------------------------------------------------------------------------

#define AppName "Windows QoS Trafficer"
#define AppVersion "1.0.0"
#define AppExeName "WindowsQoSTrafficer.exe"
#define AppPublisher "PWare"
#define AppId "{{B4E7D2A9-3C5F-4A18-9E6D-1F2B8C7A4E60}"

[Setup]
AppId={{#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=WindowsQoSTrafficer-Setup-{#AppVersion}
OutputDir=..\dist
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\bt-app.ico
UninstallDisplayIcon={app}\{#AppExeName}
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
; Desktop app (GUI, x64)
Source: "..\src\release\GUI\x64\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; WinDivert runtime (signed, LGPL v3) — driver auto-installed on first run
Source: "windivert\x64\WinDivert.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "windivert\x64\WinDivert64.sys"; DestDir: "{app}"; Flags: ignoreversion
; WinDivert license (LGPL v3 redistribution requirement)
Source: "windivert\LICENSE"; DestDir: "{app}"; DestName: "WinDivert-LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
