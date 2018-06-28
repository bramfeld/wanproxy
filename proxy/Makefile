PROGRAM=wanproxy

SRCS+=	wanproxy.cc
SRCS+=	wanproxy_config.cc
SRCS+=	wanproxy_config_class_codec.cc
SRCS+=	wanproxy_config_class_interface.cc
SRCS+=	wanproxy_config_class_peer.cc
SRCS+=	wanproxy_config_class_proxy.cc
SRCS+=	wanproxy_config_type_codec.cc
SRCS+=	wanproxy_config_type_compressor.cc
SRCS+=	wanproxy_config_type_proxy_type.cc
SRCS+=	wanproxy_config_type_proxy_role.cc
SRCS+=	proxy_listener.cc
SRCS+=	proxy_connector.cc

TOPDIR=..
USE_LIBS=common common/thread common/time common/uuid config crypto event http io io/net io/socket ssh xcodec xcodec/cache/coss zlib
include ${TOPDIR}/common/program.mk

