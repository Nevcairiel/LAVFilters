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

struct scmap_t {
  WORD nChannels;
  BYTE ch[8];
  DWORD dwChannelMask;
};

CodecID FindCodecId(const CMediaType *mt);

const scmap_t* get_channel_map(AVCodecContext *avctx);

const char *get_sample_format_desc(LAVAudioSampleFormat sfFormat);
const char *get_sample_format_desc(CMediaType &mt);

WORD get_channel_from_flag(DWORD dwMask, DWORD dwFlag);
DWORD get_flag_from_channel(DWORD dwMask, WORD wChannel);
const char *get_channel_desc(DWORD dwFlag);
