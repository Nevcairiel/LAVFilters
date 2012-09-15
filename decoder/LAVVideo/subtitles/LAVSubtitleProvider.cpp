/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
#include "LAVSubtitleProvider.h"

#define OFFSET(x) offsetof(LAVSubtitleProviderContext, x)
static const SubRenderOption options[] = {
  { "name",           OFFSET(name),            SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "version",        OFFSET(version),         SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "combineBitmaps", OFFSET(combineBitmaps),  SROPT_TYPE_BOOL,   0                   },
  { 0 }
};

CLAVSubtitleProvider::CLAVSubtitleProvider(ISubRenderConsumer *pConsumer)
  : CSubRenderOptionsImpl(::options, &context)
  , CUnknown(L"CLAVSubtitleProvider", NULL)
  , m_pConsumer(pConsumer)
{
  ZeroMemory(&context, sizeof(context));
  context.name = TEXT(LAV_VIDEO);
  context.version = TEXT(LAV_VERSION_STR);

  ASSERT(m_pConsumer);

  AddRef();

  m_pConsumer->AddRef();
  m_pConsumer->Connect(this);
}

CLAVSubtitleProvider::~CLAVSubtitleProvider(void)
{
  DisconnectConsumer();
}

STDMETHODIMP CLAVSubtitleProvider::DisconnectConsumer(void)
{
  CheckPointer(m_pConsumer, S_FALSE);
  m_pConsumer->Disconnect();
  SafeRelease(&m_pConsumer);

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop)
{
  ASSERT(m_pConsumer);
  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::Disconnect(void)
{
  SafeRelease(&m_pConsumer);
  return S_OK;
}
