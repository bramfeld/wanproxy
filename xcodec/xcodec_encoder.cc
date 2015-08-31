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
#include <xcodec/xcodec_hash.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_encoder.cc                                          //
// Description:    encoding routines for the xcodex protocol                  //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

struct candidate_symbol 
{
	bool set_;
	unsigned offset_;
	uint64_t symbol_;
};

XCodecEncoder::XCodecEncoder(XCodecCache *cache)
: log_("/xcodec/encoder"),
  cache_(cache)
{ }

XCodecEncoder::~XCodecEncoder()
{ }

/*
 * This takes a view of a data stream and turns it into a series of references
 * to other data, declarations of data to be referenced, and data that needs
 * escaped.
 */
void
XCodecEncoder::encode (Buffer& output, Buffer& input)
{
	XCodecHash xcodec_hash;
	candidate_symbol candidate = {0, 0, 0};
	unsigned offset = 0;
	unsigned o = 0;
	Buffer old;

	for (Buffer::SegmentIterator it = input.segments (); ! it.end (); it.next ()) 
	{
		const BufferSegment* seg = *it;
		const uint8_t *p, *q = seg->end ();
		
		for (p = seg->data (); p < q; ++p) 
		{
			/*
			 * Add bytes to the hash until we have a complete hash.
			 */
			if (++o < XCODEC_SEGMENT_LENGTH) 
				xcodec_hash.add (*p);
			else
			{
				if (o == XCODEC_SEGMENT_LENGTH)
					xcodec_hash.add (*p);
				else
					xcodec_hash.roll (*p);
				
				/*
				 * And then mix the hash's internal state into a
				 * uint64_t that we can use to refer to that data
				 * and to look up possible past occurances of that
				 * data in the XCodecCache.
				 */
				uint64_t hash = xcodec_hash.mix ();

				/*
				 * If there is a pending candidate hash that wouldn't
				 * overlap with the data that the rolling hash presently
				 * covers, declare it now.
				 */
				if (candidate.set_ && candidate.offset_ + (XCODEC_SEGMENT_LENGTH * 2) <= offset + o) 
				{
					encode_declaration (output, input, offset, candidate.offset_, candidate.symbol_);
					o -= (candidate.offset_ + XCODEC_SEGMENT_LENGTH - offset);
					offset = (candidate.offset_ + XCODEC_SEGMENT_LENGTH);
					candidate.set_ = false;
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
					if (encode_reference (output, input, offset, offset + o - XCODEC_SEGMENT_LENGTH, hash, old)) 
					{
						/*
						 * We have output any data before this hash
						 * in escaped form, so any candidate hash
						 * before it is invalid now.
						 */
						offset += o;
						o = 0;
						xcodec_hash.reset();
						candidate.set_ = false;
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
					if (candidate.set_) 
					{
						/*
						 * We already have a hash that occurs earlier,
						 * isn't a collision and includes data that's
						 * covered by this hash, so don't remember it
						 * and keep going.
						 */
						ASSERT(log_, candidate.offset_ + (XCODEC_SEGMENT_LENGTH * 2) > offset + o);
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
						candidate.offset_ = offset + o - XCODEC_SEGMENT_LENGTH;
						candidate.symbol_ = hash;
						candidate.set_ = true;
					}
				}
			}
		}
	}

	/*
	 * There's a hash we can declare, do it.
	 */
	if (candidate.set_) 
	{
		encode_declaration (output, input, offset, candidate.offset_, candidate.symbol_);
		o -= (candidate.offset_ + XCODEC_SEGMENT_LENGTH - offset);
		offset = (candidate.offset_ + XCODEC_SEGMENT_LENGTH);
		candidate.set_ = false;
	}

	/*
	 * There's data after that hash or no candidate hash, so
	 * just escape it.
	 */
	if (offset < input.length ())
		encode_escape (output, input, offset, input.length ());
}

void
XCodecEncoder::encode_declaration (Buffer& output, Buffer& input, unsigned offset, unsigned start, uint64_t hash)
{
	if (offset < start)
		encode_escape (output, input, offset, start);
		
	cache_->enter (hash, input, start);
	
	output.append (XCODEC_MAGIC);
	output.append (XCODEC_OP_EXTRACT);
	output.append (input, start, XCODEC_SEGMENT_LENGTH);
}

void
XCodecEncoder::encode_escape (Buffer& output, Buffer& input, unsigned offset, unsigned limit)
{
	unsigned pos;

	while (offset < limit && input.find (XCODEC_MAGIC, offset, limit - offset, &pos)) 
	{
		if (offset < pos) 
			output.append (input, offset, pos - offset);
		output.append (XCODEC_MAGIC);
		output.append (XCODEC_OP_ESCAPE);
		offset = pos + 1;
	}
	
	if (offset < limit)
		output.append (input, offset, limit - offset);
}

bool
XCodecEncoder::encode_reference (Buffer& output, Buffer& input, unsigned offset, unsigned start, uint64_t hash, Buffer& old)
{
	uint8_t data[XCODEC_SEGMENT_LENGTH];
	input.copyout (data, start, XCODEC_SEGMENT_LENGTH);

	if (old.equal (data, sizeof data))
	{
		if (offset < start)
			encode_escape (output, input, offset, start);

		output.append (XCODEC_MAGIC);
		output.append (XCODEC_OP_REF);
		uint64_t behash = BigEndian::encode (hash);
		output.append (&behash);
		return true;
	}
	
	return false;
}
