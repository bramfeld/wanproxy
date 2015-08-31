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

#ifndef	PROGRAMS_WANPROXY_PROXY_LISTENER_H
#define	PROGRAMS_WANPROXY_PROXY_LISTENER_H

#include <event/action.h>
#include <event/event.h>
#include <io/net/tcp_server.h>
#include "wanproxy_codec.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           proxy_listener.h                                           //
// Description:    listens on a port spawning a connector for each client     //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class ProxyListener : public TCPServer
{
	LogHandle log_;
	std::string name_;
	WANProxyCodec* local_codec_;
	WANProxyCodec* remote_codec_;
	SocketAddressFamily local_family_;
	std::string local_address_;
	SocketAddressFamily remote_family_;
	std::string remote_address_;
	bool is_cln_, is_ssh_;
	Action* accept_action_;
	Action* stop_action_;
	
public:
	ProxyListener (const std::string&, WANProxyCodec*, WANProxyCodec*, SocketAddressFamily, const std::string&,
						SocketAddressFamily, const std::string&, bool cln, bool ssh);
	~ProxyListener ();

	void launch_service ();
	void refresh (const std::string&, WANProxyCodec*, WANProxyCodec*, SocketAddressFamily, const std::string&,
					  SocketAddressFamily, const std::string&, bool cln, bool ssh);
	void accept_complete (Event e, Socket* client);
};

#endif /* !PROGRAMS_WANPROXY_PROXY_LISTENER_H */
