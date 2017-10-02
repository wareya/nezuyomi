/*
Copyright 2017 Alexander Nadeau <wareya@gmail.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "include/GL/gl3w.h"
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <math.h>
#include <locale.h>
#ifndef M_PI
#define M_PI 3.1415926435
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "include/stb_truetype.h"

#include "include/unishim_split.h"
#include "include/unifile.h"

#ifdef _WIN32
#include "include/dirent_emulation.h"
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <string>

#include <iostream>
#include <fstream>

struct vertex {
    float x, y, z, u, v;
};

struct colorvertex {
    float x, y, z, r, g, b, a;
};

void checkerr(int line)
{
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR)
    {
        printf("GL error %04X from line %d\n", err, line);
    }   
}

void error_callback(int error, const char* description)
{
    puts(description);
}

struct value {
    double real = 0;
    std::string text = "";
    bool isnum = true;
};

std::map<std::string, value> config;

struct conf_real {
    std::string name;
    conf_real(std::string arg_name, double real)
    {
        name = arg_name;
        *this = real;
    }
    double operator =(double real)
    {
        config[name] = {real, "", true};
        return real;
    }
    operator double()
    {
        return config[name].real;
    }
};

bool is_number(const std::string & string)
{
    try {std::stod(string);}
    catch(const std::invalid_argument & e){ return false; }
    catch(const std::out_of_range & e){ return false; }
    return true;
}

double double_from_string(const std::string & string)
{
    double real = 0;
    try {real = stod(string);}
    catch(const std::invalid_argument & e){}
    catch(const std::out_of_range & e){}
    return real;
}

std::vector<std::string> split_string(std::string string, const std::string & delimiter)
{
    std::vector<std::string> r;
    
    size_t start = 0;
    size_t i = string.find(delimiter, start);
    while(i != std::string::npos)
    {
        r.push_back(string.substr(start, i-start));
        start = i+delimiter.length();
        
        i = string.find(delimiter, start);
    }
    if(start < string.length())
        r.push_back(string.substr(start));
    return r;
}

struct conf_text {
    std::string name;
    conf_text(std::string arg_name, std::string text)
    {
        name = arg_name;
        *this = text;
    }
    std::string operator =(std::string text)
    {
        config[name] = {double_from_string(text), text, false};
        return text;
    }
    operator std::string()
    {
        return config[name].text;
    }
};

#define MAKEREAL(X, Y) conf_real X(#X, Y)

MAKEREAL(scalemode, 1);
MAKEREAL(usejinc, 1);
MAKEREAL(usedownscalesharpening, 1);
MAKEREAL(usesharpen, 0);
MAKEREAL(sharpwet, 1);
MAKEREAL(light_downscaling, 0);
MAKEREAL(reset_position_on_new_page, 1);
MAKEREAL(invert_x, 1);
MAKEREAL(pgup_to_bottom, 1);
MAKEREAL(speed, 2000);
MAKEREAL(scrollspeed, 100);
MAKEREAL(throttle, 0.004);

#define MAKETEXT(X, Y) conf_text X(#X, Y)

MAKETEXT(sharpenmode, "acuity");
MAKETEXT(fontname, "NotoSansCJKjp-Regular.otf");

float downscaleradius = 6.0;
float sharphardness1 = 1;
float sharphardness2 = 1;
float sharpblur1 = 0.5;
float sharpblur2 = 0.5;
float sharpradius1 = 8.0;
float sharpradius2 = 16.0;

struct renderer {
    float cam_x = 0;
    float cam_y = 0;
    float cam_scale = 0;
    // TODO: FIXME: add a real reference counter
    struct texture {
        int w, h, n;
        GLuint texid;
        unsigned char * mydata;
        texture(unsigned char * data, int w, int h, bool ismono = false)
        {
            mydata = data;
            this->w = w;
            this->h = h;
            if(ismono)
                this->n = 1;
            else
                this->n = 4;
            
            checkerr(__LINE__);
            glActiveTexture(GL_TEXTURE0);
            
            checkerr(__LINE__);
            
            glGenTextures(1, &texid);
            glBindTexture(GL_TEXTURE_2D, texid);
            printf("Actual size: %dx%d\n", this->w, this->h);
            if(ismono)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, this->w, this->h, 0, GL_RED, GL_UNSIGNED_BYTE, mydata);
            else
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, this->w, this->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, mydata);
            
            checkerr(__LINE__);
            puts("Generating mipmaps");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
            glGenerateMipmap(GL_TEXTURE_2D);
            puts("Done generating mipmaps");
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//GL_LINEAR_MIPMAP_NEAREST);
            checkerr(__LINE__);
        }
        ~texture()
        {
            stbi_image_free(mydata);
        }
    };
    void delete_texture(texture * tex)
    {
        if(glIsTexture(tex->texid))
            glDeleteTextures(1, &tex->texid);
        delete tex;
    }
    texture * load_texture(const char * filename)
    {
        auto start = glfwGetTime();
        puts("Starting load texture");
        puts(filename);
        
        fflush(stdout);
        int w, h, n;
        
        auto f = wrap_fopen(filename, "rb");
        unsigned char * data = stbi_load_from_file(f, &w, &h, &n, 4);
        fclose(f);
        puts("Done actual loading");
        if(!data) return puts("failed to open texture"), nullptr;
        else
        {
            printf("Building texture of size %dx%d\n", w, h);
            
            auto tex = new texture(data, w, h);
            
            puts("Built texture");
            
            auto end = glfwGetTime();
            printf("Time: %f\n", end-start);
            return tex;
        }
    }
    // load single-channel 8bpp texture
    texture * load_texture(uint8_t * data, int w, int h)
    {
        if(!data) return nullptr;
        //auto start = glfwGetTime();
        
        //printf("Building texture of size %dx%d from memory\n", w, h);
        
        auto tex = new texture(data, w, h, true);
        
        //puts("Built texture");
        
        //auto end = glfwGetTime();
        //printf("Time: %f\n", end-start);
        return tex;
    }
    
    
    struct postprogram {
        unsigned int program;
        unsigned int fshader;
        unsigned int vshader;
        
        postprogram(const char * name, const char * fshadersource)
        {
            const char * vshadersource =
            "#version 330 core\n\
            layout (location = 0) in vec3 aPos;\n\
            layout (location = 1) in vec2 aTex;\n\
            out vec2 myTexCoord;\n\
            void main()\n\
            {\n\
                gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n\
                myTexCoord = aTex;\n\
            }\n"
            ;
            
            checkerr(__LINE__);
            vshader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vshader, 1, &vshadersource, NULL);
            glCompileShader(vshader);
            checkerr(__LINE__);
            
            fshader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fshader, 1, &fshadersource, NULL);
            glCompileShader(fshader);
            checkerr(__LINE__);
            
            program = glCreateProgram();
            glAttachShader(program, vshader);
            glAttachShader(program, fshader);
            glLinkProgram(program);
            checkerr(__LINE__);
            
            int v,f,p;
            glGetShaderiv(vshader, GL_COMPILE_STATUS, &v);
            glGetShaderiv(fshader, GL_COMPILE_STATUS, &f);
            glGetProgramiv(program, GL_LINK_STATUS, &p);
            checkerr(__LINE__);
            if(!v or !f or !p)
            {
                char info[512];
                puts("Failed to compile shader:");
                puts(name);
                if(!v)
                {
                    glGetShaderInfoLog(vshader, 512, NULL, info);
                    puts(info);
                }
                if(!f)
                {
                    glGetShaderInfoLog(fshader, 512, NULL, info);
                    puts(info);
                }
                if(!p)
                {
                    glGetProgramInfoLog(program, 512, NULL, info);
                    puts(info);
                }
                exit(0);
            }
            
            checkerr(__LINE__);
            
            glDeleteShader(vshader);
            glDeleteShader(fshader);
        }
    };
    
    struct rectprogram {
        unsigned int program;
        unsigned int fshader;
        unsigned int vshader;
        
        rectprogram(const char * name)
        {
            const char * vshadersource =
            "#version 330 core\n\
            uniform mat4 projection;\n\
            uniform mat4 translation;\n\
            layout (location = 0) in vec3 aPos;\n\
            layout (location = 1) in vec4 aCol;\n\
            out vec4 vCol;\n\
            void main()\n\
            {\n\
                gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0) * translation * projection;\n\
                vCol = aCol;\n\
            }\n"
            ;
            
            const char * fshadersource =
            "#version 330 core\n\
            in vec4 vCol;\n\
            layout(location = 0) out vec4 fragColor;\n\
            void main()\n\
            {\n\
                fragColor = vCol;\n\
            }\n"
            ;
            
            checkerr(__LINE__);
            vshader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vshader, 1, &vshadersource, NULL);
            glCompileShader(vshader);
            checkerr(__LINE__);
            
            fshader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fshader, 1, &fshadersource, NULL);
            glCompileShader(fshader);
            checkerr(__LINE__);
            
            program = glCreateProgram();
            glAttachShader(program, vshader);
            glAttachShader(program, fshader);
            glLinkProgram(program);
            checkerr(__LINE__);
            
            int v,f,p;
            glGetShaderiv(vshader, GL_COMPILE_STATUS, &v);
            glGetShaderiv(fshader, GL_COMPILE_STATUS, &f);
            glGetProgramiv(program, GL_LINK_STATUS, &p);
            checkerr(__LINE__);
            if(!v or !f or !p)
            {
                char info[512];
                puts("Failed to compile shader:");
                puts(name);
                if(!v)
                {
                    glGetShaderInfoLog(vshader, 512, NULL, info);
                    puts(info);
                }
                if(!f)
                {
                    glGetShaderInfoLog(fshader, 512, NULL, info);
                    puts(info);
                }
                if(!p)
                {
                    glGetProgramInfoLog(program, 512, NULL, info);
                    puts(info);
                }
                exit(0);
            }
            
            checkerr(__LINE__);
            
            glDeleteShader(vshader);
            glDeleteShader(fshader);
        }
    };
    
    struct textprogram {
        unsigned int program;
        unsigned int fshader;
        unsigned int vshader;
        
        textprogram(const char * name)
        {
            const char * vshadersource =
            "#version 330 core\n\
            uniform mat4 projection;\n\
            uniform mat4 translation;\n\
            layout (location = 0) in vec3 aPos;\n\
            layout (location = 1) in vec2 aCoord;\n\
            out vec4 vCol;\n\
            out vec2 texCoord;\n\
            void main()\n\
            {\n\
                gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0) * translation * projection;\n\
                texCoord = aCoord;\n\
            }\n"
            ;
            
            const char * fshadersource =
            "#version 330 core\n\
            uniform sampler2D mytexture;\n\
            in vec2 texCoord;\n\
            layout(location = 0) out vec4 fragColor;\n\
            void main()\n\
            {\n\
                fragColor = vec4(1,1,1,texture2D(mytexture, texCoord));\n\
            }\n"
            ;
            
            checkerr(__LINE__);
            vshader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vshader, 1, &vshadersource, NULL);
            glCompileShader(vshader);
            checkerr(__LINE__);
            
            fshader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fshader, 1, &fshadersource, NULL);
            glCompileShader(fshader);
            checkerr(__LINE__);
            
            program = glCreateProgram();
            glAttachShader(program, vshader);
            glAttachShader(program, fshader);
            glLinkProgram(program);
            checkerr(__LINE__);
            
            int v,f,p;
            glGetShaderiv(vshader, GL_COMPILE_STATUS, &v);
            glGetShaderiv(fshader, GL_COMPILE_STATUS, &f);
            glGetProgramiv(program, GL_LINK_STATUS, &p);
            checkerr(__LINE__);
            if(!v or !f or !p)
            {
                char info[512];
                puts("Failed to compile shader:");
                puts(name);
                if(!v)
                {
                    glGetShaderInfoLog(vshader, 512, NULL, info);
                    puts(info);
                }
                if(!f)
                {
                    glGetShaderInfoLog(fshader, 512, NULL, info);
                    puts(info);
                }
                if(!p)
                {
                    glGetProgramInfoLog(program, 512, NULL, info);
                    puts(info);
                }
                exit(0);
            }
            
            checkerr(__LINE__);
            
            glDeleteShader(vshader);
            glDeleteShader(fshader);
        }
    };
    
    unsigned int VAO, VBO, RectVAO, RectVBO, FBO, FBOtexture1, FBOtexture2;
    int w, h;
    unsigned int vshader;
    unsigned int fshader;
    unsigned int imageprogram;
    
    float jinctexture[512];
    float sinctexture[512];
    
    bool downscaling = false;
    float infoscale = 1.0;
    
    GLFWwindow * win;
    postprogram * copy, * sharpen, * nusharpen;
    rectprogram * primitive;
    textprogram * mytextprogram;
    renderer()
    {
        glfwSwapInterval(1);
        
        if(!glfwInit()) puts("glfw failed to init"), exit(0);
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1); 
        
        win = glfwCreateWindow(1104*0.8, 600, "ネズヨミ nezuyomi, an image viewer", NULL, NULL);
        
        if(!win) puts("glfw failed to init"), exit(0);
        glfwMakeContextCurrent(win);
        
        if(gl3wInit()) puts("gl3w failed to init"), exit(0);
        
        for(int i = 0; i < 512; i++)
        {
            //jinctexture[i] = sin(float(i)*M_PI/4)*0.5+0.5;///(float(i)*M_PI/4)*0.5+0.5;
            if(i == 0) jinctexture[i] = 1.0;
            else       jinctexture[i] = 2*j1(float(i*M_PI)/8)/(float(i*M_PI)/8)*0.5+0.5;
            
            if(i == 0) sinctexture[i] = 1.0;
            else       sinctexture[i] = sin(float(i*M_PI)/8)/(float(i*M_PI)/8)*0.5+0.5;
        }
        
        //glfwSwapBuffers(win);
        glfwGetFramebufferSize(win, &w, &h);
        
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
        {
            if(severity != GL_DEBUG_SEVERITY_NOTIFICATION)
                puts(message);
            //puts(message);
        }, nullptr);
        
        
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glPrimitiveRestartIndex(65535);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, u));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        
        
        glGenVertexArrays(1, &RectVAO);
        glGenBuffers(1, &RectVBO);
        
        glBindVertexArray(RectVAO);
        glPrimitiveRestartIndex(65535);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindBuffer(GL_ARRAY_BUFFER, RectVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(colorvertex), (void*)0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(colorvertex), (void*)offsetof(colorvertex, r));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        
        
        checkerr(__LINE__);
        
        const char * vshadersource =
        "#version 330 core\n\
        uniform mat4 projection;\n\
        uniform mat4 translation;\n\
        layout (location = 0) in vec3 aPos;\n\
        layout (location = 1) in vec2 aTex;\n\
        out vec2 myTexCoord;\n\
        void main()\n\
        {\n\
            gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0) * translation * projection;\n\
            myTexCoord = aTex;\n\
        }\n"
        ;
        
        const char * fshadersource = 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        uniform sampler2D myJincLookup;\n\
        uniform sampler2D mySincLookup;\n\
        uniform vec2 mySize;\n\
        uniform vec2 myScale;\n\
        uniform int usejinc;\n\
        uniform float myradius;\n\
        in vec2 myTexCoord;\n\
        #define M_PI 3.1415926435\n\
        //int lod;\n\
        vec2 offsetRoundedCoord(int a, int b)\n\
        {\n\
            return vec2((floor(myTexCoord.x*(mySize.x)-0.5)+a+0.5)/(mySize.x),\n\
                        (floor(myTexCoord.y*(mySize.y)-0.5)+b+0.5)/(mySize.y));\n\
        }\n\
        vec4 offsetRoundedPixel(int a, int b)\n\
        {\n\
            return texture2D(mytexture, offsetRoundedCoord(a, b));\n\
        }\n\
        vec2 interpolationPhase()\n\
        {\n\
            return vec2(mod(myTexCoord.x*(mySize.x)+0.5, 1),\n\
                        mod(myTexCoord.y*(mySize.y)+0.5, 1));\n\
        }\n\
        vec2 lodRoundedPixel(int a, int b, vec2 size)\n\
        {\n\
            return vec2((floor(myTexCoord.x*(size.x)-0.5)+a+0.5)/(size.x),\n\
                        (floor(myTexCoord.y*(size.y)-0.5)+b+0.5)/(size.y));\n\
        }\n\
        vec4 lodRoundedPixel(int a, int b, vec2 size, int lod)\n\
        {\n\
            return textureLod(mytexture, lodRoundedPixel(a, b, size), lod);\n\
        }\n\
        vec2 downscalingPhase(vec2 size)\n\
        {\n\
            return vec2(mod(myTexCoord.x*(size.x)+0.5, 1),\n\
                        mod(myTexCoord.y*(size.y)+0.5, 1));\n\
        }\n\
        vec4 hermite(vec4 a, vec4 b, vec4 c, vec4 d, float i)\n\
        {\n\
            vec4  bw = (c-a)/2;\n\
            vec4  cw = (d-b)/2;\n\
            float h00 = i*i*i*2 - i*i*3 + 1;\n\
            float h10 = i*i*i - i*i*2 + i;\n\
            float h01 = -i*i*i*2 + i*i*3;\n\
            float h11 = i*i*i - i*i;\n\
            return b*h00 + bw*h10 + c*h01 + cw*h11;\n\
        }\n\
        vec4 hermiterow(int y, float i)\n\
        {\n\
            vec4 c1 = offsetRoundedPixel(-1,y);\n\
            vec4 c2 = offsetRoundedPixel(-0,y);\n\
            vec4 c3 = offsetRoundedPixel(+1,y);\n\
            vec4 c4 = offsetRoundedPixel(+2,y);\n\
            return hermite(c1, c2, c3, c4, i);\n\
        }\n\
        vec4 hermitegrid(float ix, float iy)\n\
        {\n\
            vec4 c1 = hermiterow(-1,ix);\n\
            vec4 c2 = hermiterow(-0,ix);\n\
            vec4 c3 = hermiterow(+1,ix);\n\
            vec4 c4 = hermiterow(+2,ix);\n\
            return hermite(c1, c2, c3, c4, iy);\n\
        }\n\
        float jinc(float x)\n\
        {\n\
            return texture2D(myJincLookup, vec2(x*8/512, 0)).r*2-1;\n\
        }\n\
        float sinc(float x)\n\
        {\n\
            return texture2D(mySincLookup, vec2(x*8/512, 0)).r*2-1;\n\
        }\n\
        float jincwindow(float x, float radius)\n\
        {\n\
            if(x < -radius || x > radius) return 0.0;\n\
            return jinc(x) * cos(x*M_PI/2/radius);\n\
        }\n\
        float sincwindow(float x, float radius)\n\
        {\n\
            if(x < -radius || x > radius) return 0.0;\n\
            return sinc(x) * cos(x*M_PI/2/radius);\n\
        }\n\
        bool supersamplemode;\n\
        vec4 supersamplegrid()\n\
        {\n\
            int lod = 0;\n\
            float radius;\n\
            radius = myradius;\n\
            vec2 scale = myScale;\n\
            if(scale.x > 0 && scale.x < 0.25)\n\
            {\n\
                radius /= 2;\n\
                scale *= 2;\n\
                lod += 1;\n\
            }\n\
            if(radius < 1) radius = 1;\n\
            ivec2 size = textureSize(mytexture, lod);\n\
            vec2 phase = downscalingPhase(size);\n\
            float ix = phase.x;\n\
            float iy = phase.y;\n\
            int lowi  = int(floor(-radius/scale.y + iy));\n\
            int highi = int(ceil(radius/scale.y + iy));\n\
            int lowj  = int(floor(-radius/scale.x + ix));\n\
            int highj = int(ceil(radius/scale.x + ix));\n\
            vec4 c = vec4(0);\n\
            float sampleWeight = 0;\n\
            for(int i = lowi; i <= highi; i++)\n\
            {\n\
                for(int j = lowj; j <= highj; j++)\n\
                {\n\
                    float x = (i-ix)*scale.x;\n\
                    float y = (j-iy)*scale.y;\n\
                    if(supersamplemode && sqrt(x*x+y*y) > radius) continue;\n\
                    float weight;\n\
                    if(supersamplemode)\n\
                        weight = jincwindow(sqrt(x*x+y*y), radius);\n\
                    else\n\
                        weight = sincwindow(x, radius) * sincwindow(y, radius);\n\
                    sampleWeight += weight;\n\
                    c += lodRoundedPixel(i, j, size, lod)*weight;\n\
                }\n\
            }\n\
            c /= sampleWeight;\n\
            return c;\n\
        }\n\
        layout(location = 0) out vec4 fragColor;\n\
        void main()\n\
        {\n\
            if(myScale.x < 1 || myScale.y < 1)\n\
            {\n\
                supersamplemode = (usejinc != 0);\n\
                fragColor =  supersamplegrid();\n\
            }\n\
            else\n\
            {\n\
                vec2 phase = interpolationPhase();\n\
                vec4 c = hermitegrid(phase.x, phase.y);\n\
                fragColor = c;\n\
            }\n\
        }\n"
        ;
        
        checkerr(__LINE__);
        
        vshader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vshader, 1, &vshadersource, NULL);
        glCompileShader(vshader);
        
        fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fshader, 1, &fshadersource, NULL);
        glCompileShader(fshader);
        
        imageprogram = glCreateProgram();
        glAttachShader(imageprogram, vshader);
        glAttachShader(imageprogram, fshader);
        glLinkProgram(imageprogram);
        
        checkerr(__LINE__);
        
        int vsuccess, fsuccess, psuccess;
        glGetShaderiv(vshader, GL_COMPILE_STATUS, &vsuccess);
        glGetShaderiv(fshader, GL_COMPILE_STATUS, &fsuccess);
        glGetProgramiv(imageprogram, GL_LINK_STATUS, &psuccess);
        if(!vsuccess or !fsuccess or !psuccess)
        {
            char info[512];
            puts("Failed to compile shader");
            if(!vsuccess)
            {
                glGetShaderInfoLog(vshader, 512, NULL, info);
                puts(info);
            }
            if(!fsuccess)
            {
                glGetShaderInfoLog(fshader, 512, NULL, info);
                puts(info);
            }
            if(!psuccess)
            {
                glGetProgramInfoLog(imageprogram, 512, NULL, info);
                puts(info);
            }
            
            exit(0);
        }
        checkerr(__LINE__);
        
        glUseProgram(imageprogram);
        glUniform1i(glGetUniformLocation(imageprogram, "mytexture"), 0);
        glUniform1i(glGetUniformLocation(imageprogram, "myJincLookup"), 1);
        glUniform1i(glGetUniformLocation(imageprogram, "mySincLookup"), 2);
        
        checkerr(__LINE__);
        
        glDeleteShader(fshader);
        glDeleteShader(vshader);
        
        checkerr(__LINE__);
        
        // other drawing program
        
        primitive = new rectprogram("primitive");
        
        // other drawing program
        
        mytextprogram = new textprogram("textprogram");
        
        // FBO programs
        
        copy = new postprogram("copy", 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        in vec2 myTexCoord;\n\
        layout(location = 0) out vec4 fragColor;\n\
        void main()\n\
        {\n\
            fragColor = texture2D(mytexture, myTexCoord);\n\
        }\n");
        
        glUseProgram(copy->program);
        checkerr(__LINE__);
        glUniform1i(glGetUniformLocation(copy->program, "mytexture"), 0);
        checkerr(__LINE__);
        
        sharpen = new postprogram("sharpen", 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        uniform sampler2D myJincLookup;\n\
        uniform float radius;\n\
        uniform float blur;\n\
        uniform float wetness;\n\
        in vec2 myTexCoord;\n\
        #define M_PI 3.1415926435\n\
        float jinc(float x)\n\
        {\n\
            return texture2D(myJincLookup, vec2(x*8/512, 0)).r*2-1;\n\
        }\n\
        float jincwindow(float x, float radius)\n\
        {\n\
            if(x < -radius || x > radius) return 0.0;\n\
            return jinc(x) * cos(x*M_PI/2/radius);\n\
        }\n\
        layout(location = 0) out vec4 fragColor;\n\
        void main()\n\
        {\n\
            ivec2 size = textureSize(mytexture, 0);\n\
            vec2 texel = myTexCoord*size;\n\
            vec4 color = vec4(0);\n\
            float power = 0;\n\
            for(int i = -int(floor(radius)); i <= int(ceil(radius)); i++)\n\
            {\n\
                for(int j = -int(floor(radius)); j <= int(ceil(radius)); j++)\n\
                {\n\
                    float weight = jincwindow(sqrt(i*i+j*j)/blur, radius*blur);\n\
                    power += weight;\n\
                    color += texture2D(mytexture, vec2(texel.x + i, texel.y + j)/size)*weight;\n\
                }\n\
            }\n\
            vec4 delta = texture2D(mytexture, myTexCoord)-color/power;\n\
            fragColor = texture2D(mytexture, myTexCoord) + wetness*delta;\n\
        }\n");
        
        glUseProgram(sharpen->program);
        checkerr(__LINE__);
        glUniform1i(glGetUniformLocation(sharpen->program, "mytexture"), 0);
        checkerr(__LINE__);
        glUniform1i(glGetUniformLocation(sharpen->program, "myJincLookup"), 1);
        checkerr(__LINE__);
        
        nusharpen = new postprogram("nusharpen", 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        uniform sampler2D myJincLookup;\n\
        uniform float radius1;\n\
        uniform float radius2;\n\
        uniform float blur1;\n\
        uniform float blur2;\n\
        uniform float frequency;\n\
        uniform float wetness;\n\
        uniform float hardness1;\n\
        uniform float hardness2;\n\
        in vec2 myTexCoord;\n\
        #define M_PI 3.1415926435\n\
        float jinc(float x)\n\
        {\n\
            return texture2D(myJincLookup, vec2(x*8/512, 0)).r*2-1;\n\
        }\n\
        float jincwindow(float x, float radius)\n\
        {\n\
            if(x < -radius || x > radius) return 0.0;\n\
            return jinc(x) * cos(x*M_PI/2/radius);\n\
        }\n\
        layout(location = 0) out vec4 fragColor;\n\
        void main()\n\
        {\n\
            ivec2 size = textureSize(mytexture, 0);\n\
            vec2 texel = myTexCoord*size;\n\
            vec4 color1 = vec4(0);\n\
            vec4 color2 = vec4(0);\n\
            float realradius1 = radius1;\n\
            float realradius2 = radius2;\n\
            float coordscale = 1;\n\
            if(frequency > 1.414)\n\
                coordscale = frequency;\n\
            else\n\
            {\n\
                realradius1 *= frequency;\n\
                realradius2 *= frequency;\n\
            }\n\
            float power1 = 0;\n\
            float power2 = 0;\n\
            for(int i = -int(floor(realradius1)); i <= int(ceil(realradius1)); i++)\n\
            {\n\
                for(int j = -int(floor(realradius1)); j <= int(ceil(realradius1)); j++)\n\
                {\n\
                    float dist = sqrt(i*i+j*j)/blur1;\n\
                    if(dist/(realradius1) > 1) continue;\n\
                    float weight1 = jincwindow(dist, realradius1*blur1);\n\
                    if(weight1 == 0) continue;\n\
                    power1 += weight1;\n\
                    color1 += texture2D(mytexture, vec2(texel.x + i*coordscale, texel.y + j*coordscale)/size)*weight1;\n\
                }\n\
            }\n\
            for(int i = -int(floor(realradius2)); i <= int(ceil(realradius2)); i++)\n\
            {\n\
                for(int j = -int(floor(realradius2)); j <= int(ceil(realradius2)); j++)\n\
                {\n\
                    float dist = sqrt(i*i+j*j)/blur2;\n\
                    if(dist/(realradius2) > 1) continue;\n\
                    float weight2 = jincwindow(dist, realradius2*blur2);\n\
                    power2 += weight2;\n\
                    color2 += texture2D(mytexture, vec2(texel.x + i*coordscale, texel.y + j*coordscale)/size)*weight2;\n\
                }\n\
            }\n\
            vec4 delta1 = texture2D(mytexture, myTexCoord)-color1/power1; // stuff below blur frequency\n\
            vec4 delta2 = texture2D(mytexture, myTexCoord)-color2/power2; // lower radius\n\
            fragColor = texture2D(mytexture, myTexCoord) + hardness1*wetness*delta1 + hardness2*wetness*delta2;\n\
        }\n");
        
        glUseProgram(nusharpen->program);
        checkerr(__LINE__);
        glUniform1i(glGetUniformLocation(nusharpen->program, "mytexture"), 0);
        glUniform1i(glGetUniformLocation(nusharpen->program, "myJincLookup"), 1);
        checkerr(__LINE__);
        
        // make framebuffer
        
        glGenFramebuffers(1, &FBO); 
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
        checkerr(__LINE__);
        
        glActiveTexture(GL_TEXTURE0);
        
        glGenTextures(1, &FBOtexture1);
        glGenTextures(1, &FBOtexture2);
        
        glBindTexture(GL_TEXTURE_2D, FBOtexture1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBOtexture1, 0);
        checkerr(__LINE__);
        
        glBindTexture(GL_TEXTURE_2D, FBOtexture2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, FBOtexture2, 0);
        checkerr(__LINE__);
        
        // non-framebuffer texture
        
        unsigned int jinctexid;
        glActiveTexture(GL_TEXTURE1);
        glGenTextures(1, &jinctexid);
        glBindTexture(GL_TEXTURE_2D, jinctexid);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 512, 1, 0, GL_RED, GL_FLOAT, jinctexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glActiveTexture(GL_TEXTURE2);
        unsigned int sinctexid;
        glGenTextures(1, &sinctexid);
        glBindTexture(GL_TEXTURE_2D, sinctexid);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 512, 1, 0, GL_RED, GL_FLOAT, sinctexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glActiveTexture(GL_TEXTURE0);
        
        checkerr(__LINE__);
    }
    
    void cycle_start()
    {
        checkerr(__LINE__);
        int w2, h2;
        glfwGetFramebufferSize(win, &w2, &h2);
        
        checkerr(__LINE__);
        if(w2 != w or h2 != h)
        {
            w = w2;
            h = h2;
            glViewport(0, 0, w, h);
            checkerr(__LINE__);
            
            glActiveTexture(GL_TEXTURE0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
            
            glBindTexture(GL_TEXTURE_2D, FBOtexture1);
            checkerr(__LINE__);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            checkerr(__LINE__);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBOtexture1, 0);
            checkerr(__LINE__);
            
            glBindTexture(GL_TEXTURE_2D, FBOtexture2);
            checkerr(__LINE__);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            checkerr(__LINE__);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, FBOtexture2, 0);
            checkerr(__LINE__);
        }
        
        
        float projection[16] = {
            2.0f/w,  0.0f, 0.0f,-1.0f,
            0.0f, -2.0f/h, 0.0f, 1.0f,
            0.0f,    0.0f, 1.0f, 0.0f,
            0.0f,    0.0f, 0.0f, 1.0f
        };
        
        glUseProgram(imageprogram);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        
        glUniformMatrix4fv(glGetUniformLocation(imageprogram, "projection"), 1, 0, projection);
        
        glUseProgram(primitive->program);
        glUniformMatrix4fv(glGetUniformLocation(primitive->program, "projection"), 1, 0, projection);
        
        glUseProgram(mytextprogram->program);
        glUniformMatrix4fv(glGetUniformLocation(mytextprogram->program, "projection"), 1, 0, projection);
        
        glClearColor(0,0,0,1);
        glDepthMask(true);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        checkerr(__LINE__);
    }
    void cycle_post()
    {
        checkerr(__LINE__);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        
        const vertex vertices[] = {
            {-1.f, -1.f, 0.5f, 0.0f, 0.0f},
            { 1.f, -1.f, 0.5f, 1.0f, 0.0f},
            {-1.f,  1.f, 0.5f, 0.0f, 1.0f},
            { 1.f,  1.f, 0.5f, 1.0f, 1.0f}
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        checkerr(__LINE__);
        
        int currtex = 0;
        
        auto BUFFER_A = [&]()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
            glDrawBuffer(GL_COLOR_ATTACHMENT1);
        };
        auto BUFFER_B = [&]()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
        };
        auto BUFFER_DONE = [&]()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glDrawBuffer(GL_BACK);
        };
        
        auto FLIP_SOURCE = [&]()
        {
            if(currtex == 1)
            {
                glBindTexture(GL_TEXTURE_2D, FBOtexture2);
                currtex = 2;
                BUFFER_B();
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, FBOtexture1);
                currtex = 1;
                BUFFER_A();
            }
        };
        checkerr(__LINE__);
        
        if(downscaling && usedownscalesharpening && usejinc)
        {
            FLIP_SOURCE();
            glUseProgram(sharpen->program);
            glUniform1f(glGetUniformLocation(sharpen->program, "radius"), downscaleradius);
            glUniform1f(glGetUniformLocation(sharpen->program, "blur"), 1.0f);
            glUniform1f(glGetUniformLocation(sharpen->program, "wetness"), 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        checkerr(__LINE__);
        
        if(usesharpen)
        {
            FLIP_SOURCE();
            glUseProgram(nusharpen->program);
            glUniform1f(glGetUniformLocation(nusharpen->program, "frequency"), infoscale);
            glUniform1f(glGetUniformLocation(nusharpen->program, "radius1"), sharpradius1);
            glUniform1f(glGetUniformLocation(nusharpen->program, "radius2"), sharpradius2);
            glUniform1f(glGetUniformLocation(nusharpen->program, "blur1"), sharpblur1);
            glUniform1f(glGetUniformLocation(nusharpen->program, "blur2"), sharpblur2);
            glUniform1f(glGetUniformLocation(nusharpen->program, "hardness1"), sharphardness1);
            glUniform1f(glGetUniformLocation(nusharpen->program, "hardness2"), sharphardness2);
            glUniform1f(glGetUniformLocation(nusharpen->program, "wetness"), sharpwet);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        checkerr(__LINE__);
        
        FLIP_SOURCE();
        BUFFER_DONE();
        glUseProgram(copy->program);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkerr(__LINE__);
        
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
        
    void cycle_end()
    {
        checkerr(__LINE__);
        
        glFinish();
        glfwSwapBuffers(win);
        glFinish();
        checkerr(__LINE__);
    }
    void draw_rect(float x1, float y1, float x2, float y2, float r, float g, float b, float a, bool nocamera = false)
    {
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        checkerr(__LINE__);
        glUseProgram(primitive->program);
        glBindVertexArray(RectVAO);
        glBindBuffer(GL_ARRAY_BUFFER, RectVBO);
        checkerr(__LINE__);
        
        float x, y, w, h;
        if(nocamera)
        {
            x = x1;
            y = y1;
            w = x2-x1;
            h = y2-y1;
        }
        else
        {
            x = (x1*cam_scale-cam_x);
            y = (y1*cam_scale-cam_y);
            w = (x2-x1)*cam_scale;
            h = (y2-y1)*cam_scale;
        }
        
        const colorvertex vertices[] = {
            {0, 0, 0.0f, r, g, b, a},
            {w, 0, 0.0f, r, g, b, a},
            {0, h, 0.0f, r, g, b, a},
            {w, h, 0.0f, r, g, b, a}
        };
        
        float translation[16] = {
            1.0f, 0.0f, 0.0f,    x,
            0.0f, 1.0f, 0.0f,    y,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(glGetUniformLocation(primitive->program, "translation"), 1, 0, translation);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkerr(__LINE__);
        
        glEnable(GL_DEPTH_TEST);
    }
    void draw_texture(texture * texture, float x, float y, float z)
    {
        if(!texture)
            return;
        
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        
        checkerr(__LINE__);
        glUseProgram(imageprogram);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        checkerr(__LINE__);
        
        float w = float(texture->w);
        float h = float(texture->h);
        
        const vertex vertices[] = {
            {0, 0, 0.0f, 0.0f, 0.0f},
            {w, 0, 0.0f, 1.0f, 0.0f},
            {0, h, 0.0f, 0.0f, 1.0f},
            {w, h, 0.0f, 1.0f, 1.0f}
        };
        
        float translation[16] = {
            cam_scale,      0.0f, 0.0f, round(x-cam_x),
                 0.0f, cam_scale, 0.0f, round(y-cam_y),
                 0.0f,      0.0f, 1.0f,    z,
                 0.0f,      0.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(glGetUniformLocation(imageprogram, "translation"), 1, 0, translation);
        glUniform2f(glGetUniformLocation(imageprogram, "mySize"), w, h);
        glUniform2f(glGetUniformLocation(imageprogram, "myScale"), cam_scale, cam_scale);
        glUniform1i(glGetUniformLocation(imageprogram, "usejinc"), usejinc);
        glUniform1f(glGetUniformLocation(imageprogram, "myradius"), downscaleradius);
        glBindTexture(GL_TEXTURE_2D, texture->texid);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkerr(__LINE__);
    }
    void draw_text_texture(texture * texture, float x, float y, float z)
    {
        if(!texture)
            return;
        
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        
        checkerr(__LINE__);
        glUseProgram(mytextprogram->program);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        checkerr(__LINE__);
        
        float w = float(texture->w);
        float h = float(texture->h);
        
        const vertex vertices[] = {
            {0, 0, 0.0f, 0.0f, 0.0f},
            {w, 0, 0.0f, 1.0f, 0.0f},
            {0, h, 0.0f, 0.0f, 1.0f},
            {w, h, 0.0f, 1.0f, 1.0f}
        };
        
        float translation[16] = {
            1.0f, 0.0f, 0.0f,    x,
            0.0f, 1.0f, 0.0f,    y,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(glGetUniformLocation(mytextprogram->program, "translation"), 1, 0, translation);
        glBindTexture(GL_TEXTURE_2D, texture->texid);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkerr(__LINE__);
    }
};

bool looks_like_image_filename(std::string string)
{
    
    if (string.rfind(".png") == std::string::npos
    and string.rfind(".jpg") == std::string::npos
    and string.rfind(".jpeg") == std::string::npos
    and string.rfind(".Png") == std::string::npos
    and string.rfind(".Jpg") == std::string::npos
    and string.rfind(".Jpeg") == std::string::npos
    and string.rfind(".PNG") == std::string::npos
    and string.rfind(".JPG") == std::string::npos
    and string.rfind(".JPEG") == std::string::npos)
        return false;
    else
        return true;
}


std::mutex scrollMutex;
float scroll = 0;

void myScrollEventCallback(GLFWwindow * win, double x, double y)
{
    scrollMutex.lock();
        scroll += y;
    scrollMutex.unlock();
}

void myMouseEventCallback(GLFWwindow * win, int key, int scancode, int action, int mods)
{
    
}
// FIXME: store event in a buffer with a mutex around it
void myKeyEventCallback(GLFWwindow * win, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(key == GLFW_KEY_O)
        {
            light_downscaling = !light_downscaling;
            if(light_downscaling)
            {
                puts("Switched to faster downscaling");
                if(usejinc)
                    downscaleradius = 4.0;
                else
                    downscaleradius = 2.0;
            }
            else
            {
                puts("Switched to slower downscaling");
                if(usejinc)
                    downscaleradius = 6.0;
                else
                    downscaleradius = 4.0;
            }
        }
        if(key == GLFW_KEY_P)
        {
            usejinc = !usejinc;
            
            if(usejinc)
            {
                if(light_downscaling)
                    downscaleradius = 4.0;
                else
                    downscaleradius = 6.0;
                puts("Jinc enabled");
            }
            else
            {
                if(light_downscaling)
                    downscaleradius = 2.0;
                else
                    downscaleradius = 4.0;
                puts("Sinc enabled");
            }
        }
        if(key == GLFW_KEY_I)
        {
            usedownscalesharpening = !usedownscalesharpening;
            if(usedownscalesharpening) puts("Downscale sharpening enabled");
            else puts("Downscale sharpening disabled");
        }
        if(key == GLFW_KEY_J)
        {
            sharpwet = 0.5;
            puts("Edge enhancement strength set to 0.5");
        }
        if(key == GLFW_KEY_K)
        {
            sharpwet = 1.0;
            puts("Edge enhancement strength set to 1");
        }
        if(key == GLFW_KEY_L)
        {
            sharpwet = 2.0;
            puts("Edge enhancement strength set to 2");
        }
        if(key == GLFW_KEY_B)
        {
            usesharpen = 0;
            puts("Edge enhancement disabled");
        }
        if(key == GLFW_KEY_N)
        {
            usesharpen = 1;
            sharpenmode = "acuity";
            sharpradius1 = 2.0;
            sharpradius2 = 6.0;
            sharpblur1 = 0.5;
            sharpblur2 = 6.0;
            sharphardness1 = -0.5;
            sharphardness2 = +0.25;
            puts("Edge enhancement set to 'acuity' (for upscaling)");
        }
        if(key == GLFW_KEY_M)
        {
            usesharpen = 1;
            sharpenmode = "deartifact";
            sharpradius1 = 4.0;
            sharpradius2 = 8.0;
            sharpblur1 = sqrt(0.5);
            sharpblur2 = sqrt(0.5);
            sharphardness1 = -1.5;
            sharphardness2 = +1.5;
            puts("Edge enhancement set to 'deartifact' (for downscaling)");
        }
        if(key == GLFW_KEY_S)
        {
            if(!mods&GLFW_MOD_SHIFT)
                scalemode = int(scalemode+1)%3;
            else
                scalemode = int(scalemode+3-1)%3;
            if(scalemode == 0) puts("Scaling disabled");
            if(scalemode == 1) puts("Scaling set to 'fill'");
            if(scalemode == 2) puts("Scaling set to 'fit'");
        }
        if(key == GLFW_KEY_T)
        {
            reset_position_on_new_page = !reset_position_on_new_page;
            if(reset_position_on_new_page) puts("Position will now be reset on page change");
            else puts("Position no longer reset on page change");
        }
        if(key == GLFW_KEY_R)
        {
            invert_x = !invert_x;
            if(invert_x) puts("Switched to manga (right to left) mode");
            else puts("Switched to western (left to right) mode");
        }
    }
}

void getscale(int viewport_w, int viewport_h, int image_w, int image_h, float & xscale, float & yscale, float & scale)
{
    xscale = float(viewport_w)/float(image_w);
    yscale = float(viewport_h)/float(image_h);
    if(scalemode == 0)
        scale = 1;
    else if(scalemode == 1)
        scale = std::max(xscale, yscale);
    else if(scalemode == 2)
        scale = std::min(xscale, yscale);
}

void reset_position(int viewport_w, int viewport_h, int image_w, int image_h, float xscale, float yscale, float scale, float & x, float & y, bool forwards = true)
{
    bool invert_x_here = forwards?!invert_x:invert_x;
    bool invert_y_here = forwards;
    if(scale < yscale)
        y = (image_h*scale - viewport_h)/2;
    else if(invert_y_here)
        y = 0;
    else
        y = image_h*scale - viewport_h;
    if(scale < xscale)
        x = (image_w*scale - viewport_w)/2;
    else if(invert_x_here)
        x = 0;
    else
        x = image_w*scale - viewport_w;
}
void reset_position_partial(int viewport_w, int viewport_h, int image_w, int image_h, float xscale, float yscale, float scale, float & x, float & y)
{
    if(scale < yscale)
        y = (image_h*scale - viewport_h)/2;
    
    if(scale < xscale)
        x = (image_w*scale - viewport_w)/2;
}

void limit_position(int viewport_w, int viewport_h, int image_w, int image_h, float xscale, float yscale, float scale, float & x, float & y)
{
    if(scale < yscale)
    {
        float upperlimit = image_h*scale - viewport_h;
        float lowerlimit = 0;
        if(y < upperlimit) y = upperlimit;
        if(y > lowerlimit) y = lowerlimit;
    }
    else if(scale > yscale)
    {
        float upperlimit = 0;
        float lowerlimit = image_h*scale - viewport_h;
        if(y < upperlimit) y = upperlimit;
        if(y > lowerlimit) y = lowerlimit;
    }
    else
        y = 0;
    
    if(scale < xscale)
    {
        float upperlimit = image_w*scale - viewport_w;
        float lowerlimit = 0;
        if(x < upperlimit) x = upperlimit;
        if(x > lowerlimit) x = lowerlimit;
    }
    else if(scale > xscale)
    {
        float upperlimit = 0;
        float lowerlimit = image_w*scale - viewport_w;
        if(x < upperlimit) x = upperlimit;
        if(x > lowerlimit) x = lowerlimit;
    }
    else
        x = 0;
}

// ignores \r characters leading up to the final \n, but not \r characters in the middle of the string
// allocates a buffer in *string_ref and reads a line into it
// sets *string_ref to nullptr if allocation error or file is already error/EOF before starting
// returns 0 if continuing to read makes sense
// returns -1 if feof or ferror
// returns -2 if allocation error
int freadline(FILE * f, char ** string_ref)
{
    if(feof(f) or ferror(f))
    {
        *string_ref = nullptr;
        return -1;
    }
    
    auto start = ftell(f);
    auto text_end = start;
    
    auto c = fgetc(f);
    while(c != '\n' and c != EOF and !feof(f) and !ferror(f))
    {
        if(c != '\r') text_end = ftell(f);
        c = fgetc(f);
    }
    
    auto leftoff = ftell(f);
    auto done = feof(f) or ferror(f);
    
    auto len = text_end-start;
    
    *string_ref = (char *)malloc(len+1);
    if(!*string_ref)
        return -2;
    
    fseek(f, start, SEEK_SET);
    fread(*string_ref, 1, len, f);
    (*string_ref)[len] = 0;
    
    fseek(f, leftoff, SEEK_SET);
    
    if(done)
        fgetc(f); // reset EOF state
    
    return 0;
}

std::string profile()
{
    #ifdef _WIN32
    
    int status;
    // are you fucking serious
    uint16_t * wprofilevar = utf8_to_utf16((uint8_t *)"USERPROFILE", &status);
    
    wchar_t * wprofile = _wgetenv((wchar_t *)wprofilevar);
    uint8_t * profile = utf16_to_utf8((uint16_t *)wprofile, &status);
    
    std::string r = std::string((char *)profile);
    
    free(wprofilevar);
    free(profile);
    
    #else
    
    std::string r = std::string("~");
    
    #endif
    
    if(r.length() > 0)
        r += "/";
    
    return r;
}

// "/.config/ネズヨミ/config.txt"
FILE * profile_fopen(const char * fname, const char * mode)
{
    auto path = profile() + fname;
    return wrap_fopen(path.data(), mode);
}

void load_config()
{
    auto f = profile_fopen(".config/ネズヨミ/config.txt", "rb");
    if(!f)
    {
        puts("could not open config file");
        return;
    }
    
    char * text;
    while(freadline(f, &text) == 0)
    {
        // This is the normal way to split a string in C++. Fuck the committee.
        auto str = std::string(text);
        free(text);
        
        const std::string delimiter = ":";
        
        const auto start = str.find(delimiter);
        const auto end = start + delimiter.length();
        const std::string first = str.substr(0, start);
        const std::string second = str.substr(end);
        
        config[first] = {double_from_string(second), second, is_number(second)};
    }
    
    fclose(f);
}

bool fontinitialized = false;
stbtt_fontinfo fontinfo;
uint8_t * fontbuffer;

void init_font()
{
    auto fontfile = profile_fopen((".config/ネズヨミ/"+std::string(fontname)).data(), "rb");
    if(!fontfile)
    {
        puts("failed to open font file");
        return;
    }
    
    fseek(fontfile, 0, SEEK_END);
    uint64_t fontsize = ftell(fontfile);
    fseek(fontfile, 0, SEEK_SET);
    
    fontbuffer = (uint8_t*)malloc(fontsize);
    if(!fontbuffer)
    {
        puts("could not allocate font data");
        fclose(fontfile);
        return;
    }
    if(fread(fontbuffer, 1, fontsize, fontfile) != fontsize)
    {
        puts("failed to read font file");
        fclose(fontfile);
        return;
    }
    fclose(fontfile);
    
    if(stbtt_InitFont(&fontinfo, fontbuffer, stbtt_GetFontOffsetForIndex(fontbuffer, 0)) == 0)
    {
        puts("Something happened initializing the font");
        return;
    }
    
    fontinitialized = true;
}

// Gets how tall the font's ascent and descent area is if rendered at the given size.
int font_height_pixels(float size)
{
    if(!fontinitialized) return 0;
    float fontscale = stbtt_ScaleForPixelHeight(&fontinfo, size);
    int ascent, descent;
    stbtt_GetFontVMetrics(&fontinfo, &ascent, &descent, nullptr);
    return (ascent-descent)*fontscale;
}

// Cache of codepoint -> glyph index
std::map<uint32_t, uint32_t> indexcache;
uint32_t glyph_lookup(uint32_t codepoint)
{
    if(!fontinitialized) return 0;
    //if(indexcache.count(codepoint) > 0)
    //    return indexcache[codepoint];
    //else
    {
        uint32_t index = stbtt_FindGlyphIndex(&fontinfo, codepoint);
        //indexcache[codepoint] = index;
        return index;
    }
}

struct glyph
{
    renderer::texture * texture = 0;
    int w, h, x, y;
    float advance;
    float kern;
    uint32_t index;
    renderer * myrenderer = 0;
    
    glyph(uint32_t codepoint, float size, renderer * myrenderer)
    {
        if(!fontinitialized) return;
        this->myrenderer = myrenderer;
        
        float fontscale = stbtt_ScaleForPixelHeight(&fontinfo, size);
        index = glyph_lookup(codepoint);
        
        auto data = stbtt_GetGlyphBitmap(&fontinfo, fontscale, fontscale, index, &w, &h, &x, &y);
        if(data)
        {
            texture = myrenderer->load_texture(data, w, h);
            if(!texture)
                puts("failed to generate texture");
            else
                puts("rendered glyph");
            free(data);
        }
        else
            puts("failed to render glyph");
        
        int advance, bearing;
        
        stbtt_GetGlyphHMetrics(&fontinfo, index, &advance, &bearing);
        
        this->advance = fontscale*advance;
    }
    ~glyph()
    {
        puts("deleting texture");
        myrenderer->delete_texture(texture);
    }
};

std::map<uint32_t, glyph*> textcache;
struct subtitle
{
    std::vector<uint32_t> codepoints;
    int initialized = false;
    float size;
    renderer * myrenderer;
    
    subtitle()
    {
        
    }
    subtitle(std::string text, float size, renderer * myrenderer)
    {
        if(!fontinitialized) return;
        this->size = size;
        this->myrenderer = myrenderer;
        int status = utf8_iterate((uint8_t*)text.data(), 0, [](uint32_t codepoint, UNISHIM_PUN_TYPE * userdata) -> int {
            if(codepoint == '\r') return 0;
            auto self = (subtitle *)userdata;
            if(textcache.count(codepoint) == 0)
                textcache[codepoint] = new glyph(codepoint, self->size, self->myrenderer);
            self->codepoints.push_back(codepoint);
            return 0;
        }, this);
        initialized = true;
    }
};

struct region
{
    int x1, y1, x2, y2;
    std::string text;
    int mode = 0; // 0: vertical; 1: horizontal; 2: horizontal, alternate language?
    int pixel_scale = 32;
    int xskew = 0;
    int yskew = 0;
};

int textscale = 32; // most OCR software works best at a particular pixel size per character. for the OCR setup I have, it's 32 pixels. This will be an option later.
int textlines = 1; // used for estimation after you select a region
float paddingestimate = 0.35; // estimate of internal padding between lines

subtitle currentsubtitle;

//std::vector<region> regions = {{798, 135, 798+53, 135+197, "僕とヒナちゃんの\n愛の巣は\nダメだからね", 0, 1}};
//std::vector<region> regions = {{798, 135, 798+53, 135+197, "", 0, 1}};
std::vector<region> regions;

region * currentregion = 0;

region tempregion = {0,0,0,0,"",0,0,0,0};

void load_regions(std::string folder, std::string filename, int corewidth, int coreheight)
{
    regions = {};
    if(folder.length() > 0)
        folder[folder.length()-1] = '_';
    auto f = profile_fopen((".config/ネズヨミ/region_"+folder+filename+".txt").data(), "rb");
    if(!f)
    {
        //puts("couldn't open file");
        //puts((".config/ネズヨミ/region_"+folder+filename+".txt").data());
        return;
    }
    //puts("loading regions for");
    //puts((folder+filename).data());
    
    char * text;
    bool firstline = true;
    
    int loader_width = corewidth;
    int loader_height = coreheight;
    
    while(freadline(f, &text) == 0)
    {
        auto str = std::string(text);
        free(text);
        
        auto parts = split_string(str, "\t");
        
        if(firstline and parts.size() == 2)
        {
            int width = double_from_string(parts[0]);
            if(width != 0)
                loader_width = width;
            
            int height = double_from_string(parts[1]);
            if(height != 0)
                loader_height = height;
            
            firstline = false;
            continue;
        }
        
        if(parts.size() == 7 or parts.size() == 9)
        {
            firstline = false;
            
            int x1 = double_from_string(parts[0])/corewidth*loader_width;
            int y1 = double_from_string(parts[1])/coreheight*loader_height;
            int x2 = double_from_string(parts[2])/corewidth*loader_width;
            int y2 = double_from_string(parts[3])/coreheight*loader_height;
            std::string text;
            
            bool escape = false;
            for(char c : parts[4])
            {
                if(escape)
                {
                    if(c == '\\')
                        text += '\\';
                    else if(c == 'n')
                        text += '\n';
                    else if(c == 't')
                        text += '\t';
                    else
                    {
                        text += '\\';
                        text += c;
                    }
                    
                    escape = false;
                }
                else if(c == '\\')
                    escape = true;
                else
                    text += c;
            }
            
            int mode = double_from_string(parts[5]);
            int pixel_scale = double_from_string(parts[6]);
            
            if(parts.size() == 7)
            {
                regions.push_back({x1, y1, x2, y2, text, mode, pixel_scale, 0, 0});
            }
            else if(parts.size() == 9)
            {
                int xskew = double_from_string(parts[7]);
                int yskew = double_from_string(parts[8]);
                
                regions.push_back({x1, y1, x2, y2, text, mode, pixel_scale, xskew, yskew});
            }
        }
    }
    
    //puts("done loading regions");
    fclose(f);
}

void write_regions(std::string folder, std::string filename, int width, int height)
{
    if(folder.length() > 0)
        folder[folder.length()-1] = '_';
    auto f = profile_fopen((".config/ネズヨミ/region_"+folder+filename+".txt").data(), "wb");
    if(!f)
    {
        puts("couldn't open file");
        puts((".config/ネズヨミ/region_"+folder+filename+".txt").data());
        return;
    }
    //puts("writing regions for");
    //puts((folder+filename).data());
    
    fputs(std::to_string(width).data(), f);
    fputc('\t', f);
    fputs(std::to_string(height).data(), f);
    fputc('\n', f);
    
    for(const region & r : regions)
    {
        fputs(std::to_string(r.x1).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.y1).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.x2).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.y2).data(), f);
        fputc('\t', f);
        for(char c : r.text)
        {
            if(c == '\t')
            {
                fputc('\\', f);
                fputc('t', f);
            }
            else if(c == '\n')
            {
                fputc('\\', f);
                fputc('n', f);
            }
            else if(c == '\\')
            {
                fputc('\\', f);
                fputc('\\', f);
            }
            else if(c != '\r')
                fputc(c, f);
        }
        fputc('\t', f);
        fputs(std::to_string(r.mode).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.pixel_scale).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.xskew).data(), f);
        fputc('\t', f);
        fputs(std::to_string(r.yskew).data(), f);
        fputc('\n', f);
    }
    
    fclose(f);
}

// forward declare int ocr(){} from ocr.cpp
int ocr(const char * filename, const char * commandfilename, const char * outfilename, const char * scale, const char * xshear, const char * yshear);

unsigned char * crop_copy(renderer::texture * tex, int x1, int y1, int x2, int y2, int * width, int * height)
{
    x1 = std::min(std::max(0, x1), tex->w-1);
    x2 = std::min(std::max(0, x2), tex->w);
    y1 = std::min(std::max(0, y1), tex->h-1);
    y2 = std::min(std::max(0, y2), tex->h);
    *width = x2-x1;
    *height = y2-y1;
    unsigned char * data = (unsigned char *)malloc(*width**height*4);
    
    int tw = tex->w;
    
    int i = 0;
    for(int y = y1; y < y2; y++)
    {
        for(int x = x1; x < x2; x++)
        {
            for(int c = 0; c < 4; c++)
            {
                data[i] = tex->mydata[(y*tw + x)*4 + c];
                i++;
            }
        }
    }
    return data;
}

int estimate_width(unsigned char * data, int width, int height)
{
    int first_low_saturation = -1;
    int last_low_saturation = -1;
    int first_high_saturation = -1;
    int last_high_saturation = -1;
    float typical_saturation = 0;
    float typical_saturation_normalize = 0;
    
    for(int y = 10; y < height-10; y++)
    {
        for(int x = 3; x < width-3; x++)
        {
            float saturation = 0;
            for(int c = 0; c < 3; c++)
            {
                saturation += data[(y*width + x)*4 + c];
            }
            saturation /= (256*3);
            if(x < 10 or x > width-10 or y < 23 or y > height-23)
            {
                typical_saturation += saturation;
                typical_saturation_normalize += 1;
            }
            if(saturation < 0.4 and (x < first_low_saturation or first_low_saturation == -1))
                first_low_saturation = x;
            if(saturation < 0.4 and (x > last_low_saturation or last_low_saturation == -1))
                last_low_saturation = x;
            if(saturation > 0.6 and (x < first_high_saturation or first_high_saturation == -1))
                first_high_saturation = x;
            if(saturation > 0.6 and (x > last_high_saturation or last_high_saturation == -1))
                last_high_saturation = x;
        }
    }
    typical_saturation /= typical_saturation_normalize;
    // looks like black on white
    if(typical_saturation > 0.5)
    {
        puts("looks like black on white");
        return std::max(7, last_low_saturation-first_low_saturation);
    }
    // looks like white on black
    else
    {
        puts("looks like white on black");
        return std::max(7, last_high_saturation-first_high_saturation);
    }
}

struct textobject {
    std::string text;
};

int ocrmode = 0;
int shear_x = 0;
int shear_y = 0;

#ifdef _WIN32

int wmain (int argc, wchar_t ** argv)
{
    char * arg;
    int status;
    if(argc > 1)
        arg = (char *)utf16_to_utf8((uint16_t *)(argv[1]), &status);
    else
    {
        puts("Nezuyomi must be invoked with a png or jpeg image or a directory containing png and/or jpeg images. Drag one onto it or give it a command line argument.");
        getchar();
        return 0;
    }
    
    if(!arg)
    {
        puts("failed to convert file to open to UTF-8. might contain invalid UTF-16.");
        getchar();
        return 0;
    }
    
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    
    // store CWD
    std::string cwd;
    {
        wchar_t * buffer;
        buffer = _wgetcwd(0, 0);
        if(!buffer)
        {
            puts("failed to get current working directory");
            getchar();
            return 0;
        }
        int status;
        char * buffer8 = (char *)utf16_to_utf8((uint16_t *)buffer, &status);
        if(buffer8)
        {
            puts("current working directory:");
            puts(buffer8);
            cwd = std::string(buffer8);
            if(cwd.length() > 0)
                cwd += "/";
            free(buffer8);
        }
        free(buffer);
    }
    
    bool chwd_success = false;
    {
        std::string profdir = profile()+".config/ネズヨミ/";
        int status;
        uint16_t * profdir_w = utf8_to_utf16((uint8_t *)profdir.data(), &status);
        if(profdir_w)
        {
            int status2 = _wchdir((wchar_t *)profdir_w);
            if(status2 == 0)
                chwd_success = true;
            free(profdir_w);
        }
    }
    
#else

int main(int argc, char ** argv)
{
    if(argc < 2)
    {
        puts("Nezuyomi must be invoked with a png or jpeg image or a directory containing png and/or jpeg images. Drag one onto it or give it a command line argument.");
        getchar();
        return 0;
    }
    char * arg = argv[1];
    
    // store CWD
    std::string cwd;
    {
        char * buffer;
        // almost every modern unix-like OS supports passing a null buffer to getcwd to make it allocate the buffer itself
        buffer = getcwd(0, 0);
        if(!buffer)
        {
            puts("failed to get current working directory");
            return 0;
        }
        puts("current working directory:");
        puts(buffer);
        cwd = std::string(buffer);
        if(cwd.length() > 0)
            cwd += "/";
        free(buffer);
    }
    
    bool chwd_success = false;
    {
        std::string profdir = profile()+".config/ネズヨミ/";
        int status = chdir(profdir.data());
        if(status == 0)
            chwd_success = true;
    }
    
#endif

    if(!chwd_success)
    {
        puts("Failed to set working directory to profile directory.");
        puts("Nezuyomi doesn't require the working directory to be the profile directory, but this can still cause problems.");
        puts("For example, this can cause problems like some OCR setups not working.");
    }
    
    setlocale(LC_NUMERIC, "C");
    
    load_config();
    init_font();
    
    float x = 0;
    float y = 0;
    
    std::string path = std::string(arg);
    
    #ifdef _WIN32
    if(path.length() > 2 and (path[1] != ':' or path[2] != '\\'))
        path = cwd+path;
    #else
    if(path.length() > 0 and path[0] != '/')
        path = cwd+path;
    #endif
    
    std::string folder;
    std::string filename;
    
    bool from_filename = false;
    if(looks_like_image_filename(path))
    {
        from_filename = true;
        int i;
        for(i = path.length()-1; i > 0 and path[i] != '/' and path[i] != '\\'; i--);
        
        filename = path.substr(i+1);
        path = path.substr(0, i+1);
    }
    {
        int i;
        for(i = path.length()-2; i > 0 and path[i] != '/' and path[i] != '\\'; i--);
        if(path[i] == '/' or path[i] == '\\') i += 1;
        folder = path.substr(i).data();
        puts(folder.data());
    }
    
    // TODO: abstract win32 dirent logic into own header
    // the dirent.h we're using here converts from ANSI instead of from utf-8 for the non-wchar version, making it useless
    
    #ifdef _WIN32
    
    wchar_t * dircstr = (wchar_t *)utf8_to_utf16((uint8_t *)path.data(), &status);
    if(!dircstr)
    {
        puts("failed convert directory string");
        getchar();
        return 0;
    }
    auto dir = _wopendir(dircstr);
    free(dircstr);
    
    #else
    
    auto dir = opendir(path.data());
    
    #endif
    
    if(!dir)
    {
        puts("failed to open directory");
        puts(path.data());
        getchar();
        return 0;
    }
    
    std::vector<std::string> mydir;
    std::vector<std::string> mydir_filenames;
    
    #ifdef _WIN32
    
    _wdirent * myent = _wreaddir(dir);
    #else
    
    dirent * myent = readdir(dir);
    
    #endif
    
    while(myent)
    {
        #ifdef _WIN32
        
        char * text = (char *)utf16_to_utf8((uint16_t *)myent->d_name, &status);
        if(!text) return 0;
        
        #else
        
        char * text = myent->d_name;
        
        #endif
        
        std::string str = path + text;
        if(looks_like_image_filename(str))
        {
            mydir.push_back(str);
            mydir_filenames.push_back(std::string(text));
        }
        
        #ifdef _WIN32
        
        free(text);
        myent = _wreaddir(dir);
        
        #else
        
        myent = readdir(dir);
        
        #endif
    }
    
    #ifdef _WIN32
    _wclosedir(dir);
    #else
    closedir(dir);
    #endif
    
    // we are now done operating with the filesystem!
    
    if(mydir.size() == 0) return 0;
    
    int index = 0;
    if(from_filename)
    {
        for(size_t i = 0; i < mydir.size(); i++)
        {
            std::string testpath = std::string(mydir[i]);
            int j = 0;
            for(j = testpath.length()-1; j > 0 and testpath[j] != '/' and testpath[j] != '\\'; j--);
            
            std::string testfilename = testpath.substr(j+1);
            
            if(testfilename == filename)
            {
                index = i;
                break;
            }
        }
    }
    
    renderer myrenderer;
    
    auto & win = myrenderer.win;
    glfwSetScrollCallback(win, myScrollEventCallback);
    glfwSetKeyCallback(win, myKeyEventCallback);
    glfwSetErrorCallback(error_callback);
    
    
    auto myimage = myrenderer.load_texture(mydir[index].data());
    if(!myimage) return 0;
    
    
    load_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
    // set default position
    
    float xscale, yscale, scale; // "scale" is actually used to scale the image. xscale and yscale are for logic.
    getscale(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale);
    reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
    
    float oldtime = glfwGetTime();
    while (!glfwWindowShouldClose(win))
    {
        static bool washolding = true;
        float newtime;
        float delta;
        if(washolding)
        {
            newtime = glfwGetTime();
            delta = newtime-oldtime;
            glfwPollEvents();
        }
        else
        {
            glfwWaitEventsTimeout(0.1);
            newtime = glfwGetTime();
            delta = newtime-oldtime;
            delta = std::min(delta, 0.01f);
        }
        
        oldtime = newtime;
        
        
        bool go_to_next_page = false;
        bool go_to_last_page = false;
        
        int current_m4 = glfwGetMouseButton(win, 3);
        static int last_m4 = current_m4;
        if(current_m4 == GLFW_PRESS and last_m4 != GLFW_PRESS)
            go_to_last_page = true;
        last_m4 = current_m4;
        
        int current_m5 = glfwGetMouseButton(win, 4);
        static int last_m5 = current_m5;
        if(current_m5 == GLFW_PRESS and last_m5 != GLFW_PRESS)
            go_to_next_page = true;
        last_m5 = current_m5;
        
        int pgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        static int lastPgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        if(pgUp and !lastPgUp)
            go_to_last_page = true;
        lastPgUp = pgUp;
        
        int pgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        static int lastPgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        if(pgDn and !lastPgDn)
            go_to_next_page = true;
        lastPgDn = pgDn;
        
        if(go_to_last_page and index > 0)
        {
            puts("entering A");
            repeat:
            index = std::max(index-1, 0);
            myrenderer.delete_texture(myimage);
            myimage = myrenderer.load_texture(mydir[index].data());
            if(!myimage and index > 0)
            {
                puts("looping A");
                goto repeat;
            }
            else if(!myimage)
            {
                index = 0;
                myimage = myrenderer.load_texture(mydir[0].data());
            }
            load_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
            if(reset_position_on_new_page)
            {
                getscale(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale);
                reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y, !pgup_to_bottom);
            }
        }
        if(go_to_next_page and index < int(mydir.size()-1))
        {
            //puts("entering B");
            repeat2:
            index = std::min(index+1, int(mydir.size()-1));
            myrenderer.delete_texture(myimage);
            myimage = myrenderer.load_texture(mydir[index].data());
            if(!myimage and index < int(mydir.size()-1))
            {
                //puts("looping B");
                goto repeat2;
            }
            else if(!myimage)
            {
                index = 0;
                myimage = myrenderer.load_texture(mydir[0].data());
            }
            load_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
            if(reset_position_on_new_page)
            {
                getscale(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale);
                reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
            }
        }
        
        getscale(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale);
        
        static float lastscale = scale;
        if(scale != lastscale)
        {
            if(scalemode == 2)
                reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
            else
            {
                x /= lastscale;
                x *= scale;
                y /= lastscale;
                y *= scale;
                reset_position_partial(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
            }
        }
        lastscale = scale;
        
        
        float motionscale;
        if(xscale > yscale)
            motionscale = myrenderer.w/1000.0;
        else
            motionscale = myrenderer.h/1000.0;
        
        
        washolding = false;
        if(glfwGetKey(win, GLFW_KEY_UP) or glfwGetKey(win, GLFW_KEY_E))
        {
            y -= speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_DOWN) or glfwGetKey(win, GLFW_KEY_D))
        {
            y += speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_LEFT) or glfwGetKey(win, GLFW_KEY_W))
        {
            x -= speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_RIGHT) or glfwGetKey(win, GLFW_KEY_F))
        {
            x += speed*delta*motionscale;
            washolding = true;
        }
        
        int pressing_z = glfwGetKey(win, GLFW_KEY_Z);
        static int last_pressing_z = pressing_z;
        if(pressing_z and !last_pressing_z)
        {
            ocrmode = 0;
            currentsubtitle = subtitle("ocr set to mode 1 (ocr.txt)", 24, &myrenderer);
        }
        last_pressing_z = pressing_z;
        
        int pressing_x = glfwGetKey(win, GLFW_KEY_X);
        static int last_pressing_x = pressing_x;
        if(pressing_x and !last_pressing_x)
        {
            ocrmode = 1;
            currentsubtitle = subtitle("ocr set to mode 2 (ocr2.txt)", 24, &myrenderer);
        }
        last_pressing_x = pressing_x;
        
        int pressing_c = glfwGetKey(win, GLFW_KEY_C);
        static int last_pressing_c = pressing_c;
        if(pressing_c and !last_pressing_c)
        {
            ocrmode = 2;
            currentsubtitle = subtitle("ocr set to mode 3 (ocr3.txt)", 24, &myrenderer);
        }
        last_pressing_c = pressing_c;
        
        bool altpressed = (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS or glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
        bool ctrlpressed = (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS or glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        bool shiftpressed = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS or glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        
        scrollMutex.lock();
            if(scroll != 0)
            {
                if(shiftpressed and altpressed and !ctrlpressed)
                {
                    shear_x += scroll;
                    if(shear_x > 20) shear_x = 20;
                    if(shear_x < -20) shear_x = -20;
                    std::string about = "";
                    if(shear_x > 0)
                        about = "% (pushes top edge leftwards)";
                    if(shear_x < 0)
                        about = "% (pushes top edge rightwards)";
                    currentsubtitle = subtitle(std::string("set x axis shear for OCR to ")+std::to_string((int)(shear_x))+about, 24, &myrenderer);
                }
                else if(shiftpressed and !altpressed and ctrlpressed)
                {
                    shear_y += scroll;
                    if(shear_y > 20) shear_y = 20;
                    if(shear_y < -20) shear_y = -20;
                    std::string about = "";
                    if(shear_y > 0)
                        about = "% (pushes right edge downwards)";
                    if(shear_y < 0)
                        about = "% (pushes right edge upwards)";
                    currentsubtitle = subtitle(std::string("set y axis shear for OCR to ")+std::to_string((int)(shear_y))+about, 24, &myrenderer);
                }
                else if(altpressed and ctrlpressed)
                {
                    paddingestimate += scroll*0.05;
                    paddingestimate = round(paddingestimate*20)/20;
                    if(paddingestimate < 0) paddingestimate = 0;
                    if(paddingestimate > 1) paddingestimate = 1;
                    currentsubtitle = subtitle(std::string("set padding estimate for OCR to ")+std::to_string((int)(paddingestimate*100))+"%", 24, &myrenderer);
                }
                else if(altpressed)
                {
                    textscale += scroll;
                    if(textscale < 1) textscale = 1;
                    currentsubtitle = subtitle(std::string("set expected text size for OCR to ")+std::to_string(textscale), 24, &myrenderer);
                }
                else if(ctrlpressed)
                {
                    textlines += scroll;
                    if(textlines < 1) textlines = 1;
                    currentsubtitle = subtitle(std::string("set expected line count for OCR to ")+std::to_string(textlines)+std::string(" (only used in estimations)"), 24, &myrenderer);
                }
                else
                {
                    if(xscale > yscale)
                        y -= scroll*scrollspeed*motionscale;
                    else
                        x -= scroll*scrollspeed*motionscale*(invert_x?-1:1);
                }
                scroll = 0;
            }
        scrollMutex.unlock();
        
        
        int current_m1 = glfwGetMouseButton(win, 0);
        static int last_m1 = current_m1;
        
        static double m1_mx_press = 0;
        static double m1_my_press = 0;
        static double m1_mx_release = 0;
        static double m1_my_release = 0;
        
        if(current_m1 == GLFW_PRESS and last_m1 != GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, & mx, & my);
            
            m1_mx_press = mx;
            m1_my_press = my;
        }
        else if(current_m1 != GLFW_PRESS and last_m1 == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, & mx, & my);
            m1_mx_release = mx;
            m1_my_release = my;
            
            bool foundregion = false;
            for(region & r : regions)
            {
                float x1 = (r.x1*scale-x);
                float y1 = (r.y1*scale-y);
                float x2 = (r.x2*scale-x);
                float y2 = (r.y2*scale-y);
                
                if (m1_mx_release >= x1 and m1_mx_release <= x2 and m1_mx_press >= x1 and m1_mx_press <= x2 
                and m1_my_release >= y1 and m1_my_release <= y2 and m1_my_press >= y1 and m1_my_press <= y2)
                {
                    if(r.text != std::string(""))
                    {
                        glfwSetClipboardString(win, r.text.data());
                        puts(r.text.data());
                        currentsubtitle = subtitle(r.text, 24, &myrenderer);
                        
                        currentregion = &r;
                        foundregion = true;
                        break;
                    }
                    else
                    {
                        int img_w, img_h;
                        auto data = crop_copy(myimage, r.x1, r.y1, r.x2, r.y2, &img_w, &img_h);
                        
                        puts("writing cropped image to disk");
                        auto f = wrap_fopen((profile()+".config/ネズヨミ/temp_ocr.png").data(), "wb");
                        if(f)
                        {
                            stbi_write_png_to_func([](void * file, void * data, int size){
                                fwrite(data, 1, size, (FILE *) file);
                            }, f, img_w, img_h, 4, data, img_w*4);
                            fclose(f);
                        }
                        free(data);
                        puts("done");
                        
                        puts((profile()+".config/ネズヨミ/temp_ocr.png").data());
                        puts((profile()+".config/ネズヨミ/ocr.txt").data());
                        
                        r.pixel_scale = textscale;
                        r.xskew = shear_x;
                        r.yskew = shear_y;
                        
                        std::string scale_double_percent = std::to_string(32/float(textscale)*200);
                        
                        std::string xshear_string = std::to_string(shear_x/100.0);
                        std::string yshear_string = std::to_string(shear_y/100.0);
                        
                        std::string ocrfile;
                        
                        if(ocrmode == 1)
                            ocrfile = (profile()+".config/ネズヨミ/ocr2.txt");
                        else if(ocrmode == 2)
                            ocrfile = (profile()+".config/ネズヨミ/ocr3.txt");
                        else
                            ocrfile = (profile()+".config/ネズヨミ/ocr.txt");
                        
                        ocr((profile()+".config/ネズヨミ/temp_ocr.png").data(), ocrfile.data(), (profile()+".config/ネズヨミ/temp_text.txt").data(), (scale_double_percent.data()), xshear_string.data(), yshear_string.data());
                        
                        auto f2 = wrap_fopen((profile()+".config/ネズヨミ/temp_text.txt").data(), "rb");
                        if(f2)
                        {
                            fseek(f2, 0, SEEK_END);
                            size_t len = ftell(f2);
                            fseek(f2, 0, SEEK_SET);
                            
                            char * s = (char *)malloc(len+1);
                            
                            if(s)
                            {
                                fread(s, 1, len, f2);
                                s[len] = 0;
                                
                                r.text = std::string(s);
                                glfwSetClipboardString(win, s);
                                
                                puts(r.text.data());
                                currentsubtitle = subtitle(r.text, 24, &myrenderer);
                                puts("rjkrek");
                                
                                free(s);
                                puts("fddrtht");
                            }
                            fclose(f2);
                        }
                        
                        currentregion = &r;
                        
                        write_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
                        foundregion = true;
                        break;
                    }
                }
            }
            if(!foundregion)
            {
                if(abs(m1_mx_release-m1_mx_press) > 2 and abs(m1_my_release-m1_my_press) > 2)
                {
                    float lowerx = std::min(m1_mx_release, m1_mx_press);
                    float lowery = std::min(m1_my_release, m1_my_press);
                    float upperx = std::max(m1_mx_release, m1_mx_press);
                    float uppery = std::max(m1_my_release, m1_my_press);
                    regions.push_back({int((lowerx+x)/scale), int((lowery+y)/scale), int((upperx+x)/scale), int((uppery+y)/scale), "", ocrmode, textscale});
                    auto & r = regions[regions.size()-1];
                    
                    int img_w, img_h;
                    auto data = crop_copy(myimage, r.x1, r.y1, r.x2, r.y2, &img_w, &img_h);
                    int estimated_width = estimate_width(data, img_w, img_h);
                    printf("estimated width %d\n", estimated_width);
                    free(data);
                    
                    float pxwide = estimated_width;
                    int estimate = pxwide/textlines * (1 - float(textlines-1)/(textlines)*paddingestimate); // estimate removal of internal padding
                    if(estimate < 7) estimate = 7;
                    
                    currentsubtitle = subtitle(std::string("estimated text size (if vertical and ")+std::to_string(textlines)+std::string(".0 lines): ")+std::to_string(estimate), 24, &myrenderer);
                    
                    currentregion = &(regions[regions.size()-1]);
                    
                    tempregion = {0,0,0,0,"",0,0,0,0};
                }
            }
        }
        else if(current_m1 == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, & mx, & my);
            if(abs(mx-m1_mx_press) > 2 and abs(my-m1_my_press) > 2)
            {
                float lowerx = std::min(mx, m1_mx_press);
                float upperx = std::max(mx, m1_mx_press);
                float lowery = std::min(my, m1_my_press);
                float uppery = std::max(my, m1_my_press);
                tempregion = {int((lowerx+x)/scale), int((lowery+y)/scale), int((upperx+x)/scale), int((uppery+y)/scale), "", ocrmode, textscale, shear_x, shear_y};
            }
            else
                tempregion = {0,0,0,0,"",0,0,0,0};
        }
        else
            tempregion = {0,0,0,0,"",0,0,0,0};
        last_m1 = current_m1;
        
        
        int current_m2 = glfwGetMouseButton(win, 1);
        static int last_m2 = current_m2;
        
        static double m2_mx_press = 0;
        static double m2_my_press = 0;
        static double m2_mx_release = 0;
        static double m2_my_release = 0;
        
        if(current_m2 == GLFW_PRESS and last_m2 != GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, & mx, & my);
            
            m2_mx_press = mx;
            m2_my_press = my;
        }
        else if(current_m2 != GLFW_PRESS and last_m2 == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, & mx, & my);
            m2_mx_release = mx;
            m2_my_release = my;
            
            for(size_t i = 0; i < regions.size(); i++)
            {
                region & r = regions[i];
                float x1 = (r.x1*scale-x);
                float y1 = (r.y1*scale-y);
                float x2 = (r.x2*scale-x);
                float y2 = (r.y2*scale-y);
                
                if (m2_mx_release >= x1 and m2_mx_release <= x2 and m2_mx_press >= x1 and m2_mx_press <= x2 
                and m2_my_release >= y1 and m2_my_release <= y2 and m2_my_press >= y1 and m2_my_press <= y2)
                {
                    if(&r == currentregion)
                        currentregion = 0;
                    
                    regions.erase(regions.begin()+i);
                    write_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
                    break;
                }
            }
            currentsubtitle = subtitle();
        }
        last_m2 = current_m2;
        
        
        int current_v = glfwGetKey(win, GLFW_KEY_V);
        static int last_v = current_v;
        bool ctrl_pressed = ((glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) or (glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
        
        if(current_v == GLFW_PRESS and last_v != GLFW_PRESS and ctrl_pressed)
        {
            auto s = glfwGetClipboardString(win);
            if(s)
            {
                currentregion->text = std::string(s);
                currentsubtitle = subtitle(currentregion->text, 24, &myrenderer);
                
                write_regions(folder, mydir_filenames[index], myimage->w, myimage->h);
            }
        }
        
        last_v = current_v;
        
        
        
        myrenderer.cycle_start();
        
        limit_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
        
        myrenderer.cam_x = x;
        myrenderer.cam_y = y;
        myrenderer.cam_scale = scale;
        
        myrenderer.draw_texture(myimage, 0, 0, 0.2);
        
        myrenderer.downscaling = scale < 1;
        myrenderer.infoscale = (scale>1)?(scale):(1);
        myrenderer.cycle_post();
        
        for(region r : regions)
        {
            //myrenderer.draw_rect(x1, y1, x2, y2, 0.2, 0.8, 1.0, 0.5);
            //myrenderer.draw_rect(r.x1, r.y1, r.x2, r.y2, 0.2, 0.8, 1.0, 0.5);
            float p = 1/scale;
            myrenderer.draw_rect(r.x1-p, r.y1-p, r.x2-p, r.y1+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x1-p, r.y1+p, r.x1+p, r.y2+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x1+p, r.y2-p, r.x2+p, r.y2+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x2-p, r.y1-p, r.x2+p, r.y2-p, 0.2, 0.8, 1.0, 0.5);
        }
        // with tempregion too
        {
            auto & r = tempregion;
            float p = 1/scale;
            myrenderer.draw_rect(r.x1-p, r.y1-p, r.x2-p, r.y1+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x1-p, r.y1+p, r.x1+p, r.y2+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x1+p, r.y2-p, r.x2+p, r.y2+p, 0.2, 0.8, 1.0, 0.5);
            myrenderer.draw_rect(r.x2-p, r.y1-p, r.x2+p, r.y2-p, 0.2, 0.8, 1.0, 0.5);
        
        }
        if(currentsubtitle.initialized and fontinitialized)
        {
            ////puts("bfr");
            uint32_t lastindex = 0;
            float fontscale = stbtt_ScaleForPixelHeight(&fontinfo, 24);
            
            int ascent, descent, linegap;
            stbtt_GetFontVMetrics(&fontinfo, &ascent, &descent, &linegap);
            float actual_descent = descent*fontscale;
            float actual_ascent = ascent*fontscale;
            float x = 0;
            float y = myrenderer.h + actual_descent - 5;
            
            myrenderer.draw_rect(0, y - actual_ascent, myrenderer.w, myrenderer.h, 0, 0, 0, 0.5, true);
            
            int i = 0;
            for(auto c : currentsubtitle.codepoints)
            {
                ////puts("a");
                auto glyph = textcache[c];
                ////puts("y");
                if(glyph->texture)
                    myrenderer.draw_text_texture(glyph->texture, round(x+glyph->x), round(y+glyph->y), 0.2);
                
                if(lastindex)
                    x += fontscale*stbtt_GetGlyphKernAdvance(&fontinfo, lastindex, glyph->index);
                ////puts("u");
                lastindex = glyph->index;
                
                x += glyph->advance;
                
                ////puts("b");
                i++;
            }
        }
        //myrenderer.draw_rect(-1, -1, 1, 1, 10, 0.2, 0.8, 1.0, 0.4);
        
        myrenderer.cycle_end();
        
        if(delta < throttle)
            glfwWaitEventsTimeout(throttle-delta);
    }
    glfwDestroyWindow(win);
    
    return 0;
}
