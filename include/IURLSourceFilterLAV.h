/*
 *      Copyright (C) 2024 Hendrik Leppkes
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

#pragma once

interface __declspec(uuid("C8FF17F9-5365-4F32-8AD5-6C550342C2F7")) IURLSourceFilterLAV : public IUnknown
{
    // Load a URL with the specified user agent and referrer
    // UserAgent and Referrer are optional, and either, both or none can be specified
    STDMETHOD(LoadURL)(LPCOLESTR pszURL, LPCOLESTR pszUserAgent, LPCOLESTR pszReferrer) = 0;
};
