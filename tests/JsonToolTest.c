/**
 * Created by zlhywlf on 2022/7/22.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "JsonTool.h"

int main(void) {
    int i = 0;
    char *string;
    while (i++ < INT_MAX) {

        Json *pNull = jsonCreateNull();

        Json *pFalse = jsonCreateFalse();

        Json *pTrue = jsonCreateTrue();

        Json *pNum = jsonCreateNum(12);
        printf("%d", pNum->value->number);

        Json *pStr = jsonCreateStr("str123str");

        Json *pArr = jsonCreateArr();

        Json *pObj = jsonCreateObj();

        jsonAddJsonToArr(pArr, jsonCreateNum(12));
        jsonAddJsonToArr(pArr, pFalse);
        jsonAddJsonToArr(pArr, pTrue);
        jsonAddJsonToObj(pObj, "01", pNull);
        jsonAddJsonToObj(pObj, "02", pNum);
        jsonAddJsonToObj(pObj, "03", pStr);
        jsonAddJsonToObj(pObj, "04", pArr);
        string = jsonPrint(pObj);
        printf("\n%s\n", string);
        jsonDeletePrint(string);

        jsonDeleteJson(pObj);

        Json *pJson = jsonParse(
                "{   \"J_OBJECT01\":{},\"J_OBJECT02\":    {\"key02\"    :\"value02\"},\"J_OBJECT04\":{\"key04\":{}},\"J_OBJECT06\":{\"key06\":[]},\"J_OBJECT08\":{\"key08\":[{\"key0801\":[]},{\"key0802\":[]}]},\"J_STRING01\":\"中文\",\"J_STRING02\":\"\\\",\\\\,/,\\b,\\f,\\n,\\r,\\t,你\",\"J_STRING03\":\"\",\"J_ARRAY01\":[],\"J_ARRAY02\":[1,true,\"str\"],\"J_ARRAY03\":[{}],\"J_ARRAY05\":[[]],\"J_TRUE\":true,\"J_FALSE\":false,\"J_NULL\":null,\"J_NUMBER01\":0,\"J_NUMBER02\":10000,\"J_NUMBER03\":10000,\"J_NUMBER04\":1.4,\"J_NUMBER05\":-1.4,\"J_NUMBER06\":0.0001,\"J_NUMBER07\":10000,\"J_NUMBER08\":-10000,\"exrguspmizq\":[{\"cgrlyudg\":[[false]],\"rhgqatzo\":[[507582299.676765]],\"jynywavztus\":true},{\"lsempgwgoxk\":514711473},false]}");
        string = jsonPrint(pJson);
        Json *parse = jsonParse(string);
        Json *get = jsonGet(pJson, "J_TRUE");
        jsonDeleteJson(parse);
        printf("\n%s\n", string);
        jsonDeleteJson(pJson);
        jsonDeletePrint(string);
        char *s = "\uF000";
        printf("s=%d  %s\n", strlen(s), s);
        printf("%d\n", i);
    }
    return 0;

}

