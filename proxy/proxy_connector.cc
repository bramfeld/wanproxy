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
#include <io/socket/socket.h>
#include <io/sink_filter.h>
#include <ssh/ssh_filter.h>
#include <xcodec/xcodec_filter.h>
#include <zlib/zlib_filter.h>
#include <common/count_filter.h>
#include "proxy_connector.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           proxy_connector.cc                                         //
// Description:    carries data between endpoints through a filter chain      //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

ProxyConnector::ProxyConnector (const std::string& name,
          WANProxyCodec* local_codec,
			 WANProxyCodec* remote_codec,
          Socket* local_socket,
			 SocketAddressFamily family,
			 const std::string& remote_name,
			 bool cln, bool ssh)
 : log_("/wanproxy/" + name + "/connector"),
   local_codec_(local_codec),
   remote_codec_(remote_codec),
   local_socket_(local_socket),
   remote_socket_(0),
	is_cln_(cln),
	is_ssh_(ssh),
   request_chain_(this),
   response_chain_(this),
   connect_action_(0),
   stop_action_(0),
	request_action_(0),
	response_action_(0),
	close_action_(0),
	flushing_(0)
{
	if (local_socket_ && (remote_socket_ = Socket::create (family, SocketTypeStream, "tcp", remote_name)))
	{
		connect_action_ = remote_socket_->connect (remote_name, callback (this, &ProxyConnector::connect_complete));
		stop_action_ = event_system.register_interest (EventInterestStop, callback (this, &ProxyConnector::conclude));
	}
	else
	{
		close_action_ = event_system.track (0, StreamModeWait, callback (this, &ProxyConnector::conclude));
	}
}

ProxyConnector::~ProxyConnector ()
{
   if (connect_action_)
      connect_action_->cancel ();
   if (stop_action_)
      stop_action_->cancel ();
   if (request_action_)
      request_action_->cancel ();
   if (response_action_)
      response_action_->cancel ();
	if (close_action_)
		close_action_->cancel ();
	if (local_socket_)
		local_socket_->close ();
	if (remote_socket_)
		remote_socket_->close ();
   delete local_socket_;
   delete remote_socket_;
}

void ProxyConnector::connect_complete (Event e)
{
	if (connect_action_)
		connect_action_->cancel (), connect_action_ = 0;

	switch (e.type_) 
	{
	case Event::Done:
		break;
	case Event::Error:
		INFO(log_) << "Connect failed: " << e;
		conclude (e);
		return;
	default:
		ERROR(log_) << "Unexpected event: " << e;
		conclude (e);
		return;
	}

   if (build_chains (local_codec_, remote_codec_, local_socket_, remote_socket_))
	{
		request_action_ = local_socket_->read (callback (this, &ProxyConnector::on_request_data));
		response_action_ = remote_socket_->read (callback (this, &ProxyConnector::on_response_data));
	}
}

bool ProxyConnector::build_chains (WANProxyCodec* cdc1, WANProxyCodec* cdc2, Socket* sck1, Socket* sck2)
{
   if (! sck1 || ! sck2)
      return false;
      
   response_chain_.prepend (new SinkFilter ("/wanproxy/response", sck1, is_cln_));
	
	if (is_ssh_)
	{
		SSH::EncryptFilter* enc; SSH::DecryptFilter* dec;
		request_chain_.append ((dec = new SSH::DecryptFilter ((cdc1 ? SSH::SOURCE_ENCODED : 0))));
		response_chain_.prepend ((enc = new SSH::EncryptFilter (SSH::ServerRole, (cdc1 ? SSH::SOURCE_ENCODED : 0))));
		dec->set_encrypter (enc);
	}
   
	if (cdc1) 
   {
		if (cdc1->counting_) 
      {
			request_chain_.append (new CountFilter (cdc1->request_input_bytes_));
			response_chain_.prepend (new CountFilter (cdc1->response_output_bytes_));
		}

		if (cdc1->compressor_) 
      {
			request_chain_.append (new InflateFilter ());
			response_chain_.prepend (new DeflateFilter (cdc1->compressor_level_));
		}

		if (cdc1->xcache_) 
      {
			EncodeFilter* enc; DecodeFilter* dec;
			request_chain_.append ((dec = new DecodeFilter ("/wanproxy/" + cdc1->name_ + "/dec", cdc1)));
			response_chain_.prepend ((enc = new EncodeFilter ("/wanproxy/" + cdc1->name_ + "/enc", cdc1, 1)));
         dec->set_upstream (enc);
		}

		if (cdc1->counting_) 
      {
			request_chain_.append (new CountFilter (cdc1->request_output_bytes_));
			response_chain_.prepend (new CountFilter (cdc1->response_input_bytes_, 1));
		}
	}

	if (cdc2) 
   {
		if (cdc2->counting_) 
      {
			request_chain_.append (new CountFilter (cdc2->request_input_bytes_));
			response_chain_.prepend (new CountFilter (cdc2->response_output_bytes_));
		}

		if (cdc2->xcache_) 
      {
			EncodeFilter* enc; DecodeFilter* dec;
			request_chain_.append ((enc = new EncodeFilter ("/wanproxy/" + cdc2->name_ + "/enc", cdc2)));
			response_chain_.prepend ((dec = new DecodeFilter ("/wanproxy/" + cdc2->name_ + "/dec", cdc2)));
         dec->set_upstream (enc);
		}

		if (cdc2->compressor_) 
      {
			request_chain_.append (new DeflateFilter (cdc2->compressor_level_));
			response_chain_.prepend (new InflateFilter ());
		}

		if (cdc2->counting_) 
      {
			request_chain_.append (new CountFilter (cdc2->request_output_bytes_));
			response_chain_.prepend (new CountFilter (cdc2->response_input_bytes_));
		}
	}
   
	if (is_ssh_)
	{
		SSH::EncryptFilter* enc; SSH::DecryptFilter* dec;
		request_chain_.append ((enc = new SSH::EncryptFilter (SSH::ClientRole, (cdc2 ? SSH::SOURCE_ENCODED : 0))));
		response_chain_.prepend ((dec = new SSH::DecryptFilter ((cdc2 ? SSH::SOURCE_ENCODED : 0))));
		dec->set_encrypter (enc);
	}
   
   request_chain_.append (new SinkFilter ("/wanproxy/request", sck2));
   
   return true;
}

void ProxyConnector::on_request_data (Event e)
{
	if (request_action_)
		request_action_->cancel (), request_action_ = 0;
	if (flushing_ & REQUEST_CHAIN_FLUSHING)
		return;
		
	switch (e.type_) 
	{
	case Event::Done:
		request_action_ = local_socket_->read (callback (this, &ProxyConnector::on_request_data));
		if (request_chain_.consume (e.buffer_))
			break;
	case Event::EOS:
		DEBUG(log_) << "Flushing request";
		flushing_ |= REQUEST_CHAIN_FLUSHING;
		request_chain_.flush (REQUEST_CHAIN_READY);
		break;
	default:
		DEBUG(log_) << "Unexpected event: " << e;
		conclude (e);
		return;
	}
}

void ProxyConnector::on_response_data (Event e)
{
	if (response_action_)
		response_action_->cancel (), response_action_ = 0;
	if (flushing_ & RESPONSE_CHAIN_FLUSHING)
		return;
		
	switch (e.type_) 
	{
	case Event::Done:
		response_action_ = remote_socket_->read (callback (this, &ProxyConnector::on_response_data));
		if (response_chain_.consume (e.buffer_))
			break;
	case Event::EOS:
		DEBUG(log_) << "Flushing response";
		flushing_ |= RESPONSE_CHAIN_FLUSHING;
		response_chain_.flush (RESPONSE_CHAIN_READY);
		break;
	default:
		DEBUG(log_) << "Unexpected event: " << e;
		conclude (e);
		return;
	}
}

void ProxyConnector::flush (int flg)
{
	flushing_ |= flg;
	if ((flushing_ & (REQUEST_CHAIN_READY | RESPONSE_CHAIN_READY)) == (REQUEST_CHAIN_READY | RESPONSE_CHAIN_READY))
		if (! close_action_)
			close_action_ = event_system.track (0, StreamModeWait, callback (this, &ProxyConnector::conclude));
}

void ProxyConnector::conclude (Event e)
{
   delete this;
}
