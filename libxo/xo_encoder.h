/*
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, August 2015
 */

/*
 * NOTE WELL: This file is needed to software that implements an
 * external encoder for libxo that allows libxo data to be encoded in
 * new and bizarre formats.  General libxo code should _never_
 * include this header file.
 */

#ifndef XO_ENCODER_H
#define XO_ENCODER_H

#include <string.h>

#include "xo_private.h"
#include "xo_buf.h"

/* Flags for formatting functions */
typedef unsigned long long xo_xff_flags_t;
#define XFF_COLON	(1<<0)	/* Append a ":" */
#define XFF_COMMA	(1<<1)	/* Append a "," iff there's more output */
#define XFF_WS		(1<<2)	/* Append a blank */
#define XFF_ENCODE_ONLY	(1<<3)	/* Only emit for encoding styles (XML, JSON) */

#define XFF_QUOTE	(1<<4)	/* Force quotes */
#define XFF_NOQUOTE	(1<<5)	/* Force no quotes */
#define XFF_DISPLAY_ONLY (1<<6)	/* Only emit for display styles (text, html) */
#define XFF_KEY		(1<<7)	/* Field is a key (for XPath) */

#define XFF_XML		(1<<8)	/* Force XML encoding style (for XPath) */
#define XFF_ATTR	(1<<9)	/* Escape value using attribute rules (XML) */
#define XFF_BLANK_LINE	(1<<10)	/* Emit a blank line */
#define XFF_NO_OUTPUT	(1<<11)	/* Do not make any output */

#define XFF_TRIM_WS	(1<<12)	/* Trim whitespace off encoded values */
#define XFF_LEAF_LIST	(1<<13)	/* A leaf-list (list of values) */
#define XFF_UNESCAPE	(1<<14)	/* Need to printf-style unescape the value */
#define XFF_HUMANIZE	(1<<15)	/* Humanize the value (for display styles) */

#define XFF_HN_SPACE	(1<<16)	/* Humanize: put space before suffix */
#define XFF_HN_DECIMAL	(1<<17)	/* Humanize: add one decimal place if <10 */
#define XFF_HN_1000	(1<<18)	/* Humanize: use 1000, not 1024 */
#define XFF_GT_FIELD	(1<<19) /* Call gettext() on a field */

#define XFF_GT_PLURAL	(1<<20)	/* Call dngettext to find plural form */
#define XFF_ARGUMENT	(1<<21)	/* Content provided via argument */

/* Flags to turn off when we don't want i18n processing */
#define XFF_GT_FLAGS (XFF_GT_FIELD | XFF_GT_PLURAL)

typedef unsigned xo_encoder_op_t;

/* Encoder operations; names are in xo_encoder.c:xo_encoder_op_name() */
#define XO_OP_UNKNOWN		0
#define XO_OP_CREATE		1 /* Called when the handle is init'd */
#define XO_OP_OPEN_CONTAINER	2
#define XO_OP_CLOSE_CONTAINER	3
#define XO_OP_OPEN_LIST		4
#define XO_OP_CLOSE_LIST	5
#define XO_OP_OPEN_LEAF_LIST	6
#define XO_OP_CLOSE_LEAF_LIST	7
#define XO_OP_OPEN_INSTANCE	8
#define XO_OP_CLOSE_INSTANCE	9
#define XO_OP_STRING		10 /* Quoted UTF-8 string */
#define XO_OP_CONTENT		11 /* Other content */
#define XO_OP_FINISH		12 /* Finish any pending output */
#define XO_OP_FLUSH		13 /* Flush any buffered output */
#define XO_OP_DESTROY		14 /* Clean up function */
#define XO_OP_ATTRIBUTE		15 /* Attribute name/value */
#define XO_OP_VERSION		16 /* Version string */
#define XO_OP_OPTIONS		17 /* Additional command line options */
#define XO_OP_OPTIONS_PLUS	18 /* Additional command line options */
#define XO_OP_DEADEND		19 /* Dead end (clear the top) */

#define XO_ENCODER_HANDLER_ARGS			\
	xo_handle_t *xop XO_UNUSED,		\
	xo_encoder_op_t op XO_UNUSED,	        \
	xo_buffer_t *bufp XO_UNUSED,             \
	const char *name XO_UNUSED,		\
        const char *value XO_UNUSED,		\
	void *private XO_UNUSED,		\
	xo_xff_flags_t flags XO_UNUSED

typedef int (*xo_encoder_func_t)(XO_ENCODER_HANDLER_ARGS);

/*
 * White generating filtered output, we need to make "tentative"
 * content, which might end up either as output or discarded when we
 * decide we don't need it.  For example, if the filter is "a/b/c",
 * then we need to make output for both "a" and "b" before we have a
 * chance of seeing "c".
 *
 * The problem is exacerbated by encoders possibly needing to repeat
 * "prefix" content for each child or nested field.  So this leading
 * content must be preserved outside the normal output buffering.
 *
 * We use the "whiteboard" to allow this tentative output generation.
 * We can create whiteboards for each actve "match" and write content
 * into it, recording offsets for each container/list/instance using
 * our stack.  When we pop items off the stack, the encoder can zero
 * out the byte for each frame's offset, clearing the trailing bits of
 * the content.
 *
 * If this is not sufficient, the encoder can use this offset to hold
 * any sort of opaque data that is needed.
 */

/* Operatons for whiteboards */
#define XO_WB_INIT		1 /* Initialize the whiteboard */
#define XO_WB_RECORD		2 /* Record the current offset */
#define XO_WB_ALLOW		3 /* Allow output */
#define XO_WB_DENY		4 /* Deny output */
#define XO_WB_CLEAN		5 /* Clean up and release */

typedef unsigned xo_whiteboard_op_t;	/* Whiteboard operation */

#define XO_WHITEBOARD_FUNC_ARGS              \
    xo_handle_t *xop XO_UNUSED,		     \
    xo_whiteboard_op_t op  XO_UNUSED,        \
    xo_buffer_t *wbp XO_UNUSED,		     \
    xo_off_t *offp XO_UNUSED,		     \
    void *private XO_UNUSED

typedef int (*xo_whiteboard_func_t)(XO_WHITEBOARD_FUNC_ARGS);

typedef struct xo_encoder_init_args_s {
    unsigned xei_version;	   /* Current version */
    xo_encoder_func_t xei_handler; /* Encoding handler */
    xo_whiteboard_func_t xei_wb_marker; /* Whiteboard marker function (v2) */
} xo_encoder_init_args_t;

#define XO_ENCODER_VERSION	2 /* Current version */

#define XO_ENCODER_INIT_ARGS \
    xo_encoder_init_args_t *arg XO_UNUSED

typedef int (*xo_encoder_init_func_t)(XO_ENCODER_INIT_ARGS);
/*
 * Each encoder library must define a function named xo_encoder_init
 * that takes the arguments defined in XO_ENCODER_INIT_ARGS.  It
 * should return zero for success.
 */
#define XO_ENCODER_INIT_NAME_TOKEN xo_encoder_library_init
#define XO_STRINGIFY(_x) #_x
#define XO_STRINGIFY2(_x) XO_STRINGIFY(_x)
#define XO_ENCODER_INIT_NAME XO_STRINGIFY2(XO_ENCODER_INIT_NAME_TOKEN)
extern int XO_ENCODER_INIT_NAME_TOKEN (XO_ENCODER_INIT_ARGS);

void
xo_encoder_register (const char *name, xo_encoder_func_t func);

void
xo_encoder_unregister (const char *name);

void *
xo_get_private (xo_handle_t *xop);

void
xo_encoder_path_add (const char *path);

void
xo_set_private (xo_handle_t *xop, void *opaque);

xo_encoder_func_t
xo_get_encoder (xo_handle_t *xop);

xo_whiteboard_func_t
xo_get_wb_marker (xo_handle_t *xop);

void
xo_set_encoder (xo_handle_t *xop, xo_encoder_func_t encoder,
		xo_whiteboard_func_t wb_marker);

int
xo_encoder_init (xo_handle_t *xop, const char *name);

xo_handle_t *
xo_encoder_create (const char *name, xo_xof_flags_t flags);

int
xo_encoder_handle (xo_handle_t *xop, xo_encoder_op_t op, xo_buffer_t *bufp,
		   const char *name, const char *value, xo_xff_flags_t flags);

int
xo_encoder_wb_marker (xo_handle_t *xop, xo_whiteboard_op_t op,
		      xo_buffer_t *wbp, xo_off_t *offp);

void
xo_encoders_clean (void);

const char *
xo_encoder_op_name (xo_encoder_op_t op);

const char *
xo_whiteboard_op_name (xo_whiteboard_op_t op);

/*
 * xo_failure is used to announce internal failures, when "warn" is on
 */
void
xo_failure (xo_handle_t *xop, const char *fmt, ...);

#endif /* XO_ENCODER_H */
