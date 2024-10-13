#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "latte_json.h"
// #include "json.h"
// int test_basic_json() {
//     // 创建 JSON 对象
//     json_object *root = json_object_new_object();

//     // 添加键值对
//     json_object *name = json_object_new_string("John Doe");
//     json_object_object_add(root, "name", name);

//     json_object *age = json_object_new_int(30);
//     json_object_object_add(root, "age", age);

//     // 创建数组
//     json_object *hobbies = json_object_new_array();
    
//     // 向数组中添加元素
//     json_object_array_add(hobbies, json_object_new_string("reading"));
//     json_object_array_add(hobbies, json_object_new_string("coding"));

//     // 将数组添加到对象中
//     json_object_object_add(root, "hobbies", hobbies);

//     // 生成 JSON 字符串
//     char *jsonStr = json_object_to_json_string(root);

//     // 输出 JSON 字符串
//     printf("%s\n", jsonStr);

//     // 释放内存
//     json_object_put(root);

//    // JSON 字符串
//     jsonStr = "{\"name\": \"John Doe\", \"age\": 30, \"hobbies\": [\"reading\", \"coding\"]}";

//     // 反序列化 JSON 字符串为 JSON 对象
//     root = json_tokener_parse(jsonStr);

//     if (!root) {
//         fprintf(stderr, "Failed to parse JSON string.\n");
//         return 1;
//     }

//     // 获取 JSON 对象中的数据
//     json_object *nameObj = json_object_object_get(root, "name");
//     if (nameObj && json_object_is_type(nameObj, json_type_string)) {
//         const char *name = json_object_get_string(nameObj);
//         printf("Name: %s\n", name);
//     } else {
//         fprintf(stderr, "Invalid 'name' field.\n");
//     }

//     json_object *ageObj = json_object_object_get(root, "age");
//     if (ageObj && json_object_is_type(ageObj, json_type_int)) {
//         int age = json_object_get_int(ageObj);
//         printf("Age: %d\n", age);
//     } else {
//         fprintf(stderr, "Invalid 'age' field.\n");
//     }

//     json_object *hobbiesObj = json_object_object_get(root, "hobbies");
//     if (hobbiesObj && json_object_is_type(hobbiesObj, json_type_array)) {
//         size_t numElements = json_object_array_length(hobbiesObj);
//         for (size_t i = 0; i < numElements; ++i) {
//             json_object *hobbyObj = json_object_array_get_idx(hobbiesObj, i);
//             if (hobbyObj && json_object_is_type(hobbyObj, json_type_string)) {
//                 const char *hobby = json_object_get_string(hobbyObj);
//                 printf("Hobby: %s\n", hobby);
//             } else {
//                 fprintf(stderr, "Invalid 'hobbies' element.\n");
//             }
//         }
//     } else {
//         fprintf(stderr, "Invalid 'hobbies' field.\n");
//     }

//     // 释放内存
//     json_object_put(root);


//     return 1;
// }
#include "log/log.h"
int test_encode() {
    

    value_t* root = json_map_new();
    json_map_put_sds(root, sds_new("name"), sds_new("John Doe"));
    json_map_put_int64(root, sds_new("age"), 30);
    json_map_put_bool(root, "is_student", true);
    json_map_put_longdouble(root, "memory", 3.1435926);
    value_t* vec = json_array_new();
    // json_array_resize(vec, 5);
    json_array_put_sds(vec, sds_new("readubg"));
    json_map_put_value(root, sds_new("hobbies"), vec);
    value_t* list =json_map_get_value(root, "hobbies");
    assert(list == vec);

    json_array_put_int64(list, 100);
    json_array_put_longdouble(list, 1.23456);
    json_array_put_bool(list, false);

    sds_t result = json_to_sds(root);
    printf("json: %s\n", result);

    

    
    
    return 1;

}

value_t* getMapTestValue(value_t* root) {
    assert(value_is_map(root));
    dict_entry_t* entry = dict_find(root->value.map_value, "test");
    assert(entry != NULL);
    return dict_get_val(entry);
}
int test_decode_int() {
    value_t* root1 = NULL;
    assert(1 == sds_to_json(sds_new("{\"test\":12345}"), &root1));
    assert(root1 != NULL);
    value_t* test = getMapTestValue(root1);
    assert(test != NULL);
    assert(value_is_int64(test));
    assert(value_get_int64(test) == 12345);
    value_delete(root1);
    root1 = NULL;

    assert(1 == sds_to_json(sds_new("{\"test\":9223372036854775809, \"test1\":12345}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(value_is_uint64(test));
    assert(value_get_uint64(test) == 9223372036854775809ULL);
    value_delete(root1);
    root1 = NULL;

    assert(1 == sds_to_json(sds_new("{\"test\":1.2345, \"test1\":12345}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(value_is_longdouble(test));
    assert(test->value.ld_value - 1.2345 < 0.0001);
    value_delete(root1);
    root1 = NULL;
    return 1;
}

int test_decode_object() {
    value_t* root1 = NULL;
    assert(1 == sds_to_json(sds_new("{}"), &root1));
    assert(root1 != NULL);
    assert(value_is_map(root1));
    assert(dict_size(root1->value.map_value) == 0);
    value_delete(root1);
    root1 = NULL;
    return 1;
}

int test_decode_bool() {
    value_t* root1 = NULL;
    assert(1 == sds_to_json(sds_new("{\"test\":true}"), &root1));
    assert(root1 != NULL);
    value_t* test = getMapTestValue(root1);
    assert(test != NULL);
    assert(value_is_bool(test));
    assert(test->value.bool_value == true);
    value_delete(root1);
    root1 = NULL;

    assert(1 == sds_to_json(sds_new("{\"test\":false}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(value_is_bool(test));
    assert(test->value.bool_value == false);
    value_delete(root1);
    root1 = NULL;

    return 1;
}



int test_decode_list() {
    value_t* root1 = NULL;
    assert(1 == sds_to_json(sds_new("[]"), &root1));
    assert(root1 != NULL);
    assert(value_is_array(root1));
    assert(vector_size(value_get_array(root1)) == 0);
    value_delete(root1);


    assert(1 == sds_to_json(sds_new("[{}]"), &root1));
    assert(root1 != NULL);
    assert(value_is_array(root1) == 1);
    assert(vector_size(value_get_array(root1)) == 1);
    
    value_delete(root1);
    root1 = NULL;
    return 1;
}

int test_decode() {
    test_cond( "test decode int64", 
        test_decode_int() == 1);
    test_cond( "test decode bool", 
        test_decode_bool() == 1);
    test_cond( "test decode object", 
        test_decode_object() == 1);
    test_cond( "test decode list", 
        test_decode_list() == 1);
    return 1;
}

int test_api(void) {
    log_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        
        test_cond("json encode", 
            test_encode() == 1);
        test_cond("json decode", 
            test_decode() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}