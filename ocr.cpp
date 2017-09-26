#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>

#include "include/unifile.h"

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    int i = 0;
    while(start_pos != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos = str.find(from);
        i++;
    }
    printf("replaced %s %d times\n", from.data(), i);
    return true;
}

int ocr(const char * filename, const char * commandfilename, const char * outfilename, const char * scale, const char * xshear, const char * yshear)
{
    auto f = wrap_fopen(commandfilename,  "rb");
    
    fseek(f, 0, SEEK_END);
    auto len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char * data = (char *)malloc(len+1);
    fread(data, 1, len, f);
    data[len] = 0;
    fclose(f);
    
    std::string command = std::string(data);
    free(data);
    
    replace(command, "$SCREENSHOT", std::string(filename));
    replace(command, "$OUTPUTFILE", std::string(outfilename));
    replace(command, "$SCALE", std::string(scale));
    replace(command, "$XSHEAR", std::string(xshear));
    replace(command, "$YSHEAR", std::string(yshear));
    
    //f = fopen("C:\\Users\\wareya\\Desktop\\exe\\tesseract\\testcommand.txt", "wb");

    std::istringstream af(command);
    std::string line;
    puts("running OCR");
    while (std::getline(af, line))
    {
        #ifdef _WIN32
        
        int status;
        wchar_t * wcommand = (wchar_t *)utf8_to_utf16((uint8_t *)line.data(), &status);
        if(wcommand)
        {
            _putws(wcommand);
            puts("");
            _wsystem(wcommand);
        }
        free(wcommand);
        
        #else
        
        system(line.data());
        
        #endif
    }
    puts("done running OCR");
    //fclose(f);
    
    return 0;
}
