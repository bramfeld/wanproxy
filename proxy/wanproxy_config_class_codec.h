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

#ifndef	PROGRAMS_WANPROXY_WANPROXY_CONFIG_CLASS_CODEC_H
#define	PROGRAMS_WANPROXY_WANPROXY_CONFIG_CLASS_CODEC_H

#include <config/config_type_int.h>
#include <config/config_type_string.h>
#include "wanproxy_codec.h"
#include "wanproxy_config_type_codec.h"
#include "wanproxy_config_type_compressor.h"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           wanproxy_config_class_codec.h                              //
// Description:    high-level parser for xcodec-related options               //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class WANProxyConfigClassCodec : public ConfigClass {

public:
	struct Instance : public ConfigClassInstance {

		WANProxyCodec codec_;
		WANProxyConfigCodec codec_type_;
		WANProxyConfigCompressor compressor_;
		intmax_t compressor_level_;
		intmax_t byte_counts_;
		WANProxyConfigCache cache_type_;
		std::string cache_path_;
		intmax_t local_size_;
		intmax_t remote_size_;

		Instance(void)
		: codec_type_(WANProxyConfigCodecNone),
		  compressor_(WANProxyConfigCompressorNone),
		  compressor_level_(0),
		  byte_counts_(0),
		  cache_type_(WANProxyConfigCacheMemory),
		  local_size_(0),
		  remote_size_(0)
		{
		}

		bool activate(const ConfigObject *);
	};

	WANProxyConfigClassCodec(void)
	: ConfigClass("codec", new ConstructorFactory<ConfigClassInstance, Instance>)
	{
		add_member("codec", &wanproxy_config_type_codec, &Instance::codec_type_);
		add_member("compressor", &wanproxy_config_type_compressor, &Instance::compressor_);
		add_member("compressor_level", &config_type_int, &Instance::compressor_level_);
		add_member("byte_counts", &config_type_int, &Instance::byte_counts_);

		add_member("cache", &wanproxy_config_type_cache, &Instance::cache_type_);
		add_member("cache_path", &config_type_string, &Instance::cache_path_);
		add_member("local_size", &config_type_int, &Instance::local_size_);
		add_member("remote_size", &config_type_int, &Instance::remote_size_);
	}

	~WANProxyConfigClassCodec()
	{ }
};

extern WANProxyConfigClassCodec wanproxy_config_class_codec;

#endif /* !PROGRAMS_WANPROXY_WANPROXY_CONFIG_CLASS_CODEC_H */
