#include "table.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "object.h"

// #define max(a, b)               \
//     ({                          \
//         __typeof__(a) _a = (a); \
//         __typeof__(b) _b = (b); \
//         _a > _b ? _a : _b;      \
//     })

#define TABLE_GROWTH_FACTOR 2
#define TABLE_INITIAL_BUCKET_COUNT 8
#define TABLE_MAX_LOAD 0.75
#define TABLE_REHASH_STEP 4

static size_t hashValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            return AS_BOOL(value) ? 1 : 0;
        case VAL_NIL:
            return 0;
        case VAL_NUMBER: {
            double num = AS_NUMBER(value);
            return (size_t)((int)num * 2654435761 %
                            4294967296);  // Knuth's multiplicative hash
        }
        case VAL_OBJ:
            switch
                OBJ_TYPE(value) {
                    case OBJ_STRING: {
                        ObjString* string = AS_STRING(value);
                        return string->hash;
                    }
                    default:
                        break;
                }
            // TODO: Implement a better hash function for objects
            return (size_t)AS_OBJ(value);  // Use pointer value as hash
        default:
            return 0;
    }
}

void initTable(Table* table) {
    table->buckets = NULL;
    table->bucket_count = 0;
    table->size = 0;
}

static void growTable(Table* table) {
    size_t old_bucket_count = table->bucket_count;
    TableEntry** old_buckets = table->buckets;

    size_t new_bucket_count = table->bucket_count * TABLE_GROWTH_FACTOR;
    if (new_bucket_count < TABLE_INITIAL_BUCKET_COUNT) {
        new_bucket_count = TABLE_INITIAL_BUCKET_COUNT;
    }
    TableEntry** new_buckets =
        (TableEntry**)malloc(sizeof(TableEntry*) * new_bucket_count);

    for (size_t i = 0; i < new_bucket_count; i++) {
        new_buckets[i] = NULL;
    }

    for (size_t i = 0; i < old_bucket_count; i++) {
        TableEntry* entry = old_buckets[i];
        while (entry != NULL) {
            TableEntry* next = entry->next;
            size_t hash = hashValue(entry->key);
            size_t index = hash % new_bucket_count;
            entry->next = new_buckets[index];
            new_buckets[index] = entry;
            entry = next;
        }
    }
    table->buckets = new_buckets;
    table->bucket_count = new_bucket_count;
    free(old_buckets);
}

void freeTable(Table* table) {
    for (size_t i = 0; i < table->bucket_count; i++) {
        TableEntry* entry = table->buckets[i];
        while (entry != NULL) {
            TableEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    initTable(table);
}

void tableInsert(Table* table, Value key, Value value) {
    if ((table->size + 1) > TABLE_MAX_LOAD * table->bucket_count) {
        growTable(table);
    }

    size_t hash = hashValue(key);
    size_t index = hash % table->bucket_count;
    TableEntry* entry = table->buckets[index];
    while (entry != NULL) {
        if (valuesEqual(entry->key, key)) {
            // Key already exists, update value
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    // Key does not exist, create new entry
    TableEntry* new_entry = (TableEntry*)malloc(sizeof(TableEntry));
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    table->size++;
}

void tableRemove(Table* table, Value key) {
    size_t hash = hashValue(key);

    if (table->buckets == NULL) {
        // Table is empty
        return;
    }
    size_t index = hash % table->bucket_count;
    TableEntry* prev = NULL;
    TableEntry* entry = table->buckets[index];
    while (entry != NULL) {
        if (valuesEqual(entry->key, key)) {
            if (prev == NULL) {
                table->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            free(entry);
            table->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

Value* tableGet(Table* table, Value key) {
    size_t hash = hashValue(key);

    if (table->buckets != NULL) {
        // First, check the current buckets
        size_t index = hash % table->bucket_count;
        TableEntry* entry = table->buckets[index];
        while (entry != NULL) {
            if (valuesEqual(entry->key, key)) {
                return &entry->value;
            }
            entry = entry->next;
        }
    }

    return NULL;  // Key not found
}

ObjString* tableFindString(Table* table, const char* chars, int length,
                           uint32_t hash) {
    if (table->buckets == NULL) {
        return NULL;
    }

    size_t index = hash % table->bucket_count;
    TableEntry* entry = table->buckets[index];
    while (entry != NULL) {
        if (IS_OBJ(entry->key)) {
            Obj* obj = AS_OBJ(entry->key);
            if (obj->type == OBJ_STRING) {
                ObjString* string = (ObjString*)obj;
                if (string->length == length && string->hash == hash &&
                    memcmp(string->chars, chars, length) == 0) {
                    return string;
                }
            }
        }
        entry = entry->next;
    }

    return NULL;  // String not found
}
