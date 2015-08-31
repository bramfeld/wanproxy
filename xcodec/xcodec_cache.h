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

#ifndef	XCODEC_XCODEC_CACHE_H
#define	XCODEC_XCODEC_CACHE_H

#include <ext/hash_map>
#include <map>

#include <common/buffer.h>
#include <common/uuid/uuid.h>
#include <xcodec/xcodec.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_cache.h                                             //
// Description:    base cache class and in-memory cache implementation        //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#define XCODEC_WINDOW_COUNT  64  // must be binary

/*
 * XXX
 * GCC supports hash<unsigned long> but not hash<unsigned long long>.  On some
 * of our platforms, the former is 64-bits, on some the latter.  As a result,
 * we need this wrapper structure to throw our hashes into so that GCC's hash
 * function can be reliably defined to use them.
 */
struct Hash64 
{
	uint64_t hash_;

	Hash64(const uint64_t& hash)
	: hash_(hash)
	{ }

	bool operator== (const Hash64& hash) const
	{
		return (hash_ == hash.hash_);
	}

	bool operator< (const Hash64& hash) const
	{
		return (hash_ < hash.hash_);
	}
};

namespace __gnu_cxx 
{
	template<>
	struct hash<Hash64> 
	{
		size_t operator() (const Hash64& x) const
		{
			return (x.hash_);
		}
	};
}


class XCodecCache 
{
private:
	UUID uuid_;
	size_t size_;
	struct WindowItem {uint64_t hash; const uint8_t* data;};
	WindowItem window_[XCODEC_WINDOW_COUNT];
	unsigned cursor_;

protected:
	XCodecCache (const UUID& uuid, size_t size)
	: uuid_(uuid),
	  size_(size)
	{
		memset (window_, 0, sizeof window_);
		cursor_ = 0;
	}

public:
	virtual ~XCodecCache()
	{ }
	
	const UUID& identifier ()
	{
		return uuid_;
	}

	size_t nominal_size ()
	{
		return size_;
	}

	virtual void enter (const uint64_t& hash, const Buffer& buf, unsigned off) = 0;
	virtual bool lookup (const uint64_t& hash, Buffer& buf) = 0;

protected:	
	void remember (const uint64_t& hash, const uint8_t* data)
	{
		window_[cursor_].hash = hash;
		window_[cursor_].data = data;
		cursor_ = (cursor_ + 1) & (XCODEC_WINDOW_COUNT - 1);
	}
	
	const uint8_t* find_recent (const uint64_t& hash)
	{
		WindowItem* w;
		int n;
		
		for (w = window_, n = XCODEC_WINDOW_COUNT; n > 0; --n, ++w)
			if (w->hash == hash)
				return w->data;
				
		return 0;
	}
	
	void forget (const uint64_t& hash)
	{
		WindowItem* w;
		int n;
		
		for (w = window_, n = XCODEC_WINDOW_COUNT; n > 0; --n, ++w)
			if (w->hash == hash)
				w->hash = 0;
	}
};


class XCodecMemoryCache : public XCodecCache 
{
	typedef __gnu_cxx::hash_map<Hash64, const uint8_t*> segment_hash_map_t;
	segment_hash_map_t segment_hash_map_;
	LogHandle log_;
	
public:
	XCodecMemoryCache (const UUID& uuid, size_t size)
	: XCodecCache(uuid, size),
	  log_("/xcodec/cache/memory")
	{ }

	~XCodecMemoryCache()
	{
		segment_hash_map_t::const_iterator it;
		for (it = segment_hash_map_.begin(); it != segment_hash_map_.end(); ++it)
			delete[] it->second;
		segment_hash_map_.clear();
	}

	void enter (const uint64_t& hash, const Buffer& buf, unsigned off)
	{
		ASSERT(log_, segment_hash_map_.find(hash) == segment_hash_map_.end());
		uint8_t* data = new uint8_t[XCODEC_SEGMENT_LENGTH];
		buf.copyout (data, off, XCODEC_SEGMENT_LENGTH);
		segment_hash_map_[hash] = data;
	}

	bool lookup (const uint64_t& hash, Buffer& buf)
	{
		const uint8_t* data;
		if ((data = find_recent (hash)))
		{
			buf.append (data, XCODEC_SEGMENT_LENGTH);
			return true;
		}
		segment_hash_map_t::const_iterator it = segment_hash_map_.find (hash);
		if (it != segment_hash_map_.end ())
		{
			buf.append (it->second, XCODEC_SEGMENT_LENGTH);
			remember (hash, it->second);
			return true;
		}
		return false;
	}
};

#endif /* !XCODEC_XCODEC_CACHE_H */
