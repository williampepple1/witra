; Witra Installer Script for Inno Setup

#define MyAppName "Witra"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Witra"
#define MyAppURL "https://github.com/witra"
#define MyAppExeName "witra.exe"
#define MyAppDescription "Wireless File Transfer"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=.
OutputBaseFilename=Witra-Setup-{#MyAppVersion}
SetupIconFile=resources\icons\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=120
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Branding
AppComments={#MyAppDescription}
AppContact=support@witra.app
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
VersionInfoVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel1=Welcome to [name] Setup
WelcomeLabel2=This will install [name/ver] on your computer.%n%n[name] lets you transfer files and folders between computers connected to the same WiFi network - fast, secure, and without internet.%n%nFeatures:%n  • Automatic device discovery on your network%n  • Send files and folders of any size%n  • Drag and drop support%n  • Real-time transfer progress%n%nClick Next to continue.

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "startupicon"; Description: "Start {#MyAppName} when Windows starts"; GroupDescription: "Startup options:"; Flags: unchecked

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppDescription}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startupicon

[Registry]
Root: HKCU; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "DownloadPath"; ValueData: "{code:GetDownloadPath}"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
var
  DownloadPage: TInputDirWizardPage;
  DefaultDownloadPath: String;

procedure InitializeWizard;
begin
  // Create the download folder selection page after the directory selection page
  DefaultDownloadPath := ExpandConstant('{userdocs}\Witra Downloads');
  
  DownloadPage := CreateInputDirPage(wpSelectDir,
    'Select Download Folder',
    'Where should received files be saved?',
    'Witra will save files received from other devices to this folder.' + #13#10 + #13#10 +
    'Click Next to continue, or click Browse to select a different folder.',
    False, '');
  
  DownloadPage.Add('Download folder:');
  DownloadPage.Values[0] := DefaultDownloadPath;
end;

function GetDownloadPath(Param: String): String;
begin
  Result := DownloadPage.Values[0];
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  DownloadDir: String;
begin
  if CurStep = ssPostInstall then
  begin
    // Create the download directory if it doesn't exist
    DownloadDir := DownloadPage.Values[0];
    if not DirExists(DownloadDir) then
      ForceDirectories(DownloadDir);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  
  // Validate the download folder path
  if CurPageID = DownloadPage.ID then
  begin
    if DownloadPage.Values[0] = '' then
    begin
      MsgBox('Please select a download folder.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;
