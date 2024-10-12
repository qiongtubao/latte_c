#ifndef __LATTE_JSON_H
#define __LATTE_JSON_H

#include "sds/sds.h"
#include "vector/vector.h"
#include "value/value.h"


#define json_t value_t
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

json_t* json_map_new();
int json_map_put_value(json_t* root, sds key, json_t* v);
int json_map_put_sds(json_t* root, sds key, sds sv);
int json_map_put_int64(json_t* root, sds key, int64_t sv);
int json_map_put_bool(json_t* root, sds key, bool v);
int json_map_put_longdouble(json_t* root, sds key, long double v);
json_t* json_map_get_value(json_t* root, char* key);

#define json_map_get_sds(root, key)  value_get_sds(json_map_get_value(root, key))
#define json_map_get_int64(root, key)  value_get_int64(json_map_get_value(root, key))
#define json_map_get_bool(root, key)  value_get_bool(json_map_get_value(root, key))
#define json_map_get_longdouble(root, key)  value_get_longdouble(json_map_get_value(root, key))

json_t* json_array_new();
int json_array_put_sds(json_t* v, sds element);
int json_array_put_bool(json_t* root, bool element);
int json_array_put_int64(json_t* root, int64_t element);
int json_array_put_longdouble(json_t* root, long double element);
int json_array_put_value(json_t* root, json_t* element);
int json_array_shrink(json_t* root, int max);


void json_array_resize(json_t* root, int size);



sds json_to_sds(json_t* v);

//decode string 
typedef struct json_printbuf_t
{
	char *buf;
	int bpos;
	int size;
} json_printbuf_t;
json_printbuf_t* json_printbuf_new();
void json_printbuf_delete(json_printbuf_t* buf);

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
typedef enum json_tokener_error_e {
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
} json_tokener_error_e;

/**
 * @deprecated Don't use this outside of json_tokener.c, it will be made private in a future release.
 */
typedef enum json_tokener_state_e
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
} json_tokener_state_e;

/**
 * @deprecated Don't use this outside of json_tokener.c, it will be made private in a future release.
 */
typedef struct json_tokener_srec_t
{
	enum json_tokener_state_e state, saved_state;
	struct value *obj;
	struct value *current;
	sds obj_field_name;
} json_tokener_srec_t;
json_tokener_srec_t* jsonTokenerSrecCreate();


typedef struct json_tokener_t {
    /**
	 * @deprecated Do not access any of these fields outside of json_tokener.c
	 */
	char *str;
	struct json_printbuf_t *pb;
	int max_depth, depth, is_double, st_pos;
	/**
	 * @deprecated See json_tokener_get_parse_end() instead.
	 */
	int char_offset;
	/**
	 * @deprecated See json_tokener_get_error() instead.
	 */
	json_tokener_error_e err;
	unsigned int ucs_char, high_surrogate;
	char quote_char;
	vector* stack;
	int flags;
} json_tokener_t;

json_tokener_t* json_tokener_new();
void json_tokener_delete(json_tokener_t* tokener);
int sds_to_json(sds str, json_t** result);

#endif