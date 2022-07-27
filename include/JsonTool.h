/**
 * Created by zlhywlf on 2022/7/22.
 * 
 */
#ifndef C_JSON_JSON_TOOL_H
#define C_JSON_JSON_TOOL_H

#define JSON_VERSION_MAJOR 1
#define JSON_VERSION_MINOR 0
#define JSON_VERSION_PATCH 0

/**
 * json 数据类型
 */
typedef enum JsonType {
    J_NULL = 1 << 0,
    J_FALSE = 1 << 1,
    J_TRUE = 1 << 2,
    J_NUMBER = 1 << 3,
    J_STRING = 1 << 4,
    J_ARRAY = 1 << 5,
    J_OBJECT = 1 << 6
} JsonType;

/**
 * J_NULL --> Value 为 NULL
 * J_FALSE --> Value 为 NULL
 * J_TRUE --> Value 为 NULL
 * J_NUMBER --> Value.number
 * J_STRING --> Value.string
 * J_ARRAY --> Value.child , 当 [] 时 Value 为 NULL
 * J_OBJECT --> Value.child ,当 {} 时 Value 为 NULL
 */
typedef union JsonValue {
    struct Json *child;
    char *string;
    double number;
} JsonValue;

/**
 * json对象
 */
typedef struct Json {
    struct Json *next;
    struct Json *prev;
    JsonType type;
    char *key;
    JsonValue *value;
} Json;


/**
 * 创建一个JsonObj对象
 * @return JsonObj
 */
Json *jsonCreateJson(void);

/**
 * 创建一个JsonValue对象
 * @return JsonValue
 */
JsonValue *jsonCreateJsonValue(void);

/**
 * 创建 J_NULL 类型对象
 * @return JsonObj
 */
Json *jsonCreateNull(void);

/**
 * 创建 J_FALSE 类型对象
 * @return JsonObj
 */
Json *jsonCreateFalse(void);

/**
 * 创建 J_TRUE 类型对象
 * @return JsonObj
 */
Json *jsonCreateTrue(void);

/**
 * 创建 J_NUMBER 类型对象
 * @return JsonObj
 */
Json *jsonCreateNum(double num);

/**
 * 创建 J_STRING 类型对象
 * @return JsonObj
 */
Json *jsonCreateStr(const char *str);

/**
 * 创建 J_ARRAY 类型对象
 * @return JsonObj
 */
Json *jsonCreateArr(void);

/**
 * 创建 J_OBJECT 类型对象
 * @return JsonObj
 */
Json *jsonCreateObj(void);

/**
 * 向 J_ARRAY 类型对象添加成员
 * @return
 */
_Bool jsonAddJsonToArr(Json *arr, Json *json);

/**
 * 向 J_OBJECT 类型对象添加成员
 * @return
 */
_Bool jsonAddJsonToObj(Json *obj, const char *key, Json *json);

/**
 * 获取指定名称对象
 * @param object Json
 * @param name name
 * @return Json
 */
Json *jsonGet(const Json *json, const char *name);

/**
 * Json 对象输出到字符串
 * @param jsonObj Json
 * @return json字符串
 */
char *jsonPrint(Json *json);

/**
 * 删除JsonObj对象
 * @param Json Json
 */
void jsonDeleteJson(Json *obj);

/**
 * 删除输出的字符串
 * @param printStr char *
 */
void jsonDeletePrint(char *printStr);

/**
 * 解析
 * @param jsonStr json字符串
 * @return Json
 */
Json *jsonParse(const char *jsonStr);


#endif //C_JSON_JSON_TOOL_H
