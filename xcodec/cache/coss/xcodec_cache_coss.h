/*
 *
 * XCodec COSS Cache
 * 
 * COSS = Cyclic Object Storage System
 *
 * Idea taken from Squid COSS.
 * 
 * Diego Woitasen <diegows@xtech.com.ar>
 * XTECH
 *
 */

#ifndef	XCODEC_XCODEC_CACHE_COSS_H
#define	XCODEC_XCODEC_CACHE_COSS_H

#include <string>
#include <map>
#include <fstream>

#include <common/buffer.h>
#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_cache_coss.h                                        //
// Description:    persistent cache on disk for xcodec protocol streams       //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

using namespace std;

/*
 * - In COSS, we have one file per cache (UUID). The file is divided in 
 * stripes.
 *
 * - Each stripe is composed by:
 * metadata + hash array + segment size array + segment array.
 *
 * - The arrays elements are the same order:
 * hash1, hash2, ..., hashN - size1, size2, ..., sizeN - seg1, seg2, ..., segN.
 *
 * - The segments are indexed in memory. This index is loaded when the cache is
 * openned, reading the hash array of each stripe. Takes a few millisecons in 
 * a 10 GB cache.
 *
 * - We have one active stripe in memory at a time. New segments are written
 * in the current stripe in order of appearance.
 *
 * - When a cached segment is requested and it's out of the active stripe, 
 * is copied to it.
 *
 * - When the current stripe is full, we move to the next one.
 *
 * - When we reach the EOF, first stripe is zeroed and becomes active.
 *
 */

// Changes introduced in version 2:
//
// - segment size is made independent of BUFFER_SEGMENT_SIZE and not stored explicitly  
//   since it is always XCODEC_SEGMENT_LENGTH
// - several stripes can be help simultaneously in memory, and when a segment is requested
//   which lies outside the active stripe it is read into an alternate slot together
//   with the whole stripe to be ready to satisfy requests for neighbour segments
// - an array of bits keeps track of the state of each segment within a stripe signaling
//   if it has been recently used
// - when no more place is available, the LRU stripe is purged and any segments 
//   no used during the last period are erased
 
/*
 * This values should be page aligned.
 */
 
#define CACHE_SIGNATURE				0xF150E964
#define CACHE_VERSION				2
#define STRIPE_SEGMENT_COUNT		512		// segments of XCODEC_SEGMENT_LENGTH per stripe (must fit into 16 bits)
#define LOADED_STRIPE_COUNT		16			// number of stripes held in memory (must be greater than 1)
#define CACHE_BASIC_SIZE			1024		// MB

#define CACHE_ALIGNEMENT			4096
#define HEADER_ARRAY_SIZE			(STRIPE_SEGMENT_COUNT * (sizeof (uint64_t) + sizeof (uint32_t)))
#define METADATA_SIZE				(sizeof (COSSMetadata))
#define ROUND_UP(N, S) 				((((N) + (S) - 1) / (S)) * (S))
#define HEADER_ALIGNED_SIZE		ROUND_UP(HEADER_ARRAY_SIZE + METADATA_SIZE, CACHE_ALIGNEMENT)
#define METADATA_PADDING			(HEADER_ALIGNED_SIZE - HEADER_ARRAY_SIZE - METADATA_SIZE)

struct COSSIndexEntry 
{
	uint64_t stripe_range : 48;
	uint64_t position : 16;
};

class COSSIndex 
{
	typedef __gnu_cxx::hash_map<Hash64, COSSIndexEntry> index_t;
	index_t index;

public:
	void insert (const uint64_t& hash, const COSSIndexEntry& entry)
	{
		index[hash] = entry;
	}

	const COSSIndexEntry* lookup (const uint64_t& hash)
	{
		index_t::iterator it = index.find (hash);
		return (it != index.end () ? &it->second : 0);
	}
	
	void erase (const uint64_t& hash)
	{
		index.erase (hash);
	}

	size_t size()
	{
		return index.size();
	}
};

struct COSSOnDiskSegment 
{
	uint8_t bytes[XCODEC_SEGMENT_LENGTH];
	string hexdump() {
		string dump;
		char buf[8];
		int i;
		for (i = 0; i < 70; i++) {
			snprintf(buf, 8, "%02x", bytes[i]);
			dump += buf;
		}
		return dump;
	}
};

struct COSSMetadata 
{
	uint32_t signature; 
	uint32_t version; 
	uint64_t serial_number; 
	uint64_t stripe_range; 
	uint32_t segment_index; 
	uint32_t segment_count; 
	uint64_t freshness; 
	uint64_t uses; 
	uint64_t credits; 
	uint32_t load_uses; 
	uint32_t state; 
};

struct COSSStripeHeader 
{
	COSSMetadata metadata;
	char padding[METADATA_PADDING];
	uint32_t flags[STRIPE_SEGMENT_COUNT];
	uint64_t hash_array[STRIPE_SEGMENT_COUNT];
};

struct COSSStripe 
{
	COSSStripeHeader header;
	COSSOnDiskSegment segment_array[STRIPE_SEGMENT_COUNT];

public:
	COSSStripe()  { memset (&header, 0, sizeof header); }
};

struct COSSStats 
{
	uint64_t lookups;
	uint64_t found_1;
	uint64_t found_2;
	
public:
	COSSStats()  { lookups = found_1 = found_2 = 0; }
};


class XCodecCacheCOSS : public XCodecCache 
{
	std::string file_path_;
	uint64_t file_size_; 
	fstream stream_;
	
	uint64_t serial_number_; 
	uint64_t stripe_range_;
	uint64_t stripe_limit_;
	uint64_t freshness_level_;

	COSSStripe stripe_[LOADED_STRIPE_COUNT];
	int active_;
	
	COSSMetadata* directory_;
	COSSIndex cache_index_;
	COSSStats stats_;
	LogHandle log_;

public:
	XCodecCacheCOSS (const UUID& uuid, const std::string& cache_dir, size_t cache_size);
	~XCodecCacheCOSS();

	virtual void enter (const uint64_t& hash, const Buffer& buf, unsigned off);
	virtual bool lookup (const uint64_t& hash, Buffer& buf);

private:	
	bool read_file ();
	void initialize_stripe (uint64_t range, int slot);
	bool load_stripe (uint64_t range, int slot);
	void store_stripe (int slot, size_t size);
	void new_active ();
	int best_unloadable_slot ();
	uint64_t best_erasable_stripe ();
	void detach_stripe (int slot);
	void purge_stripe (int slot);
};

#endif /* !XCODEC_XCODEC_CACHE_COSS_H */

