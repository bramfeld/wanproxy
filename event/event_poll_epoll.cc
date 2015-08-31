/*
 * Copyright (c) 2009-2011 Juli Mallett. All rights reserved.
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

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <event/event_system.h>
#include <event/io_service.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_poll_epoll.cc                                        //
// Description:    IO event handling using Linux epoll functions              //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void IoService::open_resources ()
{
	handle_ = ::epoll_create (IO_POLL_EVENT_COUNT);
	ASSERT(log_, handle_ != -1);
}

void IoService::close_resources ()
{
	if (handle_ >= 0)
		::close (handle_);
}

void IoService::set_fd (int fd, int rd, int wr, IoNode* node)
{
	struct epoll_event eev;
	int rv;
	eev.events = ((rd > 0 ? EPOLLIN : 0) | (wr > 0 ? EPOLLOUT : 0));
	eev.data.ptr = node;
	if (eev.events)
		rv = ::epoll_ctl (handle_, (rd && wr ? EPOLL_CTL_MOD : EPOLL_CTL_ADD), fd, &eev);
	else
		rv = ::epoll_ctl (handle_, EPOLL_CTL_DEL, fd, &eev);
	if (rv < 0 && errno != EEXIST && errno != ENOENT)
		CRITICAL(log_) << "Could not add event to epoll.";
}

void IoService::poll (int ms)
{
	struct epoll_event eev[IO_POLL_EVENT_COUNT];
	int evcnt;

	evcnt = ::epoll_wait (handle_, eev, IO_POLL_EVENT_COUNT, ms);
	if (evcnt < 0 && errno != EINTR) 
		CRITICAL(log_) << "Could not poll epoll.";
	
	for (int i = 0; i < evcnt; ++i) 
	{
		int flg = eev[i].events;
		IoNode* node = (IoNode*) eev[i].data.ptr;
		EventAction* act;
		Event ok;
		
		if ((flg & EPOLLIN)) 
		{
			if (node && (act = node->read_action))
			{
				Event& ev = (act->callback_ ? act->callback_->param () : ok);
				if ((act->mode_ == StreamModeAccept && (ev.type_ = Event::Done)) || read_channel (node->fd, ev, 1))
				{
					schedule (act);
					node->reading = false;
					set_fd (node->fd, -1, (node->writing ? 2 : 0), node);
				}
			}
			else if (node)
				read_channel (node->fd, ok, 0);
		}

		if ((flg & EPOLLOUT)) 
		{
			if (node && (act = node->write_action))
			{
				Event& ev = (act->callback_ ? act->callback_->param () : ok);
				if ((act->mode_ == StreamModeConnect && (ev.type_ = Event::Done)) || 
					 (act->mode_ == StreamModeWrite && write_channel (node->fd, ev)) ||
					 (act->mode_ == StreamModeEnd && close_channel (node->fd, ev)))
				{
					schedule (act);
					node->writing = false;
					set_fd (node->fd, (node->reading ? 2 : 0), -1, node);
				}
			}
		}

		if (! (flg & (EPOLLIN | EPOLLOUT))) 
		{
			if ((flg & EPOLLERR)) 
			{
				if (node && (act = node->read_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::Error;
					schedule (act);
					node->reading = false;
					set_fd (node->fd, -1, 0, node);
				} 
				if (node && (act = node->write_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::Error;
					schedule (act);
					node->writing = false;
					set_fd (node->fd, 0, -1, node);
				} 
			} 
			else if ((flg & EPOLLHUP)) 
			{
				if (node && (act = node->read_action)) 
				{
					if (act->callback_)
						act->callback_->param ().type_ = Event::EOS;
					schedule (act);
					node->reading = false;
					set_fd (node->fd, -1, (node->writing ? 2 : 0), node);
				} 
			}
		}
	}
}
