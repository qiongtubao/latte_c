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
    

    value* root = jsonMapCreate();
    jsonMapPutSds(root, sdsnew("name"), sdsnew("John Doe"));
    jsonMapPutInt64(root, sdsnew("age"), 30);
    jsonMapPutBool(root, "is_student", true);
    jsonMapPutLongDouble(root, "memory", 3.1435926);
    value* vec = jsonListCreate();
    // valueListResize(vec, 5);
    jsonListPutSds(vec, sdsnew("readubg"));
    jsonMapPutValue(root, sdsnew("hobbies"), vec);
    value* list =jsonMapGet(root, "hobbies");
    assert(list == vec);

    jsonListPutInt64(list, 100);
    jsonListPutLongDouble(list, 1.23456);
    jsonListPutBool(list, false);

    sds result = jsonEncode(root);
    printf("json: %s\n", result);

    

    
    
    return 1;

}

value* getMapTestValue(value* root) {
    assert(root->type == MAPTYPES);
    dictEntry* entry = dictFind(root->value.map_value, "test");
    assert(entry != NULL);
    return dictGetVal(entry);
}
int test_decode_int() {
    value* root1 = NULL;
    assert(1 == jsonDecode(sdsnew("{\"test\":12345}"), &root1));
    assert(root1 != NULL);
    value* test = getMapTestValue(root1);
    assert(test != NULL);
    assert(test->type == INTS);
    assert(test->value.ll_value == 12345);
    valueRelease(root1);
    root1 = NULL;

    assert(1 == jsonDecode(sdsnew("{\"test\":9223372036854775809, \"test1\":12345}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(test->type == UINTS);
    assert(test->value.ull_value == 9223372036854775809ULL);
    valueRelease(root1);
    root1 = NULL;

    assert(1 == jsonDecode(sdsnew("{\"test\":1.2345, \"test1\":12345}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(test->type == DOUBLES);
    assert(test->value.ld_value - 1.2345 < 0.0001);
    valueRelease(root1);
    root1 = NULL;
    return 1;
}

int test_decode_object() {
    value* root1 = NULL;
    assert(1 == jsonDecode(sdsnew("{}"), &root1));
    assert(root1 != NULL);
    assert(root1->type == MAPTYPES);
    assert(dictSize(root1->value.map_value) == 0);
    valueRelease(root1);
    root1 = NULL;
    return 1;
}

int test_decode_bool() {
    value* root1 = NULL;
    assert(1 == jsonDecode(sdsnew("{\"test\":true}"), &root1));
    assert(root1 != NULL);
    value* test = getMapTestValue(root1);
    assert(test != NULL);
    assert(test->type == BOOLEANS);
    assert(test->value.bool_value == true);
    valueRelease(root1);
    root1 = NULL;

    assert(1 == jsonDecode(sdsnew("{\"test\":false}"), &root1));
    assert(root1 != NULL);
    test = getMapTestValue(root1);
    assert(test != NULL);
    assert(test->type == BOOLEANS);
    assert(test->value.bool_value == false);
    valueRelease(root1);
    root1 = NULL;

    return 1;
}



int test_decode_list() {
    value* root1 = NULL;
    assert(1 == jsonDecode(sdsnew("[]"), &root1));
    assert(root1 != NULL);
    assert(root1->type == LISTTYPS);
    assert(vectorSize(root1->value.list_value) == 0);
    valueRelease(root1);


    assert(1 == jsonDecode(sdsnew("[{}]"), &root1));
    assert(root1 != NULL);
    assert(root1->type == LISTTYPS);
    assert(vectorSize(root1->value.list_value) == 1);
    
    valueRelease(root1);
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
    initLogger();
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