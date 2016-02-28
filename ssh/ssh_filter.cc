/*
 * Copyright (c) 2011-2012 Juli Mallett. All rights reserved.
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

#include <common/endian.h>
#include <event/event_callback.h>
#include <http/http_protocol.h>
#include <ssh/ssh_algorithm_negotiation.h>
#include <ssh/ssh_server_host_key.h>
#include <ssh/ssh_compression.h>
#include <ssh/ssh_encryption.h>
#include <ssh/ssh_key_exchange.h>
#include <ssh/ssh_mac.h>
#include <ssh/ssh_protocol.h>
#include "ssh_filter.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           ssh_filter.cc                                              //
// Description:    SSH encryption/decryption inside a data filter pair        //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

namespace 
{
	static const uint8_t SSHStreamPacket = 0xff;
	static uint8_t zero_padding[255];
}

// Encrypt

SSH::EncryptFilter::EncryptFilter (SSH::Role role, int flg) : BufferedFilter ("/ssh/encrypt"), session_ (role) 
{
	encoded_ = (flg & SOURCE_ENCODED) != 0;
	negotiated_ = false;
	
	session_.algorithm_negotiation_ = new SSH::AlgorithmNegotiation (&session_);
	if (session_.role_ == SSH::ServerRole) 
		session_.algorithm_negotiation_->add_algorithm (SSH::ServerHostKey::server (&session_, "ssh-server1.pem"));
	session_.algorithm_negotiation_->add_algorithms ();
	
	Buffer str ("SSH-2.0-WANProxy " + (std::string) log_);
	session_.local_version (str);
	str.append ("\r\n");
	Filter::produce (str);
}

bool SSH::EncryptFilter::consume (Buffer& buf, int flg)
{
	buf.moveout (&pending_);
	
	if (negotiated_)
	{	
		/*
		 * If we're writing data that has been encoded, we need to tag it.
		 */
		if (encoded_) 
		{
			Buffer packet;
			packet.append (SSHStreamPacket);
			pending_.moveout (&packet);
			return produce (packet, flg);
		} 
		else 
		{
			uint32_t length;
			while (pending_.length () > sizeof length) 
			{
				pending_.extract (&length);
				length = BigEndian::decode (length);
				if (pending_.length () < sizeof length + length) 
				{
					DEBUG(log_) << "Waiting for more write data.";
					return true;
				}

				Buffer packet;
				pending_.moveout (&packet, sizeof length, length);
				if (! produce (packet, flg))
					return false;
			}
		}
	}
	
	return true;
}
	
bool SSH::EncryptFilter::produce (Buffer& buf, int flg)
{
	Encryption *encryption_algorithm;
	MAC *mac_algorithm;
	Buffer packet;
	uint8_t padding_len;
	uint32_t packet_len;
	unsigned block_size;
	Buffer mac;

	encryption_algorithm = session_.active_algorithms_.local_to_remote_->encryption_;
	if (encryption_algorithm) 
	{
		block_size = encryption_algorithm->block_size();
		if (block_size < 8)
			block_size = 8;
	} 
	else 
		block_size = 8;
	mac_algorithm = session_.active_algorithms_.local_to_remote_->mac_;

	packet_len = sizeof padding_len + buf.length();
	padding_len = 4 + (block_size - ((sizeof packet_len + packet_len + 4) % block_size));
	packet_len += padding_len;

	BigEndian::append (&packet, packet_len);
	packet.append (padding_len);
	buf.moveout (&packet);
	packet.append (zero_padding, padding_len);

	if (mac_algorithm) 
	{
		Buffer mac_input;

		SSH::UInt32::encode (&mac_input, session_.local_sequence_number_);
		mac_input.append (&packet);

		if (! mac_algorithm->mac (&mac, &mac_input)) 
		{
			ERROR(log_) << "Could not compute outgoing MAC.";
			return false;
		}
	}

	if (encryption_algorithm) 
	{
		Buffer ciphertext;
		if (! encryption_algorithm->cipher (&ciphertext, &packet)) 
		{
			ERROR(log_) << "Could not encrypt outgoing packet.";
			return false;
		}
		packet = ciphertext;
	}
	if (! mac.empty ())
		packet.append (mac);

	session_.local_sequence_number_++;

	return Filter::produce (packet, flg);
}

void SSH::EncryptFilter::flush (int flg)
{
	if (flg == ALGORITHM_NEGOTIATED)
	{
		negotiated_ = true;
		Buffer bfr;
		if (! pending_.empty ())
			consume (bfr);
	}
	else
	{
		flushing_ = true;
		flush_flags_ |= flg;
	}
	if (flushing_ && negotiated_)
		Filter::flush (flush_flags_);
}

// Decrypt

SSH::DecryptFilter::DecryptFilter (int flg) : LogisticFilter ("/ssh/decrypt")
{
	session_ = 0;
	encoded_ = (flg & SOURCE_ENCODED) != 0;
	identified_ = false;
}

bool SSH::DecryptFilter::consume (Buffer& buf, int flg)
{
	buf.moveout (&pending_);

	if (! identified_) 
	{
		HTTPProtocol::ParseStatus status;

		while (! pending_.empty ()) 
		{
			Buffer line;
			status = HTTPProtocol::ExtractLine (&line, &pending_);
			switch (status) 
			{
			case HTTPProtocol::ParseSuccess:
				break;
			case HTTPProtocol::ParseFailure:
				ERROR(log_) << "Invalid line while waiting for identification string.";
				return false;
			case HTTPProtocol::ParseIncomplete:
				/* Wait for more.  */
				return true;
			}

			if (! line.prefix ("SSH-"))
				continue; /* Next line.  */

			if (! line.prefix ("SSH-2.0")) 
			{
				ERROR(log_) << "Unsupported version.";
				return false;
			}

			if (session_ && session_->algorithm_negotiation_ && upstream_) 
			{
				session_->remote_version (line);
				Buffer packet;
				if (session_->algorithm_negotiation_->init (&packet))
				{
					upstream_->produce (packet);
					identified_ = true;
					break;
				}
					
			}
			
			return false;
		}
		
		if (! identified_) 
			return true;
	}

	while (! pending_.empty ()) 
	{
		Encryption *encryption_algorithm;
		MAC *mac_algorithm;
		Buffer packet;
		Buffer mac;
		unsigned block_size;
		unsigned mac_size;
		uint32_t packet_len;
		uint8_t padding_len;
		uint8_t msg;

		encryption_algorithm = session_->active_algorithms_.remote_to_local_->encryption_;
		if (encryption_algorithm) 
		{
			block_size = encryption_algorithm->block_size();
			if (block_size < 8)
				block_size = 8;
		} 
		else 
			block_size = 8;
		mac_algorithm = session_->active_algorithms_.remote_to_local_->mac_;
		if (mac_algorithm)
			mac_size = mac_algorithm->size();
		else
			mac_size = 0;

		if (pending_.length() <= block_size) 
		{
			DEBUG(log_) << "Waiting for first block of packet.";
			return true;
		}

		if (encryption_algorithm) 
		{
			if (first_block_.empty ()) 
			{
				Buffer block;
				pending_.moveout (&block, block_size);
				if (! encryption_algorithm->cipher (&first_block_, &block)) 
				{
					ERROR(log_) << "Decryption of first block failed.";
					return false;
				}
			}
			BigEndian::extract (&packet_len, &first_block_);
		} 
		else 
		{
			BigEndian::extract (&packet_len, &pending_);
		}

		if (packet_len == 0) 
		{
			ERROR(log_) << "Need to handle 0-length packet.";
			return false;
		}

		if (encryption_algorithm) 
		{
			ASSERT(log_, !first_block_.empty());
			if (block_size + pending_.length() < sizeof packet_len + packet_len + mac_size) 
			{
				DEBUG(log_) << "Need " << sizeof packet_len + packet_len + mac_size << " bytes to decrypt encrypted packet; have " << (block_size + pending_.length()) << ".";
				return true;
			}

			first_block_.moveout (&packet);

			if (sizeof packet_len + packet_len > block_size) 
			{
				Buffer ciphertext;
				pending_.moveout (&ciphertext, sizeof packet_len + packet_len - block_size);
				if (! encryption_algorithm->cipher (&packet, &ciphertext)) 
				{
					ERROR(log_) << "Decryption of packet failed.";
					return false;
				}
			} 
			else 
			{
				DEBUG(log_) << "Packet of exactly one block.";
			}
			ASSERT(log_, packet.length() == sizeof packet_len + packet_len);
		} 
		else 
		{
			if (pending_.length() < sizeof packet_len + packet_len + mac_size) 
			{
				DEBUG(log_) << "Need " << sizeof packet_len + packet_len + mac_size << " bytes; have " << pending_.length() << ".";
				return true;
			}

			pending_.moveout (&packet, sizeof packet_len + packet_len);
		}

		if (mac_algorithm) 
		{
			Buffer expected_mac;
			Buffer mac_input;

			pending_.moveout (&mac, 0, mac_size);
			SSH::UInt32::encode (&mac_input, session_->remote_sequence_number_);
			mac_input.append (packet);
			if (! mac_algorithm->mac (&expected_mac, &mac_input)) 
			{
				ERROR(log_) << "Could not compute expected MAC.";
				return false;
			}
			if (! expected_mac.equal (&mac)) 
			{
				ERROR(log_) << "Received MAC does not match expected MAC.";
				return false;
			}
		}
		packet.skip (sizeof packet_len);

		session_->remote_sequence_number_++;

		padding_len = packet.pop();
		if (padding_len != 0) 
		{
			if (packet.length() < padding_len) 
			{
				ERROR(log_) << "Padding too large for packet.";
				return false;
			}
			packet.trim (padding_len);
		}

		if (packet.empty()) 
		{
			ERROR(log_) << "Need to handle empty packet.";
			return false;
		}

		/*
		 * Pass by range to registered handlers for each range.
		 * Unhandled messages go to the receive_callback_, and
		 * the caller can register key exchange mechanisms,
		 * and handle (or discard) whatever they don't handle.
		 *
		 * NB: The caller could do all this, but it's assumed
		 *     that they usually have better things to do.  If
		 *     they register no handlers, they can certainly do
		 *     so by hand.
		 *
		 * XXX It seems like having a separate class which handles
		 *     all these details and algorithm negotiation would be
		 *     nice, and to have this one be a bit more oriented
		 *     towards managing just the transport layer.
		 *
		 *     At the very least, it needs to take responsibility
		 *     for its failures and allow the handler functions
		 *     here to mangle the packet buffer rather than trying
		 *     to send it on to the receiver if decoding fails.
		 *     A decoding failure should result in a disconnect,
		 *     an error.
		 */
		msg = packet.peek();
		if (msg >= SSH::Message::TransportRangeBegin &&
		    msg <= SSH::Message::TransportRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for transport message.";
		} 
		else if (msg >= SSH::Message::AlgorithmNegotiationRangeBegin &&
			      msg <= SSH::Message::AlgorithmNegotiationRangeEnd) 
		{
			if (session_->algorithm_negotiation_) 
			{
				if (session_->algorithm_negotiation_->input (upstream_, &packet))
					continue;
				ERROR(log_) << "Algorithm negotiation message failed.";
				return false;
			}
			DEBUG(log_) << "Using default handler for algorithm negotiation message.";
		} 
		else if (msg >= SSH::Message::KeyExchangeMethodRangeBegin &&
			      msg <= SSH::Message::KeyExchangeMethodRangeEnd) 
		{
			if (session_->chosen_algorithms_.key_exchange_) 
			{
				if (session_->chosen_algorithms_.key_exchange_->input (upstream_, &packet))
					continue;
				ERROR(log_) << "Key exchange message failed.";
				return false;
			}
			DEBUG(log_) << "Using default handler for key exchange method message.";
		} 
		else if (msg >= SSH::Message::UserAuthenticationGenericRangeBegin &&
			      msg <= SSH::Message::UserAuthenticationGenericRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for generic user authentication message.";
		} 
		else if (msg >= SSH::Message::UserAuthenticationMethodRangeBegin &&
			      msg <= SSH::Message::UserAuthenticationMethodRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for user authentication method message.";
		} 
		else if (msg >= SSH::Message::ConnectionProtocolGlobalRangeBegin &&
			      msg <= SSH::Message::ConnectionProtocolGlobalRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for generic connection protocol message.";
		} 
		else if (msg >= SSH::Message::ConnectionChannelRangeBegin &&
			      msg <= SSH::Message::ConnectionChannelRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for connection channel message.";
		} 
		else if (msg >= SSH::Message::ClientProtocolReservedRangeBegin &&
			      msg <= SSH::Message::ClientProtocolReservedRangeEnd) 
		{
			DEBUG(log_) << "Using default handler for client protocol message.";
		} 
		else if (msg >= SSH::Message::LocalExtensionRangeBegin) 
		{
			/* Because msg is a uint8_t, it will always be <= SSH::Message::LocalExtensionRangeEnd.  */
			DEBUG(log_) << "Using default handler for local extension message.";
		} 
		else 
		{
			ASSERT(log_, msg == 0);
			ERROR(log_) << "Message outside of protocol range received.  Passing to default handler, but not expecting much.";
		}

		/*
		 * If we're reading data that has been encoded, we need to untag it.
		 * Otherwise we need to frame it.
		 */
		if (encoded_) 
		{
			if (packet.peek () != SSHStreamPacket || packet.length() == 1) 
			{
				ERROR(log_) << "Got encoded packet with wrong message.";
				return false;
			}
			packet.skip (1);
		} 
		else 
		{
			uint32_t length = packet.length ();
			length = BigEndian::encode (length);

			Buffer b;
			b.append (&length);
			packet.moveout (&b);
			packet = b;
		}
		
		return produce (packet, flg);
	}
	
	return true;
}

void SSH::DecryptFilter::flush (int flg)
{
	Buffer bfr;
	if (! pending_.empty ())
		consume (bfr);
	Filter::flush (flg);
}

