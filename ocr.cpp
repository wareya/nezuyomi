#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

int ocr(const char * filename, const char * commandfilename)
{
    puts("opening command file");
    auto f = fopen(commandfilename,  "rb");
    
    fseek(f, 0, SEEK_END);
    auto len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char * data = (char *)malloc(len+1);
    fread(data, 1, len, f);
    data[len] = 0;
    fclose(f);
    
    puts("setting command");
    std::string command = std::string(data);
    free(data);
    
    replace(command, "$SCREENSHOT", std::string(filename));
    
    //f = fopen("C:\\Users\\wareya\\Desktop\\exe\\tesseract\\testcommand.txt", "wb");
    
    std::istringstream af(command);
    std::string line;    
    while (std::getline(af, line))
    {
        //fwrite(line.data(), 1, line.length(), f);
        //fputs("\n", f);
        system(line.data());
    }
    //fclose(f);
    
    return 0;
}
