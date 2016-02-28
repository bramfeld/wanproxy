VPATH+=	${TOPDIR}/common

SRCS+=	buffer.cc
SRCS+=	log.cc
SRCS+=	count_filter.cc

CXXFLAGS+=-include common/common.h
