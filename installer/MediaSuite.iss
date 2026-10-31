; Media Suite Windows installer. Build with scripts/make-suite-release.ps1.
#ifndef AppVersion
  #define AppVersion "1.2.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\MediaSuite-" + AppVersion
#endif

[Setup]
AppId={{BF48B7AD-D1F1-45ED-AB6A-8DF807F66CAB}
AppName=Media Suite
AppVersion={#AppVersion}
AppVerName=Media Suite {#AppVersion}
AppPublisher=victorhmrod
AppPublisherURL=https://github.com/victorhmrod/MediaSuite
AppSupportURL=https://github.com/victorhmrod/MediaSuite/issues
AppUpdatesURL=https://github.com/victorhmrod/MediaSuite/releases
DefaultDirName={autopf}\Media Suite
DefaultGroupName=Media Suite
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=MediaSuite-{#AppVersion}-setup
SetupIconFile=..\suite\vendor\localflix\resources\localflix.ico
UninstallDisplayIcon={app}\MediaSuite.exe
LicenseFile=..\LICENSE
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Media Suite Setup
VersionInfoProductName=Media Suite

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Media Suite"; Filename: "{app}\MediaSuite.exe"
Name: "{autodesktop}\Media Suite"; Filename: "{app}\MediaSuite.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MediaSuite.exe"; Description: "{cm:LaunchProgram,Media Suite}"; Flags: nowait postinstall skipifsilent
