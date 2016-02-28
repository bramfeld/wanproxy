////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           sink_filter.cc                                             //
// Description:    a filter to write into a target device                     //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/errno.h>
#include "sink_filter.h"

SinkFilter::SinkFilter (const LogHandle& log, Socket* sck, bool cln) : BufferedFilter (log)   
{ 
	sink_ = sck; write_action_ = 0; client_ = cln, down_ = closing_ = false; 
}

SinkFilter::~SinkFilter ()   
{ 
	if (write_action_) write_action_->cancel (); 
}

bool SinkFilter::consume (Buffer& buf, int flg)
{
	if (! sink_ || closing_)
		return false;
		
	if (write_action_)
		pending_.append (buf);
	else
		write_action_ = sink_->write (buf, callback (this, &SinkFilter::write_complete));
	
	return (write_action_ != 0);
}

void SinkFilter::write_complete (Event e)
{
	if (write_action_)
		write_action_->cancel (), write_action_ = 0;
		
	switch (e.type_) 
	{
	case Event::Done:
		if (! pending_.empty ())
		{
			write_action_ = sink_->write (pending_, callback (this, &SinkFilter::write_complete));
			pending_.clear ();
		}
		else if (flushing_)
			flush (0);
		break;
	case Event::Error:
		if (e.error_ == EPIPE && client_)
			DEBUG(log_) << "Client closed connection";
		else
			ERROR(log_) << "Write failed: " << e;
		closing_ = true;
		break;
	}
}

void SinkFilter::flush (int flg)
{
	flushing_ = true;
	flush_flags_ |= flg;
	if (flushing_ && ! write_action_)
	{
		if (! down_)
			down_ = (sink_->shutdown (false, true) == 0);
		Filter::flush (flush_flags_);
	}
}
