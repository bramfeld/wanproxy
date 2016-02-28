////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           count_filter.h                                             //
// Description:    byte counting filter for wanproxy streams                  //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	COUNT_FILTER_H
#define	COUNT_FILTER_H

#include <common/types.h>
#include <common/filter.h>

#define TO_BE_CONTINUED  1

class CountFilter : public Filter
{
private:
	Buffer header_;
   intmax_t& total_count_;
	intmax_t expected_;
	intmax_t count_;
	int state_;
   
public:
   CountFilter (intmax_t& p, int flg = 0);
	
   virtual bool consume (Buffer& buf, int flg = 0);
   virtual void flush (int flg);
	
private:
	bool explore_stream (Buffer& buf);
};

#endif  //	COUNT_FILTER_H
