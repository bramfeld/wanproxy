#include <common/buffer.h>
#include <common/test.h>
#include <common/uuid/uuid.h>

#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/xcodec_decoder.h>
#include <xcodec/xcodec_encoder.h>
#include <xcodec/xcodec_hash.h>
#include <xcodec/cache/coss/xcodec_cache_coss.h>

#include <boost/filesystem.hpp>

#include <fstream>

#include <stdlib.h>

using namespace boost::filesystem;

int
main(void)
{

        char tmp_template[] = "/tmp/cache-coss-XXXXXX";
	path cache_path = mkdtemp(tmp_template);
	create_directory(cache_path);

        typedef pair<uint64_t, const uint8_t*> segment_list_element_t;
        typedef deque<segment_list_element_t> segment_list_t;
        segment_list_t segment_list;

	{
		TestGroup g("/test/xcodec/encode-decode-coss/2/char_kat", 
				"XCodecEncoder::encode / XCodecDecoder::decode #2");

		UUID uuid;
		std::string cache_path_str = cache_path.string();
                unsigned i, j;

		uuid.generate();

		for (j = 0; j < 4; j++) {
                        XCodecCache *cache = new XCodecCacheCOSS(uuid, cache_path_str, 
					10, 10, 10);

                        for (i = 0; i < 10000; i++) {
                                uint8_t random[XCODEC_SEGMENT_LENGTH];

                                ifstream rand_fd("/dev/urandom");
                                rand_fd.read(random, sizeof(random));
                                ASSERT("xcodec-coss1", rand_fd.good());
                        
                                uint64_t hash = XCodecHash::hash(random);
                                const uint8_t* data = cache->lookup(hash);
                                if (data)
                                   continue;
                                segment_list.push_front(make_pair(hash, data));
										  Buffer buf (data, XCODEC_SEGMENT_LENGTH);
                                cache->enter(hash, buf, 0);
                        }

                        delete cache;
                        cache = new XCodecCacheCOSS(uuid, cache_path_str,
					10, 10, 10);

                        segment_list_element_t el;
                        const uint8_t *seg1, *seg2;

                        uint64_t hash;
                        while (!segment_list.empty()){
                                el = segment_list.back();
                                segment_list.pop_back();
                                seg1 = el.second;
                                seg2 = cache->lookup(el.first);
                                hash = el.first;
                                if (!seg2)
                                        cout << "Segment not found: " << hash << endl;;
                                if (seg2) {
                                        if (memcmp (seg1, seg2, XCODEC_SEGMENT_LENGTH))
                                                cout << "Segments are not equal: " << hash <<
                                                        endl;
                                        Test _(g, "Segment are not equal.", 
                                                        seg1->equal(seg2));
                                }
                        }

                        delete cache;
                }
		
	}
	
	remove_all(cache_path);

	return (0);
}

