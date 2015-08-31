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

#include <sys/errno.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <event/event_system.h>
#include <event/io_service.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_poll_select.cc                                       //
// Description:    IO event handling using traditional select calls           //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void IoService::open_resources ()
{
}

void IoService::close_resources ()
{
}

void IoService::set_fd (int fd, int rd, int wr, IoNode* node)
{
}

void IoService::poll (int ms)
{
	fd_set read_set, write_set;
	struct timeval tv;
	int maxfd;
	int fdcnt;

	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	FD_SET(rfd_, &read_set);
	maxfd = rfd_;
	
	std::map<int, IoNode>::iterator it;
	for (it = fd_map_.begin (); it != fd_map_.end (); ++it)
	{
		if (it->second.reading)
			FD_SET (it->first, &read_set);
		if (it->second.writing)
			FD_SET (it->first, &write_set);
		if (maxfd < it->first)
			maxfd = it->first;
	}
			
	ASSERT(log_, maxfd != -1);

	if (ms != -1)
	{
		tv.tv_sec = ms / 1000;
		tv.tv_usec = ms % 1000;
	}

	fdcnt = ::select (maxfd + 1, &read_set, &write_set, 0, (ms == -1 ? 0 : &tv));
	if (fdcnt < 0 && errno != EINTR)
		CRITICAL(log_) << "Could not poll select.";

	for (int sck = 0; sck <= maxfd && fdcnt > 0; ++sck) 
	{
		if (FD_ISSET (sck, &read_set)) 
		{
			it = fd_map_.find (sck);
			IoNode* node = (it != fd_map_.end () ? &it->second : 0);
			EventAction* act;
			Event ok;
			
			if (node && (act = node->read_action))
			{
				Event& ev = (act->callback_ ? act->callback_->param () : ok);
				if ((act->mode_ == StreamModeAccept && (ev.type_ = Event::Done)) || read_channel (sck, ev, 1))
				{
					schedule (act);
					node->reading = false;
				}
			}
			else if (node)
				read_channel (sck, ok, 0);

			ASSERT(log_, fdcnt > 0);
			fdcnt--;
		}

		if (FD_ISSET (sck, &write_set)) 
		{
			it = fd_map_.find (sck);
			IoNode* node = (it != fd_map_.end () ? &it->second : 0);
			EventAction* act;
			Event ok;
			
			if (node && (act = node->write_action))
			{
				Event& ev = (act->callback_ ? act->callback_->param () : ok);
				if ((act->mode_ == StreamModeConnect && (ev.type_ = Event::Done)) || 
					 (act->mode_ == StreamModeWrite && write_channel (sck, ev)) ||
					 (act->mode_ == StreamModeEnd && close_channel (sck, ev)))
				{
					schedule (act);
					node->writing = false;
				}
			}

			ASSERT(log_, fdcnt > 0);
			fdcnt--;
		}
	}
}
