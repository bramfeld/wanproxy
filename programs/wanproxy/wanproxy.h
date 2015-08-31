////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           wanproxy.h                                                 //
// Description:    global data for the wanproxy application                   //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-08-31                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	PROGRAMS_WANPROXY_WANPROXY_CORE_H
#define	PROGRAMS_WANPROXY_WANPROXY_CORE_H

#include <event/event_system.h>
#include <common/uuid/uuid.h>
#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/cache/coss/xcodec_cache_coss.h>
#include "wanproxy_codec.h"
#include "wanproxy_config.h"
#include "wanproxy_config_type_codec.h"
#include "proxy_listener.h"


struct WanProxyCore
{
private:
	std::string config_file_;
	Action* reload_action_;
	ProxyListener* listener_;
	std::map<UUID, XCodecCache*> caches_;
	
public:
	std::string proxy_name_;
	bool proxy_client_;
	bool proxy_secure_;
	SocketAddressFamily local_protocol_;
	std::string local_address_;
	WANProxyCodec local_codec_;
	SocketAddressFamily remote_protocol_;
	std::string remote_address_;
	WANProxyCodec remote_codec_;
	WANProxyConfigCache cache_type_;
	std::string cache_path_;
	size_t cache_size_;
	UUID cache_uuid_;

	WanProxyCore ()
	{
		reload_action_ = 0;
		listener_ = 0;
		proxy_client_ = proxy_secure_ = false; local_protocol_ = remote_protocol_ = SocketAddressFamilyIP;
		cache_type_ = WANProxyConfigCacheMemory; cache_size_ = 0;
	}
	
	bool configure (const std::string& file)
	{
		WANProxyConfig config;
		config_file_ = file;
		if (reload_action_)
			reload_action_->cancel ();
		reload_action_ = event_system.register_interest (EventInterestReload, callback (this, &WanProxyCore::reload));
		return (! config_file_.empty () && config.configure (config_file_)); 
	}
	
	void launch_listener ()
	{
		listener_ = new ProxyListener (proxy_name_, &local_codec_, &remote_codec_, local_protocol_, local_address_, 
												 remote_protocol_, remote_address_, proxy_client_, proxy_secure_);
	}
	
	void reload ()
	{
		if (configure (config_file_) && listener_) 
			listener_->refresh (proxy_name_, &local_codec_, &remote_codec_, local_protocol_, local_address_, 
									  remote_protocol_, remote_address_, proxy_client_, proxy_secure_);
		else
			INFO("wanproxy/core") << "Could not reconfigure proxies.";
	}	
	
	XCodecCache* add_cache (UUID uuid, size_t size)
	{
		XCodecCache* cache = 0;
		switch (cache_type_)
		{
		case WANProxyConfigCacheMemory:
			cache = new XCodecMemoryCache (uuid, size);
			break;
		case WANProxyConfigCacheCOSS: 
			cache = new XCodecCacheCOSS (uuid, cache_path_, size);
			break;
		}
		ASSERT("/xcodec/cache", caches_.find(uuid) == caches_.end());
		if (cache)
			caches_[uuid] = cache;
		return cache;
	}
	
	XCodecCache* find_cache (UUID uuid)
	{
		std::map<UUID, XCodecCache*>::const_iterator it = caches_.find (uuid);
		if (it != caches_.end ())
			return it->second;
		return 0;
	}

	void terminate ()
	{
		if (reload_action_)
			reload_action_->cancel (), reload_action_ = 0;
		delete listener_; listener_ = 0;
		std::map<UUID, XCodecCache*>::iterator it;
		for (it = caches_.begin(); it != caches_.end(); it++)
			delete it->second;
		caches_.clear ();
	}
};

extern WanProxyCore wanproxy;	
	
#endif /* !PROGRAMS_WANPROXY_WANPROXY_CORE_H */
