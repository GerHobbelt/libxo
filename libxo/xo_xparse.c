/*
 * Copyright (c) 2006-2023, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * xo_xparse.c -- lex xpath input into lexical tokens
 *
 * This file is derived from libslax/slaxlexer.c
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/param.h>

#include <libxo/xo.h>
#include <libxo/xo_encoder.h>
#include <libxo/xo_buf.h>

#include "xo_xpath.tab.h"
#include "xo_xparse.h"

#define XD_BUF_FUDGE (BUFSIZ/8)
#define XD_BUF_INCR BUFSIZ

#define XO_MAX_CHAR	128	/* Size of our character tables */

static int xo_xparse_setup;	/* Have we initialized? */

xo_xparse_node_t xo_xparse_dead_node; /* Dead space when you just don't care */

/*
 * These are lookup tables for one and two character literal tokens.
 */
static short xo_single_wide[XO_MAX_CHAR];
static char xo_double_wide[XO_MAX_CHAR];
static char xo_triple_wide[XO_MAX_CHAR];

/*
 * Define all one character literal tokens, mapping the single
 * character to the xo_xpath.y token
 */
static int xo_single_wide_data[] = {
    L_AT, '@',
    L_CBRACE, '}',
    L_CBRACK, ']',
    L_COMMA, ',',
    L_COLON, ':',
    L_CPAREN, ')',
    L_DOT, '.',
    L_EOS, ';',
    L_EQUALS, '=',
    L_GRTR, '>',
    L_LESS, '<',
    L_MINUS, '-',
    L_NOT, '!',
    L_OBRACE, '{',
    L_OBRACK, '[',
    L_OPAREN, '(',
    L_PLUS, '+',
    L_QUESTION, '?',
    L_SLASH, '/',
    L_STAR, '*',
    L_UNDERSCORE, '_',
    L_VBAR, '|',
    0
};

/*
 * Define all two character literal tokens, mapping two contiguous
 * characters into a single xo_xpath.y token.  N.B.: These are not
 * "double-wide" as in multi-byte characters, but are two distinct
 * characters.  See also: xo_xparse_double_wide(), where this list is
 * unfortunately duplicated.
 */
static int xo_double_wide_data[] = {
    L_ASSIGN, ':', '=',
    L_DAMPER, '&', '&',
    L_DCOLON, ':', ':',
    L_DEQUALS, '=', '=',
    L_DOTDOT, '.', '.',
    L_DSLASH, '/', '/',
    L_DVBAR, '|', '|',
    L_GRTREQ, '>', '=',
    L_LESSEQ, '<', '=',
    L_NOTEQUALS, '!', '=',
    L_PLUSEQUALS, '+', '=',
    0
};

#if 0
/*
 * Define all three character literal tokens, mapping three contiguous
 * characters into a single xo_xpath.y token.
 */
static int xo_triple_wide_data[] = {
    L_DOTDOTDOT, '.', '.', '.',
    0
};
#endif

/*
 * Define all keyword tokens, mapping the keywords into the xo_xpath.y
 * token.  We also provide KMF_* flags to indicate what context the keyword
 * is reserved.  SLAX keywords are not reserved inside XPath expressions
 * and XPath keywords are not reserved in non-XPath contexts.
 */
typedef struct keyword_mapping_s {
    int km_ttype;		/* Token type (returned from yylex) */
    const char *km_string;	/* Token string */
    int km_flags;		/* Flags for this entry */
} keyword_mapping_t;

/* Flags for km_flags: */
#define KMF_NODE_TEST	(1<<0)	/* Node test */
#define KMF_SLAX_KW	(1<<1)	/* Keyword for slax */
#define KMF_XPATH_KW	(1<<2)	/* Keyword for xpath */
#define KMF_STMT_KW	(1<<3)	/* Fancy statement (slax keyword in xpath) */
#define KMF_JSON_KW	(1<<4)	/* JSON-only keywords */

static keyword_mapping_t keywordMap[] = {
    { K_AND, "and", KMF_XPATH_KW },
    { K_COMMENT, "comment", KMF_SLAX_KW | KMF_NODE_TEST },
    { K_DIV, "div", KMF_XPATH_KW },
    { K_ID, "id", KMF_NODE_TEST }, /* Not really, but... */
    { K_KEY, "key", KMF_SLAX_KW | KMF_NODE_TEST }, /* Not really, but... */
    { K_MOD, "mod", KMF_XPATH_KW },
    { K_NODE, "node", KMF_NODE_TEST },
    { K_OR, "or", KMF_XPATH_KW },
    { K_PROCESSING_INSTRUCTION, "processing-instruction",
      KMF_SLAX_KW | KMF_NODE_TEST }, /* Not a node test, but close enough */
    { K_TEXT, "text", KMF_NODE_TEST },
    { 0, NULL, 0 }
};


typedef struct xo_xparse_ttname_map_s {
    int st_ttype;		/* Token number */
    const char *st_name;	/* Fancy, human-readable value */
} xo_xparse_ttname_map_t;

xo_xparse_ttname_map_t xo_xparse_ttname_map[] = {
    { L_AT,			"attribute axis ('@')" },
    { L_CBRACE,			"close brace ('}')" },
    { L_OBRACK,			"close bracket (']')" },
    { L_COLON,			"colon (':')" },
    { L_COMMA,			"comma (',')" },
    { L_DAMPER,			"logical AND operator ('&&')" },
    { L_DCOLON,			"axis operator ('::')" },
    { L_DEQUALS,		"equality operator ('==')" },
    { L_DOTDOT,			"parent axis ('..')" },
    { L_DOTDOTDOT,		"sequence operator ('...')" },
    { L_DSLASH,			"descendant operator ('//')" },
    { L_DVBAR,			"logical OR operator ('||')" },
    { L_EOS,			"semi-colon (';')" },
    { L_EQUALS,			"equal sign ('=')" },
    { L_GRTR,			"greater-than operator ('>')" },
    { L_GRTREQ,			"greater-or-equals operator ('>=')" },
    { L_LESS,			"less-than operator ('<')" },
    { L_LESSEQ,			"less-or-equals operator ('<=')" },
    { L_MINUS,			"minus sign ('-')" },
    { L_NOT,			"not sign ('!')" },
    { L_NOTEQUALS,		"not-equals sign ('!=')" },
    { L_OBRACE,			"open brace ('{')" },
    { L_OBRACK,			"open bracket ('[')" },
    { L_OPAREN,			"open parentheses ('(')" },
    { L_PLUS,			"plus sign ('+')" },
    { L_PLUSEQUALS,		"increment assign operator ('+=')" },
    { L_SLASH,			"slash ('/')" },
    { L_STAR,			"star ('*')" },
    { L_UNDERSCORE,		"concatenation operator ('_')" },
    { L_VBAR,			"union operator ('|')" },
    { K_COMMENT,		"'comment'" },
    { K_ID,			"'id'" },
    { K_KEY,			"'key'" },
    { K_NODE,			"'node'" },
    { K_PROCESSING_INSTRUCTION,	"'processing-instruction'" },
    { K_TEXT,			"'text'" },
    { K_AND,			"'and'" },
    { K_DIV,			"'div'" },
    { K_MOD,			"'mod'" },
    { K_OR,			"'or'" },
    { L_ASTERISK,		"asterisk ('*')" },
    { L_CBRACK,			"close bracket (']')" },
    { L_CPAREN,			"close parentheses (')')" },
    { L_DOT,			"dot ('.')" },
    { T_AXIS_NAME,		"built-in axis name" },
    { T_BARE,			"bare word string" },
    { T_FUNCTION_NAME,		"function name" },
    { T_NUMBER,			"number" },
    { T_QUOTED,			"quoted string" },
    { T_VAR,			"variable name" },
    { C_PREDICATE,		"predicate ('[test]')" },
    { C_ELEMENT,		"path element" },
    { C_ATTRIBUTE,		"attribute axis" },
    { C_ABSOLUTE,		"Absolute path" },
    { C_DESCENDANT,		"descendant child ('one//two')" },
    { C_TEST,			"node test ('node()')" },
    { C_UNION,			"union of two paths ('one|two')" },
    { C_EXPR,			"parenthetical expresions" },
    { 0, NULL }
};

/*
 * Set up the lexer's lookup tables
 */
static void
xo_xparse_setup_lexer (void)
{
    int i, ttype;

    xo_xparse_setup = 1;

    for (i = 0; xo_single_wide_data[i]; i += 2)
	xo_single_wide[xo_single_wide_data[i + 1]] = xo_single_wide_data[i];

    for (i = 0; xo_double_wide_data[i]; i += 3)
	xo_double_wide[xo_double_wide_data[i + 1]] = i + 2;

    /* There's only one triple wide, so optimize (for now) */
    xo_triple_wide['.'] = 1;

    for (i = 0; keywordMap[i].km_ttype; i++)
	xo_xparse_keyword_string[xo_xparse_token_translate(keywordMap[i].km_ttype)]
	    = keywordMap[i].km_string;

    for (i = 0; xo_xparse_ttname_map[i].st_ttype; i++) {
	ttype = xo_xparse_token_translate(xo_xparse_ttname_map[i].st_ttype);
	xo_xparse_token_name_fancy[ttype] =  xo_xparse_ttname_map[i].st_name;
    }
}

#if 0
/*
 * Does the given character end a token?
 */
static inline int
xo_xparse_is_final_char (int ch)
{
    return (ch == ';' || isspace(ch));
}
#endif

/*
 * Does the input buffer start with the given keyword?
 */
static int
xo_xparse_keyword_match (xo_xparse_data_t *xdp, const char *str)
{
    int len = strlen(str);
    int ch;

    if (xdp->xd_len - xdp->xd_start < len)
	return FALSE;

    if (memcmp(xdp->xd_buf + xdp->xd_start, str, len) != 0)
	return FALSE;

    ch = xdp->xd_buf[xdp->xd_start + len];

    if (xo_xparse_is_bare_char(ch))
	return FALSE;

    return TRUE;
}

/*
 * Return the token type for the two character token given by
 * ch1 and ch2.  Returns zero if there is none.
 */
static int
xo_xparse_double_wide (xo_xparse_data_t *xdp UNUSED, uint8_t ch1, uint8_t ch2)
{
#define DOUBLE_WIDE(ch1, ch2) (((ch1) << 8) | (ch2))
    switch (DOUBLE_WIDE(ch1, ch2)) {
    case DOUBLE_WIDE(':', '='): return L_ASSIGN;
    case DOUBLE_WIDE('&', '&'): return L_DAMPER;
    case DOUBLE_WIDE(':', ':'): return L_DCOLON;
    case DOUBLE_WIDE('=', '='): return L_DEQUALS;
    case DOUBLE_WIDE('.', '.'): return L_DOTDOT;
    case DOUBLE_WIDE('/', '/'): return L_DSLASH;
    case DOUBLE_WIDE('|', '|'): return L_DVBAR;
    case DOUBLE_WIDE('>', '='): return L_GRTREQ;
    case DOUBLE_WIDE('<', '='): return L_LESSEQ;
    case DOUBLE_WIDE('!', '='): return L_NOTEQUALS;
    case DOUBLE_WIDE('+', '='): return L_PLUSEQUALS;
    }

    return 0;
}

/*
 * Return the token type for the triple character token given by
 * ch1, ch2 and ch3.  Returns zero if there is none.
 */
static int
xo_xparse_triple_wide (xo_xparse_data_t *xdp UNUSED, uint8_t ch1, uint8_t ch2, uint8_t ch3)
{
    if (ch1 == '.' && ch2 == '.' && ch3 == '.')
	return L_DOTDOTDOT;	/* Only one (for now) */

    return 0;
}

/*
 * Returns the token type for the keyword at the start of
 * the input buffer, or zero if there isn't one.
 *
 * Ignore XPath keywords if they are not allowed.  Same for SLAX keywords.
 * For node test tokens, we look ahead for the open paren before
 * returning the token type.
 *
 * (Should this use a hash or something?)
 */
static int
xo_xparse_keyword (xo_xparse_data_t *xdp)
{
    keyword_mapping_t *kmp;
    int ch;

    for (kmp = keywordMap; kmp->km_string; kmp++) {
	if (xo_xparse_keyword_match(xdp, kmp->km_string)) {
	    return kmp->km_ttype;

	    if (kmp->km_flags & KMF_NODE_TEST) {
		int look = xdp->xd_cur + strlen(kmp->km_string);

		for ( ; look < xdp->xd_len; look++) {
		    ch = xdp->xd_buf[look];
		    if (ch == '(')
			return kmp->km_ttype;
		    if (ch != ' ' && ch != '\t')
			break;
		}

		/* Didn't see the open paren, so it's not a node test */
	    }

	    return 0;
	}
    }

    return 0;
}

/*
 * We are working with simple strings, so a read means EOF.
 */
static int
xo_xparse_get_input (xo_xparse_data_t *xdp UNUSED, int final UNUSED)
{
    return TRUE;
}

/*
 * Move the current point by one character, getting more data if needed.
 */
static int
xo_xparse_move_cur (xo_xparse_data_t *xdp)
{
    xo_dbg(NULL, "xo_xplex: move:- %u/%u/%u", xdp->xd_start, xdp->xd_cur, xdp->xd_len);

    int moved;

    if (xdp->xd_cur < xdp->xd_len) {
	xdp->xd_cur += 1;
	moved = TRUE;
    } else moved = FALSE;

    if (xdp->xd_cur == xdp->xd_len) {
       if (xo_xparse_get_input(xdp, 0)) {
           xo_dbg(NULL, "xo_xplex: move:! %u/%u/%u",
                   xdp->xd_start, xdp->xd_cur, xdp->xd_len);
	   if (moved)
	       xdp->xd_cur -= 1;
           return -1;
       }
    }

    xo_dbg(NULL, "xo_xplex: move:+ %u/%u/%u", xdp->xd_start, xdp->xd_cur, xdp->xd_len);
    return 0;
}

static void
xo_xparse_warn_default (void *data, const char *fmt, va_list vap)
{
    xo_handle_t *xop = data;

    xo_warn_hcv(xop, -1, 0, fmt, vap);
}

static void
xo_xparse_warn (xo_xparse_data_t *xdp, const char *fmt, ...)
{
    xo_xpath_warn_func_t func = xdp->xd_warn_func ?: xo_xparse_warn_default;
    void *data = xdp->xd_warn_func ? xdp->xd_warn_data : xdp->xd_xop;
    va_list vap;

    va_start(vap, fmt);
    func(data, fmt, vap);
    va_end(vap);
}

/**
 * Issue an error if the axis name is not valid
 *
 * @param xdp main xplex data structure
 * @param axis name of the axis to check
 */
void
xo_xparse_check_axis_name (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    static const char *axis_names[] = {
	"ancestor",
	"ancestor-or-self",
	"attribute",
	"child",
	"descendant",
	"descendant-or-self",
	"following",
	"following-sibling",
	"namespace",
	"parent",
	"preceding",
	"preceding-sibling",
	"self",
	NULL
    };
    const char **namep;
    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);

    if (xnp == NULL)
	return;
    
    const char *str = xo_xparse_str(xdp, xnp->xn_str);
    if (str == NULL)
	return;

    /*
     * Fix the token type correctly, since sometimes these are parsed
     * as T_BARE.
     */
    xnp->xn_type = T_AXIS_NAME;

    for (namep = axis_names; *namep; namep++) {
	if (xo_streq(*namep, str))
	    return;
    }

    xo_xparse_warn(xdp, "%s:%u:%u unknown axis name: '%s'\n",
		   xdp->xd_filename, xdp->xd_line, xdp->xd_col, str);
}

xo_xparse_str_id_t
xo_xparse_str_new (xo_xparse_data_t *xdp UNUSED)
{
    xo_off_t len = xdp->xd_cur - xdp->xd_start;
    const char *start = xdp->xd_buf + xdp->xd_start;
    xo_buffer_t *xbp = &xdp->xd_str_buf;

    xo_off_t cur = xbp->xb_curp - xbp->xb_bufp;
    char *newp = xo_buf_append_val(xbp, start, len + 1);
    newp[len] = '\0';

    xdp->xd_last_str = cur;

    return newp ? cur : 0;
}

static void
xo_xparse_dump_one_node (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			 int indent, const char *title)
{
    if (id == 0)
	return;

    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);

    const char *str = xo_xparse_str(xdp, xnp->xn_str);
    xo_xparse_node_t *next = xo_xparse_node(xdp, xnp->xn_next);
    xo_xparse_node_t *prev = xo_xparse_node(xdp, xnp->xn_prev);

    printf("%*s%s%06ld/%p: type %u (%s), str %ld %p [%s], "
	   "contents %ld (%p), next %ld (%p)%s, prev %ld (%p)%s\n",
	   indent, "", title ?: "", id, xnp,
	   xnp->xn_type, xo_xparse_token_name(xnp->xn_type),
	   xnp->xn_str, str, str ?: "",
	   xnp->xn_contents, xo_xparse_node(xdp, xnp->xn_contents),
	   xnp->xn_next, next, (next && next->xn_prev != id) ? " BAD" : "",
	   xnp->xn_prev, prev, (prev && prev->xn_next != id) ? " BAD" : "");
}

static void
xo_xparse_dump_node (xo_xparse_data_t *xdp, xo_xparse_node_id_t id, int indent)
{
    xo_xparse_node_t *xnp;

    for ( ; id; id = xnp->xn_next) {
	xo_xparse_dump_one_node(xdp, id, indent, NULL);
	xnp = xo_xparse_node(xdp, id);
	if (xnp->xn_contents)
	    xo_xparse_dump_node(xdp, xnp->xn_contents, indent + 4);
    }
}

void
xo_xparse_dump (xo_xparse_data_t *xdp, xo_xparse_node_id_t id)
{
    xo_xparse_dump_node(xdp, id, 0);
}

static int
xo_xparse_feature_warn_one_node (const char *tag, xo_xparse_data_t *xdp UNUSED,
				 const int *map, int len,
				 xo_xparse_node_id_t id UNUSED,
				 xo_xparse_node_t *xnp)
{
    int type = xnp->xn_type;

    if (type > 0 && type < len && map[type]) {
	xo_xparse_warn(xdp, "%s%sxpath feature is unsupported: %s\n",
		       tag ?: "", tag ? ": " : "",
		       xo_xparse_fancy_token_name(type));
	return 1;
    }

    return 0;
}

static int
xo_xparse_feature_warn_node (const char *tag, xo_xparse_data_t *xdp,
			     const int *map, int len,
			     xo_xparse_node_id_t id)
{
    int hit = 0;
    xo_xparse_node_t *xnp;

    for ( ; id; id = xnp->xn_next) {
	xnp = xo_xparse_node(xdp, id);
	hit += xo_xparse_feature_warn_one_node(tag, xdp, map, len, id, xnp);
	if (xnp->xn_contents)
	    hit += xo_xparse_feature_warn_node(tag, xdp, map, len,
					       xnp->xn_contents);
    }

    return hit;
}

int
xo_xpath_feature_warn (const char *tag, xo_xparse_data_t *xdp,
		       const int *tokens, const char *bytes)
{
    if (xdp->xd_result == 0)
	return 0;

    int len = xo_xparse_num_tokens;
    int map[len];
    int i;

    bzero(map, len * sizeof(map[0]));

    if (tokens) {
	for (i = 0; tokens[i] && i < len; i++)
	    if (tokens[i])
		map[tokens[i]] = 1;
    }

    if (bytes) {
	for (i = 0; bytes[i] && i < len; i++)
	    if (bytes[i]) {
		int num = bytes[i];
		    int val = xo_single_wide[num];
		if (val >= 0 && val <= len)
		    map[val] = 1;
	    }
    }

    return xo_xparse_feature_warn_node(tag, xdp, map, len, xdp->xd_result);
}

int
xo_xparse_ternary_rewrite (xo_xparse_data_t *xdp UNUSED,
			   xo_xparse_node_id_t *d0 UNUSED,
			   xo_xparse_node_id_t *d1 UNUSED,
			   xo_xparse_node_id_t *d2 UNUSED,
			   xo_xparse_node_id_t *d3 UNUSED,
			   xo_xparse_node_id_t *d4 UNUSED,
			   xo_xparse_str_id_t *d5 UNUSED)
{
    return 0;
}

int
xo_xparse_concat_rewrite (xo_xparse_data_t *xdp UNUSED,
			  xo_xparse_node_id_t *d0 UNUSED,
			  xo_xparse_node_id_t *d1 UNUSED,
			  xo_xparse_node_id_t *d2 UNUSED,
			  xo_xparse_node_id_t *d3 UNUSED)
{
    return 0;
}

int
xo_xparse_yyval (xo_xparse_data_t *xdp UNUSED, xo_xparse_node_id_t id)
{
    xo_dbg(NULL, "xo_xparse_yyval: $$ = %ld", id);

    return id;
}

void
xo_xparse_node_set_next (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			xo_xparse_node_id_t value)
{
    xo_xparse_node_id_t next = 0;

    if (id) {
	xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);

	next = id;
	while (xnp->xn_next != 0) {
	    next = xnp->xn_next;
	    xnp = xo_xparse_node(xdp, next);
	}
	xnp->xn_next = value;

	if (value) {
	    xo_xparse_node_t *last = xo_xparse_node(xdp, value);
	    last->xn_prev = next;
	}
    }

    xo_dbg(NULL, "xo_xparse_node_set_next: id %ld, next %ld, value %ld",
	   id, next, value);
}

void
xo_xparse_node_set_contents (xo_xparse_data_t *xdp, xo_xparse_node_id_t id,
			xo_xparse_node_id_t value)
{
    xo_xparse_node_id_t next = 0;

    if (id) {
	xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
	if (xnp->xn_contents == 0) {
	    xnp->xn_contents = value;
	} else {
	    next = xnp->xn_contents;
	    xnp = xo_xparse_node(xdp, xnp->xn_contents);
	    while (xnp->xn_next != 0) {
		next = xnp->xn_next;
		xnp = xo_xparse_node(xdp, next);
	    }
	    xnp->xn_next = value;

	    if (value) {
		xo_xparse_node_t *last = xo_xparse_node(xdp, value);
		last->xn_prev = next;
	    }
	}
    }
}

/**
 * This function is the core of the lexer.
 *
 * We inspect the input buffer, finding the first token and returning
 * it's token type.
 *
 * @param xdp main slax data structure
 * @return token type
 */
static int
xo_xparse_lexer (xo_xparse_data_t *xdp)
{
    uint8_t ch1, ch2, ch3;
    int look, rc;

    /* Skip leading whitespace */
    while (xdp->xd_cur < xdp->xd_len	
	   && isspace((int) xdp->xd_buf[xdp->xd_cur])) {
	if (xdp->xd_buf[xdp->xd_cur] == '\n') {
	    xdp->xd_line += 1;
	    xdp->xd_col_start = xdp->xd_cur;
	}

	xdp->xd_cur += 1;
    }

    xdp->xd_col = xdp->xd_cur - xdp->xd_col_start;
    xdp->xd_start = xdp->xd_cur; /* Mark the start of the token */

    /* We're only parsing a string, so no data mean EOF */
    if (xdp->xd_cur == xdp->xd_len)
	return -1;
	
    ch1 = xdp->xd_buf[xdp->xd_cur];
    ch2 = (xdp->xd_cur + 1 < xdp->xd_len) ? xdp->xd_buf[xdp->xd_cur + 1] : 0;
    ch3 = (xdp->xd_cur + 2 < xdp->xd_len) ? xdp->xd_buf[xdp->xd_cur + 2] : 0;

#if 0
    /*
     * The rules for YANG are fairly simple.  But if we're
     * looking for a string argument, it's pretty easy.
     */
    if ((xdp->xd_flags & SDF_STRING)
		&& ch1 != '{' && ch1 != '}' && ch1 != ';' && ch1 != '+') {
	if (ch1 == '\'' || ch1 == '"') {
	    /*
	     * Found a quoted string.  Scan for the end.  We may
	     * need to read some more, if the string is long.
	     */
	    if (xo_xparse_move_cur(xdp))
		return -1;

	    for (;;) {
		if (xdp->xd_cur == xdp->xd_len)
		    if (xo_xparse_get_input(xdp, 0))
			return -1;

		if ((uint8_t) xdp->xd_buf[xdp->xd_cur] == ch1)
		    break;

		int bump = (xdp->xd_buf[xdp->xd_cur] == '\\') ? 1 : 0;

		if (xo_xparse_move_cur(xdp))
		    return -1;

#if 0
		if (bump && !xo_xparse_parse_is_xpath(xdp)
		    && xdp->xd_cur < xdp->xd_len)
		    xdp->xd_cur += bump;
#endif
	    }

	    if (xdp->xd_cur < xdp->xd_len)
		xdp->xd_cur += 1;	/* Move past the end quote */

	    return T_QUOTED;
	}
	
	for (xdp->xd_cur++; xdp->xd_cur < xdp->xd_len; xdp->xd_cur++) {
	    int ch = xdp->xd_buf[xdp->xd_cur];
	    if (isspace((int) ch) || ch == ';' || ch == '{' || ch == '}')
		break;
	}
	return (xdp->xd_buf[xdp->xd_start] == '$') ? T_VAR : T_BARE;
    }
#endif

    if (ch1 < XO_MAX_CHAR) {
	if (xo_triple_wide[ch1]) {
	    rc = xo_xparse_triple_wide(xdp, ch1, ch2, ch3);
	    if (rc) {
		xdp->xd_cur += 3;
		return rc;
	    }
	}

	if (xo_double_wide[ch1]) {
	    rc = xo_xparse_double_wide(xdp, ch1, ch2);
	    if (rc) {
		xdp->xd_cur += 2;
		return rc;
	    }
	}

	if (xo_single_wide[ch1]) {
	    int lit1 = xo_single_wide[ch1];
	    xdp->xd_cur += 1;

	    if (lit1 == L_STAR) {
		/*
		 * If we see a "*", we have to think about if it's a
		 * L_STAR or an L_ASTERISK.  If we put them both into
		 * the same literal, we get a shift-reduce error since
		 * there's an ambiguity between "/ * /" and "foo * foo".
		 * Is it a node test or the multiplier operator?
		 * To discriminate, we look at the last token returned.
		 */
		if (xdp->xd_last > M_MULTIPLICATION_TEST_LAST)
		    return L_STAR; /* It's the multiplcation operator */
		else
		    return L_ASTERISK; /* It's a q_name (NCName) */
	    }

            if (ch1 == '.' && isdigit((int) ch2)) {
                /* continue */
            } else if (lit1 == L_UNDERSCORE) {
                /*
                 * Underscore is a valid first character for an element
                 * name, which is troubling, since it's also the concatenation
                 * operator in SLAX.  We look ahead to see if the next
                 * character is a valid character before making our
                 * decision.
                 */
		if (!xo_xparse_is_bare_char(ch2))
		    return lit1;
#if 0
	    } else if (xdp->xd_parse == M_JSON
		       && (lit1 == L_PLUS || lit1 == L_MINUS)) {
		static const char digits[] = "0123456789.+-eE";
		unsigned char ch;

		rc = lit1;
		for ( ; xdp->xd_cur < xdp->xd_len; xdp->xd_cur++) {
		    ch = xdp->xd_buf[xdp->xd_cur];
		    if (strchr(digits, (int) ch) == NULL)
			return rc;
		    rc = T_NUMBER;
		}

		return rc;
#endif
	    } else {
		return lit1;
	    }
	}

	if (ch1 == '\'' || ch1 == '"') {
	    /*
	     * Found a quoted string.  Scan for the end.  We may
	     * need to read some more, if the string is long.
	     */
	    if (xo_xparse_move_cur(xdp)) /* Move past the first quote */
		return -1;

	    for (;;) {
		if (xdp->xd_cur == xdp->xd_len)
		    if (xo_xparse_get_input(xdp, 0))
			return -1;

		if ((uint8_t) xdp->xd_buf[xdp->xd_cur] == ch1)
		    break;

#if 0
		int bump = (xdp->xd_buf[xdp->xd_cur] == '\\') ? 1 : 0;
#endif

		if (xo_xparse_move_cur(xdp))
		    return -1;

#if 0
		if (bump && !xo_xparse_parse_is_xpath(xdp)
			&& xdp->xd_cur < xdp->xd_len)
		    xdp->xd_cur += bump;
#endif
	    }

	    if (xdp->xd_cur < xdp->xd_len)
		xdp->xd_cur += 1;	/* Move past the end quote */
	    return T_QUOTED;
	}

	if (ch1 == '$') {
	    /*
	     * Found a variable; scan for the end of it.
	     */
	    xdp->xd_cur += 1;
	    while (xdp->xd_cur < xdp->xd_len
		   && xo_xparse_is_var_char(xdp->xd_buf[xdp->xd_cur]))
		xdp->xd_cur += 1;
	    return T_VAR;
	}

	rc = xo_xparse_keyword(xdp);
	if (rc) {
	    xdp->xd_cur += strlen(xo_xparse_keyword_string[xo_xparse_token_translate(rc)]);
	    return rc;
	}

	if (isdigit(ch1) || (ch1 == '.' && isdigit(ch2))) {
	    int seen_e = FALSE;

	    for ( ; xdp->xd_cur < xdp->xd_len; xdp->xd_cur++) {
		int ch4 =  xdp->xd_buf[xdp->xd_cur];
		if (isdigit(ch4))
		    continue;
		if (ch4 == '.')
		    continue;
		if (ch4 == 'e' || ch4 == 'E') {
		    seen_e = TRUE;
		    continue;
		}
		if (seen_e && (ch4 == '+' || ch4 == '-'))
		    continue;
		break;		/* Otherwise, we're done */
	    }
	    return T_NUMBER;
	}
    }

#if 0
    /*
     * The rules for JSON are a bit simpler.  T_BARE can't contain
     * colons and we don't need the fancy exceptions below.
     */
    if (xdp->xd_parse == M_JSON) {
	for ( ; xdp->xd_cur < xdp->xd_len; xdp->xd_cur++) {
	    int ch = xdp->xd_buf[xdp->xd_cur];
	    if (!isalnum((int) ch) && ch != '_')
		break;
	}
	return T_BARE;
    }
#endif

    /*
     * Must have found a bare word or a function name, since they
     * are the only things left.  We scan forward for the next
     * character that doesn't fit in a T_BARE, but allow "foo:*"
     * as a special case.
     */
    for ( ; xdp->xd_cur < xdp->xd_len; xdp->xd_cur++) {
	if (xdp->xd_cur + 1 < xdp->xd_len && xdp->xd_buf[xdp->xd_cur] == ':'
		&& xdp->xd_buf[xdp->xd_cur + 1] == ':')
	    return T_AXIS_NAME;
	if (xo_xparse_is_bare_char(xdp->xd_buf[xdp->xd_cur]))
	    continue;
	if (xdp->xd_cur > xdp->xd_start && xdp->xd_buf[xdp->xd_cur] == '*'
		&& xdp->xd_buf[xdp->xd_cur - 1] == ':')
	    continue;
	break;
    }

    /*
     * It's a hack, but it's a standard-specified hack:
     * We need to see if this is a function name (T_FUNCTION_NAME)
     * or an NCName (q_name) (T_BARE).
     * So we look ahead for a '('.  If we find one, it's a function;
     * if not it's a q_name.
     */
    for (look = xdp->xd_cur; look < xdp->xd_len; look++) {
	ch1 = xdp->xd_buf[look];
	if (ch1 == '(')
	    return T_FUNCTION_NAME;
	if (!isspace(ch1))
	    break;
    }

    if (xdp->xd_cur == xdp->xd_start && xdp->xd_buf[xdp->xd_cur] == '#') {
	/*
	 * There's a special token "#default" that's used for
	 * namespace-alias.  It's an absurd hack, but we
	 * have to dummy it up as a T_BARE.
	 */
	static const char pdef[] = "#default";
	static const int plen = sizeof(pdef) - 1;
	if (xdp->xd_len - xdp->xd_cur > plen
	    && memcmp(xdp->xd_buf + xdp->xd_cur,
		      pdef, plen) == 0
	    && !xo_xparse_is_bare_char(xdp->xd_buf[xdp->xd_cur + plen])) {
	    xdp->xd_cur += sizeof(pdef) - 1;
	}
    }

    return T_BARE;
}

/**
 * Lexer function called from bison's yyparse()
 *
 * We lexically analyze the input and return the token type to
 * bison's yyparse function, which we've renamed to xo_xpath_parse.
 *
 * @param xdp main xpath data structure
 * @param yylvalp pointer to bison's lval (stack value)
 * @return token type
 */
int
xo_xpath_yylex (xo_xparse_data_t *xdp, xo_xparse_node_id_t *yylvalp)
{
    int rc, look;

    if (!xo_xparse_setup)
	xo_xparse_setup_lexer();

    xo_xparse_node_id_t id = xo_xparse_node_new(xdp);
    xo_xparse_node_t *xnp = xo_xparse_node(xdp, id);
    *yylvalp = id;

    bzero(xnp, sizeof(*xnp));

    /*
     * If we've saved a token type into xd_ttype, then we return
     * it as if we'd just lexed it.
     */
    if (xdp->xd_ttype) {
	rc = xdp->xd_ttype;
	xdp->xd_ttype = 0;

	xnp->xn_type = rc;

	return rc;
    }

#if 0
    /* Add the record flags to the current set */
    xdp->xd_flags |= xdp->xd_flags_next;
    xdp->xd_flags_next = 0;
#endif

    /*
     * Discard the previous token by moving the start pointer
     * past it.
     */
    xdp->xd_start = xdp->xd_cur;

    rc = xo_xparse_lexer(xdp);
    if (M_OPERATOR_FIRST < rc && rc < M_OPERATOR_LAST
	&& xdp->xd_last < M_MULTIPLICATION_TEST_LAST)
	rc = T_BARE;

    /*
     * It's a hack, but it's a standard-specified hack: We need to see
     * if this is a function name (T_FUNCTION_NAME) or an NCName
     * (q_name) (T_BARE).  So we look ahead for a '('.  If we find
     * one, it's a function; if not it's an T_BARE;
     */
    if (rc == T_BARE /* && xdp->xd_parse != M_JSON*/) {
	for (look = xdp->xd_cur; look < xdp->xd_len; look++) {
	    unsigned char ch = xdp->xd_buf[look];
	    if (ch == '(') {
		rc = T_FUNCTION_NAME;
		break;
	    }

	    if (!isspace((int) ch))
		break;
	}
    }

    /*
     * Save the token type in xd_last so we can do these same
     * hacks next time thru
     */
    xdp->xd_last = rc;

    if (rc > 0 && xdp->xd_start == xdp->xd_cur) {
	xo_dbg(xdp->xd_xop, "%s:%u:%u: xpath: lex: zero length token: %d/%s",
	       xdp->xd_filename, xdp->xd_line, xdp->xd_col,
	       rc, xo_xparse_token_name(rc));
	rc = M_ERROR;

	/*
	 * We're attempting to return a reasonable token type, but
	 * with a zero length token.  Something it very busted with
	 * the input.  We can't just sit here, but, well, there are
	 * no good options.  We're going to move the current point
	 * forward in the hope that we'll see good input eventually.
	 */
	if (xdp->xd_cur < xdp->xd_len)
	    xdp->xd_cur += 1;
    }

    xnp->xn_type = rc;
    if (rc > 0)
	xnp->xn_str = xo_xparse_str_new(xdp);

    xo_dbg(NULL, "xo_xplex: lex: %p '%.*s' -> %d/%s %s",
	   xnp, xdp->xd_cur - xdp->xd_start,
	   xdp->xd_buf + xdp->xd_start,
	   rc, (rc > 0) ? xo_xparse_token_name(rc) : "",
	   xnp->xn_str ? xo_xparse_str(xdp, xnp->xn_str) : "");

    xo_xparse_dump_one_node(xdp, id, 0, "yylex:: ");

    return rc;
}

/*
 * Return a better class of error message
 * @returns freshly allocated string containing error message
 */
static char *
xo_xparse_syntax_error (xo_xparse_data_t *xdp UNUSED, const char *token,
		       int yystate, int yychar)
{
    char buf[BUFSIZ], *cp = buf, *ep = buf + sizeof(buf);

    /*
     * If yystate is 1, then we're in our initial state and have
     * not seen any valid input.  We handle this state specifically
     * to give better error messages.
     */
    if (yystate == 1) {
	if (yychar == -1)
	    return strdup("unexpected end-of-file found (empty input)");

	if (yychar == L_LESS)
	    return strdup("unexpected '<'; file may be XML/XSLT");

	SNPRINTF(cp, ep, "missing 'version' statement");
	if (token)
	    SNPRINTF(cp, ep, "; %s is not legal", token);

    } else if (yychar == -1) {
	SNPRINTF(cp, ep, "unexpected end-of-expression");

#if 0
	nodep = slaxFindOpenNode(xdp);
	if (nodep) {
	    int lineno = xmlGetLineNo(nodep);

	    if (lineno > 0) {
		if (nodep->ns && nodep->ns->href
			&& streq((const char *)nodep->ns->href, XSL_URI)) {
		    SNPRINTF(cp, ep,
			     "; unterminated statement (<xsl:%s>) on line %d",
			     nodep->name, lineno);
		} else {
		    SNPRINTF(cp, ep, "; open element (<%s>) on line %d",
			     nodep->name, lineno);
		}
	    }
	}
#endif

    } else {
#if 1
	char *msg = xo_xparse_expecting_error(token, yystate, yychar);
	if (msg)
	    return msg;
#endif

	SNPRINTF(cp, ep, "unexpected input");
	if (token)
	    SNPRINTF(cp, ep, ": %s", token);
    }

    return strdup(buf);
}

/**
 * Callback from yacc when an error is detected.
 *
 * @param xdp main slax data structure
 * @param str error message
 * @param value stack entry from bison's lexical stack
 * @return zero
 */
int
xo_xpath_yyerror (xo_xparse_data_t *xdp, const char *str, int yystate)
{
#ifdef HAVE_BISON
    static const char leader[] = "syntax error, unexpected";
#else /* HAVE_BISON */
    static const char leader[] = "syntax error";
#endif /* HAVE_BISON */
    
    static const char leader2[] = "error recovery ignores input";
    const char *token;
    char buf[BUFSIZ * 4];

    if (strncmp(str, leader2, sizeof(leader2) - 1) != 0)
	xdp->xd_errors += 1;

    token = xo_xparse_token_name_fancy[xo_xparse_token_translate(xdp->xd_last)];

    buf[0] = '\0';

    /*
     * Two possibilities: generic "syntax error" or some
     * specific error.  If the message has a generic
     * prefix, use our logic instead.  This avoids tossing
     * token names (K_VERSION) at the user.
     */
    if (strncmp(str, leader, sizeof(leader) - 1) == 0) {
	char *msg = xo_xparse_syntax_error(xdp, token, yystate, xdp->xd_last);

	if (msg) {
	    xo_xparse_warn(xdp, "%s:%u:%u: %s%s\n",
			   xdp->xd_filename, xdp->xd_line, xdp->xd_col,
			   msg, buf);
	    xo_free(msg);
	    return 0;
	}
    }

    xo_xparse_warn(xdp, "%s:%u:%u: %s%s%s%s%s\n",
		   xdp->xd_filename, xdp->xd_line, xdp->xd_col, str,
		   token ? " before " : "", token, token ? ": " : "", buf);

    return 0;
}

void
xo_xparse_init (xo_xparse_data_t *xdp)
{
    bzero(xdp, sizeof(*xdp));
    xdp->xd_line = 1;

    xo_buf_append_val(&xdp->xd_str_buf, "@EOF", 1);
}

void
xo_xparse_clean (xo_xparse_data_t *xdp)
{
    if (xdp) {
	xo_buf_cleanup(&xdp->xd_node_buf);
	xo_buf_cleanup(&xdp->xd_str_buf);
	xo_free(xdp->xd_buf);
    }
}

#ifdef TEST_XPLEX
int
main (int argc, char **argv)
{
    argc = xo_parse_args(argc, argv);
    if (argc < 0)
        return 1;

    if (argc == 0)
	return 2;

    xo_xparse_data_t xd;
    xo_xparse_node_id_t id;
    xo_xparse_node_t *xnp;

    xo_xparse_init(&xd);

    strncpy(xd.xd_filename, "me", sizeof(xd.xd_filename));
    xd.xd_buf = strdup(argv[1]);
    xd.xd_len = strlen(xd.xd_buf);

    for (;;) {
	int rc = xo_xpath_yylex(&xd, &id);
	if (rc <= 0)
	    break;

	xnp = xo_xparse_node(&xd, id);
	if (xnp) {
	    xo_dbg(NULL, "parse: type: %u (%p), str: %lu (%p), "
		   "left: %lu (%p), right %lu (%p)",
		   xnp->xn_type,
		   xnp->xn_str, xo_xparse_str(&xd, xnp->xn_str),
		   xnp->xn_contents, xo_xparse_node(&xd, xnp->xn_contents),
		   xnp->xn_next, xo_xparse_node(&xd, xnp->xn_next));
	}
    }

    return 0;
}

#endif /* TEST_XPLEX */

#ifdef TEST_XPATH
int
main (int argc, char **argv)
{
    argc = xo_parse_args(argc, argv);
    if (argc < 0)
        return 1;

    for (argc = 1; argv[argc] && argv[argc][0] == '-'; argc++) {
	if (xo_streq(argv[argc], "--debug"))
	    xo_set_flags(NULL, XOF_DEBUG);
	else if (xo_streq(argv[argc], "--yydebug"))
	    xo_xpath_yydebug = 1;
    }

    if (argc == 0)
	return 2;

    xo_xparse_data_t xd;
    xo_xparse_node_t *xnp UNUSED;

    xo_xparse_init(&xd);

    strncpy(xd.xd_filename, "test", sizeof(xd.xd_filename));
    xd.xd_buf = strdup(argv[1]);
    xd.xd_len = strlen(xd.xd_buf);

    for (;;) {
	int rc = xo_xpath_yyparse(&xd);
	if (rc <= 0)
	    break;
	break;
    }

    xo_xparse_dump(&xd, xd.xd_result);

    int bad_horse[] = { C_DESCENDANT, 0 };

    xo_xpath_feature_warn("test", &xd, bad_horse, "+");

    xo_xparse_clean(&xd);

    return 0;
}

#endif /* TEST_XPATH */
