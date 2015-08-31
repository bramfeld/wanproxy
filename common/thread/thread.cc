/*
 * Copyright (c) 2010-2012 Juli Mallett. All rights reserved.
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

#include <common/thread/thread.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           thread.cc                                                  //
// Description:    basic structure for generic thread objects                 //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

namespace 
{
	static void* thread_posix_start (void* arg)
	{
		Thread* td = (Thread*) arg;
		td->main ();
		return 0;
	}
}

Thread::Thread (const std::string& name) : name_(name), thread_id_(0), stop_(false)
{ 
}

bool Thread::start ()
{
	int rv = pthread_create (&thread_id_, NULL, thread_posix_start, this);
	if (rv != 0) 
	{
		ERROR("/thread/posix") << "Unable to start thread.";
		return false;
	}
	return true;
}

void Thread::stop ()
{
	stop_ = true;
	void* val;
	int rv = pthread_join (thread_id_, &val);
	if (rv == -1)
		ERROR("/thread/posix") << "Thread join failed.";
}

