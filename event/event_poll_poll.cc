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
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <event/event_system.h>
#include <event/io_service.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_poll_poll.cc                                         //
// Description:    IO event handling using standard poll functions            //
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
	struct pollfd fds[fd_map_.size () + 1];
	int i, j, f;
	int cnt;
	
	fds[0].fd = rfd_;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	j = 1;
	
	std::map<int, IoNode>::iterator it;
	for (it = fd_map_.begin (); it != fd_map_.end (); ++it)
	{
		if ((f = (it->second.reading ? POLLIN : 0) | (it->second.writing ? POLLOUT : 0)))
		{
			fds[j].fd = it->first;
			fds[j].events = f;
			fds[j].revents = 0;
			++j;
		}
	}
			
	cnt = ::poll (fds, j, ms);
	if (cnt < 0 && errno != EINTR)
		CRITICAL(log_) << "Could not poll.";

	for (i = 0; i < j; ++i) 
	{
		if (fds[i].revents == 0)
			continue;
		if (cnt-- == 0)
			break;
			
		int sck = fds[i].fd;
		int flg = fds[i].revents;
		it = fd_map_.find (sck);
		IoNode* node = (it != fd_map_.end () ? &it->second : 0);
		EventAction* act;
		Event ok;
		
		if ((flg & POLLIN)) 
		{
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
		}

		if ((flg & POLLOUT)) 
		{
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
		}

		if (! (flg & (POLLIN | POLLOUT))) 
		{
			if ((flg & (POLLERR | POLLNVAL))) 
			{
				if (node && (act = node->read_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::Error;
					schedule (act);
					node->reading = false;
				} 
				if (node && (act = node->write_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::Error;
					schedule (act);
					node->writing = false;
				} 
			} 
			else if ((flg & POLLHUP)) 
			{
				if (node && (act = node->read_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::EOS;
					schedule (act);
					node->reading = false;
				} 
			}
		}
	}
}
