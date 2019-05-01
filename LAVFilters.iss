; LAV Filters Inno Setup script

#include <idp.iss>

; Include version info
#define ISPP_INCLUDED
#include "common\includes\version.h"

#if LAV_VERSION_BUILD > 0
    #define LAV_VERSION_STRING str(LAV_VERSION_MAJOR) + "." + str(LAV_VERSION_MINOR) + "." + str(LAV_VERSION_REVISION) + "-" + str(LAV_VERSION_BUILD)
#else
    #if LAV_VERSION_REVISION > 0
        #define LAV_VERSION_STRING str(LAV_VERSION_MAJOR) + "." + str(LAV_VERSION_MINOR) + "." + str(LAV_VERSION_REVISION)
    #else
        #define LAV_VERSION_STRING str(LAV_VERSION_MAJOR) + "." + str(LAV_VERSION_MINOR)
    #endif
#endif

[Setup]
AllowCancelDuringInstall  = no
AllowNoIcons              = yes
AllowUNCPath              = no
AppId                     = lavfilters
AppName                   = LAV Filters
AppPublisher              = Hendrik Leppkes
AppPublisherURL           = https://1f0.de/
AppVerName                = LAV Filters {#=LAV_VERSION_STRING}
AppVersion                = {#=LAV_VERSION_STRING}
VersionInfoVersion        = {#=LAV_VERSION_MAJOR}.{#=LAV_VERSION_MINOR}.{#=LAV_VERSION_REVISION}.{#=LAV_VERSION_BUILD}
VersionInfoCompany        = 1f0.de
VersionInfoCopyright      = GPLv2
OutputBaseFilename        = LAVFilters-{#=LAV_VERSION_STRING}
OutputDir                 = .
Compression               = lzma2/ultra64
SolidCompression          = yes
MinVersion                = 0,6.0
PrivilegesRequired        = admin
CreateAppDir              = yes
DefaultDirName            = {pf}\LAV Filters
DefaultGroupName          = LAV Filters
DisableStartupPrompt      = yes
Uninstallable             = yes
DisableDirPage            = auto
DisableProgramGroupPage   = auto
UsePreviousTasks          = yes
#ifexist "..\LAVSignInfo.txt"
SignTool                  = LAVSignTool
SignedUninstaller         = yes
#endif

[Messages]
WelcomeLabel1=[name/ver]
WelcomeLabel2=This will install [name] on your computer.%n%nIt is recommended that you close all other applications before continuing.
WinVersionTooLowError=This software only works on Windows XP SP3 and newer.

[Types]
Name: default; Description: Default
Name: full;    Description: Full
Name: custom;  Description: Custom; Flags: iscustom

[Components]
Name: lavsplitter32; Description: LAV Splitter (x86); Types: default full custom;
Name: lavsplitter64; Description: LAV Splitter (x64); Types: default full custom; Check: IsWin64;
Name: lavaudio32;    Description: LAV Audio (x86);    Types: default full custom;
Name: lavaudio64;    Description: LAV Audio (x64);    Types: default full custom; Check: IsWin64;
Name: lavvideo32;    Description: LAV Video (x86);    Types: default full custom;
Name: lavvideo64;    Description: LAV Video (x64);    Types: default full custom; Check: IsWin64;
Name: mvc3d;         Description: H.264 MVC 3D Decoder (extra download); Types: full; ExtraDiskSpaceRequired: 41242624;

[Tasks]
Name: icons;          Description: "Create Start Menu Shortcuts";
Name: reset_settings; Description: "Reset Settings"; Flags: checkedonce unchecked; Check: SettingsExistCheck()

[Files]
Source: bin_Win32\avcodec-lav-58.dll;  DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\avfilter-lav-7.dll;  DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\avformat-lav-58.dll; DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32
Source: bin_Win32\avresample-lav-4.dll;DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\avutil-lav-56.dll;   DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\swscale-lav-5.dll;   DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\libbluray.dll;       DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32
Source: bin_Win32\LAVAudio.ax;         DestDir: {app}\x86; Flags: 32bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio32
Source: bin_Win32\LAVSplitter.ax;      DestDir: {app}\x86; Flags: 32bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32
Source: bin_Win32\LAVVideo.ax;         DestDir: {app}\x86; Flags: 32bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32
Source: bin_Win32\LAVFilters.Dependencies.manifest; DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter32 lavaudio32 lavvideo32
Source: bin_Win32\IntelQuickSyncDecoder.dll; DestDir: {app}\x86; Flags: 32bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo32

Source: bin_x64\avcodec-lav-58.dll;    DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\avfilter-lav-7.dll;    DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\avformat-lav-58.dll;   DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64
Source: bin_x64\avresample-lav-4.dll;  DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\avutil-lav-56.dll;     DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\swscale-lav-5.dll;     DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\libbluray.dll;         DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64
Source: bin_x64\LAVAudio.ax;           DestDir: {app}\x64; Flags: 64bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavaudio64
Source: bin_x64\LAVSplitter.ax;        DestDir: {app}\x64; Flags: 64bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64
Source: bin_x64\LAVVideo.ax;           DestDir: {app}\x64; Flags: 64bit regserver ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64
Source: bin_x64\LAVFilters.Dependencies.manifest; DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavsplitter64 lavaudio64 lavvideo64
Source: bin_x64\IntelQuickSyncDecoder.dll; DestDir: {app}\x64; Flags: 64bit ignoreversion restartreplace uninsrestartdelete; Components: lavvideo64

Source: COPYING;                       DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete
Source: README.txt;                    DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete
Source: CHANGELOG.txt;                 DestDir: {app};     Flags: ignoreversion restartreplace uninsrestartdelete

Source: thirdparty\contrib\7za.exe;    DestDir: {tmp};     Flags: dontcopy

[UninstallDelete]
Type: files; Name: {app}\x86\libmfxsw32.dll
Type: files; Name: {app}\x64\libmfxsw64.dll

[Icons]
Name: {group}\LAV Splitter Configuration;        Filename: rundll32.exe; Parameters: """{app}\x86\LAVSplitter.ax"",OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVSplitter.ax; IconIndex: 0; Tasks: icons; Components: lavsplitter32
Name: {group}\LAV Splitter Configuration;        Filename: rundll32.exe; Parameters: """{app}\x64\LAVSplitter.ax"",OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVSplitter.ax; IconIndex: 0; Tasks: icons; Components: lavsplitter64 AND NOT lavsplitter32
Name: {group}\LAV Audio Configuration;           Filename: rundll32.exe; Parameters: """{app}\x86\LAVAudio.ax"",OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVAudio.ax; IconIndex: 0; Tasks: icons; Components: lavaudio32
Name: {group}\LAV Audio Configuration;           Filename: rundll32.exe; Parameters: """{app}\x64\LAVAudio.ax"",OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVAudio.ax; IconIndex: 0; Tasks: icons; Components: lavaudio64 AND NOT lavaudio32
Name: {group}\LAV Video Configuration;           Filename: rundll32.exe; Parameters: """{app}\x86\LAVVideo.ax"",OpenConfiguration"; WorkingDir: {app}\x86; IconFilename: {app}\x86\LAVVideo.ax; IconIndex: 0; Tasks: icons; Components: lavvideo32
Name: {group}\LAV Video Configuration;           Filename: rundll32.exe; Parameters: """{app}\x64\LAVVideo.ax"",OpenConfiguration"; WorkingDir: {app}\x64; IconFilename: {app}\x64\LAVVideo.ax; IconIndex: 0; Tasks: icons; Components: lavvideo64 AND NOT lavvideo32
Name: {group}\Visit LAV Filters Home Page;       Filename: "https://1f0.de/"; Tasks: icons
Name: {group}\Visit LAV Filters on Doom9;        Filename: "https://forum.doom9.org/showthread.php?t=156191"; Tasks: icons
Name: {group}\Uninstall LAV Filters;             Filename: {uninstallexe}; Tasks: icons

[Registry]
Root: HKCU; Subkey: Software\LAV;                  Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: Software\LAV\Audio;            Flags: uninsdeletekey; Components: lavaudio32 lavaudio64
Root: HKCU; Subkey: Software\LAV\Splitter;         Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64
Root: HKCU; Subkey: Software\LAV\Splitter\Formats; Flags: uninsdeletekey; Components: lavsplitter32 lavsplitter64
Root: HKCU; Subkey: Software\LAV\Video;            Flags: uninsdeletekey; Components: lavvideo32 lavvideo64
Root: HKCU; Subkey: Software\LAV\Video\Formats;    Flags: uninsdeletekey; Components: lavvideo32 lavvideo64
Root: HKCU; Subkey: Software\LAV\Video\Output;     Flags: uninsdeletekey; Components: lavvideo32 lavvideo64

[Run]
Description: "Open LAV Splitter Configuration"; Filename: rundll32.exe; Parameters: """{app}\x86\LAVSplitter.ax"",OpenConfiguration"; WorkingDir: {app}\x86; Components: lavsplitter32; Flags: postinstall nowait unchecked
Description: "Open LAV Splitter Configuration"; Filename: rundll32.exe; Parameters: """{app}\x64\LAVSplitter.ax"",OpenConfiguration"; WorkingDir: {app}\x64; Components: lavsplitter64 AND NOT lavsplitter32; Flags: postinstall nowait unchecked
Description: "Open LAV Audio Configuration";    Filename: rundll32.exe; Parameters: """{app}\x86\LAVAudio.ax"",OpenConfiguration"; WorkingDir: {app}\x86; Components: lavaudio32; Flags: postinstall nowait unchecked
Description: "Open LAV Audio Configuration";    Filename: rundll32.exe; Parameters: """{app}\x64\LAVAudio.ax"",OpenConfiguration"; WorkingDir: {app}\x64; Components: lavaudio64 AND NOT lavaudio32; Flags: postinstall nowait unchecked
Description: "Open LAV Video Configuration";    Filename: rundll32.exe; Parameters: """{app}\x86\LAVVideo.ax"",OpenConfiguration"; WorkingDir: {app}\x86; Components: lavvideo32; Flags: postinstall nowait unchecked
Description: "Open LAV Video Configuration";    Filename: rundll32.exe; Parameters: """{app}\x64\LAVVideo.ax"",OpenConfiguration"; WorkingDir: {app}\x64; Components: lavvideo64 AND NOT lavvideo32; Flags: postinstall nowait unchecked

[Code]
type
  Format = record
             id: String;
             name: String;
             default: Boolean;
             subtype: String;
             asyncSource: Boolean;
             protocol: Boolean;
             extensions: Array of String;
             chkbytes: Array of String;
           end;

const
  NumFormatsMinusOne = 22;
  LavGUID = '{B98D13E7-55DB-4385-A33D-09FD1BA26338}';
  StreamGUID = '{E436EB83-524F-11CE-9F53-0020AF0BA770}';
  LavSplitterFormatsReg = 'Software\LAV\Splitter\Formats';

var
  SplitterPage: TInputOptionWizardPage;
  SplitterFormats: Array [0..NumFormatsMinusOne] of Format;

function IsProcessorFeaturePresent(Feature: Integer): Boolean;
external 'IsProcessorFeaturePresent@kernel32.dll stdcall';

function Is_SSE2_Supported(): Boolean;
begin
  // PF_XMMI64_INSTRUCTIONS_AVAILABLE
  Result := IsProcessorFeaturePresent(10);
end;

function SettingsExistCheck(): Boolean;
begin
  if RegKeyExists(HKCU, 'Software\LAV') then
    Result := True
  else
    Result := False;
end;

function IsUpdate(): Boolean;
var
  sPrevPath: String;
begin
  sPrevPath := WizardForm.PrevAppDir;
  Result := (sPrevPath <> '');
end;

procedure FR(var f: Format; const id, desc: String; const default: Boolean; const extensions: Array of String);
begin
  f.id         := id;
  f.name       := desc;
  f.default    := default;
  f.extensions := extensions;
  f.subtype    := '';
  f.chkbytes   := [''];
  f.protocol   := False;
end;

procedure FP(var f: Format; const id, desc: String; const default: Boolean; const extensions: Array of String);
begin
  FR(f, id, desc, default, extensions);
  f.protocol := True;
end;

procedure FS(var f: Format; const subtype: String; const chkbytes : Array of String; UseAsync: Boolean);
begin
  f.subtype     := subtype;
  f.chkbytes    := chkbytes;
  f.asyncSource := UseAsync;
end;

procedure InitFormats();
begin
  FR(SplitterFormats[0], 'matroska', 'Matroska/WebM', True, ['mkv','mka', 'mks', 'mk3d', 'webm', '']);
  FS(SplitterFormats[0], '{1AC0BEBD-4D2B-45ad-BCEB-F2C41C5E3788}', ['0,4,,1A45DFA3', ''], True);
  FR(SplitterFormats[1], 'avi', 'AVI', True, ['avi','divx', 'vp6', 'amv', '']);
  FS(SplitterFormats[1], '{e436eb88-524f-11ce-9f53-0020af0ba770}', ['0,4,,52494646,8,4,,41564920', '0,4,,52494646,8,4,,41564958', '0,4,,52494646,8,4,,414D5620', ''], True);
  FR(SplitterFormats[2], 'mp4', 'MP4/MOV', True, ['mp4', 'mov', '3gp', '3ga', 'm4v', 'qt', '']);
  FS(SplitterFormats[2], '{08E22ADA-B715-45ed-9D20-7B87750301D4}', ['4,4,,66747970', '4,4,,6d6f6f76', '4,4,,6d646174', '4,4,,736b6970', '4,4,,75647461',
                                                                    '4,12,ffffffff00000000ffffffff,77696465000000006d646174', '4,12,ffffffff00000000ffffffff,776964650000000066726565',
                                                                    '4,12,ffffffff00000000ffffffff,6672656500000000636D6F76', '4,12,ffffffff00000000ffffffff,66726565000000006D766864',
                                                                    '4,14,ffffffff000000000000ffffffff,706E6F7400000000000050494354', ''], True);
  FR(SplitterFormats[3], 'mpegts', 'MPEG-TS', True, ['ts', 'm2ts', 'mts', 'tp', 'ssif', '']);
  FS(SplitterFormats[3], '{e06d8023-db46-11cf-b4d1-00805f6cbbea}', ['0,1,,47,188,1,,47,376,1,,47', '4,1,,47,196,1,,47,388,1,,47', '0,4,,54467263,1660,1,,47', ''], True);
  FR(SplitterFormats[4], 'mpeg', 'MPEG-PS/VOB/EVO', True, ['mpg', 'mpeg', 'vob', 'evo', '']);
  FS(SplitterFormats[4], '{e06d8022-db46-11cf-b4d1-00805f6cbbea}', ['0,5,FFFFFFFFC0,000001BA40', ''], True);
  FR(SplitterFormats[5], 'bluray', 'Blu-ray', True, ['bdmv', 'mpls', '']);
  FS(SplitterFormats[5], '{20884BC2-629F-45EA-B1C5-FA4FFA438250}', ['0,4,,494E4458', '0,4,,4D4F424A', '0,4,,4D504C53', ''], False);
  FR(SplitterFormats[6], 'flv', 'FLV', True, ['flv', '']);
  FS(SplitterFormats[6], '{F2FAC0F1-3852-4670-AAC0-9051D400AC54}', ['0,4,,464C5601', ''], True);
  FR(SplitterFormats[7], 'ogg', 'Ogg/OGM', True, ['ogg', 'ogv', 'ogm', '']);
  FS(SplitterFormats[7], '{D2855FA9-61A7-4db0-B979-71F297C17A04}', ['0,4,,4F676753', ''], True);
  FR(SplitterFormats[8], 'rm', 'RealMedia (rm/rmvb)', True, ['rm', 'rmvb', '']);
  FR(SplitterFormats[9], 'wtv', 'Windows Television (wtv)', False, ['wtv', '']);
  FR(SplitterFormats[10], 'asf', 'WMV / ASF / DVR-MS', True, ['wmv', 'asf', 'dvr-ms', '']);
  FR(SplitterFormats[11], 'mxf', 'MXF (Material Exchange Format)', True, ['mxf', '']);
  FR(SplitterFormats[12], 'bink', 'Bink', True, ['bik', '']);

  FR(SplitterFormats[13], 'avisynth', 'AviSynth scripts', True, ['avs', '']);

  FP(SplitterFormats[14], 'rtmp', 'RTMP Streaming Protocol', False, ['rtmp', 'rtmpt', '']);
  FP(SplitterFormats[15], 'rtsp', 'RTSP Streaming Protocol', True, ['rtsp', 'rtspu', 'rtspm', 'rtspt', 'rtsph', '']);
  FP(SplitterFormats[16], 'rtp', 'RTP Streaming Protocol', True, ['rtp', '']);
  FP(SplitterFormats[17], 'mms', 'MMS Streaming Protocol', True, ['mms', 'mmsh', 'mmst', '']);

  FR(SplitterFormats[18], 'dts', 'DTS Audio', True, ['dts', 'dtshd', '']);
  FR(SplitterFormats[19], 'ac3', 'AC3 Audio', True, ['ac3', 'eac3', '']);
  FR(SplitterFormats[20], 'aac', 'AAC Audio', True, ['aac', '']);
  FR(SplitterFormats[21], 'mp3', 'MP3 Audio', True, ['mp3', '']);
  FR(SplitterFormats[22], 'flac', 'FLAC Audio', True, ['flac', '']);
end;

procedure RegWriteStringWithBackup(const RootKey: Integer; const SubKeyName, ValueName, Data: String);
var
  OldValue: String;
begin
  if RegQueryStringValue(RootKey, SubKeyName, ValueName, OldValue) then begin
    if CompareText(OldValue, Data) <> 0 then begin
      RegWriteStringValue(RootKey, SubKeyName, ValueName + '.LAV', OldValue);
    end
  end;
  RegWriteStringValue(RootKey, SubKeyName, ValueName, Data);
end;

function RegRestoreBackup(const RootKey: Integer; const SubKeyName, ValueName, CheckValue: String; const DeleteMode: Integer) : Boolean;
var
  CurrentValue: String;
  BackupValue: String;
begin
  Result := False;
  if RegQueryStringValue(RootKey, SubKeyName, ValueName, CurrentValue) then begin
    if CompareText(CurrentValue, CheckValue) = 0 then begin
      if RegQueryStringValue(RootKey, SubKeyName, ValueName + '.LAV', BackupValue) then begin
        RegWriteStringValue(RootKey, SubKeyName, ValueName, BackupValue);
        Result := True;
      end else begin
        if DeleteMode = 1 then begin
          RegDeleteValue(RootKey, SubKeyName, ValueName);
          RegDeleteKeyIfEmpty(RootKey, SubKeyName);
        end else if DeleteMode = 2 then
          RegDeleteKeyIncludingSubkeys(RootKey, SubKeyName);
      end
    end
  end;
  RegDeleteValue(RootKey, SubKeyName, ValueName + '.LAV');
end;

procedure RegisterSourceFormatGUIDs(f: Format);
var
  i: Integer;
  source: String;
begin
  i := 0;
  if Length(f.subtype) > 0 then
    begin
      if f.asyncSource then
        source := '{e436ebb5-524f-11ce-9f53-0020af0ba770}'
      else
        source := LavGUID;
      if IsComponentSelected('lavsplitter32') then
        RegWriteStringWithBackup(HKCR32, 'Media Type\' + StreamGUID + '\' + f.subtype, 'Source Filter', source);
      if IsComponentSelected('lavsplitter64') then
        RegWriteStringWithBackup(HKCR64, 'Media Type\' + StreamGUID + '\' + f.subtype, 'Source Filter', source);
    end;

  while Length(f.chkbytes[i]) > 0 do
    begin
      if IsComponentSelected('lavsplitter32') then
        RegWriteStringValue(HKCR32, 'Media Type\' + StreamGUID + '\' + f.subtype, IntToStr(i), f.chkbytes[i]);
      if IsComponentSelected('lavsplitter64') then
        RegWriteStringValue(HKCR64, 'Media Type\' + StreamGUID + '\' + f.subtype, IntToStr(i), f.chkbytes[i]);

      i := i+1;
    end;
end;

procedure UnregisterSourceFormatGUIDs(f: Format);
var
  SourceGuid: String;
  DelMode: Integer;
begin
  if Length(f.subtype) > 0 then
  begin
    if f.asyncSource then begin
      SourceGuid := '{e436ebb5-524f-11ce-9f53-0020af0ba770}';
      DelMode := 0;
    end else begin
      SourceGuid := LavGUID;
      DelMode := 2;
    end;
    RegRestoreBackup(HKCR32, 'Media Type\' + StreamGUID + '\' + f.subtype, 'Source Filter', SourceGuid, DelMode);
    if IsWin64 then
      RegRestoreBackup(HKCR64, 'Media Type\' + StreamGUID + '\' + f.subtype, 'Source Filter', SourceGuid, DelMode);
  end;
end;

procedure RegisterSourceFormatExtensions(f: Format);
var
  i: Integer;
begin
  i := 0;
  while Length(f.extensions[i]) > 0 do
    begin
      if f.protocol then begin
        RegWriteStringWithBackup(HKCR, f.extensions[i], 'Source Filter', LavGUID);
      end else begin
        if IsComponentSelected('lavsplitter32') then begin
          RegWriteStringWithBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'Source Filter', LavGUID);
          RegWriteStringWithBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'Media Type', StreamGUID);
          if Length(f.subtype) > 0 then
            RegWriteStringWithBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'SubType', f.subtype);
        end;
        if IsComponentSelected('lavsplitter64') then begin
          RegWriteStringWithBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'Source Filter', LavGUID);
          RegWriteStringWithBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'Media Type', StreamGUID);
          if Length(f.subtype) > 0 then
            RegWriteStringWithBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'SubType', f.subtype);
        end
      end;

      i := i+1;
    end;
end;

procedure UnregisterSourceFormatExtensions(f: Format);
var
  source: String;
  i: Integer;
begin
  i := 0;
  while Length(f.extensions[i]) > 0 do
    begin
      if f.protocol then
        RegRestoreBackup(HKCR, f.extensions[i], 'Source Filter', LavGUID, 1)
      else begin
        RegRestoreBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'Source Filter', LavGUID, 2);
        RegRestoreBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'Media Type', StreamGUID, 0);
        if Length(f.subtype) > 0 then
          RegRestoreBackup(HKCR32, 'Media Type\Extensions\.' + f.extensions[i], 'SubType', f.subtype, 0);
        if IsWin64 then begin
          RegRestoreBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'Source Filter', LavGUID, 2);
          RegRestoreBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'Media Type', StreamGUID, 0);
          if Length(f.subtype) > 0 then
            RegRestoreBackup(HKCR64, 'Media Type\Extensions\.' + f.extensions[i], 'SubType', f.subtype, 0)
        end
      end;

      i := i+1;
    end;
end;

function GetDefaultFormatSetting(f: Format): Boolean;
var
  value: Cardinal;
begin
  if RegQueryDWordValue(HKCU, LavSplitterFormatsReg, f.id, value) then begin
    Result := value > 0;
  end else begin
    Result := f.default;
  end
end;

procedure ResetSettings();
begin
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\LAV');
end;

procedure DoUnzip(source: String; targetdir: String);
var
    unzipTool: String;
    ReturnCode: Integer;
begin
    // source contains tmp constant, so resolve it to path name
    source := ExpandConstant(source);

    unzipTool := ExpandConstant('{tmp}\7za.exe');

    if not FileExists(unzipTool)
    then MsgBox('UnzipTool not found: ' + unzipTool, mbError, MB_OK)
    else if not FileExists(source)
    then MsgBox('File was not found while trying to unzip: ' + source, mbError, MB_OK)
    else begin
         if Exec(unzipTool, ' x "' + source + '" -o"' + targetdir + '" -y',
                 '', SW_HIDE, ewWaitUntilTerminated, ReturnCode) = false
         then begin
             MsgBox('Unzip failed:' + source, mbError, MB_OK)
         end;
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  i: Integer;
  targetPath : String;
begin
  if (CurStep = ssPostInstall) then begin
    if IsTaskSelected('reset_settings') then
      ResetSettings();

    if IsComponentSelected('lavsplitter32') or IsComponentSelected('lavsplitter64') then
      begin
        for i := 0 to NumFormatsMinusOne do
          begin
            if SplitterPage.Values[i] then begin
              RegisterSourceFormatGUIDs(SplitterFormats[i]);
              RegisterSourceFormatExtensions(SplitterFormats[i]);
            end else begin
              UnregisterSourceFormatGUIDs(SplitterFormats[i]);
              UnregisterSourceFormatExtensions(SplitterFormats[i]);
            end;
            RegWriteDWordValue(HKCU, LavSplitterFormatsReg, SplitterFormats[i].id, Ord(SplitterPage.Values[i]));
          end;
      end;

      if IsComponentSelected('mvc3d') then
      begin
        ExtractTemporaryFile('7za.exe');
        targetPath := ExpandConstant('{tmp}\');
        if IsComponentSelected('lavvideo32') then
          DoUnzip(targetPath + 'libmfxsw32-v3.7z', ExpandConstant('{app}\x86'));
        if IsComponentSelected('lavvideo64') then
          DoUnzip(targetPath + 'libmfxsw64-v3.7z', ExpandConstant('{app}\x64'));
      end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
    if CurPageID = wpReady then
    begin
      // User can navigate to 'Ready to install' page several times, so we
      // need to clear file list to ensure that only needed files are added.
      idpClearFiles;

      if IsComponentSelected('mvc3d') then
      begin
        if IsComponentSelected('lavvideo32') then
          idpAddFile('https://files.1f0.de/lavf/plugins/libmfxsw32-v3.7z', ExpandConstant('{tmp}\libmfxsw32-v3.7z'));
        if IsComponentSelected('lavvideo64') then
          idpAddFile('https://files.1f0.de/lavf/plugins/libmfxsw64-v3.7z', ExpandConstant('{tmp}\libmfxsw64-v3.7z'));
      end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  i: Integer;
begin
  if (CurUninstallStep = usUninstall) then begin
    for i := 0 to NumFormatsMinusOne do
      begin
        UnregisterSourceFormatGUIDs(SplitterFormats[i]);
        UnregisterSourceFormatExtensions(SplitterFormats[i]);
      end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = SplitterPage.ID then begin
    if not (IsComponentSelected('lavsplitter32') or IsComponentSelected('lavsplitter64')) then
      Result := True;
  end;
end;

function InitializeSetup(): Boolean;
begin
  InitFormats;
  Result := True;

  if not Is_SSE2_Supported() then begin
    SuppressibleMsgBox('LAV Filters requires a CPU with SSE2 instruction support.'#10'Your CPU does not have these capabilities.', mbCriticalError, MB_OK, MB_OK);
    Result := False;
  end;
end;

function InitializeUninstall(): Boolean;
begin
  InitFormats;
  Result := True;
end;

procedure InitializeWizard();
var
  i: Integer;
begin
  SplitterPage := CreateInputOptionPage(wpSelectTasks,
    'LAV Splitter Formats',
    'Select which formats LAV Splitter should be setup to handle',
    'Select for which formats LAV Splitter should be setup to be the Source Filter.'#10'Note: These are only the file formats for LAV Splitter, audio and video codecs are configured separately.',
    False, False);

  for i := 0 to NumFormatsMinusOne do
    begin
      SplitterPage.Add(SplitterFormats[i].name);
      SplitterPage.Values[i] := GetDefaultFormatSetting(SplitterFormats[i]);
    end;

  // Adjust tasks page
  WizardForm.SelectTasksLabel.Hide;
  WizardForm.TasksList.Top    := 0;
  WizardForm.TasksList.Height := PageFromID(wpSelectTasks).SurfaceHeight;

  idpDownloadAfter(wpReady);
end;
