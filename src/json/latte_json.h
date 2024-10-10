#ifndef __LATTE_JSON_H
#define __LATTE_JSON_H

#include "sds/sds.h"
#include "vector/vector.h"
#include "value/value.h"

/**
 * Be strict when parsing JSON input.  Use caution with
 * this flag as what is considered valid may become more
 * restrictive from one release to the next, causing your
 * code to fail on previously working input.
 *
 * Note that setting this will also effectively disable parsing
 * of multiple json objects in a single character stream
 * (e.g. {"foo":123}{"bar":234}); if you want to allow that
 * also set JSON_TOKENER_ALLOW_TRAILING_CHARS
 *
 * This flag is not set by default.
 *
 * @see json_tokener_set_flags()
 */
#define JSON_TOKENER_STRICT 0x01

/**
 * Use with JSON_TOKENER_STRICT to allow trailing characters after the
 * first parsed object.
 *
 * @see json_tokener_set_flags()
 */
#define JSON_TOKENER_ALLOW_TRAILING_CHARS 0x02

value* jsonMapCreate();
int jsonMapPutValue(value* root, sds key, value* v);
int jsonMapPutSds(value* root, sds key, sds sv);
int jsonMapPutInt64(value* root, sds key, int64_t sv);
int jsonMapPutBool(value* root, sds key, bool v);
int jsonMapPutLongDouble(value* root, sds key, long double v);
value* jsonMapGet(value* root, char* key);

value* jsonListCreate();
int jsonListPutSds(value* v, sds element);
int jsonListPutBool(value* root, bool element);
int jsonListPutInt64(value* root, int64_t element);
int jsonListPutLongDouble(value* root, long double element);
int jsonListPutValue(value* root, value* element);
int jsonListShrink(value* root, int max);


void valueListResize(value* root, int size);



sds jsonEncode(value* v);

//decode string 
typedef struct printbuf
{
	char *buf;
	int bpos;
	int size;
} printbuf;
printbuf* printbufCreate();
void printbufRelease(printbuf* buf);

/**
 * Cause json_tokener_parse_ex() to validate that input is UTF8.
 * If this flag is specified and validation fails, then
 * json_tokener_get_error(tok) will return
 * json_tokener_error_parse_utf8_string
 *
 * This flag is not set by default.
 *
 * @see json_tokener_set_flags()
 */
#define JSON_TOKENER_VALIDATE_UTF8 0x10
typedef enum jsonTokenerError {
    json_tokener_success,
	json_tokener_continue,
	json_tokener_error_depth,
	json_tokener_error_parse_eof,
	json_tokener_error_parse_unexpected,
	json_tokener_error_parse_null,
	json_tokener_error_parse_boolean,
	json_tokener_error_parse_number,
	json_tokener_error_parse_array,
	json_tokener_error_parse_object_key_name,
	json_tokener_error_parse_object_key_sep,
	json_tokener_error_parse_object_value_sep,
	json_tokener_error_parse_string,
	json_tokener_error_parse_comment,
	json_tokener_error_parse_utf8_string,
	json_tokener_error_size,   /* A string longer than INT32_MAX was passed as input */
	json_tokener_error_memory  /* Failed to allocate memory */
} jsonTokenerError;

/**
 * @deprecated Don't use this outside of json_tokener.c, it will be made private in a future release.
 */
typedef enum jsonTokenerState
{
	json_tokener_state_eatws,
	json_tokener_state_start,
	json_tokener_state_finish,
	json_tokener_state_null,
	json_tokener_state_comment_start,
	json_tokener_state_comment,
	json_tokener_state_comment_eol,
	json_tokener_state_comment_end,
	json_tokener_state_string,
	json_tokener_state_string_escape,
	json_tokener_state_escape_unicode,
	json_tokener_state_escape_unicode_need_escape,
	json_tokener_state_escape_unicode_need_u,
	json_tokener_state_boolean,
	json_tokener_state_number,
	json_tokener_state_array,
	json_tokener_state_array_add,
	json_tokener_state_array_sep,
	json_tokener_state_object_field_start,
	json_tokener_state_object_field,
	json_tokener_state_object_field_end,
	json_tokener_state_object_value,
	json_tokener_state_object_value_add,
	json_tokener_state_object_sep,
	json_tokener_state_array_after_sep,
	json_tokener_state_object_field_start_after_sep,
	json_tokener_state_inf
} jsonTokenerState;

/**
 * @deprecated Don't use this outside of json_tokener.c, it will be made private in a future release.
 */
typedef struct jsonTokenerSrec
{
	enum jsonTokenerState state, saved_state;
	struct value *obj;
	struct value *current;
	sds obj_field_name;
} jsonTokenerSrec;
jsonTokenerSrec* jsonTokenerSrecCreate();


typedef struct jsonTokener {
    /**
	 * @deprecated Do not access any of these fields outside of json_tokener.c
	 */
	char *str;
	struct printbuf *pb;
	int max_depth, depth, is_double, st_pos;
	/**
	 * @deprecated See json_tokener_get_parse_end() instead.
	 */
	int char_offset;
	/**
	 * @deprecated See json_tokener_get_error() instead.
	 */
	jsonTokenerError err;
	unsigned int ucs_char, high_surrogate;
	char quote_char;
	vector* stack;
	int flags;
} jsonTokener;

jsonTokener* jsonTokenerCreate();
void jsonTokenerRelease(jsonTokener* tokener);
int jsonDecode(sds str, value** result);

#endif