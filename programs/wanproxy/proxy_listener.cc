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

#include <event/event_system.h>
#include "proxy_connector.h"
#include "proxy_listener.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           proxy_listener.cc                                          //
// Description:    listens on a port spawning a connector for each client     //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

ProxyListener::ProxyListener (const std::string& name,
										WANProxyCodec* local_codec,
										WANProxyCodec* remote_codec,
										SocketAddressFamily local_family,
										const std::string& local_address,
										SocketAddressFamily remote_family,
										const std::string& remote_address,
										bool cln, bool ssh)
 : log_("/wanproxy/proxy/" + name + "/listener"),
   name_(name),
   local_codec_(local_codec),
   remote_codec_(remote_codec),
   local_family_(local_family),
   local_address_(local_address),
   remote_family_(remote_family),
   remote_address_(remote_address),
	is_cln_(cln),
	is_ssh_(ssh),
   accept_action_(0),
   stop_action_(0)
{
	launch_service ();
}

ProxyListener::~ProxyListener ()
{ 
	if (accept_action_)
		accept_action_->cancel ();
	if (stop_action_)
		stop_action_->cancel ();
	close ();
}

void ProxyListener::launch_service ()
{
	if (listen (local_family_, local_address_))
	{
		accept_action_ = accept (callback (this, &ProxyListener::accept_complete));
		INFO(log_) << "Listening on: " << getsockname ();
	}
	else
	{
		HALT(log_) << "Unable to create listener.";
	}
}

void ProxyListener::refresh  (const std::string& name,
										WANProxyCodec* local_codec,
										WANProxyCodec* remote_codec,
										SocketAddressFamily local_family,
										const std::string& local_address,
										SocketAddressFamily remote_family,
										const std::string& remote_address,
										bool cln, bool ssh)
{
	bool relaunch = (local_address != local_address_);
	bool redirect = (remote_address != remote_address_);
	
   name_ = name;
   local_codec_ = local_codec;
   remote_codec_ = remote_codec;
   local_family_ = local_family;
   local_address_ = local_address;
   remote_family_ = remote_family;
   remote_address_ = remote_address;
	is_cln_ = cln;
	is_ssh_ = ssh;
	
	if (relaunch)
	{
		if (accept_action_)
			accept_action_->cancel (), accept_action_ = 0;
		launch_service ();
	}
	
	if (redirect)
	{
		INFO(log_) << "Peer address: " << remote_address_;
	}
}

void ProxyListener::accept_complete (Event e, Socket* sck)
{
	switch (e.type_) 
	{
	case Event::Done:
		DEBUG(log_) << "Accepted client: " << sck->getpeername ();
		new ProxyConnector (name_, local_codec_, remote_codec_, sck, remote_family_, remote_address_, is_cln_, is_ssh_);
		break;
	case Event::Error:
		ERROR(log_) << "Accept error: " << e;
		break;
	default:
		ERROR(log_) << "Unexpected event: " << e;
		break;
	}
}

