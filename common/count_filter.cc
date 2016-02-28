////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           count_filter.cc                                            //
// Description:    byte counting filter for wanproxy streams                  //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <map>
#include <vector>
#include <common/buffer.h>
#include <http/http_protocol.h>
#include "count_filter.h"

CountFilter::CountFilter (intmax_t& p, int flg) : total_count_ (p)
{ 
	expected_ = count_ = 0; state_ = (flg & 1); 
}

bool CountFilter::consume (Buffer& buf, int flg)
{
	long n = buf.length ();
	total_count_ += n;
	
	if (state_ == 1 || state_ == 2)
	{
		header_.append (buf);
		while (!	explore_stream (header_)) continue;
	}
	else if (state_ == 3 || state_ == 4)
	{
		count_ += n;
		if (count_ >= expected_)
		{
			state_ = 1, header_.clear ();
			if (count_ > expected_)
			{
				header_ = buf, header_.skip (n - (count_ - expected_));
				while (!	explore_stream (header_)) continue;
			}
		}
	}
	
	return produce (buf, flg | (state_ == 4 ? TO_BE_CONTINUED : 0)); 
}

void CountFilter::flush (int flg)
{
	state_ = 0;
	Filter::flush (flg);
}

bool CountFilter::explore_stream (Buffer& buf)
{
	if (state_ == 1 && buf.length () >= 5)
		state_ = (buf.prefix ((const uint8_t*) "HTTP/", 5) ? 2 : 0);
	
	if (state_ == 2)
	{
		unsigned pos, ext = buf.length ();
		for (pos = 0; pos < ext - 1 && buf.find ('\n', pos, ext - pos - 1, &pos); ++pos)
		{
			uint8_t sfx[4] = {0, 0, 0, 0};
			buf.copyout (sfx, pos + 1, (ext > pos + 1 ? 2 : 1));
			if (sfx[0] == '\n' || (sfx[0] == '\r' && sfx[1] == '\n'))
			{
				HTTPProtocol::Message msg (HTTPProtocol::Message::Response);
				std::map<std::string, std::vector<Buffer> >::iterator it;
				unsigned lng = 0;
				if (msg.decode (&buf) && msg.headers_.find ("Transfer-Encoding") == msg.headers_.end () &&
					 (it = msg.headers_.find ("Content-Length")) != msg.headers_.end () && it->second.size () > 0)
				{
					Buffer val = it->second[0];
					while (val.length () > 0)
					{
						uint8_t c = val.peek ();
						val.skip (1);
						if (c >= '0' && c <= '9')
							lng = (lng * 10) + (c - '0');
						else
						{
							lng = 0;
							break;
						}
					}
				}
				if (lng > 0)
				{
					if (lng > buf.length ())
					{
						expected_ = lng;
						count_ = buf.length ();
						state_ = (lng < 1000 ? 3 : 4);  // don't wait for cookie resources
					}
					else
					{
						buf.skip (lng);
						state_ = 1;
						return false;
					}
				}
				else
				{
					state_ = 0;
				}
				break;
			}
		}
	}
	
	return true;
}
