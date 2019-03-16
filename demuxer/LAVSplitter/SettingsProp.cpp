/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "stdafx.h"

#include <assert.h>
#include <Commctrl.h>

#include "SettingsProp.h"

#include "DShowUtil.h"
#include "version.h"


#define LANG_BUFFER_SIZE 256

CLAVSplitterSettingsProp::CLAVSplitterSettingsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVF Settings"), pUnk, IDD_PROPPAGE_LAVFSETTINGS, IDS_PAGE_TITLE)
{
}

CLAVSplitterSettingsProp::~CLAVSplitterSettingsProp(void)
{
  SAFE_CO_FREE(m_pszPrefLang);
  SAFE_CO_FREE(m_pszPrefSubLang);
  SAFE_CO_FREE(m_pszAdvSubConfig);
  SafeRelease(&m_pLAVF);
}

HRESULT CLAVSplitterSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr) {
    return E_POINTER;
  }
  ASSERT(m_pLAVF == nullptr);
  return pUnk->QueryInterface(&m_pLAVF);
}

HRESULT CLAVSplitterSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pLAVF);
  return S_OK;
}

HRESULT CLAVSplitterSettingsProp::OnApplyChanges()
{
  ASSERT(m_pLAVF != nullptr);
  HRESULT hr = S_OK;
  DWORD dwVal;
  BOOL bFlag;

  WCHAR buffer[LANG_BUFFER_SIZE];
  // Save audio language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredLanguages(buffer));

  // Save subtitle language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);

  if (m_selectedSubMode == LAVSubtitleMode_Advanced) {
    CHECK_HR(hr = m_pLAVF->SetPreferredSubtitleLanguages(m_subLangBuffer));
    CHECK_HR(hr = m_pLAVF->SetAdvancedSubtitleConfig(buffer));
  } else {
    CHECK_HR(hr = m_pLAVF->SetPreferredSubtitleLanguages(buffer));
    CHECK_HR(hr = m_pLAVF->SetAdvancedSubtitleConfig(m_advSubBuffer));
  }

  // Save subtitle mode
  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubtitleMode((LAVSubtitleMode)dwVal));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BD_SEPARATE_FORCED_SUBS, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetPGSForcedStream(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BD_ONLY_FORCED_SUBS, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetPGSOnlyForced(bFlag));

  int vc1flag = (int)SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetVC1TimestampMode(vc1flag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MKV_EXTERNAL, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetLoadMatroskaExternalSegments(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubstreamsEnabled(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_STREAM_SWITCH_REMOVE_AUDIO, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetStreamSwitchRemoveAudio(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SELECT_AUDIO_QUALITY, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetPreferHighQualityAudioStreams(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_IMPAIRED_AUDIO, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetUseAudioForHearingVisuallyImpaired(bFlag));

  SendDlgItemMessage(m_Dlg, IDC_QUEUE_MEM, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  int maxMem = _wtoi(buffer);
  CHECK_HR(hr = m_pLAVF->SetMaxQueueMemSize(maxMem));

  SendDlgItemMessage(m_Dlg, IDC_QUEUE_PACKETS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  int maxPackets = _wtoi(buffer);
  CHECK_HR(hr = m_pLAVF->SetMaxQueueSize(maxPackets));

  SendDlgItemMessage(m_Dlg, IDC_STREAM_ANADUR, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  int duration = _wtoi(buffer);
  CHECK_HR(hr = m_pLAVF->SetNetworkStreamAnalysisDuration(duration));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetTrayIcon(bFlag));

  LoadData();

done:    
  return hr;
}

void CLAVSplitterSettingsProp::UpdateSubtitleMode(LAVSubtitleMode mode)
{
  if (mode == LAVSubtitleMode_NoSubs) {
    WCHAR *note = L"No subtitles: Subtitles are disabled and will not be loaded by default.";
    SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_NOTE, WM_SETTEXT, 0, (LPARAM)note);
  } else if (mode == LAVSubtitleMode_ForcedOnly) {
    WCHAR *note = L"Only Forced Subtitles: Only subtitles marked as \"forced\" will be loaded.";
    SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_NOTE, WM_SETTEXT, 0, (LPARAM)note);
  } else if (mode == LAVSubtitleMode_Default) {
    WCHAR *note = L"Default Mode: Subtitles matching the preferred languages, as well as \"default\" and \"forced\" subtitles will be loaded.";
    SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_NOTE, WM_SETTEXT, 0, (LPARAM)note);
  } else if (mode == LAVSubtitleMode_Advanced) {
    WCHAR *note = L"Advanced Mode: Refer to the README or the documentation on http://1f0.de for details.";
    SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_NOTE, WM_SETTEXT, 0, (LPARAM)note);
  } else {
    WCHAR *empty = L"";
    SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_NOTE, WM_SETTEXT, 0, (LPARAM)empty);
  }

  LAVSubtitleMode oldMode = m_selectedSubMode;
  m_selectedSubMode = mode;
  // Switch away from advanced
  if (oldMode != mode && oldMode == LAVSubtitleMode_Advanced) {
    SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&m_advSubBuffer);
    SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_SETTEXT, 0, (LPARAM)&m_subLangBuffer);
  // Switch to advanced
  } else if (oldMode != mode && mode == LAVSubtitleMode_Advanced) {
    SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&m_subLangBuffer);
    SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_SETTEXT, 0, (LPARAM)&m_advSubBuffer);
  }
}

HRESULT CLAVSplitterSettingsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pLAVF != nullptr);

  const WCHAR *version = TEXT(LAV_SPLITTER) L" " TEXT(LAV_VERSION_STR);
  SendDlgItemMessage(m_Dlg, IDC_SPLITTER_FOOTER, WM_SETTEXT, 0, (LPARAM)version);

  hr = LoadData();
  memset(m_subLangBuffer, 0, sizeof(m_advSubBuffer));
  memset(m_advSubBuffer, 0, sizeof(m_advSubBuffer));

  m_selectedSubMode = LAVSubtitleMode_Default;
  if (m_pszAdvSubConfig)
    wcsncpy_s(m_advSubBuffer, m_pszAdvSubConfig, _TRUNCATE);

  // Notify the UI about those settings
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_SETTEXT, 0, (LPARAM)m_pszPrefLang);
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_SETTEXT, 0, (LPARAM)m_pszPrefSubLang);

  // Init the Combo Box
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_RESETCONTENT, 0, 0);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_NO_SUBS);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_FORCED_SUBS);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_DEFAULT);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_ADVANCED);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_SETCURSEL, m_subtitleMode, 0);
  addHint(IDC_SUBTITLE_MODE, L"Configure how subtitles are selected.");

  SendDlgItemMessage(m_Dlg, IDC_BD_SEPARATE_FORCED_SUBS, BM_SETCHECK, m_PGSForcedStream, 0);
  addHint(IDC_BD_SEPARATE_FORCED_SUBS, L"Enabling this causes the creation of a new \"Forced Subtitles\" stream, which will try to always display forced subtitles matching your selected audio language.\n\nNOTE: This option may not work on all Blu-ray discs.\nRequires restart to take effect.");

  SendDlgItemMessage(m_Dlg, IDC_BD_ONLY_FORCED_SUBS, BM_SETCHECK, m_PGSOnlyForced, 0);
  addHint(IDC_BD_ONLY_FORCED_SUBS, L"When enabled, all Blu-ray (PGS) subtitles will be filtered, and only forced subtitles will be sent to the renderer.\n\nNOTE: When this option is active, you will not be able to get the \"full\" subtitles.");

  SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_SETCHECK, m_VC1Mode, 0);
  addHint(IDC_VC1TIMESTAMP, L"Checked - Frame timings will be corrected.\nUnchecked - Frame timings will be sent untouched.\nIndeterminate (Auto) - Only enabled for decoders that rely on the splitter doing the corrections.\n\nNOTE: Only for debugging, if unsure, set to \"Auto\".");

  SendDlgItemMessage(m_Dlg, IDC_MKV_EXTERNAL, BM_SETCHECK, m_MKVExternal, 0);

  SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_SETCHECK, m_substreams, 0);
  addHint(IDC_SUBSTREAMS, L"Controls if sub-streams should be exposed as a separate stream.\nSub-streams are typically streams for backwards compatibility, for example the AC3 part of TrueHD streams on Blu-rays.");

  SendDlgItemMessage(m_Dlg, IDC_STREAM_SWITCH_REMOVE_AUDIO, BM_SETCHECK, m_StreamSwitchRemoveAudio, 0);
  addHint(IDC_STREAM_SWITCH_REMOVE_AUDIO, L"Remove the old Audio Decoder from the Playback Chain before switching the audio stream, forcing DirectShow to select a new one.\n\nThis option ensures that the preferred decoder is always used, however it does not work properly with all players.");

  addHint(IDC_SELECT_AUDIO_QUALITY, L"Controls if the stream with the highest quality (matching your language preferences) should always be used.\nIf disabled, the first stream is always used.");
  SendDlgItemMessage(m_Dlg, IDC_SELECT_AUDIO_QUALITY, BM_SETCHECK, m_PreferHighQualityAudio, 0);

  SendDlgItemMessage(m_Dlg, IDC_IMPAIRED_AUDIO, BM_SETCHECK, m_ImpairedAudio, 0);

  SendDlgItemMessage(m_Dlg, IDC_QUEUE_MEM_SPIN, UDM_SETRANGE32, 0, 2048);

  addHint(IDC_QUEUE_MEM, L"Set the maximum memory a frame queue can use for buffering (in megabytes).\nNote that this is the maximum value, only very high bitrate files will usually even reach the default maximum value.");
  addHint(IDC_QUEUE_MEM_SPIN, L"Set the maximum memory a frame queue can use for buffering (in megabytes).\nNote that this is the maximum value, only very high bitrate files will usually even reach the default maximum value.");

  swprintf_s(stringBuffer, L"%d", m_QueueMaxMem);
  SendDlgItemMessage(m_Dlg, IDC_QUEUE_MEM, WM_SETTEXT, 0, (LPARAM)stringBuffer);

  SendDlgItemMessage(m_Dlg, IDC_QUEUE_PACKETS_SPIN, UDM_SETRANGE32, 100, 100000);

  addHint(IDC_QUEUE_PACKETS, L"Set the maximum numbers of packets to buffer in the frame queue.\nNote that the frame queue will never exceed the memory limited set above.");
  addHint(IDC_QUEUE_PACKETS_SPIN, L"Set the maximum numbers of packets to buffer in the frame queue.\nNote that the frame queue will never exceed the memory limited set above.");

  swprintf_s(stringBuffer, L"%d", m_QueueMaxPackets);
  SendDlgItemMessage(m_Dlg, IDC_QUEUE_PACKETS, WM_SETTEXT, 0, (LPARAM)stringBuffer);

  SendDlgItemMessage(m_Dlg, IDC_STREAM_ANADUR_SPIN, UDM_SETRANGE32, 200, 10000);

  addHint(IDC_STREAM_ANADUR, L"Set the duration (in milliseconds) a network stream is analyzed for before playback starts.\nA longer duration ensures the stream parameters are properly detected, however it will delay playback start.\n\nDefault: 1000 (1 second)");
  addHint(IDC_STREAM_ANADUR_SPIN, L"Set the duration (in milliseconds) a network stream is analyzed for before playback starts.\nA longer duration ensures the stream parameters are properly detected, however it will delay playback start.\n\nDefault: 1000 (1 second)");

  swprintf_s(stringBuffer, L"%d", m_NetworkAnalysisDuration);
  SendDlgItemMessage(m_Dlg, IDC_STREAM_ANADUR, WM_SETTEXT, 0, (LPARAM)stringBuffer);

  UpdateSubtitleMode(m_subtitleMode);

  SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_SETCHECK, m_TrayIcon, 0);

  return hr;
}
HRESULT CLAVSplitterSettingsProp::LoadData()
{
  HRESULT hr = S_OK;

  // Free old strings
  SAFE_CO_FREE(m_pszPrefLang);
  SAFE_CO_FREE(m_pszPrefSubLang);
  SAFE_CO_FREE(m_pszAdvSubConfig);

  // Query current settings
  CHECK_HR(hr = m_pLAVF->GetPreferredLanguages(&m_pszPrefLang));
  CHECK_HR(hr = m_pLAVF->GetPreferredSubtitleLanguages(&m_pszPrefSubLang));
  CHECK_HR(hr = m_pLAVF->GetAdvancedSubtitleConfig(&m_pszAdvSubConfig));
  m_subtitleMode = m_pLAVF->GetSubtitleMode();
  m_PGSForcedStream = m_pLAVF->GetPGSForcedStream();
  m_PGSOnlyForced = m_pLAVF->GetPGSOnlyForced();
  m_VC1Mode = m_pLAVF->GetVC1TimestampMode();
  m_substreams = m_pLAVF->GetSubstreamsEnabled();
  m_MKVExternal = m_pLAVF->GetLoadMatroskaExternalSegments();

  m_StreamSwitchRemoveAudio = m_pLAVF->GetStreamSwitchRemoveAudio();
  m_PreferHighQualityAudio = m_pLAVF->GetPreferHighQualityAudioStreams();
  m_ImpairedAudio = m_pLAVF->GetUseAudioForHearingVisuallyImpaired();
  m_QueueMaxMem = m_pLAVF->GetMaxQueueMemSize();
  m_QueueMaxPackets = m_pLAVF->GetMaxQueueSize();
  m_NetworkAnalysisDuration = m_pLAVF->GetNetworkStreamAnalysisDuration();

  m_TrayIcon = m_pLAVF->GetTrayIcon();

done:
  return hr;
}

INT_PTR CLAVSplitterSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COMMAND:
    // Mark the page dirty if the text changed
    if(HIWORD(wParam) == EN_CHANGE
      && (LOWORD(wParam) == IDC_PREF_LANG || LOWORD(wParam) == IDC_PREF_LANG_SUBS)) {

        WCHAR buffer[LANG_BUFFER_SIZE];
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);

        int dirty = 0;
        WCHAR *source = nullptr;
        if(LOWORD(wParam) == IDC_PREF_LANG) {
          source = m_pszPrefLang;
        } else {
          source = (m_selectedSubMode == LAVSubtitleMode_Advanced) ? m_pszAdvSubConfig : m_pszPrefSubLang;
        }

        if (source) {
          dirty = _wcsicmp(buffer, source);
        } else {
          dirty = (int)wcslen(buffer);
        }

        if(dirty != 0) {
          SetDirty();
        }
    } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_SUBTITLE_MODE) {
      DWORD dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
      UpdateSubtitleMode((LAVSubtitleMode)dwVal);
      if (dwVal != m_subtitleMode) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_BD_SEPARATE_FORCED_SUBS) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BD_SEPARATE_FORCED_SUBS, BM_GETCHECK, 0, 0);
      if (bFlag != m_PGSForcedStream) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_BD_ONLY_FORCED_SUBS) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BD_ONLY_FORCED_SUBS, BM_GETCHECK, 0, 0);
      if (bFlag != m_PGSOnlyForced) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_VC1TIMESTAMP) {
      int iFlag = (int)SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_GETCHECK, 0, 0);
      if (iFlag != m_VC1Mode) {
        SetDirty();
      }
    }  else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_MKV_EXTERNAL) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MKV_EXTERNAL, BM_GETCHECK, 0, 0);
      if (bFlag != m_MKVExternal) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SUBSTREAMS) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_GETCHECK, 0, 0);
      if (bFlag != m_substreams) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_STREAM_SWITCH_REMOVE_AUDIO) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_STREAM_SWITCH_REMOVE_AUDIO, BM_GETCHECK, 0, 0);
      if (bFlag != m_StreamSwitchRemoveAudio) {
        SetDirty();
      }
    }  else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SELECT_AUDIO_QUALITY) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SELECT_AUDIO_QUALITY, BM_GETCHECK, 0, 0);
      if (bFlag != m_PreferHighQualityAudio) {
        SetDirty();
      }
    }  else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_IMPAIRED_AUDIO) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_IMPAIRED_AUDIO, BM_GETCHECK, 0, 0);
      if (bFlag != m_ImpairedAudio) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_TRAYICON) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_GETCHECK, 0, 0);
      if (bFlag != m_TrayIcon) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_QUEUE_MEM && HIWORD(wParam) == EN_CHANGE) {
      WCHAR buffer[100];
      SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, 100, (LPARAM)&buffer);
      int maxMem = _wtoi(buffer);
      size_t len = wcslen(buffer);
      if (maxMem == 0 && (buffer[0] != L'0' || len > 1)) {
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), EM_UNDO, 0, 0);
      } else {
        swprintf_s(buffer, L"%d", maxMem);
        if (wcslen(buffer) != len)
          SendDlgItemMessage(m_Dlg, IDC_QUEUE_MEM, WM_SETTEXT, 0, (LPARAM)buffer);
        if (maxMem != m_QueueMaxMem)
          SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_QUEUE_PACKETS && HIWORD(wParam) == EN_CHANGE) {
      WCHAR buffer[100];
      SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, 100, (LPARAM)&buffer);
      int maxMem = _wtoi(buffer);
      size_t len = wcslen(buffer);
      if (maxMem == 0 && (buffer[0] != L'0' || len > 1)) {
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), EM_UNDO, 0, 0);
      } else {
        swprintf_s(buffer, L"%d", maxMem);
        if (wcslen(buffer) != len)
          SendDlgItemMessage(m_Dlg, IDC_QUEUE_PACKETS, WM_SETTEXT, 0, (LPARAM)buffer);
        if (maxMem != m_QueueMaxPackets)
          SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_STREAM_ANADUR && HIWORD(wParam) == EN_CHANGE) {
      WCHAR buffer[100];
      SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, 100, (LPARAM)&buffer);
      int duration = _wtoi(buffer);
      size_t len = wcslen(buffer);
      if (duration == 0 && (buffer[0] != L'0' || len > 1)) {
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), EM_UNDO, 0, 0);
      } else {
        swprintf_s(buffer, L"%d", duration);
        if (wcslen(buffer) != len)
          SendDlgItemMessage(m_Dlg, IDC_STREAM_ANADUR, WM_SETTEXT, 0, (LPARAM)buffer);
        if (duration != m_NetworkAnalysisDuration)
          SetDirty();
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}


CLAVSplitterFormatsProp::CLAVSplitterFormatsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVF Settings"), pUnk, IDD_PROPPAGE_FORMATS, IDS_INPUT_FORMATS)
{
}

CLAVSplitterFormatsProp::~CLAVSplitterFormatsProp(void)
{
  SAFE_CO_FREE(m_bFormats);
  SafeRelease(&m_pLAVF);
}

HRESULT CLAVSplitterFormatsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr) {
    return E_POINTER;
  }
  ASSERT(m_pLAVF == nullptr);
  return pUnk->QueryInterface(&m_pLAVF);
}

HRESULT CLAVSplitterFormatsProp::OnDisconnect()
{
  SafeRelease(&m_pLAVF);
  return S_OK;
}

HRESULT CLAVSplitterFormatsProp::OnApplyChanges()
{
  HRESULT hr = S_OK;
  ASSERT(m_pLAVF != nullptr);

  HWND hlv = GetDlgItem(m_Dlg, IDC_FORMATS);

  int nItem = 0;
  std::set<FormatInfo>::const_iterator it;
  for (it = m_Formats.begin(); it != m_Formats.end(); ++it) {
    m_bFormats[nItem] = ListView_GetCheckState(hlv, nItem);
    m_pLAVF->SetFormatEnabled(it->strName, m_bFormats[nItem]);

    nItem++;
  }

  return hr;
}

HRESULT CLAVSplitterFormatsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pLAVF != nullptr);

  memset(stringBuffer, 0, sizeof(stringBuffer));

  const char *pszInput = m_pLAVF->GetInputFormat();
  if (pszInput) {
    _snwprintf_s(stringBuffer, _TRUNCATE, L"%S", pszInput);
  }
  SendDlgItemMessage(m_Dlg, IDC_CUR_INPUT, WM_SETTEXT, 0, (LPARAM)stringBuffer);

  m_Formats = m_pLAVF->GetInputFormats();

  // Setup ListView control for format configuration
  SendDlgItemMessage(m_Dlg, IDC_FORMATS, CCM_DPISCALE, TRUE, 0);

  HWND hlv = GetDlgItem(m_Dlg, IDC_FORMATS);
  ListView_SetExtendedListViewStyle(hlv, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

  int nCol = 1;
  LVCOLUMN lvc = {LVCF_WIDTH, 0, 20, 0};
  ListView_InsertColumn(hlv, 0, &lvc);
  ListView_AddCol(hlv, nCol,  75, L"Format", false);
  ListView_AddCol(hlv, nCol, 210, L"Description", false);

  ListView_DeleteAllItems(hlv);
  ListView_SetItemCount(hlv, m_Formats.size());

  SAFE_CO_FREE(m_bFormats);
  m_bFormats = (BOOL *)CoTaskMemAlloc(sizeof(BOOL) * m_Formats.size());
  if (!m_bFormats)
    return E_OUTOFMEMORY;
  memset(m_bFormats, 0, sizeof(BOOL) * m_Formats.size());

  // Create entries for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  std::set<FormatInfo>::const_iterator it;
  for (it = m_Formats.begin(); it != m_Formats.end(); ++it) {
    // Create main entry
    lvi.iItem = nItem + 1;
    ListView_InsertItem(hlv, &lvi);

    // Set sub item texts
    _snwprintf_s(stringBuffer, _TRUNCATE, L"%S", it->strName);
    ListView_SetItemText(hlv, nItem, 1, (LPWSTR)stringBuffer);

    _snwprintf_s(stringBuffer, _TRUNCATE, L"%S", it->strDescription);
    ListView_SetItemText(hlv, nItem, 2, (LPWSTR)stringBuffer);

    m_bFormats[nItem] =  m_pLAVF->IsFormatEnabled(it->strName);
    ListView_SetCheckState(hlv, nItem, m_bFormats[nItem]);

    nItem++;
  }

  return hr;
}

INT_PTR CLAVSplitterFormatsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_NOTIFY:
    NMHDR *hdr = (LPNMHDR)lParam;
    if (hdr->idFrom == IDC_FORMATS) {
      switch (hdr->code) {
      case LVN_ITEMCHANGED:
        LPNMLISTVIEW nmlv = (LPNMLISTVIEW)lParam;
        BOOL check = ListView_GetCheckState(hdr->hwndFrom, nmlv->iItem);
        if (check != m_bFormats[nmlv->iItem]) {
          SetDirty();
        }
        return TRUE;
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
