/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "numeric_range.h"
#include "../rmalloc.h"
#include "../../parser/grammar.h"
#include <math.h>
#include <assert.h>

NumericRange* NumericRange_New(void) {
    NumericRange *range = rm_malloc(sizeof(NumericRange));
    range->valid = true;
    range->max = INFINITY;
    range->min = -INFINITY;
    range->include_min = false;
    range->include_max = false;
    return range;
}

bool NumericRange_IsValid(const NumericRange *range) {
    if(range->include_min && range->include_max) {
        // X >= y AND X <= z
        return (range->min <= range->max);
    } else {
        // X >= y AND X < z
        // X > y AND X <= z
        // X > y AND X < z
        return (range->min < range->max);
    }
}

bool NumericRange_ContainsValue(const NumericRange *range, double v) {
    if(!range->valid) return false;

    // Make sure v is <= max.
    if(range->include_max) {
        if(v > range->max) return false;
    } else {
        if(v >= range->max) return false;
    }

    // Make sure v >= min.
    if(range->include_min) {
        if(v < range->min) return false;
    } else {
        if(v <= range->min) return false;
    }

    return true;
}

void NumericRange_TightenRange(NumericRange *range, int op, double v) {
    if(!range->valid) return;

     switch (op) {
        case LT:    // <
            if(range->max >= v) {
                range->include_max = false;
                range->max = v;
            }
            break;
        case LE:    // <=
            if(range->max > v) {
                range->include_max = true;
                range->max = v;
            }
            break;
        case GT:    // >
            if(range->min <= v) {
                range->include_min = false;
                range->min = v;
            }
            break;
        case GE:    // >=
            if(range->min < v) {
                range->include_min = true;
                range->min = v;
            }
            break;
        case EQ:    // =
            // Make sure v is within range.
            if(!NumericRange_ContainsValue(range, v)) {
                range->valid = true;
                return;
            }

            range->include_min = true;
            range->include_max = true;
            range->min = v;
            range->max = v;
            break;
    }

    // See if range is still valid.
    range->valid = NumericRange_IsValid(range);
}

void NumericRange_ToString(const NumericRange *range) {
    assert(range);
    int offset = 0;
    char buff[1024];

    if(range->include_min) offset += sprintf(buff + offset, "[");
    else offset += sprintf(buff + offset, "(");

    if(range->min == -INFINITY) offset += sprintf(buff + offset, "-inf");
    else offset += sprintf(buff + offset, "%f", range->min);
    
    offset += sprintf(buff + offset, ",");

    if(range->max == -INFINITY) offset += sprintf(buff + offset, "inf");
    else offset += sprintf(buff + offset, "%f", range->max);

    if(range->include_max) offset += sprintf(buff + offset, "]");
    else offset += sprintf(buff + offset, ")");
    printf("%s\n", buff);
}

void NumericRange_Free(NumericRange *range) {
    assert(range);
    rm_free(range);
}