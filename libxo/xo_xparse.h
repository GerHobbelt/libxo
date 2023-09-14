/*
 * Copyright (c) 2023, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, July 2023
 */

#ifndef XO_XPARSE_H
#define XO_XPARSE_H

#include <ctype.h>
#include "xo_private.h"

typedef unsigned xo_xparse_node_type_t;
typedef xo_off_t xo_xparse_str_id_t;
typedef xo_off_t xo_xparse_node_id_t;

typedef void (*xo_xpath_warn_func_t)(void *data, const char *, va_list);

typedef struct xo_xparse_node_s {
    unsigned xn_type;		/* Type of this node (token) */
    xo_off_t xn_str;		/* String value (in xd_str_buf) */
    xo_xparse_node_id_t xn_contents; /* Child node (main) (in xd_node_buf) */
    xo_xparse_node_id_t xn_next; /* Next node (in xd_node_buf) */
    xo_xparse_node_id_t xn_prev; /* Previous node (in xd_node_buf) */
} xo_xparse_node_t;

typedef struct xo_xparse_data_s {
    xo_handle_t *xd_xop;	/* libxo handle */

    int xd_errors;              /* Number of errors seen */
    char xd_filename[MAXPATHLEN]; /* Path of current file */
    char *xd_buf;		/* XPath contents */
    unsigned xd_line;           /* Line number */
    unsigned xd_col;		/* Column number */
    unsigned xd_col_start;	/* First column of current line */
    int xd_last;		/* Last token returned */
    int xd_ttype;		/* Magic token to return */
    xo_xparse_node_id_t xd_result; /* Final resultsn */

    xo_off_t xd_len;            /* Last valid byte in xd_buf (+1) */
    xo_off_t xd_start;          /* Next valid byte in xd_buf */
    xo_off_t xd_cur;            /* Current byte in xd_buf */
    xo_off_t xd_size;           /* Size of xd_buf */

    unsigned xd_flags;          /* Flags */
    xo_buffer_t xd_str_buf;	/* Buffer for allocating string data */
    xo_buffer_t xd_node_buf;    /* Buffer for allocating node data */
    xo_off_t xd_last_node;	/* Node offset of last allocated node */
    xo_off_t xd_last_str;	/* Node offset of last allocated string */

    xo_xpath_warn_func_t xd_warn_func; /* Function to emit warnings */
    void *xd_warn_data;	       /* Opaque data passed to xd_warn_func */
} xo_xparse_data_t;

/* Flags for xd_flags */
#define XDF_EOF                 (1<<0)  /* EOF seen */
#define XDF_NO_SLAX_KEYWORDS    (1<<1)  /* Do not allow slax keywords */
#define XDF_NO_XPATH_KEYWORDS   (1<<2)  /* Do not allow xpath keywords */
#define XDF_OPEN_COMMENT        (1<<3)  /* EOF with open comment */

extern const int xo_xparse_num_tokens;
extern const char *xo_xparse_keyword_string[];
extern const char *xo_xparse_token_name_fancy[];
extern int xo_xpath_yydebug;

int
xo_xparse_token_translate (int ttype);

static inline int
xo_xparse_is_bare_char (int ch)
{
    return (isalnum(ch) || (ch == ':') || (ch == '_') || (ch == '.')
            || (ch == '-') || (ch & 0x80));
}

/*
 * Initialize all the input data fields in xdp
 */
static inline void
xo_xparse_set_data (xo_xparse_data_t *xdp, const char *str)
{
    ssize_t len = strlen(str);

    xdp->xd_buf = xo_realloc(xdp->xd_buf, len);
    xdp->xd_len = xdp->xd_buf ? len : 0;
}

/*
 * Return a pointer to a string in the string buffer
 * Note: the lifetime of the pointer ends when a new allocation is made
 */
static inline const char *
xo_xparse_str (xo_xparse_data_t *xdp, xo_xparse_str_id_t off)
{
    return off ? xo_buf_data(&xdp->xd_str_buf, off) : NULL;
}

/*
 * Return a node pointer in the node buffer
 * Note: the lifetime of the pointer ends when a new allocation is made
 */
static inline xo_xparse_node_t *
xo_xparse_node (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    xo_off_t off = id * sizeof(xo_xparse_node_t);
    return id ? (void *) xo_buf_data(&xdp->xd_node_buf, off) : NULL;
}

/*
 * Return a node pointer in the node buffer
 * Note: the lifetime of the pointer ends when a new allocation is made
 */
extern xo_xparse_node_t xo_xparse_dead_node;

static inline xo_xparse_node_t *
xo_xparse_node_ok (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    xo_off_t off = id * sizeof(xo_xparse_node_t);
    return id ? (void *) xo_buf_data(&xdp->xd_node_buf, off)
	: &xo_xparse_dead_node;
}

static inline xo_xparse_node_id_t
xo_xparse_node_new (xo_xparse_data_t *xdp)
{
    xo_off_t new_node = xdp->xd_last_node + 1;
    if (!xo_buf_make_room(&xdp->xd_node_buf,
			 new_node * sizeof(xo_xparse_node_t)))
	return 0;

    xdp->xd_last_node += 1;
    return new_node;
}

/*
 * Report a parser error (aka yyerror)
 */
int
xo_xpath_yyerror (xo_xparse_data_t *, const char *, int yystate);

int
xo_xpath_yyparse (xo_xparse_data_t *);

int
xo_xpath_yylex (xo_xparse_data_t *, xo_xparse_node_id_t *);

int
xo_xparse_ternary_rewrite (xo_xparse_data_t *, xo_xparse_node_id_t *d0,
			  xo_xparse_node_id_t *d1, xo_xparse_node_id_t *d2,
			  xo_xparse_node_id_t *d3, xo_xparse_node_id_t *d4,
			  xo_xparse_node_id_t *d5);

int
xo_xparse_concat_rewrite (xo_xparse_data_t *, xo_xparse_node_id_t *d0,
			 xo_xparse_node_id_t *d1, xo_xparse_node_id_t *d2,
			 xo_xparse_node_id_t *d3);

void
xo_xparse_check_axis_name (xo_xparse_data_t *, xo_xparse_node_id_t id);

int
xo_xpath_parse (xo_xparse_data_t *);

/*
 * Return a human-readable name for a given token type
 */
const char *
xo_xparse_token_name (int ttype);

/*
 * Expose YYTRANSLATE outside the yacc file
 */
int
xo_xparse_token_translate (int ttype);

/*
 * Return a better class of error message
 */
char *
xo_xparse_expecting_error (const char *token, int yystate, int yychar);

/*
 * Is the given character valid inside variable names (T_VAR)?
 */
static inline int
xo_xparse_is_var_char (int ch)
{
    return (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == ':');
}

static inline void
xo_xparse_node_set_type (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			 unsigned type)
{
    if (id) {
	xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
	xnp->xn_type = type;
    }
}

static inline void
xo_xparse_node_set_str (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			xo_xparse_node_id_t value)
{
    if (id) {
	xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
	xnp->xn_str = value;
    }
}

void
xo_xparse_node_set_next (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			 xo_xparse_node_id_t value);

void
xo_xparse_node_set_contents (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			     xo_xparse_node_id_t value);

static inline xo_xparse_node_id_t
xo_xparse_node_contents (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    if (id == 0)
	return 0;

    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
    return xnp->xn_contents;
}

static inline xo_xparse_node_type_t
xo_xparse_node_type (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    if (id == 0)
	return 0;

    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
    return xnp->xn_type;
}

static inline xo_xparse_str_id_t
xo_xparse_node_str (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    if (id == 0)
	return 0;

    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
    return xnp->xn_str;
}

/*
 * Fetch the string from inside a node.
 *
 * Note this is very different turning a string id into a string, like
 * xo_xparse_str() does.
 */
static inline const char *
xo_xparse_node_extract_string (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    xo_xparse_str_id_t sid = xo_xparse_node_str(xdp, id);
    if (sid == 0)
	return NULL;

    return xo_xparse_str(xdp, sid);
}

xo_xparse_str_id_t
xo_xparse_str_new (xo_xparse_data_t *xdp);

void
xo_xparse_init (xo_xparse_data_t *xdp);

void
xo_xparse_clean (xo_xparse_data_t *xdp);

void
xo_xparse_dump (xo_xparse_data_t *xdp, xo_xparse_node_id_t id);

int
xo_xparse_yyval (xo_xparse_data_t *xdp, xo_xparse_node_id_t id);

const char *
xo_xparse_fancy_token_name (int id);

int
xo_xpath_feature_warn (const char *tag, xo_xparse_data_t *xdp,
		       const int *tokens, const char *bytes);

static inline void
xo_xpath_set_warn_func (xo_xparse_data_t *xdp, xo_xpath_warn_func_t func,
			void *data)
{
    xdp->xd_warn_func = func;
    xdp->xd_warn_data = data;
}

static inline int
xo_xparse_node_is_attr_axis (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    if (id == 0)
	return 0;

    if (xo_xparse_node_type(xdp, id) != T_AXIS_NAME)
	return 0;

    const char *str = xo_xparse_node_extract_string(xdp, id);
    if (str == NULL)
	return 0;

    return xo_streq(str, "attribute");
}

#endif /* XO_XPARSE_H */
