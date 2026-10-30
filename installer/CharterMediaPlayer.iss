; Call To ORDER installer. Keep AppId and install path stable for later updates.
#ifndef CharterBinary
  #define CharterBinary "..\build\CharterMediaPlayer.exe"
#endif

[Setup]
AppId={{CBBD77EA-6212-49EE-9433-97257FC637CF}
AppName=Charter Media Player
AppVersion=1.1.0
AppVerName=Charter Media Player 1.1.0 - Call To ORDER
AppPublisher=Super's Network
DefaultDirName={localappdata}\Programs\Charter Media Player
DefaultGroupName=Charter Media Player
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
UsePreviousAppDir=yes
OutputDir=..\build
OutputBaseFilename=CharterMediaPlayer-1.1.0-Setup
SetupIconFile=..\assets\CharterMediaPlayer.ico
UninstallDisplayIcon={app}\CharterMediaPlayer.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
AppMutex=Local\SuperNetwork.CharterMediaPlayer.SingleInstance,Local\SuperNetwork.CharterMediaPlayer.Running
VersionInfoVersion=1.1.0.1026
VersionInfoDescription=Charter Media Player - Call To ORDER installer

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#CharterBinary}"; DestDir: "{app}"; DestName: "CharterMediaPlayer.exe"; Flags: ignoreversion
Source: "..\release.json"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Charter Media Player"; Filename: "{app}\CharterMediaPlayer.exe"
Name: "{autodesktop}\Charter Media Player"; Filename: "{app}\CharterMediaPlayer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CharterMediaPlayer.exe"; Parameters: "--register"; Flags: runhidden waituntilterminated
Filename: "{app}\CharterMediaPlayer.exe"; Description: "Launch Charter Media Player"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\CharterMediaPlayer.exe"; Parameters: "--unregister"; Flags: runhidden waituntilterminated; RunOnceId: "UnregisterCharterMedia"

[Code]
var
  MaintenancePage: TInputOptionWizardPage;
  UninstallExecutable: String;
  MaintenanceUninstalled: Boolean;

function FindInstalledUninstaller: Boolean;
var
  Command: String;
begin
  Result := RegQueryStringValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{CBBD77EA-6212-49EE-9433-97257FC637CF}_is1',
    'UninstallString', Command);
  if Result then
  begin
    UninstallExecutable := RemoveQuotes(Command);
    Result := FileExists(UninstallExecutable);
  end;
end;

procedure InitializeWizard;
begin
  if not WizardSilent and FindInstalledUninstaller then
  begin
    MaintenancePage := CreateInputOptionPage(wpWelcome,
      'Charter Media Player is already installed',
      'Choose what you would like to do.',
      'Repair will reinstall Charter. Uninstall removes the program and keeps your media library.',
      True, False);
    MaintenancePage.Add('&Repair Charter Media Player');
    MaintenancePage.Add('&Uninstall Charter Media Player');
    MaintenancePage.SelectedValueIndex := 0;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ExitCode: Integer;
begin
  Result := True;
  if (MaintenancePage <> nil) and (CurPageID = MaintenancePage.ID) and
     (MaintenancePage.SelectedValueIndex = 1) then
  begin
    if not Exec(UninstallExecutable, '', '', SW_SHOW,
      ewWaitUntilTerminated, ExitCode) or (ExitCode <> 0) then
    begin
      MsgBox('Charter could not be uninstalled. Close all Charter windows and try again.',
        mbError, MB_OK);
      Result := False;
      Exit;
    end;
    MaintenanceUninstalled := True;
    WizardForm.Close;
    Result := False;
  end;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if MaintenanceUninstalled then Confirm := False;
end;
