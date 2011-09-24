/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

CodecID FindCodecId(const CMediaType *mt);
int getThreadFlags(CodecID codecId);

#define MAX_NUM_CC_CODECS 3

struct codec_config_t {
  int nCodecs;
  CodecID codecs[MAX_NUM_CC_CODECS];
  const wchar_t *name;
  const wchar_t *description;
};
const codec_config_t *get_codec_config(LAVVideoCodec codec);
