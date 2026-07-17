/* A minimal JSON parser -- just enough to read calm-shell's theme
 * files (flat objects of strings, one level of nesting). Not a
 * general-purpose JSON library: no streaming, no unicode escape
 * decoding beyond passthrough, no number types beyond what strtod
 * gives us. Kept dependency-free on purpose. */
#ifndef CALM_JSON_H
#define CALM_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

typedef struct {
    char *key;
    JsonValue *value;
} JsonMember;

struct JsonValue {
    JsonType type;
    bool boolean;
    double number;
    char *string;
    JsonValue **items;
    size_t item_count;
    JsonMember *members;
    size_t member_count;
};

/* Parses `input` into a tree of JsonValues. Returns NULL and fills
 * *err_msg (caller must free()) on failure. The returned value (and
 * everything under it) must be freed with json_free(). */
JsonValue *json_parse(const char *input, char **err_msg);
void json_free(JsonValue *v);

/* Looks up `key` in a JSON_OBJECT value. Returns NULL if `v` isn't an
 * object or the key is absent. */
const JsonValue *json_object_get(const JsonValue *v, const char *key);
/* Returns the string, or NULL if `v` isn't a JSON_STRING. */
const char *json_as_string(const JsonValue *v);

#endif /* CALM_JSON_H */
