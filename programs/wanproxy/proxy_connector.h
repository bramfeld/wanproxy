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

#ifndef	PROGRAMS_WANPROXY_PROXY_CONNECTOR_H
#define	PROGRAMS_WANPROXY_PROXY_CONNECTOR_H

#define REQUEST_CHAIN_FLUSHING	0x10000
#define RESPONSE_CHAIN_FLUSHING	0x20000
#define REQUEST_CHAIN_READY		0x40000
#define RESPONSE_CHAIN_READY		0x80000

#include <common/filter.h>
#include <event/action.h>
#include <event/event.h>
#include <io/socket/socket_types.h>
#include "wanproxy_codec.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           proxy_connector.h                                          //
// Description:    carries data between endpoints through a filter chain      //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class ProxyConnector : public Filter
{
	LogHandle log_;
	WANProxyCodec* local_codec_;
	WANProxyCodec* remote_codec_;
	Socket* local_socket_;
	Socket* remote_socket_;
	bool is_cln_, is_ssh_;
	FilterChain request_chain_;
	FilterChain response_chain_;
	Action* connect_action_;
	Action* stop_action_;
	Action* request_action_;
	Action* response_action_;
	Action* close_action_;
   int flushing_;

public:
	ProxyConnector (const std::string&, WANProxyCodec*, WANProxyCodec*, 
						 Socket*, SocketAddressFamily, const std::string&, bool cln, bool ssh);
	virtual ~ProxyConnector ();

	void connect_complete (Event e);
	bool build_chains (WANProxyCodec* cdc1, WANProxyCodec* cdc2, Socket* sck1, Socket* sck2);
	void on_request_data (Event e);
	void on_response_data (Event e);
   virtual void flush (int flg);
   void conclude (Event e);
};

#endif /* !PROGRAMS_WANPROXY_PROXY_CONNECTOR_H */
