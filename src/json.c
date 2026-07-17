#include "json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct {
    const char *p;
    const char *end;
} JsonParser;

static void skip_ws(JsonParser *jp) {
    while (jp->p < jp->end && (*jp->p == ' ' || *jp->p == '\t' || *jp->p == '\n' || *jp->p == '\r')) {
        jp->p++;
    }
}

static JsonValue *json_alloc(JsonType type) {
    JsonValue *v = xmalloc(sizeof(JsonValue));
    v->type = type;
    v->boolean = false;
    v->number = 0;
    v->string = NULL;
    v->items = NULL;
    v->item_count = 0;
    v->members = NULL;
    v->member_count = 0;
    return v;
}

void json_free(JsonValue *v) {
    if (!v) {
        return;
    }
    switch (v->type) {
        case JSON_STRING:
            free(v->string);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < v->item_count; i++) {
                json_free(v->items[i]);
            }
            free(v->items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < v->member_count; i++) {
                free(v->members[i].key);
                json_free(v->members[i].value);
            }
            free(v->members);
            break;
        default:
            break;
    }
    free(v);
}

const JsonValue *json_object_get(const JsonValue *v, const char *key) {
    if (!v || v->type != JSON_OBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < v->member_count; i++) {
        if (strcmp(v->members[i].key, key) == 0) {
            return v->members[i].value;
        }
    }
    return NULL;
}

const char *json_as_string(const JsonValue *v) {
    if (!v || v->type != JSON_STRING) {
        return NULL;
    }
    return v->string;
}

static JsonValue *parse_value(JsonParser *jp, char **err_msg);

static char *parse_string_raw(JsonParser *jp, char **err_msg) {
    /* Assumes *jp->p == '"'. */
    jp->p++;
    StrBuilder sb;
    sb_init(&sb);
    while (jp->p < jp->end && *jp->p != '"') {
        char c = *jp->p;
        if (c == '\\' && jp->p + 1 < jp->end) {
            char esc = jp->p[1];
            switch (esc) {
                case '"':
                    sb_append_char(&sb, '"');
                    break;
                case '\\':
                    sb_append_char(&sb, '\\');
                    break;
                case '/':
                    sb_append_char(&sb, '/');
                    break;
                case 'n':
                    sb_append_char(&sb, '\n');
                    break;
                case 't':
                    sb_append_char(&sb, '\t');
                    break;
                case 'r':
                    sb_append_char(&sb, '\r');
                    break;
                case 'b':
                    sb_append_char(&sb, '\b');
                    break;
                case 'f':
                    sb_append_char(&sb, '\f');
                    break;
                case 'u':
                    /* Unicode escapes: passed through as '?' rather than
                     * decoded to UTF-8 -- theme files never use them (all
                     * hex colors and plain-ASCII names), so this is dead
                     * weight to build out fully for a fixed, tiny schema. */
                    sb_append_char(&sb, '?');
                    jp->p += 4;
                    break;
                default:
                    sb_append_char(&sb, esc);
                    break;
            }
            jp->p += 2;
        } else {
            sb_append_char(&sb, c);
            jp->p++;
        }
    }
    if (jp->p >= jp->end || *jp->p != '"') {
        if (err_msg) {
            *err_msg = xstrdup("unterminated string");
        }
        sb_free(&sb);
        return NULL;
    }
    jp->p++;
    return sb_take(&sb);
}

static JsonValue *parse_object(JsonParser *jp, char **err_msg) {
    jp->p++; /* '{' */
    JsonValue *v = json_alloc(JSON_OBJECT);
    size_t cap = 0;
    skip_ws(jp);
    if (jp->p < jp->end && *jp->p == '}') {
        jp->p++;
        return v;
    }
    while (true) {
        skip_ws(jp);
        if (jp->p >= jp->end || *jp->p != '"') {
            if (err_msg) {
                *err_msg = xstrdup("expected string key in object");
            }
            json_free(v);
            return NULL;
        }
        char *key = parse_string_raw(jp, err_msg);
        if (!key) {
            json_free(v);
            return NULL;
        }
        skip_ws(jp);
        if (jp->p >= jp->end || *jp->p != ':') {
            if (err_msg) {
                *err_msg = xstrdup("expected ':' after object key");
            }
            free(key);
            json_free(v);
            return NULL;
        }
        jp->p++;
        skip_ws(jp);
        JsonValue *val = parse_value(jp, err_msg);
        if (!val) {
            free(key);
            json_free(v);
            return NULL;
        }
        if (v->member_count == cap) {
            cap = cap == 0 ? 4 : cap * 2;
            v->members = xrealloc(v->members, cap * sizeof(JsonMember));
        }
        v->members[v->member_count].key = key;
        v->members[v->member_count].value = val;
        v->member_count++;

        skip_ws(jp);
        if (jp->p < jp->end && *jp->p == ',') {
            jp->p++;
            continue;
        }
        if (jp->p < jp->end && *jp->p == '}') {
            jp->p++;
            break;
        }
        if (err_msg) {
            *err_msg = xstrdup("expected ',' or '}' in object");
        }
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_array(JsonParser *jp, char **err_msg) {
    jp->p++; /* '[' */
    JsonValue *v = json_alloc(JSON_ARRAY);
    size_t cap = 0;
    skip_ws(jp);
    if (jp->p < jp->end && *jp->p == ']') {
        jp->p++;
        return v;
    }
    while (true) {
        skip_ws(jp);
        JsonValue *item = parse_value(jp, err_msg);
        if (!item) {
            json_free(v);
            return NULL;
        }
        if (v->item_count == cap) {
            cap = cap == 0 ? 4 : cap * 2;
            v->items = xrealloc(v->items, cap * sizeof(JsonValue *));
        }
        v->items[v->item_count++] = item;

        skip_ws(jp);
        if (jp->p < jp->end && *jp->p == ',') {
            jp->p++;
            continue;
        }
        if (jp->p < jp->end && *jp->p == ']') {
            jp->p++;
            break;
        }
        if (err_msg) {
            *err_msg = xstrdup("expected ',' or ']' in array");
        }
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_value(JsonParser *jp, char **err_msg) {
    skip_ws(jp);
    if (jp->p >= jp->end) {
        if (err_msg) {
            *err_msg = xstrdup("unexpected end of input");
        }
        return NULL;
    }
    char c = *jp->p;
    if (c == '{') {
        return parse_object(jp, err_msg);
    }
    if (c == '[') {
        return parse_array(jp, err_msg);
    }
    if (c == '"') {
        char *s = parse_string_raw(jp, err_msg);
        if (!s) {
            return NULL;
        }
        JsonValue *v = json_alloc(JSON_STRING);
        v->string = s;
        return v;
    }
    if (starts_with(jp->p, "true") && jp->p + 4 <= jp->end) {
        jp->p += 4;
        JsonValue *v = json_alloc(JSON_BOOL);
        v->boolean = true;
        return v;
    }
    if (starts_with(jp->p, "false") && jp->p + 5 <= jp->end) {
        jp->p += 5;
        JsonValue *v = json_alloc(JSON_BOOL);
        v->boolean = false;
        return v;
    }
    if (starts_with(jp->p, "null") && jp->p + 4 <= jp->end) {
        jp->p += 4;
        return json_alloc(JSON_NULL);
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        char *endptr = NULL;
        double n = strtod(jp->p, &endptr);
        if (endptr == jp->p) {
            if (err_msg) {
                *err_msg = xstrdup("invalid number");
            }
            return NULL;
        }
        jp->p = endptr;
        JsonValue *v = json_alloc(JSON_NUMBER);
        v->number = n;
        return v;
    }
    if (err_msg) {
        *err_msg = xsprintf("unexpected character '%c'", c);
    }
    return NULL;
}

JsonValue *json_parse(const char *input, char **err_msg) {
    JsonParser jp = {.p = input, .end = input + strlen(input)};
    JsonValue *v = parse_value(&jp, err_msg);
    if (!v) {
        return NULL;
    }
    skip_ws(&jp);
    if (jp.p != jp.end) {
        if (err_msg) {
            *err_msg = xstrdup("trailing data after top-level JSON value");
        }
        json_free(v);
        return NULL;
    }
    return v;
}
