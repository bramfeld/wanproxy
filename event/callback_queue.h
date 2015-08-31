/*
 * Copyright (c) 2010-2011 Juli Mallett. All rights reserved.
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

#ifndef	EVENT_CALLBACK_QUEUE_H
#define	EVENT_CALLBACK_QUEUE_H

#include <deque>
#include <event/callback.h>
#include <event/action.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           callback_queue.h                                           //
// Description:    collection classes for event callback management           //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class CallbackQueue 
{
	class QueuedAction : public Action 
	{
		CallbackQueue& queue_;
		uint64_t generation_;
		Callback* callback_;
		
	public:
		QueuedAction (CallbackQueue& q, uint64_t g, Callback* cb)
		: queue_(q),
		  generation_(g),
		  callback_(cb)
		{ 
		}
		
		~QueuedAction ()
		{
			delete callback_;
		}
		
		virtual void cancel ()
		{
			cancelled_ = true;
			queue_.cancel (this);
		}
		
   	friend class CallbackQueue;
	};

	std::deque<QueuedAction*> queue_;
	uint64_t generation_;
	
public:
	CallbackQueue ()
	: queue_(),
	  generation_(0)
	{ }

	~CallbackQueue ()
	{
		std::deque<QueuedAction*>::iterator it;
		for (it = queue_.begin (); it != queue_.end (); ++it) 
		{
			delete *it;
			*it = 0;
		}
	}

	Action* schedule (Callback* cb)
	{
		QueuedAction* a = new QueuedAction (*this, generation_, cb);
		queue_.push_back (a);
		return (a);
	}

	/*
	 * Runs all callbacks that have already been queued, but none that
	 * are added by callbacks that are called as part of the drain
	 * operation.  Returns true if there are queued callbacks that were
	 * added during drain.
	 */
	bool drain (void)
	{
		generation_++;
		while (! queue_.empty ()) 
		{
			QueuedAction* a = queue_.front ();
			if (a->generation_ >= generation_)
				return (true);
			queue_.pop_front ();
			if (a->callback_)
				a->callback_->execute ();
		}
		return (false);
	}

	bool empty () const
	{
		return (queue_.empty ());
	}

	void perform ()
	{
		if (! queue_.empty ())
		{
			QueuedAction* a = queue_.front ();
			if (a->callback_)
				a->callback_->execute ();
		}
	}

	void cancel (QueuedAction* a)
	{
		std::deque<QueuedAction*>::iterator it;
		for (it = queue_.begin (); it != queue_.end (); ++it) 
		{
			if (*it == a)
			{
				queue_.erase (it);
				break;
			}
		}
		delete a;
	}
};

#endif /* !EVENT_CALLBACK_QUEUE_H */
