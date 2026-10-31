; YT Downloader installer (Inno Setup 6). Build it with scripts\make-release.cmd, which passes
; AppVersion and SourceDir. Installs per user by default (no administrator rights needed).

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\pyinstaller\dist\YT Downloader"
#endif

[Setup]
AppId={{6C1E5F0B-7A2D-4F38-9B1C-3D7E2A9F4B61}
AppName=YT Downloader
AppVersion={#AppVersion}
AppVerName=YT Downloader {#AppVersion}
AppPublisher=victorhmrod
AppPublisherURL=https://github.com/victorhmrod/yt-downloader
AppSupportURL=https://github.com/victorhmrod/yt-downloader/issues
AppUpdatesURL=https://github.com/victorhmrod/yt-downloader/releases
DefaultDirName={autopf}\YT Downloader
DefaultGroupName=YT Downloader
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=YTDownloader-{#AppVersion}-setup
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\YT Downloader.exe
LicenseFile=..\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoDescription=YT Downloader Setup
VersionInfoProductName=YT Downloader

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.RemoveSettings=Also delete your YT Downloader settings (download folder, default format and quality)?%n%nDownloaded videos are never deleted.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\YT Downloader"; Filename: "{app}\YT Downloader.exe"
Name: "{autodesktop}\YT Downloader"; Filename: "{app}\YT Downloader.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\YT Downloader.exe"; Description: "{cm:LaunchProgram,YT Downloader}"; Flags: nowait postinstall skipifsilent

[Code]
// Uninstalling removes the program only. Settings (HKCU\Software\YTDownloader) are kept unless the
// user says otherwise; downloaded files are never touched. Silent uninstalls always keep settings.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    if RegKeyExists(HKCU, 'Software\YTDownloader') and (not UninstallSilent) then
      if MsgBox(CustomMessage('RemoveSettings'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        RegDeleteKeyIncludingSubkeys(HKCU, 'Software\YTDownloader');
end;
