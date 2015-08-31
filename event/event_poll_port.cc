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
#include <sys/errno.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>
#include <port.h>
#include <unistd.h>
#include <event/event_system.h>
#include <event/io_service.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_poll_port.cc                                         //
// Description:    IO event handling using SunOS port functions               //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void IoService::open_resources ()
{
	handle_ = port_create ();
	ASSERT(log_, handle_ != -1);
}

void IoService::close_resources ()
{
	if (handle_ >= 0)
		::close (handle_);
}

void IoService::set_fd (int fd, int rd, int wr, IoNode* node)
{
	int events;
	int rv;
	events = ((rd > 0 ? EPOLLIN : 0) | (wr > 0 ? EPOLLOUT : 0));
	if (events) 
	{
		rv = ::port_associate (handle_, PORT_SOURCE_FD, fd, events, node);
		if (rv < 0 && errno != EEXIST)
			CRITICAL(log_) << "Could not add event to port.";
	}
	else
	{
		rv = ::port_dissociate (handle_, PORT_SOURCE_FD, fd);
		if (rv < 0 && errno != ENOENT)
			CRITICAL(log_) << "Could not add event to port.";
	}
}

void IoService::poll (int ms)
{
	port_event_t pev[IO_POLL_EVENT_COUNT];
	unsigned int evcnt = 1;
	struct timespec ts;
	int rv;
	
	if (ms != -1)
	{
		ts.tv_sec = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000000;
	}
	
	rv = ::port_getn (handle_, pev, IO_POLL_EVENT_COUNT, &evcnt, (ms == -1 ? 0 : &ts));
	if (rv < 0)
	{
		if (errno != EINTR)
			CRITICAL(log_) << "Could not poll port.";
		evcnt = 0;
	}
			
	for (unsigned int i = 0; i < evcnt; ++i) 
	{
		int sck = pev[i].portev_object;
		int flg = pev[i].portev_events;
		IoNode* node = (IoNode*) pev[i].portev_user;
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
				else
					set_fd (sck, 1, (node->writing ? 2 : 0), node);
			}
			else if (node)
			{
				read_channel (sck, ok, 0);
				set_fd (sck, 1, (node->writing ? 2 : 0), node);
			}
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
				else
					set_fd (sck, (node->reading ? 2 : 0), 1, node);
			}
		}

		if (! (flg & (POLLIN | POLLOUT))) 
		{
			if ((flg & POLLERR)) 
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
