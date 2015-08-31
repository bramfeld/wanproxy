////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           filter.h                                                   //
// Description:    base classes for chained data processors                   //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	COMMON_FILTER_H
#define	COMMON_FILTER_H

#include <list>
#include <common/log.h>
#include <common/buffer.h>

class Filter
{
private:
   Filter* recipient_;
   
public:
   Filter ()										{ recipient_ = 0; }
   virtual ~Filter ()							{ }
   
   void chain (Filter* nxt)					{ recipient_ = nxt; }
   virtual bool consume (Buffer& buf)		{ return produce (buf); }
   virtual bool produce (Buffer& buf)		{ return (recipient_ && recipient_->consume (buf)); }
   virtual void flush (int flg)				{ if (recipient_) recipient_->flush (flg); }
};

class CountFilter : public Filter 
{
private:
   intmax_t& counter_;
   
public:
   CountFilter (intmax_t& p) : counter_(p)	   { }
   virtual bool consume (Buffer& buf)		      { counter_ += buf.length (); return produce (buf); }
};

class BufferedFilter : public Filter
{
protected:
   LogHandle log_;
   Buffer pending_;
	bool flushing_;
	int flush_flags_;
	
public:
   BufferedFilter (const LogHandle& log) : log_ (log)   { flushing_ = 0; flush_flags_ = 0; }
};

class LogisticFilter : public BufferedFilter
{
protected:
   Filter* upstream_;
	
public:
   LogisticFilter (const LogHandle& log) : BufferedFilter (log)   { upstream_ = 0; }
   void set_upstream (Filter* f)   { upstream_ = f; }
};

class FilterChain : public Filter
{
private:
	std::list<Filter*> nodes_;
	Filter* holder_;

public:
   FilterChain (Filter* f)			{ holder_ = f; }
   virtual ~FilterChain ()			{ while (! nodes_.empty ()) { delete nodes_.front (); nodes_.pop_front (); }}
  
   void prepend (Filter* f)  		{ Filter* act = (nodes_.empty () ? holder_ : nodes_.front ()); 
											  if (f && act) nodes_.push_front (f), chain (f), f->chain (act); }
   void append (Filter* f)  		{ Filter* act = (nodes_.empty () ? this : nodes_.front ()); 
											  if (f && act) nodes_.push_front (f), act->chain (f), f->chain (holder_); }
   virtual void flush (int flg)	{ if (nodes_.empty ()) chain (holder_); Filter::flush (flg); }
};

#endif /* !COMMON_FILTER_H */
