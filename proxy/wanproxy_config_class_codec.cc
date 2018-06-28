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
#include <common/uuid/uuid.h>
#include <config/config_class.h>
#include <config/config_object.h>
#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/cache/coss/xcodec_cache_coss.h>
#include "wanproxy_config_class_codec.h"
#include "wanproxy.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           wanproxy_config_class_codec.cc                             //
// Description:    high-level parser for xcodec-related options               //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

WANProxyConfigClassCodec wanproxy_config_class_codec;

bool
WANProxyConfigClassCodec::Instance::activate(const ConfigObject *co)
{
	UUID uuid;
	XCodecCache* cache;
	
	codec_.name_ = co->name_;

	switch (codec_type_) 
	{
	case WANProxyConfigCodecXCodec:
		/*
		 * Fetch UUID from permanent storage if there is any.
		 */
		if (cache_path_.empty())
		{
			uuid.generate();
		}
		else
		{   
			std::string uuid_path = cache_path_ + "/UUID";
			if (! uuid.from_file (uuid_path))
			{
				uuid.generate();
				uuid.to_file (uuid_path);
			}
		}

		codec_.cache_type_ = cache_type_;
		codec_.cache_path_ = cache_path_;
		codec_.cache_size_ = local_size_;
		codec_.cache_uuid_ = uuid;

		if (! (cache = wanproxy.find_cache (uuid)))
			cache = wanproxy.add_cache (cache_type_, cache_path_, local_size_, uuid);
		codec_.xcache_ = cache;
		break;
	case WANProxyConfigCodecNone:
		codec_.xcache_ = 0;
		break;
	default:
		ERROR("/wanproxy/config/codec") << "Invalid codec type.";
		return (false);
	}

	INFO("/wanproxy/config/cache/type") << cache_type_;
	INFO("/wanproxy/config/cache/path") << cache_path_;
	INFO("/wanproxy/config/cache/size") << local_size_;
	INFO("/wanproxy/config/cache/uuid") << uuid;
		
	switch (compressor_) {
	case WANProxyConfigCompressorZlib:
		if (compressor_level_ < 0 || compressor_level_ > 9) {
			ERROR("/wanproxy/config/codec") << "Compressor level must be in range 0..9 (inclusive.)";
			return (false);
		}

		codec_.compressor_ = true;
		codec_.compressor_level_ = (char) compressor_level_;
		break;
	case WANProxyConfigCompressorNone:
		if (compressor_level_ > 0) {
			ERROR("/wanproxy/config/codec") << "Compressor level set but no compressor.";
			return (false);
		}

		codec_.compressor_ = false;
		codec_.compressor_level_ = 0;
		break;
	default:
		ERROR("/wanproxy/config/codec") << "Invalid compressor type.";
		return (false);
	}

   codec_.counting_ = (byte_counts_ != 0);

	return (true);
}
