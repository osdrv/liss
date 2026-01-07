#include "table.h"

#include <stdlib.h>
#include <string.h>

#include "minunit.h"
#include "value.h"

static char* test_table_initTable(void) {
    Table table;
    initTable(&table);

    mu_assert("Table bucket count should be 0", table.bucket_count == 0);
    mu_assert("Table size should be 0", table.size == 0);
    mu_assert("Table buckets should be NULL", table.buckets == NULL);

    freeTable(&table);
    return NULL;
}

static char* test_table_insert_and_get(void) {
    Table table;
    initTable(&table);

    Value key = NUMBER_VAL(42.0);
    Value value = NUMBER_VAL(3.14);

    tableInsert(&table, key, value);

    Value* retrieved = tableGet(&table, key);
    mu_assert("Retrieved value should not be NULL", retrieved != NULL);
    mu_assert("Retrieved value should equal inserted value",
              valuesEqual(*retrieved, value));

    freeTable(&table);
    return NULL;
}

static ObjString* newObjString(const char* chars, int length) {
    ObjString* string = malloc(sizeof(ObjString));
    string->obj.type = OBJ_STRING;
    string->length = length;
    string->chars = malloc(length + 1);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hashString(chars, length);
    return string;
}

static Value newObjValue(Obj* obj) {
    Value value;
    value.type = VAL_OBJ;
    value.as.obj = obj;
    return value;
}

static char* test_table_insert_string_obj(void) {
    Table table;
    initTable(&table);

    Value key = NUMBER_VAL(1.0);
    ObjString* obj_str = newObjString("foo", 4);
    Value value = OBJ_VAL(obj_str);

    tableInsert(&table, key, value);

    Value* retrieved = tableGet(&table, key);
    mu_assert("Retrieved value should not be NULL", retrieved != NULL);
    mu_assert("Retrieved value should equal inserted object value",
              valuesEqual(*retrieved, value));

    free(obj_str->chars);
    free(obj_str);
    freeTable(&table);
    return NULL;
}

static char* test_table_insert_and_get_nonexistent_key(void) {
    Table table;
    initTable(&table);

    Value key = NUMBER_VAL(100.0);
    Value value = NUMBER_VAL(1.23);

    tableInsert(&table, key, value);

    Value nonexistent_key = NUMBER_VAL(200.0);
    Value* retrieved = tableGet(&table, nonexistent_key);
    mu_assert("Retrieved value for nonexistent key should be NULL",
              retrieved == NULL);

    freeTable(&table);
    return NULL;
}

static char* test_table_remove_key(void) {
    Table table;
    initTable(&table);

    Value key = NUMBER_VAL(50.0);
    Value value = NUMBER_VAL(5.55);

    tableInsert(&table, key, value);

    Value* retrieved = tableGet(&table, key);
    mu_assert("Retrieved value should not be NULL before removal",
              retrieved != NULL);

    tableRemove(&table, key);

    retrieved = tableGet(&table, key);
    mu_assert("Retrieved value should be NULL after removal",
              retrieved == NULL);

    freeTable(&table);
    return NULL;
}

void table_suite(void) {
    printf("--- Table Suite ---\n");
    mu_run_test(test_table_initTable);
    mu_run_test(test_table_insert_and_get);
    mu_run_test(test_table_insert_and_get_nonexistent_key);
    mu_run_test(test_table_remove_key);
}
