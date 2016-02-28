////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           zlib_filter.cc                                             //
// Description:    data filters for zlib inflate/deflate streams              //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "zlib_filter.h"

// Deflate

DeflateFilter::DeflateFilter (int level) : BufferedFilter ("/zlib/deflate")
{
	stream_.zalloc = Z_NULL;
	stream_.zfree = Z_NULL;
	stream_.opaque = Z_NULL;
	stream_.next_in = Z_NULL;
	stream_.avail_in = 0;
	stream_.next_out = outbuf;
	stream_.avail_out = sizeof outbuf;

	if (deflateInit (&stream_, level) != Z_OK)
		CRITICAL(log_) << "Could not initialize deflate stream.";
}

DeflateFilter::~DeflateFilter ()
{
	if (deflateEnd (&stream_) != Z_OK)
		ERROR(log_) << "Deflate stream did not end cleanly.";
}

bool DeflateFilter::consume (Buffer& buf, int flg)
{
	const BufferSegment* seg;
	int cnt = 0, i = 0, rv;
	
	for (Buffer::SegmentIterator it = buf.segments (); ! it.end (); it.next (), ++cnt);
	pending_.clear ();

	for (Buffer::SegmentIterator it = buf.segments (); ! it.end (); it.next (), ++i) 
	{
		seg = *it;
		stream_.next_in = (Bytef*) (uintptr_t) seg->data ();
		stream_.avail_in = seg->length ();

		while (stream_.avail_in > 0) 
		{
			rv = deflate (&stream_, (i < cnt - 1 ? Z_NO_FLUSH : Z_SYNC_FLUSH));
			if (rv == Z_STREAM_ERROR || rv == Z_DATA_ERROR || rv == Z_MEM_ERROR) 
			{
				ERROR(log_) << "deflate(): " << zError(rv);
				return false;
			}

			if (stream_.avail_out < sizeof outbuf)
			{
				pending_.append (outbuf, sizeof outbuf - stream_.avail_out);
				stream_.next_out = outbuf;
				stream_.avail_out = sizeof outbuf;
			}
		}
	}
	
	return produce (pending_, flg);
}

void DeflateFilter::flush (int flg)
{
	pending_.clear ();
	stream_.next_in = Z_NULL;
	stream_.avail_in = 0;
	
	while (deflate (&stream_, Z_FINISH) == Z_OK && stream_.avail_out < sizeof outbuf)
	{	
		pending_.append (outbuf, sizeof outbuf - stream_.avail_out);
		stream_.next_out = outbuf;
		stream_.avail_out = sizeof outbuf;
	}
	
	if (! pending_.empty ())
		produce (pending_);

	Filter::flush (flg);
}

// Inflate

InflateFilter::InflateFilter () : BufferedFilter ("/zlib/inflate")
{
	stream_.zalloc = Z_NULL;
	stream_.zfree = Z_NULL;
	stream_.opaque = Z_NULL;
	stream_.next_in = Z_NULL;
	stream_.avail_in = 0;
	stream_.next_out = outbuf;
	stream_.avail_out = sizeof outbuf;

	if (inflateInit (&stream_) != Z_OK)
		CRITICAL(log_) << "Could not initialize inflate stream.";
}

InflateFilter::~InflateFilter()
{
	if (inflateEnd (&stream_) != Z_OK)
		ERROR(log_) << "Inflate stream did not end cleanly.";
}

bool InflateFilter::consume (Buffer& buf, int flg)
{
	const BufferSegment* seg;
	int cnt = 0, i = 0, rv;
	
	for (Buffer::SegmentIterator it = buf.segments (); ! it.end (); it.next (), ++cnt);
	pending_.clear ();

	for (Buffer::SegmentIterator it = buf.segments (); ! it.end (); it.next (), ++i) 
	{
		seg = *it;
		stream_.next_in = (Bytef*) (uintptr_t) seg->data ();
		stream_.avail_in = seg->length ();

		while (stream_.avail_in > 0) 
		{
			rv = inflate (&stream_, (i < cnt - 1 ? Z_NO_FLUSH : Z_SYNC_FLUSH));
			if (rv == Z_NEED_DICT || rv == Z_DATA_ERROR || rv == Z_MEM_ERROR) 
			{
				ERROR(log_) << "inflate(): " << zError(rv);
				return false;
			}

			if (stream_.avail_out < sizeof outbuf)
			{
				pending_.append (outbuf, sizeof outbuf - stream_.avail_out);
				stream_.next_out = outbuf;
				stream_.avail_out = sizeof outbuf;
			}
		}
	}
	
	return produce (pending_, flg);
}

void InflateFilter::flush (int flg)
{
	pending_.clear ();
	stream_.next_in = Z_NULL;
	stream_.avail_in = 0;
	
	while (inflate (&stream_, Z_FINISH) == Z_OK && stream_.avail_out < sizeof outbuf)
	{	
		pending_.append (outbuf, sizeof outbuf - stream_.avail_out);
		stream_.next_out = outbuf;
		stream_.avail_out = sizeof outbuf;
	}
	
	if (! pending_.empty ())
		produce (pending_);

	Filter::flush (flg);
}
