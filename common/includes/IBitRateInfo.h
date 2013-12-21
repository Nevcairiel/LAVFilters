/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 MPC-HC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

interface __declspec(uuid("EB2CD9E6-BA08-4acb-AA0F-3D8D0DD521CA"))
IBitRateInfo :
public IUnknown {
    STDMETHOD_(DWORD, GetCurrentBitRate)() PURE;
    STDMETHOD_(DWORD, GetAverageBitRate)() PURE;
};
