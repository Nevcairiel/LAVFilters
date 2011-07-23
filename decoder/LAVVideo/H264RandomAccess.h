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

class CByteParser;

class CH264RandomAccess
{
public:
  CH264RandomAccess();
  ~CH264RandomAccess();

  void flush(int threadCount);
  BOOL searchRecoveryPoint(uint8_t *buf, int buf_size);
  void judgeFrameUsability(AVFrame *pFrame, int *got_picture_ptr);

  void SetAVCNALSize(int avcNALSize) { m_AVCNALSize = avcNALSize; }

private:
  int decode_sei_recovery_point(CByteParser *pParser);
  int parseForRecoveryPoint(uint8_t *buf, int buf_size, int *recoveryFrameCount);

private:
  int m_RecoveryMode; // 0: OK; 1: Searching; 2: Found; 3: 
  int m_RecoveryFrameCount;
  int m_RecoveryPOC;
  int m_ThreadDelay;

  int m_AVCNALSize;
};
