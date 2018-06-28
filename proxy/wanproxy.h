////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           wanproxy.h                                                 //
// Description:    global data for the wanproxy application                   //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
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

struct WanProxyInstance
{
	std::string proxy_name_;
	bool proxy_client_;
	bool proxy_secure_;
	SocketAddressFamily local_protocol_;
	std::string local_address_;
	WANProxyCodec local_codec_;
	SocketAddressFamily remote_protocol_;
	std::string remote_address_;
	WANProxyCodec remote_codec_;
	ProxyListener* listener_;
	
	WanProxyInstance ()
	{
		proxy_client_ = proxy_secure_ = false; 
		local_protocol_ = remote_protocol_ = SocketAddressFamilyIP;
		listener_ = 0;
	}
	
	~WanProxyInstance ()
	{
		delete listener_;
	}
};

struct WanProxyCore
{
private:
	std::string config_file_;
	Action* reload_action_;
	std::map<UUID, XCodecCache*> caches_;
	std::map<std::string, WanProxyInstance> proxies_;

public:
	WanProxyCore ()
	{
		reload_action_ = 0;
	}
	
	bool configure (const std::string& file)
	{
		WANProxyConfig config;
		config_file_ = file;
		if (reload_action_)
			reload_action_->cancel ();
		reload_action_ = event_system.register_interest (EventInterestReload, callback (this, &WanProxyCore::reload));
		return config.read_file (config_file_); 
	}
	
	void reload ()
	{
		if (configure (config_file_)) 
			INFO("wanproxy/core") << "Reloaded proxy configuration.";
		else
			INFO("wanproxy/core") << "Could not reconfigure proxies.";
	}	
	
	void add_proxy (std::string& name, WanProxyInstance& data)
	{
	   WanProxyInstance& prx = proxies_[name];
	   
	   prx.proxy_name_ = data.proxy_name_;
	   prx.proxy_client_ = data.proxy_client_;
	   prx.proxy_secure_ = data.proxy_secure_;
	   prx.local_protocol_ = data.local_protocol_;
	   prx.local_address_ = data.local_address_;
	   prx.local_codec_ = data.local_codec_;
	   prx.remote_protocol_ = data.remote_protocol_;
	   prx.remote_address_ = data.remote_address_;
	   prx.remote_codec_ = data.remote_codec_;
	   
	   if (! prx.listener_)
			prx.listener_ = new ProxyListener (prx.proxy_name_, &prx.local_codec_, &prx.remote_codec_, 
														  prx.local_protocol_, prx.local_address_, prx.remote_protocol_, prx.remote_address_,
														  prx.proxy_client_, prx.proxy_secure_);
	   else
			prx.listener_->refresh (prx.proxy_name_, &prx.local_codec_, &prx.remote_codec_, 
											prx.local_protocol_, prx.local_address_, prx.remote_protocol_, prx.remote_address_, 
											prx.proxy_client_, prx.proxy_secure_);
	}
	
	XCodecCache* add_cache (WANProxyConfigCache type, std::string& path, size_t size, UUID& uuid)
	{
		XCodecCache* cache = 0;
		switch (type)
		{
		case WANProxyConfigCacheMemory:
			cache = new XCodecMemoryCache (uuid, size);
			break;
		case WANProxyConfigCacheCOSS: 
			cache = new XCodecCacheCOSS (uuid, path, size);
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
			
		std::map<std::string, WanProxyInstance>::iterator prx;
		for (prx = proxies_.begin(); prx != proxies_.end(); prx++)
		   print_stream_counts (prx->second);
		proxies_.clear ();
		   
		std::map<UUID, XCodecCache*>::iterator it;
		for (it = caches_.begin(); it != caches_.end(); it++)
			delete it->second;
		caches_.clear ();
	}
	
	void print_stream_counts (WanProxyInstance& prx)
	{
		if (prx.local_codec_.counting_ || prx.remote_codec_.counting_)
			INFO("wanproxy/core") << "Stream counts for proxy: " << prx.proxy_name_;
		
		if (prx.local_codec_.counting_)
		{
			INFO("wanproxy/core") << "Local codec request input bytes:    " << prx.local_codec_.request_input_bytes_;
			INFO("wanproxy/core") << "Local codec request output bytes:   " << prx.local_codec_.request_output_bytes_;
			INFO("wanproxy/core") << "Local codec response input bytes:   " << prx.local_codec_.response_input_bytes_;
			INFO("wanproxy/core") << "Local codec response output bytes:  " << prx.local_codec_.response_output_bytes_;
		}
		
		if (prx.remote_codec_.counting_)
		{
			INFO("wanproxy/core") << "Remote codec request input bytes:   " << prx.remote_codec_.request_input_bytes_;
			INFO("wanproxy/core") << "Remote codec request output bytes:  " << prx.remote_codec_.request_output_bytes_;
			INFO("wanproxy/core") << "Remote codec response input bytes:  " << prx.remote_codec_.response_input_bytes_;
			INFO("wanproxy/core") << "Remote codec response output bytes: " << prx.remote_codec_.response_output_bytes_;
		}
	}
};

extern WanProxyCore wanproxy;	
	
#endif /* !PROGRAMS_WANPROXY_WANPROXY_CORE_H */
