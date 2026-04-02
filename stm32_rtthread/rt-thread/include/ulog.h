/*
 * Forwarding header for ulog to make <ulog.h> available from
 * the common include path when building with make.
 *
 * This file intentionally forwards to the real header in
 * ../components/utilities/ulog/ulog.h so that existing -I
 * flags (which include ../rt-thread/include) will find it.
 */
#ifndef RT_USING_ULOG_FORWARD_H
#define RT_USING_ULOG_FORWARD_H

#include "../components/utilities/ulog/ulog.h"

#endif /* RT_USING_ULOG_FORWARD_H */
