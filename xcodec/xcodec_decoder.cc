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

#include <common/buffer.h>
#include <common/endian.h>

#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/xcodec_decoder.h>
#include <xcodec/xcodec_encoder.h>
#include <xcodec/xcodec_hash.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_decoder.cc                                          //
// Description:    decoding routines for the xcodex protocol                  //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

XCodecDecoder::XCodecDecoder(XCodecCache* cache)
: log_("/xcodec/decoder"),
  cache_(cache)
{ }

XCodecDecoder::~XCodecDecoder()
{ }

/*
 * XXX These comments are out-of-date.
 *
 * Decode an XCodec-encoded stream.  Returns false if there was an
 * inconsistency, error or unrecoverable condition in the stream.
 * Returns true if we were able to process the stream entirely or
 * expect to be able to finish processing it once more data arrives.
 * The input buffer is cleared of anything we can parse right now.
 *
 * Since some events later in the stream (i.e. ASK or LEARN) may need
 * to be processed before some earlier in the stream (i.e. REF), we
 * parse the stream into a list of actions to take, performing them
 * as we go if possible, otherwise queueing them to occur until the
 * action that is blocking the stream has been satisfied or the stream
 * has been closed.
 *
 * XXX For now we will ASK in every stream where an unknown hash has
 * occurred and expect a LEARN in all of them.  In the future, it is
 * desirable to optimize this.  Especially once we start putting an
 * instance UUID in the HELLO message and can tell which streams
 * share an originator.
 */
bool
XCodecDecoder::decode (Buffer& output, Buffer& input, std::set<uint64_t>& unknown_hashes)
{
	uint8_t data[XCODEC_SEGMENT_LENGTH];
	Buffer old;
	uint64_t behash;
	uint64_t hash;
	unsigned off;
	uint8_t op;
	
	while (! input.empty()) 
	{
		if (! input.find (XCODEC_MAGIC, &off)) 
		{
			input.moveout (&output);
			break;
		}

		if (off > 0) 
		{
			output.append (input, off);
			input.skip (off);
		}
		ASSERT(log_, !input.empty());

		/*
		 * Need the following byte at least.
		 */
		if (input.length() == 1)
			break;

		input.extract (&op, sizeof XCODEC_MAGIC);

		switch (op) 
		{
		case XCODEC_OP_ESCAPE:
			output.append (XCODEC_MAGIC);
			input.skip (sizeof XCODEC_MAGIC + sizeof op);
			break;
			
		case XCODEC_OP_EXTRACT:
			if (input.length() < sizeof XCODEC_MAGIC + sizeof op + XCODEC_SEGMENT_LENGTH)
				return (true);
				
			input.skip (sizeof XCODEC_MAGIC + sizeof op);
			input.copyout (data, XCODEC_SEGMENT_LENGTH);
			hash = XCodecHash::hash (data);
			
			if (cache_->lookup (hash, old))
			{
				if (old.equal (data, sizeof data))
				{
					DEBUG(log_) << "Declaring segment already in cache.";
				}
				else
				{
					ERROR(log_) << "Collision in <EXTRACT>.";
					return (false);
				}
				old.clear ();
			} 
			else
				cache_->enter (hash, input, 0);

			output.append (input, XCODEC_SEGMENT_LENGTH);
			input.skip (XCODEC_SEGMENT_LENGTH);
			break;
			
		case XCODEC_OP_REF:
			if (input.length() < sizeof XCODEC_MAGIC + sizeof op + sizeof behash)
				return (true);
				
			input.extract (&behash, sizeof XCODEC_MAGIC + sizeof op);
			hash = BigEndian::decode (behash);

			if (cache_->lookup (hash, output))
			{
				input.skip (sizeof XCODEC_MAGIC + sizeof op + sizeof behash);
			}
			else
			{
				if (unknown_hashes.find (hash) == unknown_hashes.end()) 
				{
					DEBUG(log_) << "Sending <ASK>, waiting for <LEARN>.";
					unknown_hashes.insert (hash);
				} 
				else 
				{
					DEBUG(log_) << "Already sent <ASK>, waiting for <LEARN>.";
				}
				return (true);
			}
			break;
			
		default:
			ERROR(log_) << "Unsupported XCodec opcode " << (unsigned)op << ".";
			return (false);
		}
	}
	
	return (true);
}
