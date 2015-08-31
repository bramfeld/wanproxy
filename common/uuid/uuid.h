/*
 * Copyright (c) 2010-2011 Juli Mallett. All rights reserved.
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

#ifndef	COMMON_UUID_UUID_H
#define	COMMON_UUID_UUID_H

#include <string.h>
#include <ostream>
#include <common/buffer.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           uuid.h                                                     //
// Description:    basic handling of standard UUID objects                    //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#if defined(__FreeBSD__)
#else
#define USE_LIBUUID
#endif

#ifdef USE_LIBUUID
#include <uuid/uuid.h>
#else
#include <uuid.h>
#endif

#define UUID_STRING_SIZE  (sizeof (uuid_t) * 2 + 4)

struct UUID 
{
	uuid_t uuid_;

	bool from_string (const uint8_t* str);
	bool to_string (uint8_t* str) const;
	bool from_file (std::string& path);
	bool to_file (std::string& path) const;
	bool decode (Buffer&);
	bool encode (Buffer& buf) const;
	void generate (void);

	UUID ()
	{
		memset (&uuid_, 0, sizeof uuid_);
	}
	
	bool operator< (const UUID& b) const
	{
		return (memcmp (&uuid_, &b.uuid_, sizeof uuid_) < 0);
	}
	
	bool is_valid () const
	{
		uuid_t u; return (memcmp (&uuid_, &u, sizeof uuid_) != 0);
	}
};

std::ostream& operator<< (std::ostream& os, const UUID& uuid);

#endif /* !COMMON_UUID_UUID_H */
