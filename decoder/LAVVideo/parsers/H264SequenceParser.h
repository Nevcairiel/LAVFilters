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

class CH264SequenceParser
{
public:
  CH264SequenceParser(void);
  virtual ~CH264SequenceParser(void);

  HRESULT ParseNALs(const BYTE *buffer, int buflen, int nal_size);

public:
  struct {
    int valid;

    int profile;
    int level;
    int chroma;
    int luma_bitdepth;
    int chroma_bitdepth;
  } sps;

  struct {
    int valid;
  } pps;

private:
  HRESULT ParseSPS(const BYTE *buffer, int buflen);
};
