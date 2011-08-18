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

#pragma once

class CBaseDSPropPage : public CBasePropertyPage
{
public:
  CBaseDSPropPage(LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, int DialogId, int TitleId);

public:
  static HRESULT ShowPropPageDialog(IBaseFilter *pFilter);

protected:
  // Convenience function to add a new column to a list view control
  static void ListView_AddCol(HWND hlv, int &ncol, int w, const wchar_t *txt, bool right);

  TOOLINFO addHint(int id, const LPWSTR text);

private:
  HWND createHintWindow(HWND parent, int timePop = 1700, int timeInit = 70, int timeReshow = 7);

private:
  HWND m_hHint;
};
