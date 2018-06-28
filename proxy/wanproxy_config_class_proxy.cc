/*
 * Copyright (c) 2009-2013 Juli Mallett. All rights reserved.
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

#include <common/buffer.h>

#include <config/config_class.h>
#include <config/config_object.h>

#include <event/event_callback.h>
#include <event/event_system.h>

#include <io/socket/socket_types.h>

#include "wanproxy_config_class_codec.h"
#include "wanproxy_config_class_interface.h"
#include "wanproxy_config_class_peer.h"
#include "wanproxy_config_class_proxy.h"
#include "wanproxy.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           wanproxy_config_class_proxy.cc                             //
// Description:    high-level parser for the global proxy object              //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

WANProxyConfigClassProxy wanproxy_config_class_proxy;

bool
WANProxyConfigClassProxy::Instance::activate(const ConfigObject *co)
{
	WANProxyConfigClassInterface::Instance *interface =
		dynamic_cast<WANProxyConfigClassInterface::Instance *>(interface_->instance_);
	if (interface == NULL)
		return (false);

	if (interface->host_ == "" || interface->port_ == "")
		return (false);

	WANProxyCodec *interface_codec;
	if (interface_codec_ != NULL) {
		WANProxyConfigClassCodec::Instance *codec =
			dynamic_cast<WANProxyConfigClassCodec::Instance *>(interface_codec_->instance_);
		if (codec == NULL)
			return (false);
		interface_codec = &codec->codec_;
	} else {
		interface_codec = NULL;
	}

	WANProxyConfigClassPeer::Instance *peer =
		dynamic_cast<WANProxyConfigClassPeer::Instance *>(peer_->instance_);
	if (peer == NULL)
		return (false);

	if (peer->host_ == "" || peer->port_ == "")
		return (false);

	WANProxyCodec *peer_codec;
	if (peer_codec_ != NULL) {
		WANProxyConfigClassCodec::Instance *codec =
			dynamic_cast<WANProxyConfigClassCodec::Instance *>(peer_codec_->instance_);
		if (codec == NULL)
			return (false);
		peer_codec = &codec->codec_;
	} else {
		peer_codec = NULL;
	}
	
	if (role_ == WANProxyConfigProxyRoleUndefined && ! interface_codec && peer_codec)
		role_ = WANProxyConfigProxyRoleClient;
		
	WanProxyInstance ins;
	ins.proxy_name_ = co->name_;
	ins.proxy_client_ = (role_ == WANProxyConfigProxyRoleClient);
	ins.proxy_secure_ = (type_ == WANProxyConfigProxyTypeSSHSSH);
	ins.local_protocol_ = interface->family_;
	ins.local_address_ = '[' + interface->host_ + ']' + ':' + interface->port_;
	ins.local_codec_ = (interface_codec ? *interface_codec : WANProxyCodec ());
	ins.remote_protocol_ = peer->family_;
	ins.remote_address_ = '[' + peer->host_ + ']' + ':' + peer->port_;
	ins.remote_codec_ = (peer_codec ? *peer_codec : WANProxyCodec ());
	wanproxy.add_proxy (ins.proxy_name_, ins);
	
	return (true);
}
