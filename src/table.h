#ifndef liss_table_h
#define liss_table_h

#include <stddef.h>

#include "common.h"
#include "value.h"

typedef struct ObjString ObjString;

typedef struct TableEntry {
    Value key;
    Value value;
    struct TableEntry* next;
} TableEntry;

typedef struct {
    TableEntry** buckets;
    size_t bucket_count;
    size_t old_bucket_count;
    size_t size;
} Table;

void initTable(Table* table);
void initTableWithCapacity(Table* table, size_t capacity);
void freeTable(Table* table);

void tableInsert(Table* table, Value key, Value value);
void tableRemove(Table* table, Value key);
Value* tableGet(Table* table, Value key);
ObjString* tableFindString(Table* table, const char* chars, int length,
                           uint32_t hash);

#endif
