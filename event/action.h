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

#ifndef	EVENT_ACTION_H
#define	EVENT_ACTION_H

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           action.h                                                   //
// Description:    basic classes for event callback management                //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class Action 
{
protected:
	bool cancelled_;
	
	Action () : cancelled_(false)
	{ 
	}
	
public:
	virtual ~Action ()
	{
		ASSERT("/action", cancelled_);
	}

	virtual void cancel () = 0;
	
	bool is_cancelled ()
	{
		return cancelled_;
	}
};


template<class S, class C> class CallbackAction : public Action
{
	typedef void (S::*const method_t)(void);

public:
	S* const obj_;
	method_t method_;
	C* callback_;
	
public:
	CallbackAction (S* obj, method_t method, C* cb)	: obj_(obj), method_(method)
	{ 
		ASSERT("/action", obj && method);
		callback_ = cb;
	}

	~CallbackAction()
	{
		delete callback_;
	}

	virtual void cancel ()
	{
		cancelled_ = true;
		(obj_->*method_) ();
	}
};

#endif /* !EVENT_ACTION_H */
