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
 */


#include "stdafx.h"
#include "BaseDSPropPage.h"

CBaseDSPropPage::CBaseDSPropPage(LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, int DialogId, int TitleId)
  : CBasePropertyPage(pName, pUnk, DialogId, TitleId), m_hHint(0)
{
}

HWND CBaseDSPropPage::createHintWindow(HWND parent,int timePop,int timeInit,int timeReshow)
{
  HWND hhint = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
    WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
    CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
    parent, NULL, NULL, NULL);
  SetWindowPos(hhint,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  SendMessage(hhint,TTM_SETDELAYTIME,TTDT_AUTOPOP,MAKELONG(timePop,0));
  SendMessage(hhint,TTM_SETDELAYTIME,TTDT_INITIAL,MAKELONG(timeInit,0));
  SendMessage(hhint,TTM_SETDELAYTIME,TTDT_RESHOW,MAKELONG(timeReshow,0));
  SendMessage(hhint,TTM_SETMAXTIPWIDTH,0,470);
  return hhint;
}

TOOLINFO CBaseDSPropPage::addHint(int id, const LPWSTR text)
{
  if (!m_hHint) m_hHint = createHintWindow(m_Dlg,15000);
  TOOLINFO ti;
  ti.cbSize = sizeof(TOOLINFO);
  ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
  ti.hwnd = m_Dlg;
  ti.uId = (LPARAM)GetDlgItem(m_Dlg, id);
  ti.lpszText = text;
  SendMessage(m_hHint, TTM_ADDTOOL, 0, (LPARAM)&ti);
  return ti;
}

void CBaseDSPropPage::ListView_AddCol(HWND hlv, int &ncol, int w, const wchar_t *txt, bool right)
{
  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
  lvc.iSubItem = ncol;
  lvc.pszText = (LPWSTR)txt;
  lvc.cx = w;
  lvc.fmt = right ? LVCFMT_RIGHT : LVCFMT_LEFT;
  ListView_InsertColumn(hlv, ncol, &lvc);
  ncol++;
}
