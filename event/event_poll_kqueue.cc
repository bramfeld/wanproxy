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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <event/event_system.h>
#include <event/io_service.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_poll_kqueue.cc                                       //
// Description:    IO event handling using FreeBSD kevent functions           //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void IoService::open_resources ()
{
	handle_ = kqueue ();
	ASSERT(log_, handle_ != -1);
}

void IoService::close_resources ()
{
	if (handle_ >= 0)
		::close (handle_);
}

void IoService::set_fd (int fd, int rd, int wr, IoNode* node)
{
	struct kevent kev;
	int rv;
	if (rd == 1)
		EV_SET (&kev, fd, EVFILT_READ, EV_ADD, 0, 0, node);
	else if (wr == 1)
		EV_SET (&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, node);
	else if (rd == -1)
		EV_SET (&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, node);
	else if (wr == -1)
		EV_SET (&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, node);
	rv = ::kevent (handle_, &kev, 1, 0, 0, 0);
	if (rv < 0 && errno != EEXIST && errno != ENOENT)
		CRITICAL(log_) << "Could not add event to kqueue.";
}

void IoService::poll (int ms)
{
	struct kevent kev[IO_POLL_EVENT_COUNT];
	struct timespec ts;
	int evcnt;
	
	if (ms != -1)
	{
		ts.tv_sec = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000000;
	}
	
	evcnt = ::kevent (handle_, 0, 0, kev, IO_POLL_EVENT_COUNT, (ms == -1 ? 0 : &ts));
	if (evcnt < 0 && errno != EINTR)
		CRITICAL(log_) << "Could not poll kqueue.";

	for (int i = 0; i < evcnt; i++) 
	{
		int sck = kev[i].ident;
		int flt = kev[i].filter;
		int flg = kev[i].flags;
		IoNode* node = (IoNode*) kev[i].udata;
		EventAction* act;
		Event ok;
		
		if ((flg & EV_ERROR)) 
		{
			if (flt == EVFILT_READ && node && (act = node->read_action)) 
			{
				if (act->callback_)
					act->callback_->param ().type_ = Event::Error;
				schedule (act);
				node->reading = false;
				set_fd (sck, -1, (node->writing ? 2 : 0), node);
			} 
			else if (flt == EVFILT_WRITE && node && (act = node->write_action)) 
			{
				if (act->callback_)
					act->callback_->param ().type_ = Event::Error;
				schedule (act);
				node->writing = false;
				set_fd (sck, (node->reading ? 2 : 0), -1, node);
			}
		} 
		else if ((flg & EV_EOF)) 
		{
			if (flt == EVFILT_READ && node && (act = node->read_action)) 
			{
				if (act->callback_)
					act->callback_->param ().type_ = Event::EOS;
				schedule (act);
				node->reading = false;
				set_fd (sck, -1, (node->writing ? 2 : 0), node);
			} 
		}
		else if ((flt == EVFILT_READ)) 
		{
			if (node && (act = node->read_action))
			{
				Event& ev = (act->callback_ ? act->callback_->param () : ok);
				if ((act->mode_ == StreamModeAccept && (ev.type_ = Event::Done)) || read_channel (sck, ev, 1))
				{
					schedule (act);
					node->reading = false;
					set_fd (sck, -1, (node->writing ? 2 : 0), node);
				}
			}
			else if (node)
				read_channel (sck, ok, 0);
		}
		else if ((flt == EVFILT_WRITE)) 
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
					set_fd (sck, (node->reading ? 2 : 0), -1, node);
				}
			}
		}
	}
}
