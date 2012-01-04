#define version_major = 0
#define version_minor = 43

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
DisableDirPage=auto
DisableProgramGroupPage=auto

[Messages]
WelcomeLabel1=[name/ver]
WelcomeLabel2=This will install [name] on your computer.%n%nIt is recommended that you close all other applications before continuing.
WinVersionTooLowError=This software only works on Windows XP SP2 and newer.

[Types]
Name: Normal; Description: Normal; Flags: iscustom

[Components]
Name: lavsplitter32; Description: LAV Splitter (x86); Types: Normal;
Name: lavsplitter64; Description: LAV Splitter (x64); Types: Normal; Check: IsWin64;
Name: lavaudio32;    Description: LAV Audio (x86);    Types: Normal;
Name: lavaudio64;    Description: LAV Audio (x64);    Types: Normal; Check: IsWin64;
Name: lavvideo32;    Description: LAV Video (x86);    Types: Normal;
Name: lavvideo64;    Description: LAV Video (x64);    Types: Normal; Check: IsWin64;

[Tasks]
Name: lavs_avi;    Description: AVI;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_bluray; Description: Blu-ray;  GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_mkv;    Description: Matroska; GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_mp4;    Description: MP4;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_ogg;    Description: Ogg;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_flv;    Description: FLV;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_ts;     Description: MPEG-TS;  GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_ps;     Description: MPEG-PS;  GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_rm;     Description: RealMedia;GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64
Name: lavs_wtv;    Description: WTV;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_wmv;    Description: WMV;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_flac;   Description: FLAC;     GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_aac;    Description: AAC;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_amr;    Description: AMR;      GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_wv;     Description: WavPack;  GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_mpc;    Description: Musepack; GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;
Name: lavs_tta;    Description: TrueAudio;GroupDescription: "Use LAV Splitter for these file formats:"; Components: lavsplitter32 lavsplitter64; Flags: unchecked;

Name: lavv_h264;      Description: H.264/AVC1;         GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_vc1;       Description: VC-1;               GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64; Flags: unchecked;
Name: lavv_mpeg1;     Description: MPEG1;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_mpeg2;     Description: MPEG2;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_mpeg4;     Description: MPEG4;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_msmpeg4;   Description: MS-MPEG4;           GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_vp8;       Description: VP8;                GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_wmv3;      Description: WMV3;               GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_wmv12;     Description: WMV1/2;             GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_mjpeg;     Description: M-JPEG;             GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_theora;    Description: Theora;             GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_flv;       Description: Flash Video 1;      GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_vp6;       Description: VP6;                GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_svq;       Description: SVQ 1/3;            GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_h261;      Description: H.261;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_h263;      Description: H.263;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_indeo;     Description: Intel Indea;        GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_camtasia;  Description: TechSmith/Camtasia; GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_fraps;     Description: Fraps;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_huffyuv;   Description: HuffYUV;            GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_qtrle;     Description: QTRle;              GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_dvvideo;   Description: DV;                 GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_bink;      Description: Bink;               GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_smackvid;  Description: Smacker;            GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_rv12;      Description: Real Video 1/2;     GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64; Flags: unchecked;
Name: lavv_rv34;      Description: Real Video 3/4;     GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_lagarith;  Description: Lagarith;           GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64; Flags: unchecked;
Name: lavv_cinepak;   Description: Cinepak;            GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64; Flags: unchecked;
Name: lavv_camstudio; Description: Camstudio;          GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_qpeg;      Description: QPEG;               GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64; Flags: unchecked;
Name: lavv_zlib;      Description: ZLIB;               GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64
Name: lavv_rpza;      Description: QTRpza;             GroupDescription: "Use LAV Video for these codecs:"; Components: lavvideo32 lavvideo64

[Files]
Source: bin_Win32\avcodec-lav-53.dll;  DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\avfilter-lav-2.dll;  DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\avformat-lav-53.dll; DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32
Source: bin_Win32\avutil-lav-51.dll;   DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\swscale-lav-2.dll;   DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\libbluray.dll;       DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32
Source: bin_Win32\LAVAudio.ax;         DestDir: {app}\x86; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio32
Source: bin_Win32\LAVSplitter.ax;      DestDir: {app}\x86; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32
Source: bin_Win32\LAVVideo.ax;         DestDir: {app}\x86; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\IntelQuickSyncDecoder.dll; DestDir: {app}\x86; Flags: ignoreversion restartreplace uninsrestartdelete skipifsourcedoesntexist; Components: lavvideo32

Source: bin_x64\avcodec-lav-53.dll;    DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\avfilter-lav-2.dll;    DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\avformat-lav-53.dll;   DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: bin_x64\avutil-lav-51.dll;     DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\swscale-lav-2.dll;     DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\libbluray.dll;         DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64
Source: bin_x64\LAVAudio.ax;           DestDir: {app}\x64; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio64
Source: bin_x64\LAVSplitter.ax;        DestDir: {app}\x64; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64
Source: bin_x64\LAVVideo.ax;           DestDir: {app}\x64; Flags: regserver ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\IntelQuickSyncDecoder.dll; DestDir: {app}\x64; Flags: ignoreversion restartreplace uninsrestartdelete skipifsourcedoesntexist; Components: lavvideo32

Source: COPYING;                       DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete
Source: README.txt;                    DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete
Source: CHANGELOG.txt;                 DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete

[Icons]
Name: {group}\LAV Splitter Configuration;        Filename: rundll32.exe; Parameters: "LAVSplitter.ax,OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVSplitter.ax; IconIndex: 0; Components: lavsplitter32
Name: {group}\LAV Splitter Configuration;        Filename: rundll32.exe; Parameters: "LAVSplitter.ax,OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVSplitter.ax; IconIndex: 0; Components: lavsplitter64 AND NOT lavsplitter32
Name: {group}\LAV Audio Configuration;           Filename: rundll32.exe; Parameters: "LAVAudio.ax,OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVAudio.ax; IconIndex: 0; Components: lavaudio32
Name: {group}\LAV Audio Configuration;           Filename: rundll32.exe; Parameters: "LAVAudio.ax,OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVAudio.ax; IconIndex: 0; Components: lavaudio64 AND NOT lavaudio32
Name: {group}\LAV Video Configuration;           Filename: rundll32.exe; Parameters: "LAVVideo.ax,OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVVideo.ax; IconIndex: 0; Components: lavvideo32
Name: {group}\LAV Video Configuration;           Filename: rundll32.exe; Parameters: "LAVVideo.ax,OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVVideo.ax; IconIndex: 0; Components: lavvideo64 AND NOT lavvideo32
Name: {group}\Visit LAV Filters Home Page;       Filename: "http://1f0.de/"
Name: {group}\Visit LAV Filters on Doom9;        Filename: "http://forum.doom9.org/showthread.php?t=156191"
Name: {group}\Uninstall LAV Filters;             Filename: {uninstallexe};

[Registry]
Root: HKCU; Subkey: Software\LAV;                  Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: Software\LAV\Audio;            Flags: uninsdeletekey; Components: lavaudio32 lavaudio64
Root: HKCU; Subkey: Software\LAV\Splitter;         Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64
Root: HKCU; Subkey: Software\LAV\Splitter\Formats; Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64
Root: HKCU; Subkey: Software\LAV\Video;            Flags: uninsdeletekey; Components: lavvideo32 lavvideo64
Root: HKCU; Subkey: Software\LAV\Video\Formats;    Flags: uninsdeletekey; Components: lavvideo32 lavvideo64
Root: HKCU; Subkey: Software\LAV\Video\Output;     Flags: uninsdeletekey; Components: lavvideo32 lavvideo64

[Code]
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

procedure ConfigureVideoFormat(format: String);
begin
  RegWriteDWordValue(HKCU, 'Software\LAV\Video\Formats', format, ord(IsTaskSelected('lavv_' + format)));
end;

procedure CleanMediaTypeExt32(extension: String);
begin
  CleanMediaTypeExt(HKCR32, extension, '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure CleanMediaTypeExt64(extension: String);
begin
  CleanMediaTypeExt(HKCR64, extension, '{B98D13E7-55DB-4385-A33D-09FD1BA26338}');
end;

procedure CleanMediaTypeExtWrap(extension: String);
begin
  CleanMediaTypeExt32(extension);
  if IsWin64 then begin
    CleanMediaTypeExt64(extension);
  end;
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

procedure DoExtension(extension, option: String);
begin
  if IsComponentSelected('lavsplitter32') then begin
    DoExtension32(extension, option);
  end;
  if IsComponentSelected('lavsplitter64') then begin
    DoExtension64(extension, option);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) then begin
    // Disable unwanted formats
    if IsComponentSelected('lavsplitter32') or IsComponentSelected('lavsplitter64') then begin
      ConfigureFormat(HKCU, 'matroska', IsTaskSelected('lavs_mkv'));
      DoExtension('.mkv', 'lavs_mkv');
      DoExtension('.mka', 'lavs_mkv');
      ConfigureFormat(HKCU, 'bluray', IsTaskSelected('lavs_bluray'));
      DoExtension('.bdmv', 'lavs_bluray');
      DoExtension('.mpls', 'lavs_bluray');
      ConfigureFormat(HKCU, 'mp4', IsTaskSelected('lavs_mp4'));
      DoExtension('.mp4', 'lavs_mp4');
      DoExtension('.mov', 'lavs_mp4');
      DoExtension('.3gp', 'lavs_mp4');
      ConfigureFormat(HKCU, 'avi', IsTaskSelected('lavs_avi'));
      DoExtension('.avi', 'lavs_avi');
      DoExtension('.divx', 'lavs_avi');
      ConfigureFormat(HKCU, 'flv', IsTaskSelected('lavs_flv'));
      DoExtension('.flv', 'lavs_flv');
      ConfigureFormat(HKCU, 'ogg', IsTaskSelected('lavs_ogg'));
      DoExtension('.ogg', 'lavs_ogg');
      DoExtension('.ogm', 'lavs_ogg');
      DoExtension('.ogv', 'lavs_ogg');
      ConfigureFormat(HKCU, 'mpeg', IsTaskSelected('lavs_ps'));
      DoExtension('.mpg', 'lavs_ps');
      DoExtension('.vob', 'lavs_ps');
      DoExtension('.evo', 'lavs_ps');
      ConfigureFormat(HKCU, 'mpegts', IsTaskSelected('lavs_ts'));
      DoExtension('.ts', 'lavs_ts');
      DoExtension('.mts', 'lavs_ts');
      DoExtension('.m2ts', 'lavs_ts');
      DoExtension('.wtv', 'lavs_wtv');
      DoExtension('.wmv', 'lavs_wmv');
      DoExtension('.rm', 'lavs_rm');
      DoExtension('.rmvb', 'lavs_rm');
      DoExtension('.flac', 'lavs_flac');
      DoExtension('.aac', 'lavs_aac');
      DoExtension('.amr', 'lavs_amr');
      DoExtension('.wv', 'lavs_amr');
      DoExtension('.mpc', 'lavs_mpc');
      DoExtension('.tta', 'lavs_tta');
    end;

    if IsComponentSelected('lavvideo32') or IsComponentSelected('lavvideo64') then begin
        ConfigureVideoFormat('h264');
        ConfigureVideoFormat('vc1');
        ConfigureVideoFormat('mpeg1');
        ConfigureVideoFormat('mpeg2');
        ConfigureVideoFormat('mpeg4');
        ConfigureVideoFormat('msmpeg4');
        ConfigureVideoFormat('vp8');
        ConfigureVideoFormat('wmv3');
        ConfigureVideoFormat('wmv12');
        ConfigureVideoFormat('mjpeg');
        ConfigureVideoFormat('theora');
        ConfigureVideoFormat('flv');
        ConfigureVideoFormat('vp6');
        ConfigureVideoFormat('svq');
        ConfigureVideoFormat('h261');
        ConfigureVideoFormat('h263');
        ConfigureVideoFormat('indeo');
        ConfigureVideoFormat('camtasia');
        ConfigureVideoFormat('fraps');
        ConfigureVideoFormat('huffyuv');
        ConfigureVideoFormat('qtrle');
        ConfigureVideoFormat('dvvideo');
        ConfigureVideoFormat('bink');
        ConfigureVideoFormat('smackvid');
        ConfigureVideoFormat('rv12');
        ConfigureVideoFormat('rv34');
        ConfigureVideoFormat('lagarith');
        ConfigureVideoFormat('cinepak');
        ConfigureVideoFormat('camstudio');
        ConfigureVideoFormat('qpeg');
        ConfigureVideoFormat('zlib');
        ConfigureVideoFormat('rpza');
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) then begin
    CleanMediaTypeExtWrap('.mkv');
    CleanMediaTypeExtWrap('.mka');
    CleanMediaTypeExtWrap('.bdmv');
    CleanMediaTypeExtWrap('.mpls');
    CleanMediaTypeExtWrap('.mp4');
    CleanMediaTypeExtWrap('.mov');
    CleanMediaTypeExtWrap('.3gp');
    CleanMediaTypeExtWrap('.avi');
    CleanMediaTypeExtWrap('.divx');
    CleanMediaTypeExtWrap('.flv');
    CleanMediaTypeExtWrap('.ogg');
    CleanMediaTypeExtWrap('.ogm');
    CleanMediaTypeExtWrap('.ogv');
    CleanMediaTypeExtWrap('.mpg');
    CleanMediaTypeExtWrap('.vob');
    CleanMediaTypeExtWrap('.evo');
    CleanMediaTypeExtWrap('.ts');
    CleanMediaTypeExtWrap('.mts');
    CleanMediaTypeExtWrap('.m2ts');
    CleanMediaTypeExtWrap('.wtv');
    CleanMediaTypeExtWrap('.wmv');
    CleanMediaTypeExtWrap('.rm');
    CleanMediaTypeExtWrap('.rmvb');
    CleanMediaTypeExtWrap('.flac');
    CleanMediaTypeExtWrap('.aac');
    CleanMediaTypeExtWrap('.amr');
    CleanMediaTypeExtWrap('.wv');
    CleanMediaTypeExtWrap('.mpc');
    CleanMediaTypeExtWrap('.tta');
  end;
end;

procedure InitializeWizard();
begin
  // Adjust tasks page
	WizardForm.SelectTasksLabel.Hide;
	WizardForm.TasksList.Top    := 0;
	WizardForm.TasksList.Height := PageFromID(wpSelectTasks).SurfaceHeight;
end;
