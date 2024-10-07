#include "latte_json.h"
#include "dict/dict_plugins.h"
// #include "json.h"
#include "utils/utils.h"
#include <errno.h>
#include <string.h>
#include "log/log.h"
// map
dictType jsonDictTyep  = {
    dictCharHash,
    NULL,
    NULL,
    dictCharKeyCompare,
    dictSdsDestructor,
    NULL,
    NULL
};
value* jsonMapCreate() {
    value* r = valueCreate();
    dict* d = dictCreate(&jsonDictTyep);
    valueSetDict(r, d);
    return r;
}

bool isMap(value* v) {
    return v != NULL && v->type == MAPTYPES;
}
int jsonMapPutValue(value* root, sds key, value* v) {
    if (!isMap(root)) return 0;
    dictEntry* node = dictFind(root->value.map_value, key);
    if (node != NULL) {
        value* old_v = (value*)dictGetVal(node);
        valueRelease(old_v);
        dictSetVal(root->value.map_value, node, v);
        return 1;
    } else {
        return dictAdd(root->value.map_value, key, v);
    }
}


int jsonMapPutSds(value* root, sds key, sds sv) {
    if (!isMap(root)) return 0;
    value* v = valueCreate();
    valueSetSds(v, sv);
    return jsonMapPutValue(root, key, v);
}

int jsonMapPutInt64(value* root, sds key, int64_t ll) {
    if (!isMap(root)) return 0;
    value* v = valueCreate();
    valueSetInt64(v, ll);
    return jsonMapPutValue(root, key, v);
}

int jsonMapPutBool(value* root, sds key, bool b) {
    if (!isMap(root)) return 0;
    value* v = valueCreate();
    valueSetBool(v, b);
    return jsonMapPutValue(root, key, v);
}

int jsonMapPutLongDouble(value* root, sds key, long double ld) {
    if (!isMap(root)) return 0;
    value* v = valueCreate();
    valueSetLongDouble(v, ld);
    return jsonMapPutValue(root, key, v);
}

value* jsonMapGet(value* root, char* key) {
    latte_assert(isMap(root), "json is not map!!!");
    dictEntry* entry = dictFind(root->value.map_value, key);
    if (entry == NULL) return NULL;
    return dictGetVal(entry);
}

// list 

value* jsonListCreate() {
    value* r = valueCreate();
    vector* d = vectorCreate();
    valueSetArray(r, d);
    return r;
}

bool isList(value* v) {
    return v != NULL && v->type == LISTTYPS;
}

void valueListResize(value* root, int size) {
    if (!isList(root)) return;
    vectorResize(root->value.list_value, size);
}



int jsonListPutSds(value* root, sds element) {
    if (!isList(root)) return 0;
    value* v = valueCreate();
    valueSetSds(v, element);
    vectorPush(root->value.list_value, v);
    return 1;
}

int jsonListPutBool(value* root, bool element) {
    if (!isList(root)) return 0;
    value* v = valueCreate();
    valueSetBool(v, element);
    return vectorPush(root->value.list_value, v);
}


int jsonListPutInt64(value* root, int64_t element) {
    if (!isList(root)) return 0;
    value* v = valueCreate();
    valueSetInt64(v, element);
    return vectorPush(root->value.list_value, v);
}

int jsonListPutLongDouble(value* root, long double element) {
    if (!isList(root)) return 0;
    value* v = valueCreate();
    valueSetLongDouble(v, element);
    return vectorPush(root->value.list_value, v);
}

int jsonListPutValue(value* root, value* element) {
    if (!isList(root)) return 0;
    return vectorPush(root->value.list_value, element);
}

int jsonListShrink(value* root, int max) {
    if (!isList(root)) return 0;
    return vectorShrink(root->value.list_value, max);
}

static void json_tokener_reset_level(struct jsonTokener *tok, int depth)
{
    jsonTokenerSrec* srec = vectorGet(tok->stack, depth);
    srec->state = json_tokener_state_eatws;
	// tok->stack[depth].state = json_tokener_state_eatws;
    srec->saved_state = json_tokener_state_start;
	// tok->stack[depth].saved_state = json_tokener_state_start;
	// json_object_put(tok->stack[depth].current);
	// tok->stack[depth].current = NULL;
    srec->current = NULL;
	sdsfree(srec->obj_field_name);
	srec->obj_field_name = NULL;
}

void jsonTokenerReset(jsonTokener* tok) {
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
printbuf* printbufCreate() {
    printbuf* buf = zmalloc(sizeof(printbuf));
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
void printbufRelease(printbuf* buf) {
    zfree(buf->buf);
    zfree(buf);
}

/**
 * 
 *  -1 创建对象失败
 *  0 不处理
 *  1 处理成功
*/
jsonTokenerSrec* jsonTokenerSrecCreate() {
    jsonTokenerSrec* srec = zmalloc(sizeof(jsonTokenerSrec));
    return srec;
}
int expandTokenerStack(vector* v, int size) {
    if (size < v->count) return 0;
    vectorResize(v, size);
    while(v->count < size) {
        jsonTokenerSrec* srec = jsonTokenerSrecCreate();
        if (srec == NULL) return -1;
        vectorPush(v, srec);
    }
    return 1;
}



jsonTokener* jsonTokenerCreateSize(int max_depth) {
    jsonTokener* tokener = zmalloc(sizeof(jsonTokener));
    if (!tokener) {
        return NULL;
    }
    
    tokener->stack = vectorCreate();
    if (!tokener->stack) {
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[jsonTokenerCreate] vectorCreate fail");
        return NULL;
    }

    if (expandTokenerStack(tokener->stack, max_depth) < 0) {
        vectorRelease(tokener->stack);
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[jsonTokenerCreate] expandTokenerStack szie=32 fail");
        return NULL;
    }
    tokener->max_depth = max_depth;

    tokener->pb = printbufCreate();
    if (!tokener->pb) {
        vectorRelease(tokener->stack);
        zfree(tokener);
        LATTE_LIB_LOG(LOG_DEBUG, "[jsonTokenerCreate] printbufCreate fail");
        return NULL;
    } 
    
    jsonTokenerReset(tokener);
    return tokener;
}

jsonTokener* jsonTokenerCreate() {
    return jsonTokenerCreateSize(32);
}
void jsonTokenerRelease(jsonTokener* tokener) {
    vectorRelease(tokener->stack);
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

jsonTokenerState getTokenerState(jsonTokener* tok) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    return srec->state;
}

void setTokenerState(jsonTokener* tok, jsonTokenerState state) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    srec->state = state;
}
jsonTokenerState getTokenerSavedState(jsonTokener* tok) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    return srec->saved_state;
}

void setTokenerSavedState(jsonTokener* tok, jsonTokenerState state) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    srec->saved_state = state;
}

value* getTokenerCurrent(jsonTokener* tok) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    return srec->current;
}

void setTokenerCurrent(jsonTokener* tok, value* value) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    srec->current = value;
}

sds getTokenerFieldName(jsonTokener* tok) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
    return srec->obj_field_name;
}

void setTokenerFieldName(jsonTokener* tok, sds* value) {
    jsonTokenerSrec* srec = vectorGet(tok->stack, tok->depth);
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
	     ? (((tok)->depth == 0 && getTokenerState(tok) == json_tokener_state_eatws &&   \
	         getTokenerSavedState(tok) == json_tokener_state_finish)                   \
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
static int printbuf_extend(struct printbuf *p, int min_size)
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
	LATTE_LIB_LOG(LOG_DEBUG,"printbuf_extend: realloc "
	         "bpos=%d min_size=%d old_size=%d new_size=%d\n",
	         p->bpos, min_size, p->size, new_size);
#endif /* PRINTBUF_DEBUG */
	if (!(t = (char *)realloc(p->buf, new_size)))
		return -1;
	p->size = new_size;
	p->buf = t;
	return 0;
}

int printbuf_memappend(struct printbuf *p, const char *buf, int size)
{
	/* Prevent signed integer overflows with large buffers. */
	if (size < 0 || size > INT_MAX - p->bpos - 1)
	{
		errno = EFBIG;
		return -1;
	}
	if (p->size <= p->bpos + size + 1)
	{
		if (printbuf_extend(p, p->bpos + size + 1) < 0)
			return -1;
	}
	memcpy(p->buf + p->bpos, buf, size);
	p->bpos += size;
	p->buf[p->bpos] = '\0';
	return size;
}
/* printbuf_memappend_checked(p, s, l) macro:
 *   Add string s of length l to printbuffer p.
 *   If operation fails abort parse operation with memory error.
 */
#define printbuf_memappend_checked(p, s, l)                   \
	do {                                                  \
		if (printbuf_memappend((p), (s), (l)) < 0)    \
		{                                             \
			tok->err = json_tokener_error_memory; \
			goto out;                             \
		}                                             \
	} while (0)

void printbuf_reset(struct printbuf *p)
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

int jsonDecodeVerBose(char* str, int len, value** result, jsonTokener* tok) {
    LATTE_LIB_LOG(LOG_INFO, "jsonDecodeVerBose func");
    value* obj = NULL;
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

    while (PEEK_CHAR(c, tok)) {
        redo_char:
            switch (getTokenerState(tok)) 
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
                        printbuf_memappend_checked(tok->pb, &c, 1);
                        setTokenerState(tok, json_tokener_state_comment_start); //注释
                    } 
                    else 
                    {
                        setTokenerState(tok, getTokenerSavedState(tok));
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
                        setTokenerState(tok, json_tokener_state_eatws);
                        //标记状态map
                        setTokenerSavedState(tok, json_tokener_state_object_field_start);
                        setTokenerCurrent(tok, jsonMapCreate());
                        if (getTokenerCurrent(tok) == NULL) { //申请内存失败
                            tok->err = json_tokener_error_memory;
                            goto out;
                        }
                        break;
                    case '[':
                        setTokenerState(tok, json_tokener_state_eatws);
                        //标记状态list
                        setTokenerSavedState(tok, json_tokener_state_array);
                        setTokenerCurrent(tok, jsonListCreate());
                        if (getTokenerCurrent(tok) == NULL) { //申请内存失败
                            tok->err = json_tokener_error_memory;
                            goto out;
                        }
                        break;
                    case 'I':
                    case 'i': //inf 
                        setTokenerState(tok, json_tokener_state_inf);
                        printbuf_reset(tok->pb);
                        tok->st_pos = 0;
                        goto redo_char;
                    case 'N':
                    case 'n': //null or nan
                        setTokenerState(tok, json_tokener_state_null);
                        printbuf_reset(tok->pb);
				        tok->st_pos = 0;
                        goto redo_char;
                    case '\'':
                        if (tok->flags & JSON_TOKENER_STRICT) { //严格模式
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                    case '"':
                        setTokenerState(tok, json_tokener_state_string);
                        printbuf_reset(tok->pb);
				        tok->quote_char = c;
                        break;
                    case 'T':
                    case 't':
                    case 'F':
                    case 'f':
                        setTokenerState(tok, json_tokener_state_boolean); //解析true or false
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
                        setTokenerState(tok, json_tokener_state_number); //解析数字
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
                    obj = getTokenerCurrent(tok);
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
                    }
                }
                    break;
                case json_tokener_state_null:  //解析null 或者 nan
                {
                    int size;
                    int size_nan;
                    printbuf_memappend_checked(tok->pb, &c, 1);
                    size = latte_min(tok->st_pos + 1, json_null_str_len);
                    size_nan = latte_min(tok->st_pos + 1, json_nan_str_len);
                    if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                        strncasecmp(json_null_str, tok->pb, size) == 0) ||
                        (strncmp(json_null_str, tok->pb, size) == 0)) //对比null
                    {
                        if (tok->st_pos == json_null_str_len)
                        {
                            setTokenerCurrent(tok, NULL);
                            setTokenerSavedState(tok, json_tokener_state_finish);
                            setTokenerState(tok, json_tokener_state_eatws);
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
                            value* current = valueCreate();
                            
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            valueSetLongDouble(current, NAN);
                            setTokenerCurrent(tok, current);
                            setTokenerSavedState(tok, json_tokener_state_finish);
                            // saved_state = json_tokener_state_finish;
                            // state = json_tokener_state_eatws;
                            setTokenerState(tok, json_tokener_state_eatws);
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
                        setTokenerState(tok, json_tokener_state_comment);
                    }
                    else if (c == '/')
                    {
                        setTokenerState(tok, json_tokener_state_comment_eol);
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_comment;
                        goto out;
                    }
                    printbuf_memappend_checked(tok->pb, &c, 1);
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
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                    printbuf_memappend_checked(tok->pb, case_start, 1 + str - case_start);
                    setTokenerState(tok, json_tokener_state_comment_end);
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
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            goto out;
                        }
                    }
                    printbuf_memappend_checked(tok->pb, case_start, str - case_start);
                    LATTE_LIB_LOG(LOG_DEBUG,"json_tokener_comment: %s\n", tok->pb->buf);
                    // state = json_tokener_state_eatws;
                    setTokenerState(tok, json_tokener_state_eatws);
                }
                    break;

                case json_tokener_state_comment_end: //注释结束
                {
                    printbuf_memappend_checked(tok->pb, &c, 1);
                    if (c == '/')
                    {
                        LATTE_LIB_LOG(LOG_DEBUG,"json_tokener_comment: %s\n", tok->pb->buf);
                        setTokenerState(tok, json_tokener_state_eatws);
                        // state = json_tokener_state_eatws;
                    }
                    else
                    {
                        setTokenerState(tok, json_tokener_state_comment);
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
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            value* current = valueCreate();
                            valueSetSds(current, sdsnewlen(tok->pb->buf, tok->pb->bpos));
                            // current =
                            //     json_object_new_string_len(tok->pb->buf, tok->pb->bpos);
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            
                            // saved_state = json_tokener_state_finish;
                            setTokenerSavedState(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            setTokenerState(tok, json_tokener_state_eatws);
                            break;
                        }
                        else if (c == '\\')
                        {
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            // saved_state = json_tokener_state_string;
                            setTokenerSavedState(tok, json_tokener_state_string);
                            // state = json_tokener_state_string_escape;
                            setTokenerState(tok, json_tokener_state_string_escape);
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
                            printbuf_memappend_checked(tok->pb, case_start,
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
                            printbuf_memappend_checked(tok->pb, &c, 1);
                            // state = saved_state;
                            setTokenerState(tok, getTokenerSavedState(tok));
                            break;
                        case 'b':
                        case 'n':
                        case 'r':
                        case 't':
                        case 'f':
                            if (c == 'b')
                                printbuf_memappend_checked(tok->pb, "\b", 1);
                            else if (c == 'n')
                                printbuf_memappend_checked(tok->pb, "\n", 1);
                            else if (c == 'r')
                                printbuf_memappend_checked(tok->pb, "\r", 1);
                            else if (c == 't')
                                printbuf_memappend_checked(tok->pb, "\t", 1);
                            else if (c == 'f')
                                printbuf_memappend_checked(tok->pb, "\f", 1);
                            // state = saved_state;
                            setTokenerState(tok, getTokenerSavedState(tok));
                            break;
                        case 'u':
                            tok->ucs_char = 0;
                            tok->st_pos = 0;
                            setTokenerState(tok, json_tokener_state_escape_unicode);
                            break;
                        default: 
                            tok->err = json_tokener_error_parse_string; goto out;
                    }
                }
                    break;

                case json_tokener_state_escape_unicode:
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
                            printbuf_memappend_checked(tok->pb,
                                                    (char *)utf8_replacement_char, 3);
                        }
                        tok->high_surrogate = 0;
                    }
                    if (tok->ucs_char < 0x80)
                    {
                        unsigned char unescaped_utf[1];
                        unescaped_utf[0] = tok->ucs_char;
                        printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 1);
                    }
                    else if (tok->ucs_char < 0x800)
                    {
                        unsigned char unescaped_utf[2];
                        unescaped_utf[0] = 0xc0 | (tok->ucs_char >> 6);
                        unescaped_utf[1] = 0x80 | (tok->ucs_char & 0x3f);
                        printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 2);
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
                        // state = json_tokener_state_escape_unicode_need_escape;
                        setTokenerState(tok, json_tokener_state_escape_unicode_need_escape);
                        break;
                    }
                    else if (IS_LOW_SURROGATE(tok->ucs_char))
                    {
                        /* Got a low surrogate not preceded by a high */
                        printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                    }
                    else if (tok->ucs_char < 0x10000)
                    {
                        unsigned char unescaped_utf[3];
                        unescaped_utf[0] = 0xe0 | (tok->ucs_char >> 12);
                        unescaped_utf[1] = 0x80 | ((tok->ucs_char >> 6) & 0x3f);
                        unescaped_utf[2] = 0x80 | (tok->ucs_char & 0x3f);
                        printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 3);
                    }
                    else if (tok->ucs_char < 0x110000)
                    {
                        unsigned char unescaped_utf[4];
                        unescaped_utf[0] = 0xf0 | ((tok->ucs_char >> 18) & 0x07);
                        unescaped_utf[1] = 0x80 | ((tok->ucs_char >> 12) & 0x3f);
                        unescaped_utf[2] = 0x80 | ((tok->ucs_char >> 6) & 0x3f);
                        unescaped_utf[3] = 0x80 | (tok->ucs_char & 0x3f);
                        printbuf_memappend_checked(tok->pb, (char *)unescaped_utf, 4);
                    }
                    else
                    {
                        /* Don't know what we got--insert the replacement char */
                        printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                    }
                    // state = saved_state; // i.e. _state_string or _state_object_field
                    setTokenerState(tok, getTokenerSavedState(tok));
                }
                    break;
                
                case json_tokener_state_escape_unicode_need_escape:
                {
                    // We get here after processing a high_surrogate
                    // require a '\\' char
                    if (!c || c != '\\')
                    {
                        /* Got a high surrogate without another sequence following
                        * it.  Put a replacement char in for the high surrogate
                        * and pop back up to _state_string or _state_object_field.
                        */
                        printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                        tok->high_surrogate = 0;
                        tok->ucs_char = 0;
                        tok->st_pos = 0;
                        // state = saved_state;
                        setTokenerState(tok, getTokenerSavedState(tok));
                        goto redo_char;
                    }
                    // state = json_tokener_state_escape_unicode_need_u;
                    setTokenerState(tok, json_tokener_state_escape_unicode_need_u);
                }
                    break;
                case json_tokener_state_escape_unicode_need_u:
                {
                    /* We already had a \ char, check that it's \u */
                    if (!c || c != 'u')
                    {
                        /* Got a high surrogate with some non-unicode escape
                        * sequence following it.
                        * Put a replacement char in for the high surrogate
                        * and handle the escape sequence normally.
                        */
                        printbuf_memappend_checked(tok->pb, (char *)utf8_replacement_char, 3);
                        tok->high_surrogate = 0;
                        tok->ucs_char = 0;
                        tok->st_pos = 0;
                        // state = json_tokener_state_string_escape;
                        setTokenerState(tok, json_tokener_state_string_escape);
                        goto redo_char;
                    }
                    // state = json_tokener_state_escape_unicode;
                    setTokenerState(tok, json_tokener_state_escape_unicode);
                }
                    break;

                case json_tokener_state_boolean: //解析true or false
                {
                    int size1, size2;
                    printbuf_memappend_checked(tok->pb, &c, 1);
                    size1 = latte_min(tok->st_pos + 1, json_true_str_len);
                    size2 = latte_min(tok->st_pos + 1, json_false_str_len);
                    LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_boolean] parse(%s)", tok->pb->buf);
                    if ((!(tok->flags & JSON_TOKENER_STRICT) &&
                        strncasecmp(json_true_str, tok->pb->buf, size1) == 0) ||
                        (strncmp(json_true_str, tok->pb->buf, size1) == 0))
                    {
                        if (tok->st_pos == json_true_str_len)
                        {
                            value* current = valueCreate();
                            // current = json_object_new_boolean(1);
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            valueSetBool(current, true);
                            setTokenerCurrent(tok, current);
                            // saved_state = json_tokener_state_finish;
                            setTokenerSavedState(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            setTokenerState(tok, json_tokener_state_eatws);
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
                            value* current = valueCreate();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            valueSetBool(current, false);
                            setTokenerCurrent(tok, current);
                            // saved_state = json_tokener_state_finish;
                            setTokenerSavedState(tok, json_tokener_state_finish);
                            // state = json_tokener_state_eatws;
                            setTokenerState(tok, json_tokener_state_eatws);
                            goto redo_char;
                        }
                    }
                    else
                    {
                        tok->err = json_tokener_error_parse_boolean;
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
                            printbuf_memappend_checked(tok->pb, case_start, case_len);
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
                        printbuf_memappend_checked(tok->pb, case_start, case_len);

                    // Check for -Infinity
                    if (tok->pb->buf[0] == '-' && case_len <= 1 && (c == 'i' || c == 'I')) //inf
                    {
                        // state = json_tokener_state_inf;
                        setTokenerState(tok, json_tokener_state_inf);
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
                            value* current = valueCreate();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            valueSetInt64(current, num64);
                            setTokenerCurrent(tok, current);

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
                            value* current = valueCreate();
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            if (numuint64 <= INT64_MAX)
                            {
                                num64 = (int64_t)numuint64;
                                // current = json_object_new_int64(num64);
                                valueSetInt64(current, num64);
                            }
                            else
                            {
                                // current = json_object_new_uint64(numuint64);
                                valueSetUint64(current, numuint64);
                            }
                            setTokenerCurrent(tok, current);
                        }
                        else if (tok->is_double &&
                                string2ld(
                                    tok->pb->buf, printbuf_length(tok->pb), &numd) == 1)
                        {
                            // current = json_object_new_double_s(numd, tok->pb->buf);
                            value* current = valueCreate();    
                            if (current == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            valueSetLongDouble(current, numd);
                            setTokenerCurrent(tok, current);
                        }
                        else
                        {
                            tok->err = json_tokener_error_parse_number;
                            LATTE_LIB_LOG(LOG_DEBUG, "[json_tokener_state_number] parse number error : string2ld or string2ull or string2ll fail %s", tok->pb->buf);
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        setTokenerSavedState(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
                        goto redo_char;
                    }
                    break;

                case json_tokener_state_array_after_sep:
		        case json_tokener_state_array: //解析数组
                {
                    if (c == ']')
                    {
                        // Minimize memory usage; assume parsed objs are unlikely to be changed
                        jsonListShrink(getTokenerCurrent(tok), 0);

                        if (getTokenerState(tok) == json_tokener_state_array_after_sep &&
                            (tok->flags & JSON_TOKENER_STRICT))
                        {
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        setTokenerSavedState(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
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
                        setTokenerState(tok, json_tokener_state_array_add);
                        tok->depth++;
                        json_tokener_reset_level(tok, tok->depth); //清理数据
                        goto redo_char;
                    }
                }
                    break;
                case json_tokener_state_array_add:
                {
                    if (jsonListPutValue(getTokenerCurrent(tok), obj) != 0)
                    {
                        tok->err = json_tokener_error_memory;
                        goto out;
                    }
                    // saved_state = json_tokener_state_array_sep;
                    setTokenerSavedState(tok, json_tokener_state_array_sep);
                    // state = json_tokener_state_eatws;
                    setTokenerState(tok, json_tokener_state_eatws);
                    goto redo_char;
                }    
                    break; //无效
                case json_tokener_state_array_sep:
                {
                    if (c == ']')
                    {
                        // Minimize memory usage; assume parsed objs are unlikely to be changed
                        jsonListShrink(getTokenerCurrent(tok), 0);

                        // saved_state = json_tokener_state_finish;
                        setTokenerSavedState(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
                    }
                    else if (c == ',')
                    {
                        // saved_state = json_tokener_state_array_after_sep;
                        setTokenerSavedState(tok, json_tokener_state_array_after_sep);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
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
                        if (getTokenerState(tok) == json_tokener_state_object_field_start_after_sep &&
                            (tok->flags & JSON_TOKENER_STRICT))
                        {
                            tok->err = json_tokener_error_parse_unexpected;
                            goto out;
                        }
                        // saved_state = json_tokener_state_finish;
                        setTokenerSavedState(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
                    }
                    else if (c == '"' || c == '\'')
                    {
                        tok->quote_char = c;
                        printbuf_reset(tok->pb);
                        // state = json_tokener_state_object_field;
                        setTokenerState(tok, json_tokener_state_object_field);
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
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            setTokenerFieldName(tok, sdsnew(tok->pb->buf));
                            if (getTokenerFieldName(tok) == NULL)
                            {
                                tok->err = json_tokener_error_memory;
                                goto out;
                            }
                            // saved_state = json_tokener_state_object_field_end;
                            setTokenerSavedState(tok, json_tokener_state_object_field_end);
                            // state = json_tokener_state_eatws;
                            setTokenerState(tok, json_tokener_state_eatws);
                            break;
                        }
                        else if (c == '\\')
                        {
                            printbuf_memappend_checked(tok->pb, case_start,
                                                    str - case_start);
                            // saved_state = json_tokener_state_object_field;
                            setTokenerSavedState(tok, json_tokener_state_object_field);
                            // state = json_tokener_state_string_escape;
                            setTokenerState(tok, json_tokener_state_string_escape);
                            break;
                        }
                        if (!ADVANCE_CHAR(str, tok) || !PEEK_CHAR(c, tok))
                        {
                            printbuf_memappend_checked(tok->pb, case_start,
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
                        setTokenerSavedState(tok, json_tokener_state_object_value);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
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
                    setTokenerState(tok, json_tokener_state_object_value_add);
                    tok->depth++;
                    json_tokener_reset_level(tok, tok->depth);
                    goto redo_char;
                }
                    break;
                case json_tokener_state_object_value_add:
                {
                    if (jsonMapPutValue(getTokenerCurrent(tok), getTokenerFieldName(tok), obj) != 0)
                    {
                        tok->err = json_tokener_error_memory;
                        goto out;
                    }
                    // free(obj_field_name);
                    // obj_field_name = NULL;
                    setTokenerFieldName(tok, NULL);
                    // saved_state = json_tokener_state_object_sep;
                    setTokenerSavedState(tok, json_tokener_state_object_sep);
                    // state = json_tokener_state_eatws;
                    setTokenerState(tok, json_tokener_state_eatws);
                    goto redo_char;
                }
                    break;
                case json_tokener_state_object_sep:
                {
                    /* { */
                    if (c == '}')
                    {
                        // saved_state = json_tokener_state_finish;
                        setTokenerSavedState(tok, json_tokener_state_finish);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
                    }
                    else if (c == ',') //还有下个属性
                    {
                        // saved_state = json_tokener_state_object_field_start_after_sep;
                        setTokenerSavedState(tok, json_tokener_state_object_field_start_after_sep);
                        // state = json_tokener_state_eatws;
                        setTokenerState(tok, json_tokener_state_eatws);
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
	if (c && (getTokenerState(tok) == json_tokener_state_finish) && (tok->depth == 0) &&
	    (tok->flags & (JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS)) ==
	        JSON_TOKENER_STRICT)
	{
		/* unexpected char after JSON data */
		tok->err = json_tokener_error_parse_unexpected;
	}
	if (!c)
	{
		/* We hit an eof char (0) */
		if (getTokenerState(tok) != json_tokener_state_finish && getTokenerSavedState(tok) != json_tokener_state_finish)
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
		value *ret = getTokenerCurrent(tok);
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

int jsonDecode(sds str, value** result) {
    LATTE_LIB_LOG(LOG_INFO, "jsonDecode func");
    jsonTokener* tok = jsonTokenerCreate();
    int v = jsonDecodeVerBose(str, sdslen(str), result, tok);
    jsonTokenerRelease(tok);
    return v;
}

// encode

sds sdsEncode(sds v) {
    return sdscatfmt(sdsempty(), "\"%s\"", v);
}


sds jsonEncode(value* v) {
    switch(v->type) {
        case SDSS:
            return sdsEncode(v->value.sds_value);
        case INTS:
            return ll2sds(v->value.ll_value);
        case DOUBLES:
            return ld2sds(v->value.ld_value, 0);
        case BOOLEANS:
            return v->value.bool_value == true? sdsnew("true"): sdsnew("false");
        case LISTTYPS: {
            sds result = sdsnew("[");
            Iterator* itor = vectorGetIterator(v->value.list_value);
            int frist = 1;
            while(iteratorHasNext(itor)) {
                value* v = iteratorNext(itor);
                sds r = jsonEncode(v);
                if (frist) {
                    result = sdscatfmt(result, "%s", r);
                    frist = 0;
                } else {
                    result = sdscatfmt(result, ",%s", r);
                }
                sdsfree(r);
            }
            iteratorRelease(itor);
            return sdscatfmt(result, "]");
        }
        case MAPTYPES: {
            sds result = sdsnew("{");
            dictIterator* itor = dictGetIterator(v->value.list_value);
            int frist = 1;
            dictEntry* entry = NULL;
            while(NULL != (entry = dictNext(itor))) {
                sds k = sdsEncode(dictGetEntryKey(entry));
                value* v = (value*)dictGetEntryVal(entry);
                sds r = jsonEncode(v);
                if (frist) {
                    result = sdscatfmt(result, "%s:%s", k , r);
                    frist = 0;
                } else {
                    result = sdscatfmt(result, ",%s:%s",k,  r);
                }
                sdsfree(k);
                sdsfree(r);
            }
            dictReleaseIterator(itor);
            return sdscatfmt(result, "}");
        }
        break;
        default:
            return sdsnew("null");
    }
}