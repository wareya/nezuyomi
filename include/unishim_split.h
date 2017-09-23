#ifndef INCLUDE_UNISHIM_SPLIT_H
#define INCLUDE_UNISHIM_SPLIT_H

/* unishim_split.h - C99/C++ utf-8/utf-16/utf-32 conversion header

This file is released to the public domain under US law,
and also released under any version of the Creative Commons Zero license: 
https://creativecommons.org/publicdomain/zero/1.0/
https://creativecommons.org/publicdomain/zero/1.0/legalcode
*/

// REPLACES unishim.h -- only include one!

// #define UNISHIM_PUN_TYPE_IS_CHAR before including for the callback userdata type to be char* instead of void*. 
// This might work around broken strict aliasing optimizations that
// don't allow T* -> void* -> T*, only T* -> char* -> T*.

// #define UNISHIM_NO_STDLIB to not include stdlib.h and not use malloc/free in the code.
// This prevents utfX_to_utfY functions from being declared.

// #define UNISHIM_DECLARATION_PREFIX to change the declaration prefix from "static" to anything else

#include <stdint.h>
#ifndef UNISHIM_NO_STDLIB
#include <stdlib.h>
#endif
#include <iso646.h>

#ifdef UNISHIM_PUN_TYPE_IS_CHAR
#define UNISHIM_PUN_TYPE char
#else
#define UNISHIM_PUN_TYPE void
#endif

#ifndef UNISHIM_DECLARATION_PREFIX
#define UNISHIM_DECLARATION_PREFIX static
#endif

typedef int (*unishim_callback)(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata);

/*
utf8: pointer to array of uint8_t values, storing utf-8 code units, encoding utf-8 text
max: zero if array is terminated by a null code unit, nonzero if array has a particular length
callback: int unishim_callback(uint32_t codepoint, void * userdata);
userdata: user data, given to callback

BUFFER utf8 MUST NOT BE MODIFIED BY ANOTHER THREAD DURING ITERATION.

Executes callback(codepoint, userdata) on each codepoint in the string, from start to end.
If callback is NULL/nullptr/0, callback is not executed. Useful if you only want the return status code.
If callback returns non-zero, execution returns immediately AND RETURNS THE VALUE THE CALLBACK RETURNED.
CALLBACK CAN BE CALLED WITH A CODEPOINT VALUE OF ZERO IF max IS NOT ZERO.

If an encoding error is encountered, an appropriate status code is returned:
-1: argument is invalid (utf8 buffer pointer is null)
0: no error, covered all codepoints
1: continuation code unit where initial code unit expected or initial code unit is illegal
2: codepoint encoding truncated by null terminator or end of buffer (position >= max)
3: initial code unit where continuation code unit expected
4: codepoint is a surrogate, which is forbidden
5: codepoint is too large to encode in utf-16, which is forbidden
6: codepoint is overlong (encoded using too many code units)

error 6 takes priority over error 4

CALLBACK IS NOT RUN ON ANY CODEPOINTS STARTING AT (INCLUSIVE) THE FIRST CODEPOINT TO CAUSE AN ERROR. RETURN IS IMMEDIATE.

Reads at most "max" CODE UNITS (uint8_t) from the utf8 buffer.
If "max" is zero, stops at null instead.
CALLBACK DOES NOT RUN ON NULL TERMINATOR IF MAX IS ZERO.
*/
UNISHIM_DECLARATION_PREFIX int utf8_iterate(uint8_t * utf8, size_t max, unishim_callback callback, void * userdata)
{
    if(!utf8)
        return -1;
    
    uint8_t * counter = utf8;
    size_t i = 0;
    
    while((max) ? (i < max) : (counter[0] != 0))
    {   
        // trivial byte
        if(counter[0] < 0x80)
        {
            if(callback)
            {
                int r = callback(counter[0], userdata);
                if(r) return r;
            }
            
            counter += 1;
            i += 1;
        }
        // continuation byte where initial byte expected
        else if(counter[0] < 0xC0)
        {
            return 1;
        }
        // two byte
        else if(counter[0] < 0xE0)
        {
            for(int index = 1; index <= 1; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t high = counter[0]&0x1F;
            uint32_t low = counter[1]&0x3F;
            
            uint32_t codepoint = (high<<6) | low;
            
            if(codepoint < 0x80)
                return 6;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 2;
            i += 2;
        }
        // three byte
        else if(counter[0] < 0xF0)
        {
            for(int index = 1; index <= 2; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t high = counter[0]&0x0F;
            uint32_t mid = counter[1]&0x3F;
            uint32_t low = counter[2]&0x3F;
            
            uint32_t codepoint = (high<<12) | (mid<<6) | low;
            
            if(codepoint < 0x800)
                return 6;
            
            if(codepoint > 0xD800 and codepoint < 0xE000)
                return 4;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 3;
            i += 3;
        }
        // four byte
        else if(counter[0] < 0xF8)
        {
            for(int index = 1; index <= 3; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t top = counter[0]&0x07;
            uint32_t high = counter[1]&0x3F;
            uint32_t mid = counter[2]&0x3F;
            uint32_t low = counter[3]&0x3F;
            
            uint32_t codepoint = (top<<18) | (high<<12) | (mid<<6) | low;
            
            if(codepoint < 0x10000)
                return 6;
            
            if(codepoint >= 0x110000)
                return 5;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 4;
            i += 4;
        }
        else
            return 1;
    }
    return 0;
}

/*
utf16: pointer to array of uint16_t values, storing utf-16 code units, encoding utf-16 text
max: zero if array is terminated by a null code unit, nonzero if array has a particular length
callback: int unishim_callback(uint32_t codepoint, void * userdata);
userdata: user data, given to callback

BUFFER utf16 MUST NOT BE MODIFIED BY ANOTHER THREAD DURING ITERATION.

Executes callback(codepoint, userdata) on each codepoint in the string, from start to end.
If callback is NULL/nullptr/0, callback is not executed. Useful if you only want the return status code.
If callback returns non-zero, execution returns immediately AND RETURNS THE VALUE THE CALLBACK RETURNED.
CALLBACK CAN BE CALLED WITH A CODEPOINT VALUE OF ZERO IF max IS NOT ZERO.

If an encoding error is encountered, an appropriate status code is returned:
-1: argument is invalid (utf16 buffer pointer is null)
0: no error, covered all codepoints
1: continuation code unit where initial code unit expected
2: codepoint encoding truncated by null terminator or end of buffer (position >= max)
3: initial code unit where continuation code unit expected

CALLBACK IS NOT RUN ON ANY CODEPOINTS STARTING AT (INCLUSIVE) THE FIRST CODEPOINT TO CAUSE AN ERROR. RETURN IS IMMEDIATE.

Reads at most "max" CODE UNITS (uint16_t) from the utf16 buffer.
If "max" is zero, stops at null instead.
CALLBACK DOES NOT RUN ON NULL TERMINATOR IF MAX IS ZERO.
*/
UNISHIM_DECLARATION_PREFIX int utf16_iterate(uint16_t * utf16, size_t max, unishim_callback callback, void * userdata)
{
    if(!utf16)
        return -1;
    
    uint16_t * counter = utf16;
    size_t i = 0;
    
    while((max) ? (i < max) : (counter[0] != 0))
    {
        // trivial codepoint
        if(counter[0] < 0xD800 or counter[0] >= 0xE000)
        {
            if(callback)
            {
                int r = callback(counter[0], userdata);
                if(r) return r;
            }
            counter += 1;
            i += 1;
        }
        // surrogate
        else
        {
            // continuation surrogate
            if(counter[0] >= 0xDC00)
                return 1;
            // counter[0] is now guaranteed to be a valid first/high surrogate
            // unexpected termination
            else if((max and i+1 >= max) or counter[1] == 0)
                return 2;
            // continuation surrogate is not a continuation surrogate
            else if(counter[1] < 0xDC00 or counter[0] >= 0xE000)
                return 3;
            // counter[1] is now guaranteed to be a valid second/low surrogate
            
            uint32_t in_high = counter[0]&0x03FF;
            uint32_t in_low = counter[1]&0x03FF;
            uint32_t codepoint = (in_high<<10) | in_low;
            codepoint += 0x10000;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 2;
            i += 2;
        }
    }
    return 0;
}


/*
utf32: pointer to array of uint32_t values, storing utf-32 code units, encoding utf-32 text
max: zero if array is terminated by a null code unit, nonzero if array has a particular length
callback: callback
userdata: user data, given to callback

BUFFER utf32 MUST NOT BE MODIFIED BY ANOTHER THREAD DURING ITERATION.

Executes callback(codepoint, userdata) on each codepoint in the string, from start to end.
If callback is NULL/nullptr/0, callback is not executed. Useful if you only want the return status code.
If callback returns non-zero, execution returns immediately AND RETURNS THE VALUE THE CALLBACK RETURNED.
CALLBACK CAN BE CALLED WITH A CODEPOINT VALUE OF ZERO IF max IS NOT ZERO.

If an encoding error is encountered, an appropriate status code is returned:
-1: argument is invalid (utf16 buffer pointer is null)
0: no error, covered all codepoints
1: codepoint is a surrogate, which is forbidden
2: codepoint is too large to encode in utf-16, which is forbidden

CALLBACK IS NOT RUN ON ANY CODEPOINTS STARTING AT (INCLUSIVE) THE FIRST CODEPOINT TO CAUSE AN ERROR. RETURN IS IMMEDIATE.

Reads at most "max" CODE UNITS (uint32_t) from the utf32 buffer.
If "max" is zero, stops at null instead.
CALLBACK DOES NOT RUN ON NULL TERMINATOR IF MAX IS ZERO.
*/
UNISHIM_DECLARATION_PREFIX int utf32_iterate(uint32_t * utf32, size_t max, unishim_callback callback, void * userdata)
{
    if(!utf32)
        return -1;
    
    uint32_t * counter = utf32;
    size_t i = 0;
    
    while((max) ? (i < max) : (counter[0] != 0))
    {   
        if(counter[0] >= 0xD800 and counter[0] < 0xE000)
            return 1;
        else if(counter[0] >= 0x110000)
            return 2;
        callback(counter[0], userdata);
        
        counter += 1;
        i += 1;
    }
    return 0;
}


// Returns the number of code units required to encode the given codepoint in utf-8, or zero if it's illegal.
// This returns 1 for an input of 0.
UNISHIM_DECLARATION_PREFIX int utf8_code_unit_length(uint32_t codepoint)
{
    if(codepoint >= 0xD800 and codepoint < 0xE000)
        return 0;
    if(codepoint < 0x80)
        return 1;
    if(codepoint < 0x800)
        return 2;
    if(codepoint < 0x10000)
        return 3;
    if(codepoint < 0x110000)
        return 4;
    return 0;
}

// Returns the number of code units required to encode the given codepoint in utf-16, or zero if it's illegal.
// This returns 1 for an input of 0.
UNISHIM_DECLARATION_PREFIX int utf16_code_unit_length(uint32_t codepoint)
{
    if(codepoint >= 0xD800 and codepoint < 0xE000)
        return 0;
    if(codepoint < 0x10000)
        return 1;
    if(codepoint < 0x110000)
        return 2;
    return 0;
}

// Returns the number of code units required to encode the given codepoint in utf-32, or zero if it's illegal.
// This returns 1 for an input of 0.
UNISHIM_DECLARATION_PREFIX int utf32_code_unit_length(uint32_t codepoint)
{
    if(codepoint >= 0xD800 and codepoint < 0xE000)
        return 0;
    if(codepoint < 0x110000)
        return 1;
    return 0;
}

// Encodes the given codepoint into the given array of utf-8 code units using the given number of code units.
// Array must be able to contain the given number of code units.
// DOES NOT VALIDATE ANYTHING. Short encodings are truncated with modulus. Can encode illegal unicode.
// If the pointer to the array is NULL/nullptr/0, does nothing and returns -1.
// If count is not 1, 2, 3, or 4, does nothing and returns 0.
// Otherwise, returns count.
// The -1 return takes priority over the 0 return.
UNISHIM_DECLARATION_PREFIX int utf8_encode(uint8_t * utf8, uint32_t codepoint, int count)
{
    if(!utf8)
        return -1;
    if(count == 1)
    {
        utf8[0] = codepoint&0x7F;
        return 1;
    }
    if(count == 2)
    {
        uint8_t high = (codepoint>>6)&0x1F;
        uint8_t low = codepoint&0x3F;
        high |= 0xC0;
        low |= 0x80;
        utf8[0] = high;
        utf8[1] = low;
        return 2;
    }
    if(count == 3)
    {
        uint8_t high = (codepoint>>12)&0x0F;
        uint8_t mid = (codepoint>>6)&0x3F;
        uint8_t low = codepoint&0x3F;
        high |= 0xE0;
        mid |= 0x80;
        low |= 0x80;
        utf8[0] = high;
        utf8[1] = mid;
        utf8[2] = low;
        return 3;
    }
    if(count == 4)
    {
        uint8_t top = (codepoint>>18)&0x07;
        uint8_t high = (codepoint>>12)&0x3F;
        uint8_t mid = (codepoint>>6)&0x3F;
        uint8_t low = codepoint&0x3F;
        top |= 0xF0;
        high |= 0x80;
        mid |= 0x80;
        low |= 0x80;
        utf8[0] = top;
        utf8[1] = high;
        utf8[2] = mid;
        utf8[3] = low;
        return 4;
    }
    return 0;
}


// Encodes the given codepoint into the given array of utf-16 code units using the given number of code units.
// Array must be able to contain the given number of code units.
// DOES NOT VALIDATE ANYTHING. Short encodings are truncated with modulus. Can encode illegal unicode.
// If the pointer to the array is NULL/nullptr/0, does nothing and returns -1.
// If count is not 1 or 2, does nothing and returns 0.
// Otherwise, returns count.
// The -1 return takes priority over the 0 return.
UNISHIM_DECLARATION_PREFIX int utf16_encode(uint16_t * utf16, uint32_t codepoint, int count)
{
    if(!utf16)
        return -1;
    if(count == 1)
    {
        utf16[0] = codepoint&0xFFFF;
        return 1;
    }
    if(count == 2)
    {
        codepoint -= 0x10000;
        uint16_t high = (codepoint>>10)&0x3FF;
        uint16_t low = codepoint&0x3FF;
        high |= 0xD800;
        low |= 0xDC00;
        utf16[0] = high;
        utf16[1] = low;
        return 2;
    }
    return 0;
}


// This function is basically useless, but has the same behavior as the above two for OCD reasons.
// Encodes the given codepoint into the given array of utf-32 code units using the given number of code units.
// Array must be able to contain the given number of code units.
// DOES NOT VALIDATE ANYTHING. Can encode illegal unicode.
// If the pointer to the array is NULL/nullptr/0, does nothing and returns -1.
// If count is not 1, does nothing and returns 0.
// Otherwise, returns count.
// The -1 return takes priority over the 0 return.
UNISHIM_DECLARATION_PREFIX int utf32_encode(uint32_t * utf32, uint32_t codepoint, int count)
{
    if(!utf32)
        return -1;
    if(count == 1)
    {
        utf32[0] = codepoint;
        return 1;
    }
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf8_length_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    size_t * length = (size_t *)userdata;
    int count = utf8_code_unit_length(codepoint);
    if(count == 0) return -2;
    *length += count;
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf16_length_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    size_t * length = (size_t *)userdata;
    int count = utf16_code_unit_length(codepoint);
    if(count == 0) return -2;
    *length += count;
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf32_length_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    size_t * length = (size_t *)userdata;
    int count = utf32_code_unit_length(codepoint);
    if(count == 0) return -2;
    *length += count;
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf8_encode_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    uint8_t ** utf8 = (uint8_t **)userdata;
    int count = utf8_code_unit_length(codepoint);
    if(count == 0) return -2;
    *utf8 += utf8_encode(*utf8, codepoint, count);
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf16_encode_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    uint16_t ** utf16 = (uint16_t **)userdata;
    int count = utf16_code_unit_length(codepoint);
    if(count == 0) return -2;
    *utf16 += utf16_encode(*utf16, codepoint, count);
    return 0;
}

UNISHIM_DECLARATION_PREFIX int utf32_encode_callback(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata)
{
    uint32_t ** utf32 = (uint32_t **)userdata;
    int count = utf32_code_unit_length(codepoint);
    if(count == 0) return -2;
    *utf32 += utf32_encode(*utf32, codepoint, count);
    return 0;
}


#ifndef UNISHIM_NO_STDLIB
// Following six functions: Convert a UTF-X string into a freshly allocated null terminated UTF-Y buffer.
// If there's any error iterating over the UTF-X string,
// Status is set to the status code from utfX_iterate (see top of file). This is 0 if there is no error.
// If there's an internal logic error (about codepoint encoding length), status is set to -2
// If there's an allocation error, status is set to 7 and 0 is returned. For ALL conversion functions.
// If there is no error, status is set to 0.
// If there is any error, status is set to nonzero as described above, and any allocated buffer is freed.
// If there is any error, 0 is returned. Otherwise, the allocated converted buffer is returned.

UNISHIM_DECLARATION_PREFIX uint16_t * utf8_to_utf16(uint8_t * utf8, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf8_iterate(utf8, 0, utf16_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint16_t * utf16 = (uint16_t *)malloc((length+1)*sizeof(uint16_t));
    if(!utf16)
    {
        *status = 7;
        return 0;
    }
    uint16_t * utf16_reference = utf16;
    return_status = utf8_iterate(utf8, 0, utf16_encode_callback, &utf16_reference);
    if(return_status != 0)
    {
        free(utf16);
        *status = return_status;
        return 0;
    }
    utf16[length] = 0;
    return utf16;
}

UNISHIM_DECLARATION_PREFIX uint8_t * utf16_to_utf8(uint16_t * utf16, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf16_iterate(utf16, 0, utf8_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint8_t * utf8 = (uint8_t *)malloc((length+1)*sizeof(uint8_t));
    if(!utf8)
    {
        *status = 7;
        return 0;
    }
    uint8_t * utf8_reference = utf8;
    return_status = utf16_iterate(utf16, 0, utf8_encode_callback, &utf8_reference);
    if(return_status != 0)
    {
        free(utf8);
        *status = return_status;
        return 0;
    }
    utf8[length] = 0;
    return utf8;
}

UNISHIM_DECLARATION_PREFIX uint32_t * utf8_to_utf32(uint8_t * utf8, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf8_iterate(utf8, 0, utf32_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint32_t * utf32 = (uint32_t *)malloc((length+1)*sizeof(uint32_t));
    if(!utf32)
    {
        *status = 7;
        return 0;
    }
    uint32_t * utf32_reference = utf32;
    return_status = utf8_iterate(utf8, 0, utf32_encode_callback, &utf32_reference);
    if(return_status != 0)
    {
        free(utf32);
        *status = return_status;
        return 0;
    }
    utf32[length] = 0;
    return utf32;
}

UNISHIM_DECLARATION_PREFIX uint8_t * utf32_to_utf8(uint32_t * utf32, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf32_iterate(utf32, 0, utf8_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint8_t * utf8 = (uint8_t *)malloc((length+1)*sizeof(uint8_t));
    if(!utf8)
    {
        *status = return_status;
        return 0;
    }
    uint8_t * utf8_reference = utf8;
    return_status = utf32_iterate(utf32, 0, utf8_encode_callback, &utf8_reference);
    if(return_status != 0)
    {
        free(utf8);
        *status = return_status;
        return 0;
    }
    utf8[length] = 0;
    return utf8;
}

UNISHIM_DECLARATION_PREFIX uint16_t * utf32_to_utf16(uint32_t * utf32, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf32_iterate(utf32, 0, utf16_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint16_t * utf16 = (uint16_t *)malloc((length+1)*sizeof(uint16_t));
    if(!utf16)
    {
        *status = return_status;
        return 0;
    }
    uint16_t * utf16_reference = utf16;
    return_status = utf32_iterate(utf32, 0, utf16_encode_callback, &utf16_reference);
    if(return_status != 0)
    {
        free(utf16);
        *status = return_status;
        return 0;
    }
    utf16[length] = 0;
    return utf16;
}

UNISHIM_DECLARATION_PREFIX uint32_t * utf16_to_utf32(uint16_t * utf16, int * status)
{
    size_t length = 0;
    int return_status = 0;
    return_status = utf16_iterate(utf16, 0, utf32_length_callback, &length);
    if(return_status != 0)
    {
        *status = return_status;
        return 0;
    }
    uint32_t * utf32 = (uint32_t *)malloc((length+1)*sizeof(uint32_t));
    if(!utf32)
    {
        *status = return_status;
        return 0;
    }
    uint32_t * utf32_reference = utf32;
    return_status = utf16_iterate(utf16, 0, utf32_encode_callback, &utf32_reference);
    if(return_status != 0)
    {
        free(utf32);
        *status = return_status;
        return 0;
    }
    utf32[length] = 0;
    return utf32;
}
#endif

#endif
