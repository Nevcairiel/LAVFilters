#define version_major = 0
#define version_minor = 23

; ToDo
; - Maybe create custom page for the format selection. That for example allows using two columns to use space more effectively.

[Setup]
AllowCancelDuringInstall=no
AllowNoIcons=yes
AllowUNCPath=no
AppId=lavfilters
AppName=LAV Filters
AppVerName=LAV Filters {#=version_major}.{#=version_minor}
AppVersion={#=version_major}.{#=version_minor}
Compression=lzma/ultra
CreateAppDir=yes
DefaultDirName={pf}\LAV Filters
DefaultGroupName=LAV Filters
DisableStartupPrompt=yes
MinVersion=0,5.01SP2
OutputBaseFilename=LAVFilters-{#=version_major}.{#=version_minor}
OutputDir=.
PrivilegesRequired=admin
SolidCompression=yes
Uninstallable=yes
VersionInfoVersion={#=version_major}.{#=version_minor}.0.0

[Messages]
WelcomeLabel1=[name/ver]
WelcomeLabel2=This will install [name] on your computer.%n%nIt is recommended that you close all other applications before continuing.
WinVersionTooLowError=This software only works on Windows XP SP2 and newer.

[Types]
Name: Normal; Description: Normal; Flags: iscustom

[Components]
Name: lavsplitter32; Description: LAV Splitter (x86); Types: Normal;
Name: lavsplitter64; Description: LAV Splitter (x64); Types: Normal; Check: IsWin64;
Name: lavaudio32; Description: LAV Audio (x86); Types: Normal;
Name: lavaudio64; Description: LAV Audio (x64); Types: Normal; Check: IsWin64;

[Tasks]
Name: lavs32_avi;    Description: AVI;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_bluray; Description: Blu-ray;  GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_mkv;    Description: Matroska; GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_mp4;    Description: MP4;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_ogg;    Description: Ogg;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_flv;    Description: FLV;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_ts;     Description: MPEG-TS;  GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32
Name: lavs32_ps;     Description: MPEG-PS;  GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32;               
Name: lavs32_wtv;    Description: WTV;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32; Flags: unchecked;
Name: lavs32_wmv;    Description: WMV;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32; Flags: unchecked;
Name: lavs32_rm;     Description: RealMedia;GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_flac;   Description: FLAC;     GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_aac;    Description: AAC;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_amr;    Description: AMR;      GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_wv;     Description: WavPack;  GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_mpc;    Description: Musepack; GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;
Name: lavs32_tta;    Description: TrueAudio;GroupDescription: "Use LAV Splitter (x86) for these file formats:"; Components: lavsplitter32 AND lavaudio32; Flags: unchecked;

Name: lavs64_avi;    Description: AVI;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_bluray; Description: Blu-ray;  GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_mkv;    Description: Matroska; GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_mp4;    Description: MP4;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_ogg;    Description: Ogg;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_flv;    Description: FLV;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_ts;     Description: MPEG-TS;  GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_ps;     Description: MPEG-PS;  GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64
Name: lavs64_wtv;    Description: WTV;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64; Flags: unchecked;
Name: lavs64_wmv;    Description: WMV;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64; Flags: unchecked;
Name: lavs64_rm;     Description: RealMedia;GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked; 
Name: lavs64_flac;   Description: FLAC;     GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;
Name: lavs64_aac;    Description: AAC;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;
Name: lavs64_amr;    Description: AMR;      GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;
Name: lavs64_wv;     Description: WavPack;  GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;
Name: lavs64_mpc;    Description: Musepack; GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;
Name: lavs64_tta;    Description: TrueAudio;GroupDescription: "Use LAV Splitter (x64) for these file formats:"; Components: lavsplitter64 AND lavaudio64; Flags: unchecked;

[Files]
Source: bin_Win32\avcodec-53.dll;  DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32
Source: bin_Win32\avformat-53.dll; DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32
Source: bin_Win32\avutil-51.dll;   DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32
Source: bin_Win32\libbluray.dll;   DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32
Source: bin_Win32\LAVAudio.ax;     DestDir: {app}\x86; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio32
Source: bin_Win32\LAVSplitter.ax;  DestDir: {app}\x86; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32

Source: bin_x64\avcodec-53.dll;  DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: bin_x64\avformat-53.dll; DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: bin_x64\avutil-51.dll;   DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: bin_x64\libbluray.dll;   DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64
Source: bin_x64\LAVAudio.ax;     DestDir: {app}\x64; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio64
Source: bin_x64\LAVSplitter.ax;  DestDir: {app}\x64; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64

Source: COPYING;                 DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: README.txt;              DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: CHANGELOG.txt;           DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64

[Icons]
Name: {group}\Uninstall LAV Filters; Filename: {uninstallexe};

[Registry]
Root: HKCU; Subkey: Software\LAV; Flags: uninsdeletekeyifempty; Components: lavsplitter32 lavsplitter64 lavaudio32 lavaudio64
Root: HKCU; Subkey: Software\LAV\Audio; Flags: uninsdeletekey; Components: lavaudio32 lavaudio64
Root: HKCU; Subkey: Software\LAV\Splitter; Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64
Root: HKCU; Subkey: Software\LAV\Splitter\Formats; Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64

[Code]
const
   VC2010_x86     = '{196BB40D-1578-3D01-B289-BEFC77A11A1E}';
   VC2010_x64     = '{DA5E371C-6333-3D8A-93A4-6FD5B20BCC6E}';
   VC2010_SP1_x86 = '{F0C3E5D1-1ADE-321E-8167-68EF0DE699A5}';
   VC2010_SP1_x64 = '{1D8E6291-B0D5-35EC-8441-6616F567A0F7}';

function HasMSI(): Boolean;
begin
	Result := FileExists(ExpandConstant('{sys}\msi.dll'));
end;

function MsiQueryProductState(szProduct: AnsiString): Integer; external 'MsiQueryProductStateA@msi.dll stdcall';

function HasVCRuntime(guid: String): Boolean;
begin
	Result := FileExists(ExpandConstant('{sys}\msi.dll')) AND (MsiQueryProductState(guid) = 5);
end;

function HasVCRuntime_2010_x86(): Boolean;
begin
	Result := HasMSI AND (HasVCRuntime(VC2010_x86) OR HasVCRuntime(VC2010_SP1_x86));
end;

function HasVCRuntime_2010_x64(): Boolean;
begin
	Result := HasMSI AND (HasVCRuntime(VC2010_x64) OR HasVCRuntime(VC2010_SP1_x64));
end;

function DetectVC2010Runtime(): Boolean;
var
  prompt_shown: Boolean;
  ResultCode: Integer;
begin
  Result := True;

  if NOT HasVCRuntime_2010_x86 then begin
    if (msgbox('LAV Filter requires the "Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)".'#13#10'That runtime package is currently not yet installed on your computer.'#13#10#13#10'Would you like to visit the Microsoft website to download the runtime package?', mbConfirmation, mb_yesno) = IDYES) then begin
      ShellExec('open', 'http://www.microsoft.com/downloads/en/details.aspx?FamilyID=C32F406A-F8FC-4164-B6EB-5328B8578F03', '', '', SW_SHOW, ewNoWait, ResultCode);
    end;
    prompt_shown := True;
    Result := False;
  end;
  if IsWin64 AND NOT HasVCRuntime_2010_x64 then begin
    if prompt_shown OR (msgbox('LAV Filter requires the "Microsoft Visual C++ 2010 SP1 Redistributable Package (x64)".'#13#10'That runtime package is currently not yet installed on your computer.'#13#10#13#10'Would you like to visit the Microsoft website to download the runtime package?', mbConfirmation, mb_yesno) = IDYES) then begin
      ShellExec('open', 'http://www.microsoft.com/downloads/en/details.aspx?FamilyID=C68CCBB6-75EF-4C9D-A326-879EAB4FCDF8', '', '', SW_SHOW, ewNoWait, ResultCode);
    end;
    Result := False;
  end;
end;

procedure CleanMediaTypeExt(rootkey: Integer; extension, clsid: String);
var
  temp: String;
begin
  if RegQueryStringValue(rootkey, 'Media Type\Extensions\' + extension, 'Source Filter', temp) then begin
    if Lowercase(clsid) = Lowercase(temp) then begin
      RegDeleteValue(rootkey, 'Media Type\Extensions\' + extension, 'Source Filter');
      RegDeleteKeyIfEmpty(rootkey, 'Media Type\Extensions\' + extension);
    end;
  end;
end;

procedure ConfigureFormat(rootkey: Integer; format: String; value: Boolean);
begin
  RegWriteDWordValue(rootkey, 'Software\LAV\Splitter\Formats', format, ord(value));
end;

procedure CleanMediaTypeExt32(extension: String);
begin
  CleanMediaTypeExt(HKCR32, extension, '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure CleanMediaTypeExt64(extension: String);
begin
  CleanMediaTypeExt(HKCR64, extension, '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure SetMediaTypeExt32(extension: String);
begin
  RegWriteStringValue(HKCR32, 'Media Type\Extensions\' + extension, 'Source Filter', '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure SetMediaTypeExt64(extension: String);
begin
  RegWriteStringValue(HKCR64, 'Media Type\Extensions\' + extension, 'Source Filter', '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure DoExtension32(extension, option: String);
begin
  if IsTaskSelected(option) then begin
    SetMediaTypeExt32(extension);
  end
  else begin
    CleanMediaTypeExt32(extension);
  end;
end;

procedure DoExtension64(extension, option: String);
begin
  if IsTaskSelected(option) then begin
    SetMediaTypeExt64(extension);
  end
  else begin
    CleanMediaTypeExt64(extension);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) then begin
    // Disable unwanted formats
    if IsComponentSelected('lavsplitter32') then begin
      ConfigureFormat(HKCU32, 'mkv', IsTaskSelected('lavs32_mkv'));
      ConfigureFormat(HKCU32, 'bluray', IsTaskSelected('lavs32_bluray'));
      ConfigureFormat(HKCU32, 'mp4', IsTaskSelected('lavs32_mp4'));
      ConfigureFormat(HKCU32, 'avi', IsTaskSelected('lavs32_avi'));
      ConfigureFormat(HKCU32, 'flv', IsTaskSelected('lavs32_flv'));
      ConfigureFormat(HKCU32, 'ogg', IsTaskSelected('lavs32_ogg'));
      ConfigureFormat(HKCU32, 'mp4', IsTaskSelected('lavs32_mp4'));
      ConfigureFormat(HKCU32, 'mpeg', IsTaskSelected('lavs32_ps'));
      ConfigureFormat(HKCU32, 'mpegts', IsTaskSelected('lavs32_ts'));
      DoExtension32('.wtv', 'lavs32_wtv');
      DoExtension32('.wmv', 'lavs32_wmv');
      DoExtension32('.rm', 'lavs32_rm');
      DoExtension32('.rmvb', 'lavs32_rm');
      DoExtension32('.flac', 'lavs32_flac');
      DoExtension32('.aac', 'lavs32_aac');
      DoExtension32('.amr', 'lavs32_amr');
      DoExtension32('.wv', 'lavs32_amr');
      DoExtension32('.mpc', 'lavs32_mpc');
      DoExtension32('.tta', 'lavs32_tta');
    end;

    if IsComponentSelected('lavsplitter64') then begin
      ConfigureFormat(HKCU64, 'mkv', IsTaskSelected('lavs64_mkv'));
      ConfigureFormat(HKCU64, 'bluray', IsTaskSelected('lavs64_bluray'));
      ConfigureFormat(HKCU64, 'mp4', IsTaskSelected('lavs64_mp4'));
      ConfigureFormat(HKCU64, 'avi', IsTaskSelected('lavs64_avi'));
      ConfigureFormat(HKCU64, 'flv', IsTaskSelected('lavs64_flv'));
      ConfigureFormat(HKCU64, 'ogg', IsTaskSelected('lavs64_ogg'));
      ConfigureFormat(HKCU64, 'mp4', IsTaskSelected('lavs64_mp4'));
      ConfigureFormat(HKCU64, 'mpeg', IsTaskSelected('lavs64_ps'));
      ConfigureFormat(HKCU64, 'mpegts', IsTaskSelected('lavs64_ts'));
      DoExtension64('.wtv', 'lavs64_wtv');
      DoExtension64('.wmv', 'lavs64_wmv');
      DoExtension64('.rm', 'lavs64_rm');
      DoExtension64('.rmvb', 'lavs64_rm');
      DoExtension64('.flac', 'lavs64_flac');
      DoExtension64('.aac', 'lavs64_aac');
      DoExtension64('.amr', 'lavs64_amr');
      DoExtension64('.wv', 'lavs64_amr');
      DoExtension64('.mpc', 'lavs64_mpc');
      DoExtension64('.tta', 'lavs64_tta');
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) then begin
    CleanMediaTypeExt32('.wtv');
    CleanMediaTypeExt32('.wmv');
    CleanMediaTypeExt32('.rm');
    CleanMediaTypeExt32('.rmvb');
    CleanMediaTypeExt32('.flac');
    CleanMediaTypeExt32('.aac');
    CleanMediaTypeExt32('.amr');
    CleanMediaTypeExt32('.wv');
    CleanMediaTypeExt32('.mpc');
    CleanMediaTypeExt32('.tta');

    if IsWin64 then begin
      CleanMediaTypeExt64('.wtv');
      CleanMediaTypeExt64('.wmv');
      CleanMediaTypeExt64('.rm');
      CleanMediaTypeExt64('.rmvb');
      CleanMediaTypeExt64('.flac');
      CleanMediaTypeExt64('.aac');
      CleanMediaTypeExt64('.amr');
      CleanMediaTypeExt64('.wv');
      CleanMediaTypeExt64('.mpc');
      CleanMediaTypeExt64('.tta');
    end;
  end;
end;

function InitializeSetup(): Boolean;
begin
	Result := DetectVC2010Runtime;
end;

procedure InitializeWizard();
begin
  // Adjust tasks page
	WizardForm.SelectTasksLabel.Hide;
	WizardForm.TasksList.Top    := 0;
	WizardForm.TasksList.Height := PageFromID(wpSelectTasks).SurfaceHeight;
end;
