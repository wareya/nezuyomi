#include "unishim_split.h"
#include <stdio.h>

static FILE * wrap_fopen(const char * fname, const char * mode)
{
    #ifdef _WIN32
    
    int status;
    uint16_t * wpath = utf8_to_utf16((uint8_t *)fname, &status);
    uint16_t * wmode = utf8_to_utf16((uint8_t *)mode, &status);
    
    auto f = _wfopen((wchar_t *)wpath,(wchar_t *) wmode);
    
    free(wpath);
    free(wmode);
    
    return f;
    
    #else
    
    return fopen(fname, mode);
    
    #endif
}
