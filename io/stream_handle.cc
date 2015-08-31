/*
 * Copyright (c) 2008-2011 Juli Mallett. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fcntl.h>
#include <event/event_system.h>
#include <io/stream_handle.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           stream_handle.cc                                           //
// Description:    basic operations on stream objects                         //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

StreamHandle::StreamHandle (int fd) : log_("/file/descriptor"), fd_(fd)
{
	int flags = ::fcntl (fd_, F_GETFL, 0);
	if (flags == -1)
		ERROR(log_) << "Could not get flags for file descriptor.";
	else 
	{
		flags = ::fcntl (fd_, F_SETFL, flags | O_NONBLOCK);
		if (flags == -1)
			ERROR(log_) << "Could not set flags for file descriptor, some operations may block.";
	}
}

Action* StreamHandle::read (EventCallback* cb)
{
	return event_system.track (fd_, StreamModeRead, cb);
}

Action* StreamHandle::write (Buffer& buf, EventCallback* cb)
{
	if (cb)
		cb->param ().buffer_ = buf;
		
	return event_system.track (fd_, StreamModeWrite, cb);
}

Action* StreamHandle::close (EventCallback* cb)
{
	return event_system.track (fd_, StreamModeEnd, cb);
}
