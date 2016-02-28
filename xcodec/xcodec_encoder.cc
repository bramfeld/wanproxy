/*
 * Copyright (c) 2009-2011 Juli Mallett. All rights reserved.
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
#include <xcodec/xcodec_encoder.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_encoder.cc                                          //
// Description:    encoding routines for the xcodex protocol                  //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

XCodecEncoder::XCodecEncoder(XCodecCache *cache)
: log_("/xcodec/encoder"),
  cache_(cache)
{
	  candidate_start_ = -1;
	  candidate_symbol_ = 0;
}

XCodecEncoder::~XCodecEncoder()
{ }

/*
 * This takes a view of a data stream and turns it into a series of references
 * to other data, declarations of data to be referenced, and data that needs
 * escaped.
 */

void XCodecEncoder::encode (Buffer& output, Buffer& input)
{
	int off = source_.length ();
	Buffer old;
	
	source_.append (input);

	for (Buffer::SegmentIterator it = input.segments (); ! it.end (); it.next ()) 
	{
		const BufferSegment* seg = *it;
		const uint8_t *p, *q = seg->end ();
		
		for (p = seg->data (); p < q; ++p) 
		{
			/*
			 * Add bytes to the hash until we have a complete hash.
			 */
			if (++off < XCODEC_SEGMENT_LENGTH) 
				xcodec_hash_.add (*p);
			else
			{
				if (off == XCODEC_SEGMENT_LENGTH)
					xcodec_hash_.add (*p);
				else
					xcodec_hash_.roll (*p);
				
				/*
				 * And then mix the hash's internal state into a
				 * uint64_t that we can use to refer to that data
				 * and to look up possible past occurances of that
				 * data in the XCodecCache.
				 */
				uint64_t hash = xcodec_hash_.mix ();

				/*
				 * If there is a pending candidate hash that wouldn't
				 * overlap with the data that the rolling hash presently
				 * covers, declare it now.
				 */
				if (candidate_start_ >= 0 && candidate_start_ + (XCODEC_SEGMENT_LENGTH * 2) <= off) 
				{
					encode_declaration (output, source_, candidate_start_, candidate_symbol_);
					off -= (candidate_start_ + XCODEC_SEGMENT_LENGTH);
					candidate_start_ = -1;
				}

				/*
				 * Now attempt to encode this hash as a reference if it
				 * has been defined before.
				 */
				
				if (cache_->lookup (hash, old))
				{
					/*
					 * This segment already exists.  If it's
					 * identical to this chunk of data, then that's
					 * positively fantastic.
					 */
					if (encode_reference (output, source_, off - XCODEC_SEGMENT_LENGTH, hash, old)) 
					{
						/*
						 * We have output any data before this hash
						 * in escaped form, so any candidate hash
						 * before it is invalid now.
						 */
						off = 0;
						xcodec_hash_.reset();
						candidate_start_ = -1;
					}
					else
					{
						/*
						 * This hash isn't usable because it collides
						 * with another, so keep looking for something
						 * viable.
						 */
						DEBUG(log_) << "Collision in first pass.";
					}
					
					old.clear ();
				}
				else
				{
					/*
					 * Not defined before, it's a candidate for declaration
					 * if we don't already have one.
					 */
					if (candidate_start_ >= 0) 
					{
						/*
						 * We already have a hash that occurs earlier,
						 * isn't a collision and includes data that's
						 * covered by this hash, so don't remember it
						 * and keep going.
						 */
						ASSERT(log_, candidate_start_ + (XCODEC_SEGMENT_LENGTH * 2) > off);
					}
					else
					{
						/*
						 * The hash at this offset doesn't collide with any
						 * other and is the first viable hash we've seen so far
						 * in the stream, so remember it so that if we don't
						 * find something to reference we can declare this one
						 * for future use.
						 */
						candidate_start_ = off - XCODEC_SEGMENT_LENGTH;
						candidate_symbol_ = hash;
					}
				}
			}
		}
	}
}

bool XCodecEncoder::flush (Buffer& output)
{
	bool vld = false;
	
	/*
	 * There's a hash we can declare, do it.
	 */
	if (candidate_start_ >= 0) 
	{
		encode_declaration (output, source_, candidate_start_, candidate_symbol_);
		candidate_start_ = -1;
		vld = true;
	}

	/*
	 * There's data after that hash or no candidate hash, so just escape it.
	 */
	if (source_.length () > 0)
	{
		encode_escape (output, source_, source_.length ());
		vld = true;
	}
	
	xcodec_hash_.reset();
	
	return vld;
}

void XCodecEncoder::encode_declaration (Buffer& output, Buffer& input, unsigned start, uint64_t hash)
{
	if (start > 0)
		encode_escape (output, input, start);
		
	cache_->enter (hash, input, 0);
	
	output.append (XCODEC_MAGIC);
	output.append (XCODEC_OP_EXTRACT);
	output.append (input, XCODEC_SEGMENT_LENGTH);
	
	input.skip (XCODEC_SEGMENT_LENGTH);
}

void XCodecEncoder::encode_escape (Buffer& output, Buffer& input, unsigned length)
{
	unsigned pos;

	while (length > 0)
	{
		if (input.find (XCODEC_MAGIC, 0, length, &pos))
		{
			if (pos > 0) 
				output.append (input, 0, pos);
			output.append (XCODEC_MAGIC);
			output.append (XCODEC_OP_ESCAPE);
			input.skip (pos + 1);
			length -= pos + 1;
		}
		else
		{
			output.append (input, length);
			input.skip (length);
			break;
		}
	}
}

bool XCodecEncoder::encode_reference (Buffer& output, Buffer& input, unsigned start, uint64_t hash, Buffer& old)
{
	uint8_t data[XCODEC_SEGMENT_LENGTH];
	input.copyout (data, start, XCODEC_SEGMENT_LENGTH);

	if (old.equal (data, sizeof data))
	{
		if (start > 0)
			encode_escape (output, input, start);

		output.append (XCODEC_MAGIC);
		output.append (XCODEC_OP_REF);
		uint64_t behash = BigEndian::encode (hash);
		output.append (&behash);
		input.skip (XCODEC_SEGMENT_LENGTH);
		return true;
	}
	
	return false;
}
