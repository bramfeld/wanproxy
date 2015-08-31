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

#include <common/buffer.h>
#include <common/endian.h>
#include <programs/wanproxy/wanproxy.h>
#include "xcodec_filter.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_filter.cc                                           //
// Description:    instantiation of encoder/decoder in a data filter pair     //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
 * Usage:
 * 	<OP_HELLO> length[uint8_t] data[uint8_t x length]
 *
 * Effects:
 * 	Must appear at the start of and only at the start of an encoded	stream.
 *
 * Sife-effects:
 * 	Possibly many.
 */
#define	XCODEC_PIPE_OP_HELLO	((uint8_t)0xff)

/*
 * Usage:
 * 	<OP_LEARN> data[uint8_t x XCODEC_PIPE_SEGMENT_LENGTH]
 *
 * Effects:
 * 	The `data' is hashed, the hash is associated with the data if possible.
 *
 * Side-effects:
 * 	None.
 */
#define	XCODEC_PIPE_OP_LEARN	((uint8_t)0xfe)

/*
 * Usage:
 * 	<OP_ASK> hash[uint64_t]
 *
 * Effects:
 * 	An OP_LEARN will be sent in response with the data corresponding to the
 * 	hash.
 *
 * 	If the hash is unknown, error will be indicated.
 *
 * Side-effects:
 * 	None.
 */
#define	XCODEC_PIPE_OP_ASK	((uint8_t)0xfd)

/*
 * Usage:
 * 	<OP_EOS>
 *
 * Effects:
 * 	Alert the other party that we have no intention of sending more data.
 *
 * Side-effects:
 * 	The other party will send <OP_EOS_ACK> when it has processed all of
 * 	the data we have sent.
 */
#define	XCODEC_PIPE_OP_EOS	((uint8_t)0xfc)

/*
 * Usage:
 * 	<OP_EOS_ACK>
 *
 * Effects:
 * 	Alert the other party that we have no intention of reading more data.
 *
 * Side-effects:
 * 	The connection will be torn down.
 */
#define	XCODEC_PIPE_OP_EOS_ACK	((uint8_t)0xfb)

/*
 * Usage:
 * 	<FRAME> length[uint16_t] data[uint8_t x length]
 *
 * Effects:
 * 	Frames an encoded chunk.
 *
 * Side-effects:
 * 	None.
 */
#define	XCODEC_PIPE_OP_FRAME	((uint8_t)0x00)

#define	XCODEC_PIPE_MAX_FRAME	(32768)

// Encoding

bool EncodeFilter::consume (Buffer& buf)
{
	Buffer output;
	Buffer enc;

	ASSERT(log_, ! flushing_);

	if (! encoder_) 
   {
		if (! cache_ || ! cache_->identifier().is_valid ()) 
      {
			ERROR(log_) << "Could not encode UUID for <HELLO>.";
			return false;
		}
		
		output.append (XCODEC_PIPE_OP_HELLO);
		uint64_t mb = cache_->nominal_size ();
		output.append ((uint8_t) (UUID_STRING_SIZE + sizeof mb));
		cache_->identifier().encode (output);
		output.append (&mb);

		if (! (encoder_ = new XCodecEncoder (cache_)))
			return false;
	}

	encoder_->encode (enc, buf);

	while (! enc.empty ()) 
	{
		int n = enc.length ();
		if (n > XCODEC_PIPE_MAX_FRAME)
			n = XCODEC_PIPE_MAX_FRAME;
			
		Buffer frame;
		enc.moveout (&frame, n);
		
		uint16_t len = n;
		len = BigEndian::encode (len);

		output.append (XCODEC_PIPE_OP_FRAME);
		output.append (&len);
		output.append (frame);
	}
   
   return produce (output);
}
	
void EncodeFilter::flush (int flg)
{
	if (flg == XCODEC_PIPE_OP_EOS_ACK)
		eos_ack_ = true;
	else
	{
		flushing_ = true;
		flush_flags_ |= flg;
		if (! sent_eos_)
		{
			Buffer output;
			output.append (XCODEC_PIPE_OP_EOS);
			sent_eos_ = produce (output);
		}
	}
	if (flushing_ && eos_ack_)
		Filter::flush (flush_flags_);
}

// Decoding
	
bool DecodeFilter::consume (Buffer& buf)
{
   if (! upstream_) 
   {
		ERROR(log_) << "Decoder not configured";
      return false;
   }
   
	pending_.append (buf);

	while (! pending_.empty ()) 
   {
		uint8_t op = pending_.peek ();
		switch (op) 
      {
		case XCODEC_PIPE_OP_HELLO:
			if (decoder_cache_) 
         {
				ERROR(log_) << "Got <HELLO> twice.";
				return false;
			}
			else
			{
		      uint8_t len;
		      if (pending_.length() < sizeof op + sizeof len)
		         return true;
		      pending_.extract (&len, sizeof op);
		      if (pending_.length() < sizeof op + sizeof len + len)
		         return true;

				uint64_t mb;
		      if (len != UUID_STRING_SIZE + sizeof mb) 
		      {
		         ERROR(log_) << "Unsupported <HELLO> length: " << (unsigned)len;
		         return false;
		      }

		      UUID uuid;
		      pending_.skip (sizeof op + sizeof len);
		      if (! uuid.decode (pending_)) 
		      {
		         ERROR(log_) << "Invalid UUID in <HELLO>.";
		         return false;
		      }
		      pending_.extract (&mb);
		      pending_.skip (sizeof mb);

				if (! (decoder_cache_ = wanproxy.find_cache (uuid)))
					decoder_cache_ = wanproxy.add_cache (uuid, mb);

		      ASSERT(log_, decoder_ == NULL);
				if (decoder_cache_)
					decoder_ = new XCodecDecoder (decoder_cache_);

		      DEBUG(log_) << "Peer connected with UUID: " << uuid;
			}
			break;
         
		case XCODEC_PIPE_OP_ASK:
			if (! encoder_cache_) 
			{
				ERROR(log_) << "Decoder not configured";
				return false;
			}
			else
         {
		      uint64_t hash;
		      if (pending_.length() < sizeof op + sizeof hash)
		         return true;
		         
		      pending_.skip (sizeof op);
		      pending_.moveout (&hash);
		      hash = BigEndian::decode (hash);
				
		      Buffer learn;
		      learn.append (XCODEC_PIPE_OP_LEARN);
		      if (encoder_cache_->lookup (hash, learn))
				{
					DEBUG(log_) << "Responding to <ASK> with <LEARN>.";
					if (! upstream_->produce (learn))
						return false;
				}
				else
		      {
		         ERROR(log_) << "Unknown hash in <ASK>: " << hash;
		         return false;
		      }
			}
			break;
         
		case XCODEC_PIPE_OP_LEARN:
			if (! decoder_cache_) 
         {
				ERROR(log_) << "Got <LEARN> before <HELLO>.";
				return false;
			} 
			else
         {
		      if (pending_.length() < sizeof op + XCODEC_SEGMENT_LENGTH)
		         return true;

		      pending_.skip (sizeof op);
				uint8_t data[XCODEC_SEGMENT_LENGTH];
		      pending_.copyout (data, XCODEC_SEGMENT_LENGTH);
		      uint64_t hash = XCodecHash::hash (data);
		      if (unknown_hashes_.find (hash) == unknown_hashes_.end ())
		         INFO(log_) << "Gratuitous <LEARN> without <ASK>.";
		      else
		         unknown_hashes_.erase (hash);
					
				Buffer old;
				if (decoder_cache_->lookup (hash, old))
				{
					if (old.equal (data, sizeof data))
					{
						DEBUG(log_) << "Redundant <LEARN>.";
					}
					else
					{
		            ERROR(log_) << "Collision in <LEARN>.";
		            return false;
		         }
					old.clear ();
		      } 
		      else 
		      {
		         DEBUG(log_) << "Successful <LEARN>.";
		         decoder_cache_->enter (hash, pending_, 0);
		      }
		      pending_.skip (XCODEC_SEGMENT_LENGTH);
		   }
			break;
         
		case XCODEC_PIPE_OP_EOS:
			if (received_eos_) 
         {
				ERROR(log_) << "Duplicate <EOS>.";
				return false;
			}
			pending_.skip (sizeof op);
			received_eos_ = true;
			break;
         
		case XCODEC_PIPE_OP_EOS_ACK:
			if (received_eos_ack_) 
         {
				ERROR(log_) << "Duplicate <EOS_ACK>.";
				return false;
			}
			pending_.skip (sizeof op);
			received_eos_ack_ = true;
			break;
         
		case XCODEC_PIPE_OP_FRAME:
			if (! decoder_) 
         {
				ERROR(log_) << "Got frame data before decoder initialized.";
				return false;
			}
			else
			{
		      uint16_t len;
		      if (pending_.length() < sizeof op + sizeof len)
		         return true;
		         
		      pending_.extract (&len, sizeof op);
		      len = BigEndian::decode (len);
		      if (len == 0 || len > XCODEC_PIPE_MAX_FRAME) 
		      {
		         ERROR(log_) << "Invalid framed data length.";
		         return false;
		      }
		      if (pending_.length() < sizeof op + sizeof len + len)
		         return true;

		      pending_.moveout (&frame_buffer_, sizeof op + sizeof len, len);
		   }
			break;
         
		default:
			ERROR(log_) << "Unsupported operation in pipe stream.";
			return false;
		}

		if (frame_buffer_.empty ()) 
			continue;

		if (! unknown_hashes_.empty ()) 
      {
			DEBUG(log_) << "Waiting for unknown hashes to continue processing data.";
			continue;
		}

		Buffer output;
		if (! decoder_->decode (output, frame_buffer_, unknown_hashes_)) 
      {
			ERROR(log_) << "Decoder exiting with error.";
			return false;
		}

		if (! output.empty ()) 
      {
			ASSERT(log_, ! flushing_);
			if (! produce (output))
				return false;
		} 
      else 
      {
			/*
			 * We should only get no output from the decoder if
			 * we're waiting on the next frame or we need an
			 * unknown hash.  It would be nice to make the
			 * encoder framing aware so that it would not end
			 * up with encoded data that straddles a frame
			 * boundary.  (Fixing that would also allow us to
			 * simplify length checking within the decoder
			 * considerably.)
			 */
			ASSERT(log_, !frame_buffer_.empty() || !unknown_hashes_.empty());
		}

		Buffer ask;
		std::set<uint64_t>::const_iterator it;
		for (it = unknown_hashes_.begin(); it != unknown_hashes_.end(); ++it) 
      {
			uint64_t hash = *it;
			hash = BigEndian::encode (hash);
			ask.append (XCODEC_PIPE_OP_ASK);
			ask.append (&hash);
		}
		if (! ask.empty ()) 
      {
			DEBUG(log_) << "Sending <ASK>s.";
			if (! upstream_->produce (ask))
				return false;
		}
	}

   if (received_eos_ && ! sent_eos_ack_ && frame_buffer_.empty ()) 
   {
      DEBUG(log_) << "Decoder received <EOS>, sending <EOS_ACK>.";

      Buffer eos_ack;
      eos_ack.append (XCODEC_PIPE_OP_EOS_ACK);
      sent_eos_ack_ = true;
      if (! upstream_->produce (eos_ack))
			return false;
   }
   
	/*
	 * If we have received EOS and not yet sent it, we can send it now.
	 * The only caveat is that if we have outstanding <ASK>s, i.e. we have
	 * not yet emptied decoder_unknown_hashes_, then we can't send EOS yet.
	 */
	if (received_eos_ && ! flushing_) 
	{
		if (unknown_hashes_.empty ()) 
		{
			if (! frame_buffer_.empty ())
				return false;
			DEBUG(log_) << "Decoder received <EOS>, shutting down decoder output channel.";
			flushing_ = true;
			Filter::flush (0);
		} 
		else 
		{
			if (frame_buffer_.empty ())
				return false;
			DEBUG(log_) << "Decoder waiting to send <EOS> until <ASK>s are answered.";
		}
	}

	/*
	 * NB:
	 * Along with the comment above, there is some relevance here.  If we
	 * use some kind of hierarchical decoding, then we need to be able to
	 * handle the case where an <ASK>'s response necessitates us to send
	 * another <ASK> or something of that sort.  There are other conditions
	 * where we may still need to send something out of the encoder, but
	 * thankfully none seem to arise yet.
	 */
	if (sent_eos_ack_ && received_eos_ack_ && ! upflushed_) 
   {
		ASSERT(log_, pending_.empty());
		ASSERT(log_, frame_buffer_.empty());
		DEBUG(log_) << "Decoder finished, got <EOS_ACK>, shutting down encoder output channel.";

		upflushed_ = true;
      upstream_->flush (XCODEC_PIPE_OP_EOS_ACK);
	}
	
	return true;
}

void DecodeFilter::flush (int flg)
{
	flushing_ = true;
	flush_flags_ |= flg;
	if (! pending_.empty ())
		DEBUG(log_) << "Flushing decoder with data outstanding.";
	if (! frame_buffer_.empty ())
		DEBUG(log_) << "Flushing decoder with frame data outstanding.";
	if (! upflushed_ && upstream_)
      upstream_->flush (XCODEC_PIPE_OP_EOS_ACK);
	Filter::flush (flush_flags_);
}

