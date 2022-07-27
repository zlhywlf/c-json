/**
 * Created by zlhywlf on 2022/7/22.
 * 
 */


#define bufferAtOffset(buffer) ((buffer)->jsonStr + (buffer)->offset)
#define bufferCanRead(buffer, size) ((buffer) && (((buffer)->offset + (size)) <= (buffer)->len))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "JsonTool.h"

#if (JSON_VERSION_MAJOR != 1) || (JSON_VERSION_MINOR != 0) || (JSON_VERSION_PATCH != 0)
#error JsonTool.h and JsonTool.c have different versions. Make sure that both have the same.
#endif

#ifndef isInf
#define isInf(d) (isNan(((d) - (d))) && !isNan((d)))
#endif
#ifndef isNan
#define isNan(d) ((d) != (d))
#endif

/**
 * 内存管理
 */
struct {
    void *(*allocate)(size_t sz);

    void (*deallocate)(void *ptr);

    void *(*reallocate)(void *ptr, size_t sz);
} hooks = {malloc, free, realloc};

/**
 * json栈
 */
typedef struct JsonStack {
    struct Json *json;
    JsonType type;
    struct JsonStack *next;
} JsonStack;

/**
 * 记录json解析时的相关数据
 */
typedef struct {
    unsigned char *jsonStr;
    size_t len;
    size_t offset;
} JsonBuffer;

/**
 * 创建对象
 * @param sz size
 * @return void *
 */
static void *createObj(size_t sz);

/**
 * 创建指定类型的json对象
 * @param type JsonType
 * @return Json
 */
static Json *createJsonItem(JsonType type);

/**
 * 转存字符串
 * @param string  char *
 * @return  char *
 */
static char *dumpStr(const char *str);

/**
 * 比较两个字符串是否相等,不区分大小写
 * @param string1
 * @param string2
 * @return
 */
static int caseInsensitiveStrcmp(const unsigned char *string1, const unsigned char *string2);

/**
 * 创建栈
 * @return JsonStack
 */
static JsonStack *createJsonStack(Json *j);

/**
 * 
 * 跳过无关字符
 * @param buffer JsonBuffer
 * @return JsonBuffer
 */
static JsonBuffer *parseBufferSkip(JsonBuffer *buffer);

/**
 * 解析json字符串value
 * @param item Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool parseValue(Json *json, JsonBuffer *buffer);

/**
 * 解析json key
 * @param item Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool parseKey(Json *json, JsonBuffer *buffer);

/**
 * 解析json {} [] 类型
 * @param item Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool parsing(JsonStack **stack, JsonBuffer *buffer);

/**
 * 解析json对象 {} [] 类型
 * @param item Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool printing(JsonStack **stack, JsonBuffer *buffer);

/**
 * 解析json字符串
 * @param item Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static char *parseString(JsonBuffer *buffer);

/**
 * 解析十六进制
 * @param input 十六进制字符串
 * @return unsigned int
 */
static unsigned parseHex4(const unsigned char *input);

/**
 * 解析 \uxxxx 类型字符
 * @param inputPointer 
 * @param inputEnd 
 * @param outputPointer 
 * @return 
 */
static unsigned char
utf16LiteralToUtf8(const unsigned char *inputPointer, const unsigned char *inputEnd, char unsigned **outputPointer);

/**
 * 更新偏移量
 * @param buffer JsonPrintBuffer
 */
static void updateOffset(JsonBuffer *buffer);

/**
 * 输出 Json->value 为字符串
 * @param json Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool printValue(const Json *json, JsonBuffer *buffer);

/**
 * 确保内存足够容纳输出的字符
 * @param buffer JsonBuffer
 * @param needed size_t
 * @return char *
 */
static unsigned char *ensure(JsonBuffer *buffer, size_t needed);

/**
 * 输出 Json->value->number 为字符串
 * @param json Json
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool printNumber(const Json *json, JsonBuffer *buffer);

/**
 * 输出 Json->value->string 为字符串
 * @param input char *
 * @param buffer JsonBuffer
 * @return _Bool
 */
static _Bool printString(const unsigned char *input, JsonBuffer *buffer);


Json *jsonCreateJson(void) {
    return (Json *) createObj(sizeof(Json));
}

JsonValue *jsonCreateJsonValue(void) {
    return (JsonValue *) createObj(sizeof(JsonValue));
}

Json *jsonCreateNull(void) {
    return createJsonItem(J_NULL);
}

Json *jsonCreateFalse(void) {
    return createJsonItem(J_FALSE);
}

Json *jsonCreateTrue(void) {
    return createJsonItem(J_TRUE);
}

Json *jsonCreateNum(double num) {
    Json *obj = createJsonItem(J_NUMBER);
    JsonValue *v = jsonCreateJsonValue();
    if (v) {
        v->number = num;
        obj->value = v;
        return obj;
    } else {
        hooks.deallocate(obj);
        return NULL;
    }
}

Json *jsonCreateStr(const char *str) {
    Json *obj = createJsonItem(J_STRING);
    JsonValue *v = jsonCreateJsonValue();
    if (v) {
        v->string = dumpStr(str);
        obj->value = v;
        return obj;
    } else {
        hooks.deallocate(obj);
        return NULL;
    }
}

Json *jsonCreateArr(void) {
    return createJsonItem(J_ARRAY);
}

Json *jsonCreateObj(void) {
    return createJsonItem(J_OBJECT);
}

_Bool jsonAddJsonToArr(Json *arr, Json *json) {
    if (!arr || !json || arr == json) {
        return false;
    }
    if (!arr->value) {
        JsonValue *p = jsonCreateJsonValue();
        if (p) {
            arr->value = p;
        } else {
            return false;
        }
    }
    Json **child = &arr->value->child;
    if (*child) {
        if ((*child)->prev) {
            (*child)->prev->next = json;
            json->prev = (*child)->prev;
            (*child)->prev = json;
        }
    } else {
        *child = json;
        json->prev = json;
    }
    return true;
}

_Bool jsonAddJsonToObj(Json *obj, const char *key, Json *json) {
    if (!obj || !json || !key || obj == json) {
        return false;
    }
    char *newKey = dumpStr(key);
    if (!newKey) {
        return false;
    }
    if (json->key) {
        hooks.deallocate(json->key);
    }
    json->key = newKey;
    return jsonAddJsonToArr(obj, json);
}

Json *jsonGet(const Json *json, const char *name) {
    if (!json || !name || !json->value) {
        return NULL;
    }
    Json *currentElement = NULL;
    currentElement = json->value->child;
    while (currentElement &&
           (caseInsensitiveStrcmp((const unsigned char *) name, (const unsigned char *) (currentElement->key)) != 0)) {
        currentElement = currentElement->next;
    }
    if (!currentElement || !currentElement->key) {
        return NULL;
    }
    return currentElement;
}

char *jsonPrint(Json *json) {
    if (!json || json->type != J_OBJECT) {
        return NULL;
    }
    static const size_t defaultBufferSize = 256;
    JsonBuffer buffer[1];
    unsigned char *printed = NULL;
    memset(buffer, 0, sizeof(buffer));
    buffer->jsonStr = (unsigned char *) hooks.allocate(defaultBufferSize);
    if (!buffer->jsonStr) {
        return NULL;
    }
    buffer->len = defaultBufferSize;
    unsigned char *outputPointer = ensure(buffer, 2);
    if (!outputPointer) {
        return false;
    }
    *outputPointer++ = '{';
    if (json->value) {
        JsonStack *stack = createJsonStack(json->value->child);
        JsonStack *nextStack;
        stack->type = J_OBJECT;
        while (stack) {
            if (!printing(&stack, buffer)) {
                hooks.deallocate(buffer->jsonStr);
                while (stack) {
                    nextStack = stack->next;
                    hooks.deallocate(stack);
                    stack = nextStack;
                }
                return NULL;
            }
        }
    } else {
        *outputPointer++ = '}';
        buffer->offset += 2;
    }
    outputPointer = ensure(buffer, 1);
    *outputPointer++ = '\0';
    printed = (unsigned char *) hooks.reallocate(buffer->jsonStr, buffer->offset + 1);
    if (printed == NULL) {
        hooks.deallocate(buffer->jsonStr);
        return NULL;
    }
    return (char *) printed;
}

void jsonDeleteJson(Json *obj) {
    if (!obj) {
        return;
    }
    JsonStack *stack = createJsonStack(obj);
    JsonStack *nextStack;
    outer:
    while (stack) {
        obj = stack->json;
        Json *next;
        while (obj) {
            next = obj->next;
            if (obj->value) {
                if (obj->value->child && (obj->type == J_OBJECT || obj->type == J_ARRAY)) {
                    JsonType type = obj->value->child->type;
                    if (type == J_NULL || type == J_FALSE || type == J_TRUE || type == J_NUMBER ||
                        type == J_STRING || type == J_ARRAY || type == J_OBJECT) {
                        nextStack = createJsonStack(obj->value->child);
                        nextStack->next = stack;
                        stack = nextStack;
                        hooks.deallocate(obj->value);
                        obj->value = NULL;
                        goto outer;
                    }
                }
                if (obj->value->string && obj->type == J_STRING) {
                    hooks.deallocate(obj->value->string);
                }
                hooks.deallocate(obj->value);
            }
            if (obj->key) {
                hooks.deallocate(obj->key);
            }
            hooks.deallocate(obj);
            obj = next;
            stack->json = next;
        }
        nextStack = stack->next;
        hooks.deallocate(stack);
        stack = nextStack;
    }

}

void jsonDeletePrint(char *printStr) {
    hooks.deallocate(printStr);
}

Json *jsonParse(const char *jsonStr) {
    if (!jsonStr) {
        return NULL;
    }
    size_t bufferLen = strlen(jsonStr);
    if (bufferLen == 0) {
        return NULL;
    }
    JsonBuffer buffer = {(unsigned char *) jsonStr, bufferLen, 0};
    if (bufferCanRead(&buffer, 4) &&
        (strncmp((const char *) bufferAtOffset(&buffer), "\xEF\xBB\xBF", 3) == 0)) {
        buffer.offset += 3;
    }
    parseBufferSkip(&buffer);
    // 根对象一定是{}类型
    if (bufferCanRead(&buffer, 0) && (*bufferAtOffset(&buffer) == '{')) {
        Json *json = jsonCreateObj();
        if (!json) {
            return NULL;
        }
        buffer.offset++;
        parseBufferSkip(&buffer);
        // 空对象
        if (bufferCanRead(&buffer, 0) && (*bufferAtOffset(&buffer) == '}')) {
            buffer.offset++;
            return json;
        }
        JsonValue *value = jsonCreateJsonValue();
        if (!value) {
            jsonDeleteJson(json);
            return NULL;
        }
        json->value = value;
        // 入栈
        JsonStack *stack = createJsonStack(json);
        JsonStack *nextStack;
        while (stack) {
            if (!parsing(&stack, &buffer)) {
                jsonDeleteJson(json);
                while (stack) {
                    nextStack = stack->next;
                    hooks.deallocate(stack);
                    stack = nextStack;
                }
                return NULL;
            }
        }
        return json;
    }
    return NULL;
}

static void *createObj(size_t sz) {
    void *p = hooks.allocate(sz);
    if (p) {
        memset(p, 0, sz);
    }
    return p;
}

static Json *createJsonItem(JsonType type) {
    Json *obj = jsonCreateJson();
    if (obj) {
        obj->type = type;
    }
    return obj;
}

static char *dumpStr(const char *str) {
    if (!str) {
        return NULL;
    }
    char *cp = (char *) hooks.allocate(strlen(str) + 1);
    if (cp) {
        strcpy(cp, str);
    }
    return cp;
}

static int caseInsensitiveStrcmp(const unsigned char *string1, const unsigned char *string2) {
    if (string2 == NULL) {
        return 1;
    }
    if (string1 == string2) {
        return 0;
    }
    for (; tolower(*string1) == tolower(*string2); (void) string1++, string2++) {
        if (*string1 == '\0') {
            return 0;
        }
    }
    return tolower(*string1) - tolower(*string2);
}

static JsonStack *createJsonStack(Json *json) {
    JsonStack *stack = (JsonStack *) createObj(sizeof(JsonStack));
    if (!stack) {
        return NULL;
    }
    stack->json = json;
    return stack;
}

static JsonBuffer *parseBufferSkip(JsonBuffer *buffer) {
    if (!buffer || !buffer->jsonStr) {
        return buffer;
    }
    if (!bufferCanRead(buffer, 0)) {
        return buffer;
    }
    while (bufferCanRead(buffer, 0) && (*bufferAtOffset(buffer) <= 32)) {
        buffer->offset++;
    }
    if (buffer->offset == buffer->len) {
        buffer->offset--;
    }
    return buffer;
}

static _Bool parseValue(Json *json, JsonBuffer *buffer) {
    if (!buffer->jsonStr) {
        return false;
    }
    // null
    if (bufferCanRead(buffer, 4) && (strncmp((const char *) bufferAtOffset(buffer), "null", 4) == 0)) {
        json->type = J_NULL;
        buffer->offset += 4;
        return true;
    }
    // false
    if (bufferCanRead(buffer, 5) && (strncmp((const char *) bufferAtOffset(buffer), "false", 5) == 0)) {
        json->type = J_FALSE;
        buffer->offset += 5;
        return true;
    }
    // true
    if (bufferCanRead(buffer, 4) && (strncmp((const char *) bufferAtOffset(buffer), "true", 4) == 0)) {
        json->type = J_TRUE;
        buffer->offset += 4;
        return true;
    }
    JsonValue *newValue = jsonCreateJsonValue();
    if (!newValue) {
        return false;
    }
    json->value = newValue;
    // string
    if (bufferCanRead(buffer, 0) && (*bufferAtOffset(buffer) == '"')) {
        char *string = parseString(buffer);
        if (string) {
            json->type = J_STRING;
            json->value->string = string;
            return true;
        }
        return false;
    }
    // number
    if (bufferCanRead(buffer, 0) && ((*bufferAtOffset(buffer) == '-') ||
                                     ((*bufferAtOffset(buffer) >= '0') &&
                                      (*bufferAtOffset(buffer) <= '9')))) {
        double number;
        unsigned char *afterEnd = NULL;
        unsigned char numberStr[64];
        unsigned char decimalPoint = (unsigned char) '.';
        size_t i = 0;
        _Bool flag = true;
        while (flag && (i < (sizeof(numberStr) - 1)) && bufferCanRead(buffer, i)) {
            switch (bufferAtOffset(buffer)[i]) {
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
                case '+':
                case '-':
                case 'e':
                case 'E':
                    numberStr[i] = bufferAtOffset(buffer)[i];
                    break;
                case '.':
                    numberStr[i] = decimalPoint;
                    break;
                default:
                    flag = false;
                    i--;
            }
            i++;
        }
        numberStr[i] = '\0';
        number = strtod((const char *) numberStr, (char **) &afterEnd);
        if (numberStr == afterEnd) {
            return false;
        }
        json->value->number = number;
        json->type = J_NUMBER;
        buffer->offset += afterEnd - numberStr;
        return true;
    }
    return false;
}

static _Bool parseKey(Json *json, JsonBuffer *buffer) {
    char *key = parseString(parseBufferSkip(buffer));
    if (key) {
        json->key = key;
        return true;
    }
    return NULL;
}

static _Bool parsing(JsonStack **stack, JsonBuffer *buffer) {
    JsonStack *nextStack;
    Json *head = NULL;
    Json *stackJson = (*stack)->json;
    char endChar;
    if (stackJson->type == J_OBJECT) {
        endChar = '}';
    } else if (stackJson->type == J_ARRAY) {
        endChar = ']';
    } else {
        return false;
    }
    if (bufferCanRead(buffer, 0) && (bufferAtOffset(buffer)[0] == endChar)) {
        buffer->offset++;
        parseBufferSkip(buffer);
        if (bufferCanRead(buffer, 0) &&
            (bufferAtOffset(buffer)[0] != '}') &&
            (bufferAtOffset(buffer)[0] != ']')) {
            buffer->offset++;
            parseBufferSkip(buffer);
        }
        nextStack = (*stack)->next;
        hooks.deallocate(*stack);
        *stack = nextStack;
        return true;
    }
    buffer->offset--;
    Json *currentItem = NULL;
    if (stackJson->value && stackJson->value->child) {
        head = stackJson->value->child;
        currentItem = head->prev;
    }
    do {
        Json *newItem = jsonCreateObj();
        if (!newItem) {
            jsonDeleteJson(head);
            return false;
        }
        if (head == NULL) {
            currentItem = head = newItem;
        } else {
            currentItem->next = newItem;
            newItem->prev = currentItem;
            currentItem = newItem;
        }
        buffer->offset++;
        if (endChar == '}') {
            if (!parseKey(currentItem, parseBufferSkip(buffer))) {
                jsonDeleteJson(head);
                return false;
            }
            parseBufferSkip(buffer);
            if (!bufferCanRead(buffer, 0) || (bufferAtOffset(buffer)[0] != ':')) {
                jsonDeleteJson(head);
                return false;
            }
            buffer->offset++;
            parseBufferSkip(buffer);
        }
        if (bufferCanRead(buffer, 0)) {
            if (bufferAtOffset(buffer)[0] == '{') {
                currentItem->type = J_OBJECT;
            }
            if (bufferAtOffset(buffer)[0] == '[') {
                currentItem->type = J_ARRAY;
            }
            if (bufferAtOffset(buffer)[0] == '{' || bufferAtOffset(buffer)[0] == '[') {
                head->prev = currentItem;
                if (!stackJson->value) {
                    JsonValue *v = jsonCreateJsonValue();
                    if (!v) {
                        jsonDeleteJson(head);
                        return false;
                    }
                    stackJson->value = v;
                }
                stackJson->value->child = head;
                nextStack = createJsonStack(currentItem);
                nextStack->next = *stack;
                *stack = nextStack;
                buffer->offset++;
                parseBufferSkip(buffer);
                return true;
            }
        }
        if (!parseValue(currentItem, buffer)) {
            jsonDeleteJson(head);
            return false;
        }
        parseBufferSkip(buffer);
    } while (bufferCanRead(buffer, 0) && (bufferAtOffset(buffer)[0] == ','));
    if ((!bufferCanRead(buffer, 0)) || (bufferAtOffset(buffer)[0] != endChar)) {
        jsonDeleteJson(head);
        return false;
    }
    head->prev = currentItem;
    if (!stackJson->value) {
        JsonValue *v = jsonCreateJsonValue();
        if (!v) {
            jsonDeleteJson(head);
            return false;
        }
        stackJson->value = v;
    }
    stackJson->value->child = head;
    buffer->offset++;
    parseBufferSkip(buffer);
    if (bufferCanRead(buffer, 0) && (bufferAtOffset(buffer)[0] == ',')) {
        buffer->offset++;
        parseBufferSkip(buffer);
    }
    nextStack = (*stack)->next;
    hooks.deallocate(*stack);
    *stack = nextStack;
    return true;
}

static _Bool printing(JsonStack **stack, JsonBuffer *buffer) {
    if (!buffer) {
        return false;
    }
    JsonStack *nextStack;
    Json *json = (*stack)->json;
    unsigned char *outputPointer = ensure(buffer, 0);

    if (json) {
        buffer->offset++;
        Json *currentItem = json;
        while (currentItem) {
            if ((*stack)->type == J_OBJECT) {
                if (!printString((unsigned char *) currentItem->key, buffer)) {
                    return false;
                }
                updateOffset(buffer);
                outputPointer = ensure(buffer, 1);
                if (!outputPointer) {
                    return false;
                }
                *outputPointer++ = ':';
                buffer->offset++;
            }
            if (currentItem->value || currentItem->type == J_NULL || currentItem->type == J_TRUE ||
                currentItem->type == J_FALSE) {
                if (currentItem->type == J_OBJECT || currentItem->type == J_ARRAY) {
                    outputPointer = ensure(buffer, 2);
                    if (!outputPointer) {
                        return false;
                    }
                    nextStack = createJsonStack(currentItem->value->child);
                    if (currentItem->type == J_OBJECT) {
                        *outputPointer++ = '{';
                        nextStack->type = J_OBJECT;
                    } else if (currentItem->type == J_ARRAY) {
                        *outputPointer++ = '[';
                        nextStack->type = J_ARRAY;
                    }
                    (*stack)->json = currentItem->next;
                    nextStack->next = *stack;
                    *stack = nextStack;
                    return true;
                }
                if (!printValue(currentItem, buffer)) {
                    return false;
                }
                updateOffset(buffer);
            } else {
                outputPointer = ensure(buffer, 2);
                if (currentItem->type == J_OBJECT) {
                    *outputPointer++ = '{';
                    *outputPointer++ = '}';
                } else if (currentItem->type == J_ARRAY) {
                    *outputPointer++ = '[';
                    *outputPointer++ = ']';
                }
                buffer->offset += 2;
            }
            outputPointer = ensure(buffer, 1);
            if (outputPointer == NULL) {
                return false;
            }
            if (currentItem->next) {
                *outputPointer++ = ',';
                buffer->offset++;
            }
            currentItem = currentItem->next;
        }
    }
    outputPointer = ensure(buffer, 1);
    if ((*stack)->type == J_OBJECT) {
        *outputPointer++ = '}';
    } else if ((*stack)->type == J_ARRAY) {
        *outputPointer++ = ']';
    }
    buffer->offset++;
    nextStack = (*stack)->next;
    if (nextStack && nextStack->json) {
        outputPointer = ensure(buffer, 1);
        *outputPointer++ = ',';
    }
    hooks.deallocate(*stack);
    *stack = nextStack;
    return true;
}

static char *parseString(JsonBuffer *buffer) {
    // 从 " 后第一个字符开始
    const unsigned char *inputPointer = bufferAtOffset(buffer) + 1;
    const unsigned char *inputEnd = bufferAtOffset(buffer) + 1;
    size_t skippedBytes = 0;
    // 字符串必须以 " 结尾,且应小于 buffer 的长度(含结束标识 \0)
    while (((inputEnd - buffer->jsonStr) < buffer->len) && (*inputEnd != '"')) {
        // \",\\,\/,\b,\f,\n,\r,\t,\u4f60
        // 转义字符将跳过,例如 \\ 长度为1
        if (*inputEnd == '\\') {
            skippedBytes++;
            // 出现转义字符,必有一个字符跟随在 \ 后
            inputEnd++;
        }
        inputEnd++;
    }
    if (((inputEnd - buffer->jsonStr) >= buffer->len) || (*inputEnd != '"')) {
        // 当前出错位置即在 " 后第一个字符开始
        buffer->offset++;
        return NULL;
    }
    size_t allocationLen = inputEnd - bufferAtOffset(buffer) - skippedBytes + 1;
    unsigned char *output = (unsigned char *) hooks.allocate(allocationLen);
    if (output == NULL) {
        // 当前出错位置即在 " 后第一个字符开始
        buffer->offset++;
        return NULL;
    }
    unsigned char *outputPointer = output;
    while (inputPointer < inputEnd) {
        if (*inputPointer != '\\') {
            *outputPointer++ = *inputPointer++;
        } else {
            unsigned char sequenceLength = 2;
            if ((inputEnd - inputPointer) < 1) {
                hooks.deallocate(output);
                buffer->offset = inputPointer - buffer->jsonStr;
                return NULL;
            }
            switch (inputPointer[1]) {
                case 'b':
                    *outputPointer++ = '\b';
                    break;
                case 'f':
                    *outputPointer++ = '\f';
                    break;
                case 'n':
                    *outputPointer++ = '\n';
                    break;
                case 'r':
                    *outputPointer++ = '\r';
                    break;
                case 't':
                    *outputPointer++ = '\t';
                    break;
                case '"':
                case '\\':
                    *outputPointer++ = inputPointer[1];
                    break;
                case 'u':
                    sequenceLength = utf16LiteralToUtf8(inputPointer, inputEnd, &outputPointer);
                    if (sequenceLength == 0) {
                        hooks.deallocate(output);
                        buffer->offset = inputPointer - buffer->jsonStr;
                        return NULL;
                    }
                    break;
                default:
                    hooks.deallocate(output);
                    buffer->offset = inputPointer - buffer->jsonStr;
                    return NULL;
            }
            inputPointer += sequenceLength;
        }
    }
    *outputPointer = '\0';
    buffer->offset = inputEnd - buffer->jsonStr + 1;
    return (char *) output;
}

static unsigned parseHex4(const unsigned char *input) {
    unsigned int h = 0;
    size_t i;
    for (i = 0; i < 4; i++) {
        if ((input[i] >= '0') && (input[i] <= '9')) {
            h += (unsigned int) input[i] - '0';
        } else if ((input[i] >= 'A') && (input[i] <= 'F')) {
            h += (unsigned int) 10 + input[i] - 'A';
        } else if ((input[i] >= 'a') && (input[i] <= 'f')) {
            h += (unsigned int) 10 + input[i] - 'a';
        } else {
            return 0;
        }
        if (i < 3) {
            h = h << 4;
        }
    }
    return h;
}

static unsigned char
utf16LiteralToUtf8(const unsigned char *inputPointer, const unsigned char *inputEnd, char unsigned **outputPointer) {
    const unsigned char *firstSequence = inputPointer;
    if ((inputEnd - firstSequence) < 6) {
        return 0;
    }
    unsigned int firstCode = parseHex4(firstSequence + 2);
    if (((firstCode >= 0xDC00) && (firstCode <= 0xDFFF))) {
        return 0;
    }
    unsigned char sequenceLen;
    long unsigned int codepoint;
    if ((firstCode >= 0xD800) && (firstCode <= 0xDBFF)) {
        const unsigned char *second_sequence = firstSequence + 6;
        unsigned int second_code;
        sequenceLen = 12;
        if ((inputEnd - second_sequence) < 6) {
            return 0;
        }
        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u')) {
            return 0;
        }
        second_code = parseHex4(second_sequence + 2);
        if ((second_code < 0xDC00) || (second_code > 0xDFFF)) {
            return 0;
        }
        codepoint = 0x10000 + (((firstCode & 0x3FF) << 10) | (second_code & 0x3FF));
    } else {
        sequenceLen = 6;
        codepoint = firstCode;
    }
    unsigned char utf8Len;
    unsigned char firstByteMark = 0;
    if (codepoint < 0x80) {
        utf8Len = 1;
    } else if (codepoint < 0x800) {
        utf8Len = 2;
        firstByteMark = 0xC0;
    } else if (codepoint < 0x10000) {
        utf8Len = 3;
        firstByteMark = 0xE0;
    } else if (codepoint <= 0x10FFFF) {
        utf8Len = 4;
        firstByteMark = 0xF0;
    } else {
        return 0;
    }
    unsigned char utf8Pos;
    for (utf8Pos = (unsigned char) (utf8Len - 1); utf8Pos > 0; utf8Pos--) {
        (*outputPointer)[utf8Pos] = (unsigned char) ((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    if (utf8Len > 1) {
        **outputPointer = (unsigned char) ((codepoint | firstByteMark) & 0xFF);
    } else {
        **outputPointer = (unsigned char) (codepoint & 0x7F);
    }
    *outputPointer += utf8Len;
    return sequenceLen;
}

static void updateOffset(JsonBuffer *buffer) {
    if (buffer->jsonStr == NULL) {
        return;
    }
    const unsigned char *bufferPointer = buffer->jsonStr + buffer->offset;
    buffer->offset += strlen((const char *) bufferPointer);
}

static _Bool printValue(const Json *json, JsonBuffer *buffer) {
    unsigned char *output = NULL;
    switch ((json->type) & 0xFF) {
        case J_NULL:
            output = ensure(buffer, 5);
            if (output == NULL) {
                return false;
            }
            strcpy((char *) output, "null");
            return true;
        case J_FALSE:
            output = ensure(buffer, 6);
            if (output == NULL) {
                return false;
            }
            strcpy((char *) output, "false");
            return true;
        case J_TRUE:
            output = ensure(buffer, 5);
            if (output == NULL) {
                return false;
            }
            strcpy((char *) output, "true");
            return true;
        case J_NUMBER:
            return printNumber(json, buffer);

        case J_STRING:
            return printString((unsigned char *) json->value->string, buffer);
        default:
            return false;
    }
}

static unsigned char *ensure(JsonBuffer *buffer, size_t needed) {
    unsigned char *newBuffer = NULL;
    size_t newSize;
    if ((buffer->len > 0) && (buffer->offset >= buffer->len)) {
        return NULL;
    }
    if (needed > INT_MAX) {
        return NULL;
    }
    needed += buffer->offset + 1;
    if (needed <= buffer->len) {
        return buffer->jsonStr + buffer->offset;
    }
    if (needed > (INT_MAX / 2)) {
        newSize = INT_MAX;
    } else {
        newSize = needed * 2;
    }
    newBuffer = (unsigned char *) hooks.reallocate(buffer->jsonStr, newSize);
    if (newBuffer == NULL) {
        hooks.deallocate(buffer->jsonStr);
        buffer->len = 0;
        buffer->jsonStr = NULL;
        return NULL;
    }
    buffer->len = newSize;
    buffer->jsonStr = newBuffer;
    return newBuffer + buffer->offset;
}

static _Bool printNumber(const Json *json, JsonBuffer *buffer) {
    unsigned char *outputPointer = NULL;
    double d = json->value->number;
    int length;
    size_t i;
    unsigned char numberBuffer[26] = {0};
    unsigned char decimalPoint = '.';
    if (isNan(d) || isInf(d)) {
        length = sprintf((char *) numberBuffer, "null");
    } else if (d == (int) json->value->number) {
        length = sprintf((char *) numberBuffer, "%g", json->value->number);
    } else {
        length = sprintf((char *) numberBuffer, "%1.17g", d);
    }
    if ((length < 0) || (length > (int) (sizeof(numberBuffer) - 1))) {
        return false;
    }
    outputPointer = ensure(buffer, (size_t) length + sizeof(""));
    if (outputPointer == NULL) {
        return false;
    }
    for (i = 0; i < ((size_t) length); i++) {
        if (numberBuffer[i] == decimalPoint) {
            outputPointer[i] = '.';
            continue;
        }
        outputPointer[i] = numberBuffer[i];
    }
    outputPointer[i] = '\0';
    buffer->offset += (size_t) length;
    return true;
}

static _Bool printString(const unsigned char *input, JsonBuffer *buffer) {
    const unsigned char *inputPointer = NULL;
    unsigned char *output = NULL;
    unsigned char *outputPointer = NULL;
    size_t outputLength;
    size_t escapeCharacters = 0;
    if (input == NULL) {
        output = ensure(buffer, sizeof("\"\""));
        if (output == NULL) {
            return false;
        }
        strcpy((char *) output, "\"\"");
        return true;
    }
    for (inputPointer = input; *inputPointer; inputPointer++) {
        switch (*inputPointer) {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                escapeCharacters++;
                break;
            default:
                if (*inputPointer < 32) {
                    escapeCharacters += 5;
                }
                break;
        }
    }
    outputLength = (size_t) (inputPointer - input) + escapeCharacters;
    output = ensure(buffer, outputLength + sizeof("\"\""));
    if (output == NULL) {
        return false;
    }
    if (escapeCharacters == 0) {
        output[0] = '\"';
        memcpy(output + 1, input, outputLength);
        output[outputLength + 1] = '\"';
        output[outputLength + 2] = '\0';

        return true;
    }
    output[0] = '\"';
    outputPointer = output + 1;
    /* copy the string */
    for (inputPointer = input; *inputPointer != '\0'; (void) inputPointer++, outputPointer++) {
        if ((*inputPointer > 31) && (*inputPointer != '\"') && (*inputPointer != '\\')) {
            *outputPointer = *inputPointer;
        } else {
            *outputPointer++ = '\\';
            switch (*inputPointer) {
                case '\\':
                    *outputPointer = '\\';
                    break;
                case '\"':
                    *outputPointer = '\"';
                    break;
                case '\b':
                    *outputPointer = 'b';
                    break;
                case '\f':
                    *outputPointer = 'f';
                    break;
                case '\n':
                    *outputPointer = 'n';
                    break;
                case '\r':
                    *outputPointer = 'r';
                    break;
                case '\t':
                    *outputPointer = 't';
                    break;
                default:
                    sprintf((char *) outputPointer, "u%04x", *inputPointer);
                    outputPointer += 4;
                    break;
            }
        }
    }
    output[outputLength + 1] = '\"';
    output[outputLength + 2] = '\0';
    return true;
}