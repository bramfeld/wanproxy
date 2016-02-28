////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           zlib_filter.h                                              //
// Description:    data filters for zlib inflate/deflate streams              //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	ZLIB_DEFLATE_FILTER_H
#define	ZLIB_DEFLATE_FILTER_H

#include <common/filter.h>
#include <zlib.h>

#define	DEFLATE_CHUNK_SIZE	0x10000
#define	INFLATE_CHUNK_SIZE	0x10000

class DeflateFilter : public BufferedFilter
{
private:
	z_stream stream_;
	uint8_t outbuf[DEFLATE_CHUNK_SIZE];
	
public:
   DeflateFilter (int level = 0);
   virtual ~DeflateFilter ();

   virtual bool consume (Buffer& buf, int flg = 0);
   virtual void flush (int flg);
};

class InflateFilter : public BufferedFilter 
{
private:
	z_stream stream_;
	uint8_t outbuf[INFLATE_CHUNK_SIZE];
	
public:
   InflateFilter ();
  ~InflateFilter ();

   virtual bool consume (Buffer& buf, int flg = 0);
   virtual void flush (int flg);
};

#endif /* !ZLIB_DEFLATE_FILTER_H */
