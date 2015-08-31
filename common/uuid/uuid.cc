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

#include <fstream>
#include <common/uuid/uuid.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           uuid.cc                                                    //
// Description:    basic handling of standard UUID objects                    //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

bool UUID::from_string (const uint8_t* str)
{
#ifdef USE_LIBUUID
	int rv = uuid_parse ((const char*) str, uuid_);
	if (rv == -1)
		return (false);
	ASSERT("/uuid/libuuid", rv == 0);
#else
	uint32_t status;
	uuid_from_string ((const char*) str, &uuid_, &status);
	if (status != uuid_s_ok)
		return (false);
#endif

	return (true);
}

bool UUID::to_string (uint8_t* str) const
{
#ifdef USE_LIBUUID
	uuid_unparse (uuid_, (char*) str);
#else
	char *p;
	uuid_to_string (&uuid_, &p, NULL);
	ASSERT("/uuid/libc", p != NULL);
	strcpy ((char*) str, p);
	free (p);
#endif
	return (true);
}

bool UUID::from_file (std::string& path)
{
	std::fstream file;
	std::string s;
	file.open (path.c_str(), std::ios::in);	
	if (file.good())
	{
		file >> s;
		return from_string ((const uint8_t*) s.c_str());
	}
	return false;
}

bool UUID::to_file (std::string& path) const
{
	std::fstream file;
	file.open (path.c_str(), std::ios::out);	
	if (file.good())
	{
		uint8_t str[UUID_STRING_SIZE + 1];
		to_string (str);
		std::string s ((const char*) str);
		file << s;
		return true;
	}
	return false;
}

bool UUID::decode (Buffer& buf)
{
	if (buf.length() < UUID_STRING_SIZE)
		return (false);

	uint8_t str[UUID_STRING_SIZE + 1];
	buf.moveout (str, UUID_STRING_SIZE);
	str[UUID_STRING_SIZE] = 0;
	return from_string (str);
}

bool UUID::encode (Buffer& buf) const
{
	uint8_t str[UUID_STRING_SIZE + 1];
	to_string (str);
	buf.append (str, UUID_STRING_SIZE);
	return (true);
}

void UUID::generate(void)
{
#ifdef USE_LIBUUID
	uuid_generate (uuid_);
#else
	uuid_create (&uuid_, NULL);
#endif
}

std::ostream& operator<< (std::ostream& os, const UUID& uuid)
{
	uint8_t str[UUID_STRING_SIZE + 1]; 
	uuid.to_string (str);
	return os << str;
}
