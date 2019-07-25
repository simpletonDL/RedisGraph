/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "string_range.h"
#include "../rmalloc.h"
#include "../../parser/grammar.h"
#include <assert.h>

StringRange* StringRange_New(void) {
    StringRange *range = rm_malloc(sizeof(StringRange));
    range->min = NULL;
    range->max = NULL;
    range->valid = true;
    range->include_min = false;
    range->include_max = false;
    return range;
}

bool StringRange_IsValid(const StringRange *range) {
    if(!range->max || !range->min) return true;

    if(range->include_min && range->include_max) {
        // X >= y AND X <= z
        return (strcmp(range->min, range->max) <= 0);
    } else {
        // X >= y AND X < z
        // X > y AND X <= z
        // X > y AND X < z
        return (strcmp(range->min, range->max) < 0);
    }
}

bool StringRange_ContainsValue(const StringRange *range, const char *v) {
    if(!range->valid) return false;

    // Make sure v is <= max.
    if(range->max) {
        if(range->include_max) {
            if(strcmp(v, range->max) > 0) return false;
        } else {
            if(strcmp(v, range->max) >= 0) return false;
        }
    }

    // Make sure v >= min.
    if(range->min) {
        if(range->include_min) {
            if(strcmp(v, range->min) < 0 ) return false;
        } else {
            if(strcmp(v, range->min) <= 0) return false;
        }
    }

    return true;
}

void StringRange_TightenRange(StringRange *range, int op, const char *v) {
    if(!range->valid) return;

    switch (op) {
        case LT:    // <
            if(!range->max || strcmp(range->max, v) >= 0) {
                range->include_max = false;
                range->max = v;
            }
            break;
        case LE:    // <=
            if(!range->max || strcmp(range->max, v) > 0) {
                range->include_max = true;
                range->max = v;
            }
            break;
        case GT:    // >
            if(!range->min || strcmp(range->min, v) <= 0 ) {
                range->include_min = false;
                range->min = v;
            }
            break;
        case GE:    // >=
            if(!range->min || strcmp(range->min, v) < 0) {
                range->include_min = true;
                range->min = v;
            }
            break;
        case EQ:    // =
            // Make sure v is within range.
            if(!StringRange_ContainsValue(range, v)) {
                range->valid = false;
                return;
            }

            range->include_min = true;
            range->include_max = true;
            range->min = v;
            range->max = v;
            break;
    }

    // See if range is still valid.
    range->valid = StringRange_IsValid(range);
}

void StringRange_ToString(const StringRange *range) {
    assert(range);
    int offset = 0;
    char buff[1024];

    if(range->include_min) offset += sprintf(buff + offset, "[");
    else offset += sprintf(buff + offset, "(");

    if(range->min == NULL) offset += sprintf(buff + offset, "-inf");
    else offset += sprintf(buff + offset, "%s", range->min);
    
    offset += sprintf(buff + offset, ",");

    if(range->max == NULL) offset += sprintf(buff + offset, "inf");
    else offset += sprintf(buff + offset, "%s", range->max);

    if(range->include_max) offset += sprintf(buff + offset, "]");
    else offset += sprintf(buff + offset, ")");
    printf("%s\n", buff);
}

void StringRange_Free(StringRange *range) {
    assert(range);
    rm_free(range);
}