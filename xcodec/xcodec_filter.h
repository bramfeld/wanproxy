////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_filter.h                                            //
// Description:    instantiation of encoder/decoder in a data filter pair     //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	XCODEC_FILTER_H
#define	XCODEC_FILTER_H

#include <set>
#include <common/filter.h>
#include <event/event.h>
#include <event/action.h>
#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/xcodec_hash.h>
#include <xcodec/xcodec_encoder.h>
#include <xcodec/xcodec_decoder.h>

class EncodeFilter : public BufferedFilter
{
private:
   XCodecCache* cache_;
	XCodecEncoder* encoder_;
	Action* wait_action_;
	bool waiting_;
	bool sent_eos_;
	bool eos_ack_;
   
public:
	EncodeFilter (const LogHandle& log, XCodecCache* cc, int flg = 0) : BufferedFilter (log) 
	{ 
		cache_ = cc; encoder_ = 0; wait_action_ = 0; waiting_ = (flg & 1); sent_eos_ = eos_ack_ = false;
	}
	
	virtual ~EncodeFilter ()  
	{ 
		if (wait_action_)
			wait_action_->cancel ();
		delete encoder_; 
	}
  
   virtual bool consume (Buffer& buf, int flg = 0);
   virtual void flush (int flg);
	
private:
	void encode_frame (Buffer& src, Buffer& trg);
	void on_read_timeout (Event e);
};

class DecodeFilter : public LogisticFilter
{
private:
   XCodecCache* encoder_cache_;
	XCodecDecoder* decoder_;
	XCodecCache* decoder_cache_;
	std::set<uint64_t> unknown_hashes_;
	Buffer frame_buffer_;
	bool received_eos_;
	bool sent_eos_ack_;
	bool received_eos_ack_;
	bool upflushed_;
   
public:
	DecodeFilter (const LogHandle& log, XCodecCache* cc) : LogisticFilter (log) 
   { 
      encoder_cache_ = cc; decoder_ = 0; decoder_cache_ = 0;   
      received_eos_ = sent_eos_ack_ = received_eos_ack_ = upflushed_ = false; 
   }
	
	~DecodeFilter ()  
	{ 
		delete decoder_; 
	}
  
   virtual bool consume (Buffer& buf, int flg = 0);
   virtual void flush (int flg);
};

#endif /* !XCODEC_FILTER_H */
