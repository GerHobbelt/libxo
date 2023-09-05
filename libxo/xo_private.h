/*
 * Copyright (c) 2023, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef XO_PRIVATE_H
#define XO_PRIVATE_H

/* Rather lame that we can't count on these... */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef UNUSED
#define UNUSED __attribute__ ((__unused__))
#endif /* UNUSED */

#define SNPRINTF(_start, _end, _fmt...) \
    do { \
        (_start) += snprintf((_start), (_end) - (_start), _fmt); \
        if ((_start) > (_end)) \
            (_start) = (_end); \
    } while (0)

#endif /* XO_PRIVATE_H */
