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

// {FA40D6E9-4D38-4761-ADD2-71A9EC5FD32F}
DEFINE_GUID(IID_ILAVVideoSettings, 
0xfa40d6e9, 0x4d38, 0x4761, 0xad, 0xd2, 0x71, 0xa9, 0xec, 0x5f, 0xd3, 0x2f);

// LAV Audio configuration interface
[uuid("FA40D6E9-4D38-4761-ADD2-71A9EC5FD32F")]
interface ILAVVideoSettings : public IUnknown
{
  // Switch to Runtime Config mode. This will reset all settings to default, and no changes to the settings will be saved
  // You can use this to programmatically configure LAV Audio without interfering with the users settings in the registry.
  // Subsequent calls to this function will reset all settings back to defaults, even if the mode does not change.
  //
  // Note that calling this function during playback is not supported and may exhibit undocumented behaviour. 
  // For smooth operations, it must be called before LAV Audio is connected to other filters.
  //STDMETHOD(SetRuntimeConfig)(BOOL bRuntimeConfig) = 0;
};
