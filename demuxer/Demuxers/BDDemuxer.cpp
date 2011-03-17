/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "BDDemuxer.h"

#define BD_READ_BUFFER_SIZE (6144 * 5)

int BDByteStreamRead(void *opaque, uint8_t *buf, int buf_size)
{
  return bd_read((BLURAY *)opaque, buf, buf_size);
}

int64_t BDByteStreamSeek(void *opaque,  int64_t offset, int whence)
{
  BLURAY *bd = (BLURAY *)opaque;

  int64_t pos = 0;
  if (whence == SEEK_SET) {
    pos = offset;
  } else if (whence == SEEK_CUR) {
    if (offset == 0)
      return bd_tell(bd);
    pos = bd_tell(bd) + offset;
  } else if (whence == SEEK_END) {
    pos = bd_get_title_size(bd) - offset;
  } else if (whence == AVSEEK_SIZE) {
    return bd_get_title_size(bd);
  } else
    return -1;
  return bd_seek(bd, pos);
}

CBDDemuxer::CBDDemuxer(CCritSec *pLock)
  : CBaseDemuxer(L"bluray demuxer", pLock), m_lavfDemuxer(NULL), m_pb(NULL), m_pBD(NULL), m_pTitle(NULL)
{
}


CBDDemuxer::~CBDDemuxer(void)
{
  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
    m_pTitle = NULL;
  }

  if (m_pBD) {
    bd_close(m_pBD);
    m_pBD = NULL;
  }

  SafeRelease(&m_lavfDemuxer);
}

STDMETHODIMP CBDDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    __super::NonDelegatingQueryInterface(riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// Demuxer Functions
STDMETHODIMP CBDDemuxer::Open(LPCOLESTR pszFileName)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from C functions

  // Convert the filename from wchar to char for libbluray
  char fileName[4096];
  ret = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName, 4096, NULL, NULL);

  size_t len = strlen(fileName);
  if (len > 16) {
    char *bd_path = fileName;
    if(_strcmpi(bd_path+strlen(bd_path) - 16, "\\BDMV\\index.bdmv") == 0) {
      bd_path[strlen(bd_path) - 15] = 0;
    } else if (len > 22 && _strcmpi(bd_path+strlen(bd_path) - 22, "\\BDMV\\MovieObject.bdmv") == 0) {
      bd_path[strlen(bd_path) - 21] = 0;
    } else {
      return E_FAIL;
    }
    // Open BluRay
    BLURAY *bd = bd_open(bd_path, NULL);
    // Fetch titles
    uint32_t num_titles = bd_get_titles(bd, TITLES_RELEVANT);

    if (num_titles <= 0) {
      return E_FAIL;
    }

    uint64_t longest_duration = 0;
    uint32_t longest_id = 0;
    for(uint32_t i = 0; i < num_titles; i++) {
      BLURAY_TITLE_INFO *info = bd_get_title_info(bd, i);
      if (info && info->duration > longest_duration) {
        longest_id = i;
        longest_duration = info->duration;
      }
      if (info) {
        bd_free_title_info(info);
      }
    }

    m_pTitle = bd_get_title_info(bd, longest_id);
    ret = bd_select_title(bd, longest_id);
    if (ret == 0) {
      return E_FAIL;
    }

    m_pBD = bd;

    uint8_t *buffer = (uint8_t *)av_mallocz(BD_READ_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
    m_pb = avio_alloc_context(buffer, BD_READ_BUFFER_SIZE, 0, bd, BDByteStreamRead, NULL, BDByteStreamSeek);

    m_lavfDemuxer = new CLAVFDemuxer(m_pLock);
    m_lavfDemuxer->OpenInputStream(m_pb);
    m_lavfDemuxer->AddRef();
  }

  return hr;
}

REFERENCE_TIME CBDDemuxer::GetDuration() const
{
  if(m_pTitle) {
    return av_rescale(m_pTitle->duration, 1000, 9);
  }
  return m_lavfDemuxer->GetDuration();
}

STDMETHODIMP CBDDemuxer::GetNextPacket(Packet **ppPacket)
{
  return m_lavfDemuxer->GetNextPacket(ppPacket);
}

STDMETHODIMP CBDDemuxer::Seek(REFERENCE_TIME rTime)
{
  uint64_t prev = bd_tell(m_pBD);

  int64_t target = bd_find_seek_point(m_pBD, ConvertDSTimeTo90Khz(rTime));

  DbgLog((LOG_TRACE, 1, "Seek Request: %I64u (time); %I64u (byte), %I64u (prev byte)", rTime, target, prev));
  return m_lavfDemuxer->SeekByte(target, prev > target ? AVSEEK_FLAG_BACKWARD : 0);
}

const char *CBDDemuxer::GetContainerFormat() const
{
  return "bluray";
}

HRESULT CBDDemuxer::StreamInfo(DWORD streamId, LCID *plcid, WCHAR **ppszName) const
{
  return m_lavfDemuxer->StreamInfo(streamId, plcid, ppszName);
}

REFERENCE_TIME CBDDemuxer::Convert90KhzToDSTime(uint64_t timestamp)
{
  return av_rescale(timestamp, 1000, 9);
}

uint64_t CBDDemuxer::ConvertDSTimeTo90Khz(REFERENCE_TIME timestamp)
{
  return av_rescale(timestamp, 9, 1000);
}
