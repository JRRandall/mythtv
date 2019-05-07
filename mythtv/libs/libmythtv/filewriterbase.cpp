/*  -*- Mode: c++ -*-
 *
 *   Class FileWriterBase
 *
 *   Copyright (C) Chris Pinkham 2011
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "mythlogging.h"
#include "filewriterbase.h"

#define LOC QString("FWB(%1): ").arg(m_filename)
#define LOC_ERR QString("FWB(%1) Error: ").arg(m_filename)

int FileWriterBase::WriteVideoFrame(VideoFrame */*frame*/)
{
    LOG(VB_RECORD, LOG_ERR, LOC + "WriteVideoFrame(): Shouldn't be here!");

    return 1;
}

int FileWriterBase::WriteAudioFrame(unsigned char */*buf*/, int /*fnum*/, long long &/*timecode*/)
{
    LOG(VB_RECORD, LOG_ERR, LOC + "WriteAudioFrame(): Shouldn't be here!");

    return 1;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
