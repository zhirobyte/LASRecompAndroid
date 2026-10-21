#include "record_range.h"

bool gles_record_range_valid(GlesRecordRange range)
{
    if (!range.start || !range.size)
        return false;

    return range.size <= UINTPTR_MAX - range.start;
}

bool gles_record_range_overlaps(
    GlesRecordRange a,
    GlesRecordRange b)
{
    if (!gles_record_range_valid(a) ||
        !gles_record_range_valid(b))
        return false;

    uintptr_t a_end = a.start + a.size;
    uintptr_t b_end = b.start + b.size;

    return a.start < b_end && b.start < a_end;
}

bool gles_record_range_contains(
    GlesRecordRange outer,
    GlesRecordRange inner)
{
    if (!gles_record_range_valid(outer) ||
        !gles_record_range_valid(inner))
        return false;

    return outer.start <= inner.start &&
           inner.start + inner.size <=
               outer.start + outer.size;
}
