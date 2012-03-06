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

#pragma once

class CH264SequenceParser
{
public:
  CH264SequenceParser(void);
  virtual ~CH264SequenceParser(void);

  HRESULT ParseNALs(const BYTE *buffer, size_t buflen, int nal_size);

public:
  struct {
    int valid;

    int profile;
    int level;
    int chroma;
    int luma_bitdepth;
    int chroma_bitdepth;
    int ref_frames;
    int interlaced;
    int ar_present;

    int full_range;
    int primaries;
    int trc;
    int colorspace;
  } sps;

  struct {
    int valid;
  } pps;

private:
  HRESULT ParseSPS(const BYTE *buffer, size_t buflen);
};
