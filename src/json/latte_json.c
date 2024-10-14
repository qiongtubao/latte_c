#include "latte_json.h"
#include "dict/dict_plugins.h"
// #include "json.h"
#include "utils/utils.h"
#include <errno.h>
#include <string.h>
#include "log/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
// map
dict_func_t jsonDictTyep  = {
    dict_char_hash,
    NULL,
    NULL,
    dict_char_key_compare,
    dict_sds_destructor,
    NULL,
    NULL
};
json_t* json_map_new() {
    json_t* r = value_new();
    dict_t* d = dict_new(&jsonDictTyep);
    value_set_map(r, d);
    return r;
}


int json_map_put_value(json_t* root, sds_t key, json_t* v) {
    if (!value_is_map(root)) return 0;
    dict_entry_t* node = dict_find(root->value.map_value, key);
    if (node != NULL) {
        json_t* old_v = (json_t*)dict_get_val(node);
        value_delete(old_v);
        dict_set_val(root->value.map_value, node, v);
        return 1;
    } else {
        return dict_add(root->value.map_value, key, v);
    }
}


int json_map_put_sds(json_t* root, sds_t key, sds_t sv) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_sds(v, sv);
    return json_map_put_value(root, key, v);
}

int json_map_put_int64(json_t* root, sds_t key, int64_t ll) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_int64(v, ll);
    return json_map_put_value(root, key, v);
}

int json_map_put_bool(json_t* root, sds_t key, bool b) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_bool(v, b);
    return json_map_put_value(root, key, v);
}

int json_map_put_longdouble(json_t* root, sds_t key, long double ld) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_longdouble(v, ld);
    return json_map_put_value(root, key, v);
}

json_t* json_map_get_value(json_t* root, char* key) {
    latte_assert(value_is_map(root), "json is not map!!!");
    dict_entry_t* entry = dict_find(root->value.map_value, key);
    if (entry == NULL) return NULL;
    return dict_get_val(entry);
}

// list 

json_t* json_array_new() {
    json_t* r = value_new();
    vector_t* d = vector_new();
    value_set_array(r, d);
    return r;
}



void json_array_resize(json_t* root, int size) {
    if (!value_is_array(root)) return;
    vector_resize(value_get_array(root), size);
}



int json_array_put_sds(json_t* root, sds_t element) {
    if (!value_is_array(root)) return 0;
    json_t* v = value_new();
    value_set_sds(v, element);
    vector_push(value_get_array(root), v);
    return 1;
}

int json_array_put_bool(json_t* root, bool element) {
    if (!value_is_array(root)) return 0;
    json_t* v = value_new();
    value_set_bool(v, element);
    return vector_push(value_get_array(root), v);
}


int json_array_put_int64(json_t* root, int64_t element) {
    if (!value_is_array(root)) return 0;
    json_t* v = value_new();
    value_set_int64(v, element);
    return vector_push(value_get_array(root), v);
}

int json_array_put_longdouble(json_t* root, long double element) {
    if (!value_is_array(root)) return 0;
    json_t* v = value_new();
    value_set_longdouble(v, element);
    return vector_push(value_get_array(root), v);
}

int json_array_put_value(json_t* root, json_t* element) {
    if (!value_is_array(root)) return 0;
    return vector_push(value_get_array(root), element);
}

int json_array_shrink(json_t* root, int max) {
    if (!value_is_array(root)) return 0;
    return vector_shrink(value_get_array(root), max);
}

static void json_tokener_reset_level(struct json_tokener_t *tok, int depth)
{
    json_tokener_srec_t* srec = vector_get(tok->stack, depth);
    srec->state = json_tokener_state_eatws;
	// tok->stack[depth].state = json_tokener_state_eatws;
    srec->saved_state = json_tokener_state_start;
	// tok->stack[depth].saved_state = json_tokener_state_start;
	// json_object_put(tok->stack[depth].current);
	// tok->stack[depth].current = NULL;
    srec->current = NULL;
	sds_delete(srec->obj_field_name);
	srec->obj_field_name = NULL;
}

void jsonTokenerReset(json_tokener_t* tok) {
    int i;
    if (!tok) return;
    for (i = tok->depth; i >= 0; i--)
        json_tokener_reset_level(tok, i);
    tok->depth = 0;
    tok->err = json_tokener_success;
}

//decode 


static const char *json_tokener_errors[] = {
	"success",
	"continue",
	"nesting too deep",
	"unexpected end of data",
	"unexpected character",
	"null expected",
	"boolean expected",
	"number expected",
	"array value separator ',' expected",
	"quoted object property name expected",
	"object property name separator ':' expected",
	"object value separator ',' expected",
	"invalid string sequence",
	"expected comment",
	"invalid utf-8 string",
	"buffer size overflow",
	"out of memory"
};
json_printbuf_t* json_printbuf_new() {
    json_printbuf_t* buf = zmalloc(sizeof(json_printbuf_t));
    if (buf == NULL) return NULL;
    buf->size = 32;
    buf->bpos = 0;
    if (!(buf->buf = zmalloc(buf->size))) {
        zfree(buf);
        return NULL;
    }
    buf->buf[0] = '\0';
    return buf;
}
void json_printbuf_delete(json_printbuf_t* buf) {
    zfree(buf->buf);
    zfree(buf);
}

/**
 * 
 *  -1 创建对象失败
 *  0 不处理
 *  1 处理成功
*/
json_tokener_srec_t* jsonTokenerSrecCreate() {
    json_tokener_srec_t* srec = zmalloc(sizeof(json_tokener_srec_t));
    return srec;
}
int expandTokenerStack(vector_t* v, size_t size) {
    if (size < v->count) return 0;
    vector_resize(v, size);
    
    while(v->count < size) {
        json_tokener_srec_t* srec = jsonTokenerSrecCreate();
        if (srec == NULL) return -1;
        vector_push(v, srec);
    }
    return 1;
}



json_tokener_t* json_tokener_newSize(size_t max_depth) {
    json_tokener_t* tokener = zmalloc(sizeof(json_tokener_t));
    if (!tokener) {
        return NULL;
    }
    
    tokener->stack = vector_new();
    if (!tokener->stack) {
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_new] vector_new fail");
        return NULL;
    }

    if (expandTokenerStack(tokener->stack, max_depth) < 0) {
        vector_delete(tokener->stack);
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_new] expandTokenerStack szie=32 fail");
        return NULL;
    }
    tokener->max_depth = max_depth;

    tokener->pb = json_printbuf_new();
    if (!tokener->pb) {
        vector_delete(tokener->stack);
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_new] json_printbuf_new fail");
        return NULL;
    } 
    //init all tokener->stack
    tokener->depth = max_depth - 1;
    jsonTokenerReset(tokener);
    return tokener;
}

json_tokener_t* json_tokener_new() {
    return json_tokener_newSize(32);
}
void json_tokener_delete(json_tokener_t* tokener) {
    vector_delete(tokener->stack);
    zfree(tokener);
}



static int json_tokener_validate_utf8(const char c, unsigned int *nBytes)
{
	unsigned char chr = c;
	if (*nBytes == 0)
	{
		if (chr >= 0x80)
		{
			if ((chr & 0xe0) == 0xc0)
				*nBytes = 1;
			else if ((chr & 0xf0) == 0xe0)
				*nBytes = 2;
			else if ((chr & 0xf8) == 0xf0)
				*nBytes = 3;
			else
				return 0;
		}
	}
	else
	{
		if ((chr & 0xC0) != 0x80)
			return 0;
		(*nBytes)--;
	}
	return 1;
}

json_tokener_state_enum json_tokener_get_state(json_tokener_t* tok) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    return srec->state;
}

void json_tokener_set_state(json_tokener_t* tok, json_tokener_state_enum state) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    srec->state = state;
}
json_tokener_state_enum json_tokener_get_saved_state(json_tokener_t* tok) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    return srec->saved_state;
}

void json_tokener_set_saved_state(json_tokener_t* tok, json_tokener_state_enum state) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    srec->saved_state = state;
}

json_t* json_tokener_get_current(json_tokener_t* tok) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    return srec->current;
}

void json_tokener_set_current(json_tokener_t* tok, json_t* value) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    srec->current = value;
}

sds_t json_tokener_get_fieldname(json_tokener_t* tok) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    return srec->obj_field_name;
}

void json_tokener_set_fieldname(json_tokener_t* tok, sds_t value) {
    json_tokener_srec_t* srec = vector_get(tok->stack, tok->depth);
    srec->obj_field_name = value;
}
// #define state tok->stack[tok->depth].state
// #define saved_state tok->stack[tok->depth].saved_state
// #define current tok->stack[tok->depth].current
// #define obj_field_name tok->stack[tok->depth].obj_field_name

/* The following helper functions are used to speed up parsing. They
 * are faster than their ctype counterparts because they assume that
 * the input is in ASCII and that the locale is set to "C". The
 * compiler will also inline these functions, providing an additional
 * speedup by saving on function calls.
 */
static inline int is_ws_char(char c)
{
	return c == ' '
	    || c == '\t'
	    || c == '\n'
	    || c == '\r';
}

static inline int is_hex_char(char c)
{
	return (c >= '0' && c <= '9')
	    || (c >= 'A' && c <= 'F')
	    || (c >= 'a' && c <= 'f');
}


/* PEEK_CHAR(dest, tok) macro:
 *   Peeks at the current char and stores it in dest.
 *   Returns 1 on success, sets tok->err and returns 0 if no more chars.
 *   Implicit inputs:  str, len, nBytesp vars
 */
#define PEEK_CHAR(dest, tok)                                                 \
	(((tok)->char_offset == len)                                         \
	     ? (((tok)->depth == 0 && json_tokener_get_state(tok) == json_tokener_state_eatws &&   \
	         json_tokener_get_saved_state(tok) == json_tokener_state_finish)                   \
	            ? (((tok)->err = json_tokener_success), 0)               \
	            : (((tok)->err = json_tokener_continue), 0))             \
	     : (((tok->flags & JSON_TOKENER_VALIDATE_UTF8) &&                \
	         (!json_tokener_validate_utf8(*str, nBytesp)))               \
	            ? ((tok->err = json_tokener_error_parse_utf8_string), 0) \
	            : (((dest) = *str), 1)))


/**
 * Extend the buffer p so it has a size of at least min_size.
 *
 * If the current size is large enough, nothing is changed.
 *
 * If extension failed, errno is set to indicate the error.
 *
 * Note: this does not check the available space!  The caller
 *  is responsible for performing those calculations.
 */
static int json_printbuf_extend(struct json_printbuf_t *p, int min_size)
{
	char *t;
	int new_size;

	if (p->size >= min_size)
		return 0;
	/* Prevent signed integer overflows with large buffers. */
	if (min_size > INT_MAX - 8)
	{
		errno = EFBIG;
		return -1;
	}
	if (p->size > INT_MAX / 2)
		new_size = min_size + 8;
	else {
		new_size = p->size * 2;
		if (new_size < min_size + 8)
			new_size = min_size + 8;
	}
#ifdef PRINTBUF_DEBUG
	LATTE_LIB_LOG(LOG_DEBUG,"json_printbuf_extend: realloc "
	         "bpos=%d min_size=%d old_size=%d new_size=%d\n",
	         p->bpos, min_size, p->size, new_size);
#endif /* PRINTBUF_DEBUG */
	if (!(t = (char *)realloc(p->buf, new_size)))
		return -1;
	p->size = new_size;
	p->buf = t;
	return 0;
}

int json_printbuf_memappend(struct json_printbuf_t *p, const char *buf, int size)
{
	/* Prevent signed integer overflows with large buffers. */
	if (size < 0 || size > INT_MAX - p->bpos - 1)
	{
		errno = EFBIG;
		return -1;
	}
	if (p->size <= p->bpos + size + 1)
	{
		if (json_printbuf_extend(p, p->bpos + size + 1) < 0)
			return -1;
	}
	memcpy(p->buf + p->bpos, buf, size);
	p->bpos += size;
	p->buf[p->bpos] = '\0';
	return size;
}
/* json_printbuf_memappend_checked(p, s, l) macro:
 *   Add string s of length l to printbuffer p.
 *   If operation fails abort parse operation with memory error.
 */
#define json_printbuf_memappend_checked(p, s, l)                   \
	do {                                                  \
		if (json_printbuf_memappend((p), (s), (l)) < 0)    \
		{                                             \
			tok->err = json_tokener_error_memory; \
			goto out;                             \
		}                                             \
	} while (0)

void printbuf_reset(struct json_printbuf_t *p)
{
	p->buf[0] = '\0';
	p->bpos = 0;
}
/* ADVANCE_CHAR() macro:
 *   Increments str & tok->char_offset.
 *   For convenience of existing conditionals, returns the old value of c (0 on eof).
 *   Implicit inputs:  c var
 */
#define ADVANCE_CHAR(str, tok) (++(str), ((tok)->char_offset)++, c)




static const char json_null_str[] = "null";
static const int json_null_str_len = sizeof(json_null_str) - 1;
static const char json_inf_str[] = "Infinity";
/* Swapped case "Infinity" to avoid need to call tolower() on input chars: */
static const char json_inf_str_invert[] = "iNFINITY";
static const unsigned int json_inf_str_len = sizeof(json_inf_str) - 1;
static const char json_nan_str[] = "NaN";
static const int json_nan_str_len = sizeof(json_nan_str) - 1;
static const char json_true_str[] = "true";
static const int json_true_str_len = sizeof(json_true_str) - 1;
static const char json_false_str[] = "false";
static const int json_false_str_len = sizeof(json_false_str) - 1;
#define jt_hexdigit(x) (((x) <= '9') ? (x) - '0' : ((x)&7) + 9)


/* Stuff for decoding unicode sequences */
#define IS_HIGH_SURROGATE(uc) (((uc)&0xFC00) == 0xD800)
#define IS_LOW_SURROGATE(uc) (((uc)&0xFC00) == 0xDC00)
#define DECODE_SURROGATE_PAIR(hi, lo) ((((hi)&0x3FF) << 10) + ((lo)&0x3FF) + 0x10000)
static unsigned char utf8_replacement_char[3] = {0xEF, 0xBF, 0xBD};

#define printbuf_length(p) ((p)->bpos)

int sds_to_json_verbose(char* str, int len, json_t** result, json_tokener_t* tok) {
    
    json_t* obj = NULL;
    char c = '\1';
	unsigned int nBytes = 0;
	unsigned int *nBytesp = &nBytes;
    tok->char_offset = 0;
    tok->err = json_tokener_success;
    /* this interface is presently not 64-bit clean due to the int len argument
	 * and the internal printbuf interface that takes 32-bit int len arguments
	 * so the function limits the maximum string size to INT32_MAX (2GB).
	 * If the function is called with len == -1 then strlen is called to check
	 * the string length is less than INT32_MAX (2GB)
	 */
    if ((len < -1) || (len == -1 && strlen(str) > INT32_MAX))
	{
		tok->err = json_tokener_error_size;
		return 0;
	}
    LATTE_LIB_LOG(LOG_DEBUG, "[sds_to_json_verbose] parse start");
    while (PEEK_CHAR(c, tok)) {
        redo_char:
            switch (json_tokener_get_state(tok)) 
            {
                case json_tokener_state_eatws: //去空格  以及判断是否开始注释
                { 
                    // LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_eatws] start");
                    while (is_ws_char(c)) 
                    {
                        if ((!ADVANCE_CHAR(str, tok)) || (!PEEK_CHAR(c, tok))) {
                            goto out;
                        }
                    }
                    if (c == '/' && !(tok->flags & JSON_TOKENER_STRICT))
                    {
                        printbuf_reset(tok->pb);
                        json_printbuf_memappend_checked(tok->pb, &c, 1);
                        json_tokener_set_state(tok, json_tokener_state_comment_start); //注释
                    } 
                    else 
                    {
                        json_tokener_set_state(tok, json_tokener_get_saved_state(tok));
                        goto redo_char;
                    }
                }
                    break;
                case json_tokener_state_start:  //开始解析
                {  
                    LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_start] start");
                    switch (c)
                    {
                    case '{': //map
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_start] map start '{' ");
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                        //标记状态map
                        json_tokener_set_saved_state(tok, json_tokener_state_object_field_start);
                        json_tokener_set_current(tok, json_map_new());
                        if (json_tokener_get_current(tok) == NULL) { //申请内存失败
                            tok->err = json_tokener_error_memory;
                            goto out;
                        }
                        break;
                    case '[':
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                        //标记状态list
                        json_tokener_set_saved_state(tok, json_tokener_state_array);
                        json_tokener_set_current(tok, json_array_new());
                        if (json_tokener_get_current(tok) == NULL) { //申请内存失败
                            tok->err = json_tokener_error_memory;
                            goto out;
                        }
                        break;
                    case 'I':
                    case 'i': //inf 
                        json_tokener_set_state(tok, json_tokener_state_inf);
                        printbuf_reset(tok->pb);
                        tok->st_pos = 0;
                        goto redo_char;
                    case 'N':
                    case 'n': //null or nan
                        json_tokener_set_state(tok, json_tokener_state_null);
                        printbuf_reset(tok->pb);
				        tok->st_pos = 0;
                        goto redo_char;
                    case '\'':
                        if (tok->flags & JSON_TOKENER_STRICT) { //严格模式
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                    case '"':
                        json_tokener_set_state(tok, json_tokener_state_string);
                        printbuf_reset(tok->pb);
				        tok->quote_char = c;
                        break;
                    case 'T':
                    case 't':
                    case 'F':
                    case 'f':
                        json_tokener_set_state(tok, json_tokener_state_boolean); //解析true or false
                        printbuf_reset(tok->pb);
                        tok->st_pos = 0;
                        goto redo_char;
                    case '0':
			        case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '-':   
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_start] parse number");
                        json_tokener_set_state(tok, json_tokener_state_number); //解析数字
                        printbuf_reset(tok->pb);
                        tok->is_double = 0;
                        goto redo_char;
                    default: tok->err = json_tokener_error_parse_unexpected; goto out;
                        break;
                    }
                }
                    break;
                case json_tokener_state_finish:  //解析完成
                {

                    LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_finish] start %d", tok->depth);
                    if (tok->depth == 0) goto out;
                    obj = json_tokener_get_current(tok);
                    json_tokener_reset_level(tok, tok->depth);
                    tok->depth--;
                    goto redo_char;
                } 
                    break;
                case json_tokener_state_inf: /* aka starts with 'i' (or 'I', or "-i", or "-I") */  // 解析inf -inf
                {
                    /* If we were guaranteed to have len set, then we could (usually) handle
                    * the entire "Infinity" check in a single strncmp (strncasecmp), but
                    * since len might be -1 (i.e. "read until \0"), we need to check it
                    * a character at a time.
                    * Trying to handle it both ways would make this code considerably more
                    * complicated with likely little performance benefit.
                    */
                    int is_negative = 0;
                    /* Note: tok->st_pos must be 0 when state is set to json_tokener_state_inf */
                    while (tok->st_pos < (int)json_inf_str_len)
                    {
                        char inf_char = *str;
                        if (inf_char != json_inf_str[tok->st_pos] &&
                            ((tok->flags & JSON_TOKENER_STRICT) ||
                            inf_char != json_inf_str_invert[tok->st_pos])
                        )
                        {
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                        tok->st_pos++; 
                        (void)ADVANCE_CHAR(str, tok); //字符推进下一个
                        if (!PEEK_CHAR(c, tok))
                        {
                            /* out of input chars, for now at least */
                            goto out;
                        }
                        /* We checked the full length of "Infinity", so create the object.
                        * When handling -Infinity, the number parsing code will have dropped
                        * the "-" into tok->pb for us, so check it now.
                        */
                        if (printbuf_length(tok->pb) > 0 && *(tok->pb->buf) == '-')
                        {
                            is_negative = 1;
                        }
                        json_t* current = value_new();
                        if (current == NULL)
                        {
                            tok->err = json_tokener_error_memory;
                            goto out;
                        }
                        value_set_longdouble(current, is_negative ? -INFINITY: INFINITY);
                        json_tokener_set_current(tok, current);
                        // current = json_object_new_double(is_negative ? -INFINITY : INFINITY);
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                        
                        // saved_state = json_tokener_state_finish;
                        // state = json_tokener_state_eatws;
                        goto redo_char;
                    }
                }
                    break;
                case json_tokener_state_null:  //解析null 或者 nan
                {
                    int size;
                    int size_nan;
                    json_printbuf_memappend_checked(tok->pb, &c, 1);
                    size = latte_min(tok->st_pos + 1, json_null_str_len);
                    size_nan = latte_min(tok->st_pos + 1, json_nan_str_len);
                    if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                        strncasecmp(json_null_str, (const char*)tok->pb, size) == 0) ||
                        (strncmp(json_null_str, (const char*)tok->pb, size) == 0)) //对比null
                    {
                        if (tok->st_pos == json_null_str_len)
                        {
                            json_tokener_set_current(tok, NULL);
                            json_tokener_set_saved_state(tok, json_tokener_state_finish);
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            goto redo_char;
                        }
                    }
                    else if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                            strncasecmp(json_nan_str, tok->pb->buf, size_nan) == 0) ||
                            (strncmp(json_nan_str, tok->pb->buf, size_nan) == 0)) //对比nan
                    {
                        if (tok->st_pos == json_nan_str_len)
                        {
                            // current = json_object_new_double(NAN);
                            json_t* current = value_new();
                            
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            value_set_longdouble(current, NAN);
                            json_tokener_set_current(tok, current);
                            json_tokener_set_saved_state(tok, json_tokener_state_finish);
                            // saved_state = json_tokener_state_finish;
                            // state = json_tokener_state_eatws;
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            goto redo_char;
                        }
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_null;
                        goto out;
                    }
                    tok->st_pos++;
                }
                    break;
                case json_tokener_state_comment_start: //判断是否开始注释
                {
                    if (c == '*')
                    {
                        json_tokener_set_state(tok, json_tokener_state_comment);
                    }
                    else if (c == '/')
                    {
                        json_tokener_set_state(tok, json_tokener_state_comment_eol);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_comment;
                        goto out;
                    }
                    json_printbuf_memappend_checked(tok->pb, &c, 1);
                }
                    break;
                case json_tokener_state_comment: //解析注释
                {
                    /* Advance until we change state */
                    const char *case_start = str;
                    while (c != '*')
                    {
                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                    json_printbuf_memappend_checked(tok->pb, case_start, 1 + str - case_start);
                    json_tokener_set_state(tok, json_tokener_state_comment_end);
                }
                    break;

                case json_tokener_state_comment_eol: //解析行结束
                {
                    /* Advance until we change state */
                    const char *case_start = str;
                    while (c != '\n')
                    {
                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                    json_printbuf_memappend_checked(tok->pb, case_start, str - case_start);
                    LATTE_LIB_LOG(LOG_DEBUG,"json_tokener_comment: %s\n", tok->pb->buf);
                    // state = json_tokener_state_eatws;
                    json_tokener_set_state(tok, json_tokener_state_eatws);
                }
                    break;

                case json_tokener_state_comment_end: //注释结束
                {
                    json_printbuf_memappend_checked(tok->pb, &c, 1);
                    if (c == '/')
                    {
                        LATTE_LIB_LOG(LOG_DEBUG,"json_tokener_comment: %s\n", tok->pb->buf);
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                        // state = json_tokener_state_eatws;
                    }
                    else
                    {
                        json_tokener_set_state(tok, json_tokener_state_comment);
                        // state = json_tokener_state_comment;
                    }
                }
                    break;

                case json_tokener_state_string: //解析字符串
                {
                    /* Advance until we change state */
			        const char *case_start = str;
                    while (1)
                    {
                        if (c == tok->quote_char)  // ' or "
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            json_t* current = value_new();
                            value_set_sds(current, sds_new_len(tok->pb->buf, tok->pb->bpos));
                            // current =
                            //     json_object_new_string_len(tok->pb->buf, tok->pb->bpos);
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            
                            // saved_state = json_tokener_state_finish;
                            json_tokener_set_saved_state(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            break;
                        }
                        else if (c == '\\')
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            // saved_state = json_tokener_state_string;
                            json_tokener_set_saved_state(tok, json_tokener_state_string);
                            // state = json_tokener_state_string_escape;
                            json_tokener_set_state(tok, json_tokener_state_string_escape);
                            break;
                        }
                        else if ((tok->flags & JSON_TOKENER_STRICT) && c <= 0x1f)
                        {
                            // Disallow control characters in strict mode
                            tok->err = json_tokener_error_parse_string;
                            goto out;
                        }
                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                }
                    break;

                case json_tokener_state_string_escape: //编码
                {
                    switch (c)
                    {
                        case '"':
                        case '\\':
                        case '/':
                            json_printbuf_memappend_checked(tok->pb, &c, 1);
                            // state = saved_state;
                            json_tokener_set_state(tok, json_tokener_get_saved_state(tok));
                            break;
                        case 'b':
                        case 'n':
                        case 'r':
                        case 't':
                        case 'f':
                            if (c == 'b')
                                json_printbuf_memappend_checked(tok->pb, "\b", 1);
                            else if (c == 'n')
                                json_printbuf_memappend_checked(tok->pb, "\n", 1);
                            else if (c == 'r')
                                json_printbuf_memappend_checked(tok->pb, "\r", 1);
                            else if (c == 't')
                                json_printbuf_memappend_checked(tok->pb, "\t", 1);
                            else if (c == 'f')
                                json_printbuf_memappend_checked(tok->pb, "\f", 1);
                            // state = saved_state;
                            json_tokener_set_state(tok, json_tokener_get_saved_state(tok));
                            break;
                        case 'u':
                            tok->ucs_char = 0;
                            tok->st_pos = 0;
                            json_tokener_set_state(tok, json_tokener_state_enumscape_unicode);
                            break;
                        default: 
                            tok->err = json_tokener_error_parse_string; goto out;
                    }
                }
                    break;

                case json_tokener_state_enumscape_unicode:
                {
                    /* Handle a 4-byte \uNNNN sequence, or two sequences if a surrogate pair */
                    while (1)
                    {
                        if (!c || !is_hex_char(c)) {
                            tok->err = json_tokener_error_parse_string;
                            goto out;
                        }
                        tok->ucs_char |=
				    ((unsigned int)jt_hexdigit(c) << ((3 - tok->st_pos) * 4));
                        tok->st_pos++;
                        if (tok->st_pos >= 4)
                            break;

                        (void)ADVANCE_CHAR(str, tok);
                        if (!PEEK_CHAR(c, tok))
                        {
                            /*
                            * We're out of characters in the current call to
                            * json_tokener_parse(), but a subsequent call might
                            * provide us with more, so leave our current state
                            * as-is (including tok->high_surrogate) and return.
                            */
                            goto out;
                        }
                    }
                    tok->st_pos = 0;
                    /* Now, we have a full \uNNNN sequence in tok->ucs_char */

                    /* If the *previous* sequence was a high surrogate ... */
                    if (tok->high_surrogate)
                    {
                        if (IS_LOW_SURROGATE(tok->ucs_char))
                        {
                            /* Recalculate the ucs_char, then fall thru to process normally */
                            tok->ucs_char = DECODE_SURROGATE_PAIR(tok->high_surrogate,
                                                                tok->ucs_char);
                        }
                        else
                        {
                            /* High surrogate was not followed by a low surrogate
                            * Replace the high and process the rest normally
                            */
                            json_printbuf_memappend_checked(tok->pb,
                                                    (char *)utf8_replacement_char, 3);
                        }
                        tok->high_surrogate = 0;
                    }
                    if (tok->ucs_char < 0x80)
                    {
                        unsigned char unescaped_utf[1];
                        unescaped_utf[0] = tok->ucs_char;
                        json_printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 1);
                    }
                    else if (tok->ucs_char < 0x800)
                    {
                        unsigned char unescaped_utf[2];
                        unescaped_utf[0] = 0xc0 | (tok->ucs_char >> 6);
                        unescaped_utf[1] = 0x80 | (tok->ucs_char & 0x3f);
                        json_printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 2);
                    }
                    else if (IS_HIGH_SURROGATE(tok->ucs_char))
                    {
                        /*
                        * The next two characters should be \u, HOWEVER,
                        * we can't simply peek ahead here, because the
                        * characters we need might not be passed to us
                        * until a subsequent call to json_tokener_parse.
                        * Instead, transition through a couple of states.
                        * (now):
                        *   _escape_unicode => _unicode_need_escape
                        * (see a '\\' char):
                        *   _unicode_need_escape => _unicode_need_u
                        * (see a 'u' char):
                        *   _unicode_need_u => _escape_unicode
                        *      ...and we'll end up back around here.
                        */
                        tok->high_surrogate = tok->ucs_char;
                        tok->ucs_char = 0;
                        // state = json_tokener_state_enumscape_unicode_need_escape;
                        json_tokener_set_state(tok, json_tokener_state_enumscape_unicode_need_escape);
                        break;
                    }
                    else if (IS_LOW_SURROGATE(tok->ucs_char))
                    {
                        /* Got a low surrogate not preceded by a high */
                        json_printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                    }
                    else if (tok->ucs_char < 0x10000)
                    {
                        unsigned char unescaped_utf[3];
                        unescaped_utf[0] = 0xe0 | (tok->ucs_char >> 12);
                        unescaped_utf[1] = 0x80 | ((tok->ucs_char >> 6) & 0x3f);
                        unescaped_utf[2] = 0x80 | (tok->ucs_char & 0x3f);
                        json_printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 3);
                    }
                    else if (tok->ucs_char < 0x110000)
                    {
                        unsigned char unescaped_utf[4];
                        unescaped_utf[0] = 0xf0 | ((tok->ucs_char >> 18) & 0x07);
                        unescaped_utf[1] = 0x80 | ((tok->ucs_char >> 12) & 0x3f);
                        unescaped_utf[2] = 0x80 | ((tok->ucs_char >> 6) & 0x3f);
                        unescaped_utf[3] = 0x80 | (tok->ucs_char & 0x3f);
                        json_printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 4);
                    }
                    else
                    {
                        /* Don't know what we got--insert the replacement char */
                        json_printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                    }
                    // state = saved_state; // i.e. _state_string or _state_object_field
                    json_tokener_set_state(tok, json_tokener_get_saved_state(tok));
                }
                    break;
                
                case json_tokener_state_enumscape_unicode_need_escape:
                {
                    // We get here after processing a high_surrogate
                    // require a '\\' char
                    if (!c || c != '\\')
                    {
                        /* Got a high surrogate without another sequence following
                        * it.  Put a replacement char in for the high surrogate
                        * and pop back up to _state_string or _state_object_field.
                        */
                        json_printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                        tok->high_surrogate = 0;
                        tok->ucs_char = 0;
                        tok->st_pos = 0;
                        // state = saved_state;
                        json_tokener_set_state(tok, json_tokener_get_saved_state(tok));
                        goto redo_char;
                    }
                    // state = json_tokener_state_enumscape_unicode_need_u;
                    json_tokener_set_state(tok, json_tokener_state_enumscape_unicode_need_u);
                }
                    break;
                case json_tokener_state_enumscape_unicode_need_u:
                {
                    /* We already had a \ char, check that it's \u */
                    if (!c || c != 'u')
                    {
                        /* Got a high surrogate with some non-unicode escape
                        * sequence following it.
                        * Put a replacement char in for the high surrogate
                        * and handle the escape sequence normally.
                        */
                        json_printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                        tok->high_surrogate = 0;
                        tok->ucs_char = 0;
                        tok->st_pos = 0;
                        // state = json_tokener_state_string_escape;
                        json_tokener_set_state(tok, json_tokener_state_string_escape);
                        goto redo_char;
                    }
                    // state = json_tokener_state_enumscape_unicode;
                    json_tokener_set_state(tok, json_tokener_state_enumscape_unicode);
                }
                    break;

                case json_tokener_state_boolean: //解析true or false
                {
                    int size1, size2;
                    json_printbuf_memappend_checked(tok->pb, &c, 1);
                    size1 = latte_min(tok->st_pos + 1, json_true_str_len);
                    size2 = latte_min(tok->st_pos + 1, json_false_str_len);
                    if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                        strncasecmp(json_true_str, tok->pb->buf, size1) == 0) ||
                        (strncmp(json_true_str, tok->pb->buf, size1) == 0))
                    {
                        if (tok->st_pos == json_true_str_len)
                        {
                            json_t* current = value_new();
                            // current = json_object_new_boolean(1);
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_boolean] parse(%s)", tok->pb->buf);
                            value_set_bool(current, true);
                            json_tokener_set_current(tok, current);
                            // saved_state = json_tokener_state_finish;
                            json_tokener_set_saved_state(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            goto redo_char;
                        }
                    }
                    else if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                            strncasecmp(json_false_str, tok->pb->buf, size2) == 0) ||
                            (strncmp(json_false_str, tok->pb->buf, size2) == 0))
                    {
                        if (tok->st_pos == json_false_str_len)
                        {
                            // current = json_object_new_boolean(0);
                            json_t* current = value_new();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_boolean] parse(%s)", tok->pb->buf);
                            value_set_bool(current, false);
                            json_tokener_set_current(tok, current);
                            // saved_state = json_tokener_state_finish;
                            json_tokener_set_saved_state(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            goto redo_char;
                        }
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_boolean;
                        LATTE_LIB_LOG(LOG_ERROR, "[json_tokener_state_boolean] parse bool fail:(%s)", tok->pb->buf);
                        goto out;
                    }
                    tok->st_pos++;
                }
                    break;
                case json_tokener_state_number:
                {
                    
                    /* Advance until we change state */
                    const char *case_start = str;
                    int case_len = 0;
                    int is_exponent = 0;
                    int neg_sign_ok = 1;
                    int pos_sign_ok = 0;
                    if (printbuf_length(tok->pb) > 0)  //保存着上次的解析数据
                    {
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] %s next", tok->pb->buf);
                       /*   我们没有保存上一次增量解析的所有状态
                            因此我们需要根据迄今为止保存的字符串重新生成它。
                            */
                        char *e_loc = strchr(tok->pb->buf, 'e'); //科学计数法 浮点数
                        if (!e_loc)
                            e_loc = strchr(tok->pb->buf, 'E');
                        if (e_loc)
                        {
                            char *last_saved_char =
                                &tok->pb->buf[printbuf_length(tok->pb) - 1];
                            is_exponent = 1;
                            pos_sign_ok = neg_sign_ok = 1;
                            /* If the "e" isn't at the end, we can't start with a '-' */
                            if (e_loc != last_saved_char)
                            {
                                neg_sign_ok = 0;
                                pos_sign_ok = 0;
                            }
                            // else leave it set to 1, i.e. start of the new input
                        }
                    }

                    while (c && ((c >= '0' && c <= '9') ||
                                (!is_exponent && (c == 'e' || c == 'E')) ||
                                (neg_sign_ok && c == '-') || (pos_sign_ok && c == '+') ||
                                (!tok->is_double && c == '.')))
                    {
                        pos_sign_ok = neg_sign_ok = 0;
                        ++case_len;

                        /* non-digit characters checks */
                        /* note: since the main loop condition to get here was
                        * an input starting with 0-9 or '-', we are
                        * protected from input starting with '.' or
                        * e/E.
                        */
                        switch (c)
                        {
                        case '.':  //小数点
                            tok->is_double = 1;  
                            pos_sign_ok = 1;
                            neg_sign_ok = 1;
                            break;
                        case 'e': /* FALLTHRU */
                        case 'E': //科学计数法
                            is_exponent = 1;
                            tok->is_double = 1; 
                            /* the exponent part can begin with a negative sign */
                            pos_sign_ok = neg_sign_ok = 1;
                            break;
                        default: break;
                        }

                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start, case_len);
                            goto out;
                        }
                    }
                    /*
                        Now we know c isn't a valid number char, but check whether
                        it might have been intended to be, and return a potentially
                        more understandable error right away.
                        However, if we're at the top-level, use the number as-is
                        because c can be part of a new object to parse on the
                        next call to json_tokener_parse().
                    */
                    if (tok->depth > 0 && c != ',' && c != ']' && c != '}' && c != '/' &&
                        c != 'I' && c != 'i' && !is_ws_char(c))
                    {

                        tok->err = json_tokener_error_parse_number;
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error %s", c);
                        goto out;
                    }
                    if (case_len > 0)
                        json_printbuf_memappend_checked(tok->pb, case_start, case_len);

                    // Check for -Infinity
                    if (tok->pb->buf[0] == '-' && case_len <= 1 && (c == 'i' || c == 'I')) //inf
                    {
                        // state = json_tokener_state_inf;
                        json_tokener_set_state(tok, json_tokener_state_inf);
                        tok->st_pos = 0;
                        goto redo_char;
                    }
                    if (tok->is_double && !(tok->flags & JSON_TOKENER_STRICT))
                    {
                        /* Trim some chars off the end, to allow things
                        like "123e+" to parse ok. */
                        while (printbuf_length(tok->pb) > 1)
                        {
                            char last_char = tok->pb->buf[printbuf_length(tok->pb) - 1];
                            if (last_char != 'e' && last_char != 'E' &&
                                last_char != '-' && last_char != '+')
                            {
                                break;
                            }
                            tok->pb->buf[printbuf_length(tok->pb) - 1] = '\0';
                            printbuf_length(tok->pb)--;
                        }
                    }
                }
                //转换成value对象
                    {
                        int64_t num64;
                        uint64_t numuint64;
                        long double numd;
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] is_double:%d, parse(%s)", tok->is_double, tok->pb->buf);
                        if (!tok->is_double && tok->pb->buf[0] == '-' &&
                            string2ll(tok->pb->buf, printbuf_length(tok->pb), &num64) == 1) //如果有负 使用int64_t 有点讲究
                        {
                            if (errno == ERANGE && (tok->flags & JSON_TOKENER_STRICT))
                            {
                                tok->err = json_tokener_error_parse_number;
                                LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error : string2ll %s", tok->pb->buf);
                                goto out;
                            }

                            // current = json_object_new_int64(num64);
                            json_t* current = value_new();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            value_set_int64(current, num64);
                            json_tokener_set_current(tok, current);

                        }
                        else if (!tok->is_double && tok->pb->buf[0] != '-' &&
                                string2ull(tok->pb->buf, &numuint64) == 1) //正数 使用uint64_t 有点讲究之后还是简化一下吧
                        {
                            if (errno == ERANGE && (tok->flags & JSON_TOKENER_STRICT))
                            {
                                tok->err = json_tokener_error_parse_number;
                                LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error : string2ull %s", tok->pb->buf);
                                goto out;
                            }
                            if (numuint64 && tok->pb->buf[0] == '0' &&
                                (tok->flags & JSON_TOKENER_STRICT))
                            {
                                tok->err = json_tokener_error_parse_number;
                                LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error : buf[0] == 0 %s", tok->pb->buf);
                                goto out;
                            }
                            json_t* current = value_new();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            if (numuint64 <= INT64_MAX)
                            {
                                num64 = (int64_t)numuint64;
                                // current = json_object_new_int64(num64);
                                value_set_int64(current, num64);
                            }
                            else
                            {
                                // current = json_object_new_uint64(numuint64);
                                value_set_uint64(current, numuint64);
                            }
                            json_tokener_set_current(tok, current);
                        }
                        else if (tok->is_double &&
                                string2ld(
                                    tok->pb->buf, printbuf_length(tok->pb), &numd) == 1)
                        {
                            // current = json_object_new_double_s(numd, tok->pb->buf);
                            json_t* current = value_new();    
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            value_set_longdouble(current, numd);
                            json_tokener_set_current(tok, current);
                        }
                        else
                        {
                            tok->err = json_tokener_error_parse_number;
                            LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error : string2ld or string2ull or string2ll fail %s", tok->pb->buf);
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                        goto redo_char;
                    }
                    break;

                case json_tokener_state_array_after_sep:
		        case json_tokener_state_array: //解析数组
                {
                    if (c == ']')
                    {
                        // Minimize memory usage; assume parsed objs are unlikely to be changed
                        json_array_shrink(json_tokener_get_current(tok), 0);

                        if (json_tokener_get_state(tok) == json_tokener_state_array_after_sep &&
                            (tok->flags & JSON_TOKENER_STRICT))
                        {
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else
                    {
                        if (tok->depth >= tok->max_depth - 1)
                        {
                            tok->err = json_tokener_error_depth;
                            LATTE_LIB_LOG(LOG_DEBUG, "[array_after_sep or array] json_tokener_error_depth");
                            goto out;
                        }
                        // state = json_tokener_state_array_add;
                        json_tokener_set_state(tok, json_tokener_state_array_add);
                        tok->depth++;
                        json_tokener_reset_level(tok, tok->depth); //清理数据
                        goto redo_char;
                    }
                }
                    break;
                case json_tokener_state_array_add:
                {
                    if (json_array_put_value(json_tokener_get_current(tok), obj) != 0)
                    {
                        tok->err = json_tokener_error_memory;
                        goto out;
                    }
                    // saved_state = json_tokener_state_array_sep;
                    json_tokener_set_saved_state(tok, json_tokener_state_array_sep);
                    // state = json_tokener_state_eatws;
                    json_tokener_set_state(tok, json_tokener_state_eatws);
                    goto redo_char;
                }    
                    break; //无效
                case json_tokener_state_array_sep:
                {
                    if (c == ']')
                    {
                        // Minimize memory usage; assume parsed objs are unlikely to be changed
                        json_array_shrink(json_tokener_get_current(tok), 0);

                        // saved_state = json_tokener_state_finish;
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else if (c == ',')
                    {
                        // saved_state = json_tokener_state_array_after_sep;
                        json_tokener_set_saved_state(tok, json_tokener_state_array_after_sep);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_array;
                        goto out;
                    }
                }
                    break;
                case json_tokener_state_object_field_start:
		        case json_tokener_state_object_field_start_after_sep:
                {
                    if (c == '}') //object 结束
                    {
                         LATTE_LIB_LOG(LOG_DEBUG, "[field_start or after_sep] map end '}' ");
                        if (json_tokener_get_state(tok) == json_tokener_state_object_field_start_after_sep &&
                            (tok->flags & JSON_TOKENER_STRICT))
                        {
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else if (c == '"' || c == '\'')
                    {
                        tok->quote_char = c;
                        printbuf_reset(tok->pb);
                        // state = json_tokener_state_object_field;
                        json_tokener_set_state(tok, json_tokener_state_object_field);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_object_key_name;
                        goto out;
                    }
                }
                    break;

                case json_tokener_state_object_field:
                {
                    /* Advance until we change state */
                    const char *case_start = str;
                    while (1)
                    {
                        if (c == tok->quote_char)
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            json_tokener_set_fieldname(tok, sds_new(tok->pb->buf));
                            if (json_tokener_get_fieldname(tok) == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            // saved_state = json_tokener_state_object_field_end;
                            json_tokener_set_saved_state(tok, json_tokener_state_object_field_end);
                            // state = json_tokener_state_eatws;
                            json_tokener_set_state(tok, json_tokener_state_eatws);
                            break;
                        }
                        else if (c == '\\')
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            // saved_state = json_tokener_state_object_field;
                            json_tokener_set_saved_state(tok, json_tokener_state_object_field);
                            // state = json_tokener_state_string_escape;
                            json_tokener_set_state(tok, json_tokener_state_string_escape);
                            break;
                        }
                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            json_printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                }    
                    break;
                case json_tokener_state_object_field_end:
                {
                    if (c == ':')
                    {
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_object_field_end] parse :");
                        // saved_state = json_tokener_state_object_value;
                        json_tokener_set_saved_state(tok, json_tokener_state_object_value);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_object_key_sep;
                        goto out;
                    }
                }
                    break;
                case json_tokener_state_object_value:
                {
                    if (tok->depth >= tok->max_depth - 1)
                    {
                        tok->err = json_tokener_error_depth;
                        LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_object_value] json_tokener_error_depth");
                        goto out;
                    }
                    // state = json_tokener_state_object_value_add;
                    json_tokener_set_state(tok, json_tokener_state_object_value_add);
                    tok->depth++;
                    json_tokener_reset_level(tok, tok->depth);
                    goto redo_char;
                }
                    break;
                case json_tokener_state_object_value_add:
                {
                    if (json_map_put_value(json_tokener_get_current(tok), json_tokener_get_fieldname(tok), obj) != 0)
                    {
                        tok->err = json_tokener_error_memory;
                        goto out;
                    }
                    // free(obj_field_name);
                    // obj_field_name = NULL;
                    json_tokener_set_fieldname(tok, NULL);
                    // saved_state = json_tokener_state_object_sep;
                    json_tokener_set_saved_state(tok, json_tokener_state_object_sep);
                    // state = json_tokener_state_eatws;
                    json_tokener_set_state(tok, json_tokener_state_eatws);
                    goto redo_char;
                }
                    break;
                case json_tokener_state_object_sep:
                {
                    /* { */
                    if (c == '}')
                    {
                        // saved_state = json_tokener_state_finish;
                        json_tokener_set_saved_state(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else if (c == ',') //还有下个属性
                    {
                        // saved_state = json_tokener_state_object_field_start_after_sep;
                        json_tokener_set_saved_state(tok, json_tokener_state_object_field_start_after_sep);
                        // state = json_tokener_state_eatws;
                        json_tokener_set_state(tok, json_tokener_state_eatws);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_object_value_sep;
                        goto out;
                    }
                }
                    break;
            };
            (void)ADVANCE_CHAR(str, tok);
            if (!c) // This is the char *before* advancing
                break;
    } /* while(PEEK_CHAR) */
out:
	if ((tok->flags & JSON_TOKENER_VALIDATE_UTF8) && (nBytes != 0))
	{
		tok->err = json_tokener_error_parse_utf8_string;
	}
	if (c && (json_tokener_get_state(tok) == json_tokener_state_finish) && (tok->depth == 0) &&
	    (tok->flags & (JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS)) ==
	        JSON_TOKENER_STRICT)
	{
		/* unexpected char after JSON data */
		tok->err = json_tokener_error_parse_unexpected;
	}
	if (!c)
	{
		/* We hit an eof char (0) */
		if (json_tokener_get_state(tok) != json_tokener_state_finish && json_tokener_get_saved_state(tok) != json_tokener_state_finish)
			tok->err = json_tokener_error_parse_eof;
	}

// #ifdef HAVE_USELOCALE
// 	uselocale(oldlocale);
// 	freelocale(newloc);
// #elif defined(HAVE_SETLOCALE)
// 	setlocale(LC_NUMERIC, oldlocale);
// 	free(oldlocale);
// #endif

	if (tok->err == json_tokener_success)
	{
		json_t *ret = json_tokener_get_current(tok);
		int ii;

		/* Partially reset, so we parse additional objects on subsequent calls. */
		for (ii = tok->depth; ii >= 0; ii--)
			json_tokener_reset_level(tok, ii);
		*result = ret;
        return 1;
	}

	LATTE_LIB_LOG(LOG_INFO, "json_tokener_parse_ex: error %s at offset %d\n", json_tokener_errors[tok->err],
	         tok->char_offset);
    *result = NULL;
	return 0;
}

int sds_to_json(sds_t str, json_t** result) {
    json_tokener_t* tok = json_tokener_new();
    int rc = sds_to_json_verbose(str, sds_len(str), result, tok);
    json_tokener_delete(tok);
    return rc;
}

// encode

sds_t sds_to_string(sds_t v) {
    return sds_cat_fmt(sds_empty(), "\"%s\"", v);
}


sds_t json_to_sds(json_t* v) {
    switch(v->type) {
        case VALUE_SDS:
            return sds_to_string(v->value.sds_value);
        case VALUE_INT:
            return ll2sds(v->value.i64_value);
        case VALUE_UINT:
            return ull2sds(v->value.u64_value);
        case VALUE_DOUBLE:
            return ld2sds(v->value.ld_value, 0);
        case VALUE_BOOLEAN:
            return v->value.bool_value == true? sds_new("true"): sds_new("false");
        case VALUE_ARRAY: {
            sds_t result = sds_new("[");
            latte_iterator_t* itor = vector_get_iterator(value_get_array(v));
            int frist = 1;
            while(latte_iterator_has_next(itor)) {
                json_t* v = latte_iterator_next(itor);
                sds_t r = json_to_sds(v);
                if (frist) {
                    result = sds_cat_fmt(result, "%s", r);
                    frist = 0;
                } else {
                    result = sds_cat_fmt(result, ",%s", r);
                }
                sds_delete(r);
            }
            latte_iterator_delete(itor);
            return sds_cat_fmt(result, "]");
        }
        case VALUE_MAP: {
            sds_t result = sds_new("{");
            latte_iterator_t* itor = dict_get_latte_iterator(value_get_map(v));
            int frist = 1;
            while(latte_iterator_has_next(itor)) {
                latte_pair_t* pair = latte_iterator_next(itor);
                sds_t k_str = sds_to_string((sds_t)latte_pair_key(pair));
                json_t* v = (json_t*)latte_pair_value(pair);
                sds_t v_str = json_to_sds(v);
                if (frist) {
                    result = sds_cat_fmt(result, "%s:%s", k_str , v_str);
                    frist = 0;
                } else {
                    result = sds_cat_fmt(result, ",%s:%s",k_str,  v_str);
                }
                sds_delete(k_str);
                sds_delete(v_str);
            }
            latte_iterator_delete(itor);
            return sds_cat_fmt(result, "}");
        }
        break;
        default:
            return sds_new("null");
    }
}