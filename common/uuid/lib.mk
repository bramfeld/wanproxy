VPATH+=	${TOPDIR}/common/uuid

SRCS+=	uuid.cc

ifeq "${OSNAME}" "Linux"
LDADD+=	-luuid
endif
