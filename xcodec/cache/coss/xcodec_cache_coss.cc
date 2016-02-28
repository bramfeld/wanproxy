/*
 *
 * XCodec COSS Cache
 *
 * COSS = Cyclic Object storage system
 *
 * Idea taken from Squid COSS cache.
 * 
 * Diego Woitasen <diegows@xtech.com.ar>
 * XTECH
 *
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <xcodec/cache/coss/xcodec_cache_coss.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           xcodec_cache_coss.cc                                       //
// Description:    persistent cache on disk for xcodec protocol streams       //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


XCodecCacheCOSS::XCodecCacheCOSS (const UUID& uuid, const std::string& cache_dir, size_t cache_size)
	: XCodecCache(uuid, cache_size), 
     log_("xcodec/cache/coss")
{
	uint8_t str[UUID_STRING_SIZE + 1];
	uuid.to_string (str);
	file_path_ = cache_dir;
	if (file_path_.size() > 0 && file_path_[file_path_.size() - 1] != '/')
		file_path_.append ("/");
	file_path_.append ((const char*) str, UUID_STRING_SIZE);
	file_path_.append (".wpc");

	struct stat st;
	if (::stat (file_path_.c_str(), &st) == 0 && (st.st_mode & S_IFREG))
		file_size_ = st.st_size;
	else
	{
		ofstream tmp (file_path_.c_str());
		file_size_ = 0;
	}
	
	serial_number_ = 0;
	stripe_range_ = 0;
	if (! cache_size)
		cache_size = CACHE_BASIC_SIZE;
	uint64_t size = ROUND_UP((uint64_t) cache_size * 1048576, sizeof (COSSStripe));
	stripe_limit_ = size / sizeof (COSSStripe);
	freshness_level_ = 0;
	active_ = 0;
	
	directory_ = new COSSMetadata[stripe_limit_];
	memset (directory_, 0, sizeof (COSSMetadata) * stripe_limit_);
	
	if (stream_.rdbuf())
		stream_.rdbuf()->pubsetbuf (0, 0);
		  
	stream_.open (file_path_.c_str(), fstream::in | fstream::out | fstream::binary);
	if (! read_file ())
	{
		stream_.close ();
		stream_.open (file_path_.c_str(), fstream::in | fstream::out | fstream::trunc | fstream::binary);
		file_size_ = 0;
		initialize_stripe (stripe_range_, active_);
	}

	DEBUG(log_) << "Cache file: " << file_path_;
	DEBUG(log_) << "Max size: " << size;
	DEBUG(log_) << "Stripe size: " << sizeof (COSSStripe);
	DEBUG(log_) << "Stripe header size: " << sizeof (COSSStripeHeader);
	DEBUG(log_) << "Serial: " << serial_number_;
	DEBUG(log_) << "Stripe number: " << stripe_range_;
}

XCodecCacheCOSS::~XCodecCacheCOSS()
{
	for (int i = 0; i < LOADED_STRIPE_COUNT; ++i)
		if (stripe_[i].header.metadata.state == 1)
			store_stripe (i, (i == active_ ? sizeof (COSSStripe) : sizeof (COSSStripeHeader)));
			
   stream_.close();

	delete[] directory_;

	/*
	INFO(log_) << "Stats: ";
	INFO(log_) << "\tLookups=" << stats_.lookups;
	INFO(log_) << "\tHits=" << (stats_.found_1 + stats_.found_2) << " (" << stats_.found_1 << " + " << stats_.found_2 << ")";
   if (stats_.lookups > 0)
      INFO(log_) << "\tHit ratio=" << ((stats_.found_1 + stats_.found_2) * 100) / stats_.lookups << "%";
	*/

	DEBUG(log_) << "Closing coss file: " << file_path_;
	DEBUG(log_) << "Serial: " << serial_number_;
	DEBUG(log_) << "Stripe number: " << stripe_range_;
	DEBUG(log_) << "Index size: " << cache_index_.size();
}

bool XCodecCacheCOSS::read_file ()
{
	COSSStripeHeader header;
	COSSIndexEntry entry;
	uint64_t serial, range, limit, level;
	uint64_t hash;
	
	serial = range = limit = level = 0;
	limit = file_size_ / sizeof (COSSStripe);
	if (limit * sizeof (COSSStripe) != file_size_)
		return false;
	if (limit > stripe_limit_)
		limit = stripe_limit_;
	stream_.seekg (0);

	for (uint64_t n = 0; n < limit; ++n)
	{
		stream_.read ((char*) &header, sizeof header);
		if (! stream_.good() || stream_.gcount () != sizeof header)
			return false;
		if (header.metadata.signature != CACHE_SIGNATURE)
			return false;
		if (header.metadata.segment_count > STRIPE_SEGMENT_COUNT)
			return false;
		stream_.seekg (sizeof (COSSStripe) - sizeof header, ios::cur);
		
		if (header.metadata.serial_number > serial) 
			serial = header.metadata.serial_number, range = n;
		if (header.metadata.freshness > level) 
			level = header.metadata.freshness;

		directory_[n] = header.metadata;
		directory_[n].state = 0;
		
		for (int i = 0; i < STRIPE_SEGMENT_COUNT; ++i) 
		{
			if ((hash = header.hash_array[i]))
			{
				entry.stripe_range = n;
				entry.position = i;
				cache_index_.insert (hash, entry);
			}
		}
	}

	if (serial > 0)
	{
		serial_number_ = serial;
		stripe_range_ = range;
		freshness_level_ = level;
		load_stripe (stripe_range_, active_);
	}
	else
	{
		initialize_stripe (stripe_range_, active_);
	}
	
	return true;
}

void XCodecCacheCOSS::enter (const uint64_t& hash, const Buffer& buf, unsigned off)
{
	COSSIndexEntry entry;
	
	while (stripe_[active_].header.metadata.segment_index >= STRIPE_SEGMENT_COUNT)
		new_active ();

	COSSStripe& act = stripe_[active_];
	act.header.hash_array[act.header.metadata.segment_index] = hash;
	buf.copyout (act.segment_array[act.header.metadata.segment_index].bytes, off, XCODEC_SEGMENT_LENGTH);
	entry.stripe_range = act.header.metadata.stripe_range;
	entry.position = act.header.metadata.segment_index;
	
	act.header.metadata.segment_index++;
	while (act.header.metadata.segment_index < STRIPE_SEGMENT_COUNT && 
			 act.header.hash_array[act.header.metadata.segment_index])
		act.header.metadata.segment_index++;
	act.header.metadata.segment_count++;
	act.header.metadata.freshness = ++freshness_level_;
	
	cache_index_.insert (hash, entry);
}

bool XCodecCacheCOSS::lookup (const uint64_t& hash, Buffer& buf)
{
	const COSSIndexEntry* entry;
	const uint8_t* data;
	int slot;

	stats_.lookups++;
	
	if ((data = find_recent (hash)))
	{
		buf.append (data, XCODEC_SEGMENT_LENGTH);
		stats_.found_1++;
		return true;
	}
		
	if (! (entry = cache_index_.lookup (hash)))
		return false;
	
	for (slot = 0; slot < LOADED_STRIPE_COUNT; ++slot)
		if (stripe_[slot].header.metadata.stripe_range == entry->stripe_range)
			break;
			
	if (slot >= LOADED_STRIPE_COUNT)
	{
		slot = best_unloadable_slot ();
		detach_stripe (slot);
		load_stripe (entry->stripe_range, slot);
	}
	
	if (stripe_[slot].header.hash_array[entry->position] != hash)
		return false;
		
	stripe_[slot].header.metadata.freshness = ++freshness_level_;
	stripe_[slot].header.metadata.uses++;
	stripe_[slot].header.metadata.credits++;
	stripe_[slot].header.metadata.load_uses++;
	stripe_[slot].header.flags[entry->position] |= 3;

	data = stripe_[slot].segment_array[entry->position].bytes;
	remember (hash, data);
	buf.append (data, XCODEC_SEGMENT_LENGTH);
	stats_.found_2++;
	return true;
}

void XCodecCacheCOSS::initialize_stripe (uint64_t range, int slot)
{
	memset (&stripe_[slot].header, 0, sizeof (COSSStripeHeader));
	stripe_[slot].header.metadata.signature = CACHE_SIGNATURE;
	stripe_[slot].header.metadata.version = CACHE_VERSION;
	stripe_[slot].header.metadata.serial_number = ++serial_number_;
	stripe_[slot].header.metadata.stripe_range = range;
	stripe_[slot].header.metadata.state = 1;
	directory_[range] = stripe_[slot].header.metadata;
}

bool XCodecCacheCOSS::load_stripe (uint64_t range, int slot)
{
	uint64_t pos = range * sizeof (COSSStripe);
	if (pos < file_size_)
	{
		stream_.seekg (pos);
		stream_.read ((char*) &stripe_[slot], sizeof (COSSStripe));
		if (stream_.gcount () == sizeof (COSSStripe))
		{
			stripe_[slot].header.metadata.stripe_range = range;
			stripe_[slot].header.metadata.load_uses = 0;
			stripe_[slot].header.metadata.state = 1;
			directory_[range].state = 1;
			return true;
		}
	}
	
	stream_.clear ();
	return false;
}

void XCodecCacheCOSS::store_stripe (int slot, size_t size)
{
	uint64_t pos = stripe_[slot].header.metadata.stripe_range * sizeof (COSSStripe);
	if (pos != (uint64_t) stream_.tellp ())
		stream_.seekp (pos);
	stream_.write ((char*) &stripe_[slot], size);
	if (stream_.good () && pos + sizeof (COSSStripe) > file_size_)
		file_size_ = pos + sizeof (COSSStripe);
	stream_.clear ();
}

void XCodecCacheCOSS::new_active ()
{
	store_stripe (active_, sizeof (COSSStripe));
	active_ = best_unloadable_slot ();
	detach_stripe (active_);
	stripe_range_ = best_erasable_stripe ();
	if (load_stripe (stripe_range_, active_))
		purge_stripe (active_);
	else
		initialize_stripe (stripe_range_, active_);
}

int XCodecCacheCOSS::best_unloadable_slot ()
{
	uint64_t v, n = 0xFFFFFFFFFFFFFFFFull;
	int j = 0;
	
	for (int i = 0; i < LOADED_STRIPE_COUNT; ++i)
	{
		if (i == active_)
			continue;
		if (stripe_[i].header.metadata.signature == 0)
			return i;
		if ((v = stripe_[i].header.metadata.freshness + stripe_[i].header.metadata.load_uses) < n)
			j = i, n = v;
	}
			
	return j;
}

uint64_t XCodecCacheCOSS::best_erasable_stripe ()
{
	COSSMetadata* m;
	uint64_t v, n = 0xFFFFFFFFFFFFFFFFull;
	uint64_t i, j = 0;

	for (m = directory_, i = 0; i < stripe_limit_; ++i, ++m)
	{
		if (m->state == 1)
			continue;
		if (m->signature == 0)
			return i;
		if ((v = m->freshness + m->uses) < n)
			j = i, n = v;
	}
	
	return j;
}

void XCodecCacheCOSS::detach_stripe (int slot)
{
	if (stripe_[slot].header.metadata.state == 1)
	{
		uint64_t range = stripe_[slot].header.metadata.stripe_range; 
		directory_[range] = stripe_[slot].header.metadata;
		directory_[range].state = 2;
		
		for (int i = 0; i < STRIPE_SEGMENT_COUNT; ++i)
		{
			if (stripe_[slot].header.flags[i] & 1)
			{
				forget (stripe_[slot].header.hash_array[i]);
				stripe_[slot].header.flags[i] &= ~1;
			}
		}
		
		stripe_[slot].header.metadata.state = 0;
		store_stripe (slot, sizeof (COSSStripeHeader));
	}
}

void XCodecCacheCOSS::purge_stripe (int slot)
{
	for (int i = STRIPE_SEGMENT_COUNT - 1; i >= 0; --i)
	{
		uint64_t hash = stripe_[slot].header.hash_array[i];
		if (hash && ! (stripe_[slot].header.flags[i] & 2))
		{
			cache_index_.erase (hash);
			stripe_[slot].header.hash_array[i] = 0;
			stripe_[slot].header.flags[i] = 0;
			stripe_[slot].header.metadata.segment_count--;
		}
		
		stripe_[slot].header.flags[i] &= ~2;
		if (! stripe_[slot].header.hash_array[i])
			stripe_[slot].header.metadata.segment_index = i;
	}
	
	stripe_[slot].header.metadata.serial_number = ++serial_number_;
	stripe_[slot].header.metadata.uses = stripe_[slot].header.metadata.credits;
	stripe_[slot].header.metadata.credits = 0;
	
	if (stripe_[slot].header.metadata.segment_count >= STRIPE_SEGMENT_COUNT)
		INFO(log_) << "No more space available in cache";
}
