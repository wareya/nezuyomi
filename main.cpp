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
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <math.h>
#include <locale.h>
#ifndef M_PI
#define M_PI 3.1415926435
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image_wrapper.h"

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
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
    try {stod(string);}
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

float downscaleradius = 6.0;
float sharphardness1 = 1;
float sharphardness2 = 1;
float sharpblur1 = 0.5;
float sharpblur2 = 0.5;
float sharpradius1 = 8.0;
float sharpradius2 = 16.0;

struct renderer {
    // TODO: FIXME: add a real reference counter
    struct texture {
        int w, h, n;
        GLuint texid;
        texture(const unsigned char * data, int w, int h)
        {
            this->w = w;
            this->h = h;
            this->n = 4;
            
            checkerr(__LINE__);
            glActiveTexture(GL_TEXTURE0);
            
            checkerr(__LINE__);
            
            glGenTextures(1, &texid);
            glBindTexture(GL_TEXTURE_2D, texid);
            printf("Actual size: %dx%d\n", this->w, this->h);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, this->w, this->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            
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
    };
    texture * lastTexture = nullptr;
    texture * load_texture(const wchar_t * filename)
    {
        auto start = glfwGetTime();
        puts("Starting load texture");
        _putws(filename);
        fflush(stdout);
        int w, h, n;
        unsigned char * data = stbi_load((const char *)filename, &w, &h, &n, 4);
        puts("Done actual loading");
        if(!data) return puts("failed to open texture"), nullptr;
        else
        {
            printf("Building texture of size %dx%d\n", w, h);
            
            if(lastTexture)
                if(glIsTexture(lastTexture->texid))
                    glDeleteTextures(1, &lastTexture->texid);
            
            auto tex = new texture(data, w, h);
            
            lastTexture = tex;
            puts("Built texture");
            stbi_image_free(data);
            auto end = glfwGetTime();
            printf("Time: %f\n", end-start);
            return tex;
        }
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
    
    unsigned int VAO, VBO, FBO, FBOtexture1, FBOtexture2;
    int w, h;
    unsigned int vshader;
    unsigned int fshader;
    unsigned int program;
    
    float jinctexture[512];
    float sinctexture[512];
    
    bool downscaling = false;
    float infoscale = 1.0;
    
    GLFWwindow * win;
    postprogram * copy, * sharpen, * nusharpen;
    renderer()
    {
        glfwSwapInterval(1);
        
        if(!glfwInit()) puts("glfw failed to init"), exit(0);
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        win = glfwCreateWindow(1104*0.8, 600, "Hello, World!", NULL, NULL);
        
        if(!win) puts("glfw failed to init"), exit(0);
        glfwMakeContextCurrent(win);
        
        if(gl3wInit()) puts("gl3w failed to init"), exit(0);
        
        for(int i = 0; i < 512; i++)
        {
            //jinctexture[i] = sin(float(i)*M_PI/4)*0.5+0.5;///(float(i)*M_PI/4)*0.5+0.5;
            if(i == 0) jinctexture[i] = 1.0;
            else       jinctexture[i] = 2*std::cyl_bessel_j(1, float(i*M_PI)/8)/(float(i*M_PI)/8)*0.5+0.5;
            
            if(i == 0) sinctexture[i] = 1.0;
            else       sinctexture[i] = sin(float(i*M_PI)/8)/(float(i*M_PI)/8)*0.5+0.5;
        }
        
        //glfwSwapBuffers(win);
        glfwGetFramebufferSize(win, &w, &h);
        
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
        {
            puts(message);
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
            if(x < -radius || x > radius) return 0;\n\
            return jinc(x) * cos(x*M_PI/2/radius);\n\
        }\n\
        float sincwindow(float x, float radius)\n\
        {\n\
            if(x < -radius || x > radius) return 0;\n\
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
        
        program = glCreateProgram();
        glAttachShader(program, vshader);
        glAttachShader(program, fshader);
        glLinkProgram(program);
        
        checkerr(__LINE__);
        
        int vsuccess, fsuccess, psuccess;
        glGetShaderiv(vshader, GL_COMPILE_STATUS, &vsuccess);
        glGetShaderiv(fshader, GL_COMPILE_STATUS, &fsuccess);
        glGetProgramiv(program, GL_LINK_STATUS, &psuccess);
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
                glGetProgramInfoLog(program, 512, NULL, info);
                puts(info);
            }
            
            exit(0);
        }
        checkerr(__LINE__);
        
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "mytexture"), 0);
        glUniform1i(glGetUniformLocation(program, "myJincLookup"), 1);
        glUniform1i(glGetUniformLocation(program, "mySincLookup"), 2);
        
        checkerr(__LINE__);
        
        glDeleteShader(fshader);
        glDeleteShader(vshader);
        
        checkerr(__LINE__);
        
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
            if(x < -radius || x > radius) return 0;\n\
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
        glUniform1i(glGetUniformLocation(sharpen->program, "myJincLookup"), 1);
        glUniform1i(glGetUniformLocation(sharpen->program, "wetness"), 1);
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
            if(x < -radius || x > radius) return 0;\n\
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
        
        glUseProgram(program);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        
        float projection[16] = {
            2.0f/w,  0.0f, 0.0f,-1.0f,
            0.0f, -2.0f/h, 0.0f, 1.0f,
            0.0f,    0.0f, 1.0f, 0.0f,
            0.0f,    0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, 0, projection);
        
        glClearColor(0,0,0,1);
        glDepthMask(true);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        checkerr(__LINE__);
    }
    void cycle_end()
    {
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
        
        if(downscaling && usedownscalesharpening && usejinc)
        {
            FLIP_SOURCE();
            glUseProgram(sharpen->program);
            glUniform1f(glGetUniformLocation(sharpen->program, "radius"), downscaleradius);
            glUniform1f(glGetUniformLocation(sharpen->program, "blur"), 1.0f);
            glUniform1f(glGetUniformLocation(sharpen->program, "wetness"), 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        
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
        
        FLIP_SOURCE();
        BUFFER_DONE();
        glUseProgram(copy->program);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        glFinish();
        glfwSwapBuffers(win);
        glFinish();
    }
    void draw_texture(texture * texture, float x, float y, float z, float xscale, float yscale)
    {
        float w = float(texture->w);
        float h = float(texture->h);
        const vertex vertices[] = {
            {0, 0, 0.0f, 0.0f, 0.0f},
            {w, 0, 0.0f, 1.0f, 0.0f},
            {0, h, 0.0f, 0.0f, 1.0f},
            {w, h, 0.0f, 1.0f, 1.0f}
        };
        
        float translation[16] = {
            xscale,   0.0f, 0.0f,    x,
              0.0f, yscale, 0.0f,    y,
              0.0f,   0.0f, 1.0f,    z,
              0.0f,   0.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(glGetUniformLocation(program, "translation"), 1, 0, translation);
        glUniform2f(glGetUniformLocation(program, "mySize"), w, h);
        glUniform2f(glGetUniformLocation(program, "myScale"), xscale, yscale);
        glUniform1i(glGetUniformLocation(program, "usejinc"), usejinc);
        glUniform1f(glGetUniformLocation(program, "myradius"), downscaleradius);
        glBindTexture(GL_TEXTURE_2D, texture->texid);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkerr(__LINE__);
    }
};

std::wstring towstring(std::string mystring)
{
    size_t S = MultiByteToWideChar(CP_UTF8, 0, mystring.data(), -1, NULL, 0)*sizeof(wchar_t);
    std::wstring W_S(S, L'#');
    MultiByteToWideChar(CP_UTF8, 0, mystring.data(), -1, &W_S[0], S);
    return W_S;
}

bool looks_like_image_filename(std::wstring string)
{
    
    if (string.rfind(L".png") == std::string::npos
    and string.rfind(L".jpg") == std::string::npos
    and string.rfind(L".jpeg") == std::string::npos
    and string.rfind(L".Png") == std::string::npos
    and string.rfind(L".Jpg") == std::string::npos
    and string.rfind(L".Jpeg") == std::string::npos
    and string.rfind(L".PNG") == std::string::npos
    and string.rfind(L".JPG") == std::string::npos
    and string.rfind(L".JPEG") == std::string::npos)
        return false;
    else
        return true;
}


// Returns a utf-32 codepoint
// Returns 0xFFFFFFFF if there was any error decoding the codepoint. There are two error types: decoding errors (invalid utf-8) and buffer overruns (the buffer cuts off mid-codepoint).
// Advance is set to the number of code units (16-bit chunks) consumed if no error occured
// Advance is set to 1 if a decoding error occured
// Advance is set to 0 if a buffer overrun occured
uint32_t utf16_pull(const uint16_t * text, size_t len, int * advance)
{
    if(len < 1)
    {
        *advance = 0;
        return 0xFFFFFFFF;
    }
    
    int i = 0;
    uint32_t c = text[i++]; // type used is large in order to do bit shifting
    
    if(c >= 0xDC00 and c <= 0xDFFF) // low surrogate when initial code unit expected
    {
        return 0xFFFFFFFF;
    }
    else if (c < 0xD800 or c > 0xDBFF) // single code unit
    {
        *advance = i;
        return c;
    }
    else if((c&0b0010'0000) == 0) // high surrogate
    {
        if(len < 2)
        {
            *advance = 0;
            return 0xFFFFFFFF;
        }
        
        uint32_t c2 = text[i++];
        if(c < 0xDC00 or c > 0xDFFF) // initial code unit when low surrogate expected
        {
            *advance = 1;
            return 0xFFFFFFFF;
        }
        
        uint32_t p = 0;
        p |=  c2&0x03FF;
        p |=  (c&0x03FF)<<10;
        
        *advance = i;
        return p;
    }
    return 0xFFFFFFFF;
}
uint32_t utf16_pull(const wchar_t * text, long long len, int * advance)
{
    return utf16_pull((const uint16_t *)text, len, advance);
}

std::string u16_to_u8(const wchar_t * text)
{
    std::string ret;
    int len;
    for(len = 0; text[len] != 0; len++);
    len++; // include null terminator
    int advance;
    uint32_t codepoint = utf16_pull(text, len, &advance);
    while(advance > 0)
    {
        text += advance;
        len -= advance;
        if(codepoint != 0xFFFFFFFF)
        {
            if(codepoint < 0x80)
            {
                ret += char(codepoint);
            }
            else if(codepoint < 0x800)
            {
                ret += char(((codepoint>> 6)&0b0001'1111)|0b1100'0000);
                ret += char(((codepoint>> 0)&0b0011'1111)|0b1000'0000);
            }
            else if(codepoint < 0x10000)
            {
                ret += char(((codepoint>>12)&0b0000'1111)|0b1110'0000);
                ret += char(((codepoint>> 6)&0b0011'1111)|0b1000'0000);
                ret += char(((codepoint>> 0)&0b0011'1111)|0b1000'0000);
            }
            else if(codepoint < 0x110000)
            {
                ret += char(((codepoint>>18)&0b0000'0111)|0b1111'0000);
                ret += char(((codepoint>>12)&0b0011'1111)|0b1000'0000);
                ret += char(((codepoint>> 6)&0b0011'1111)|0b1000'0000);
                ret += char(((codepoint>> 0)&0b0011'1111)|0b1000'0000);
            }
        }
        codepoint = utf16_pull(text, len, &advance);
    }
    return ret;
}

std::mutex scrollMutex;
float scroll = 0;

void myScrollEventCallback(GLFWwindow * win, double x, double y)
{
    scrollMutex.lock();
        scroll += y;
    scrollMutex.unlock();
}

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
        if(key == GLFW_KEY_V)
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
        if(key == GLFW_KEY_D)
        {
            reset_position_on_new_page = !reset_position_on_new_page;
            if(reset_position_on_new_page) puts("Position will now be reset on page change");
            else puts("Position no longer reset on page change");
        }
        if(key == GLFW_KEY_F)
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

void load_config()
{
    std::ifstream f("config.txt");
    std::string str;
    //puts("config:");
    while(std::getline(f, str))
    {
        // This is the normal way to split a string in C++. Fuck the committee.
        const std::string delimiter = ":";
        const auto start = str.find(delimiter);
        const auto end = start + delimiter.length();
        const std::string first = str.substr(0, start);
        const std::string second = str.substr(end);
        
        config[first] = {double_from_string(second), second, is_number(second)};
        printf("%s:%s(%f)\n", first.data(), config[first].text.data(), config[first].real);
    }
}

// Unicode file path handling on windows is complete horseshit. Sorry. This should be relatively easy to change if you're on a reasonable OS.
int wmain (int argc, wchar_t **argv)
{   
    wchar_t * arg;
    if(argc > 1) arg = argv[1];
    else return 0;
    
    setlocale(LC_NUMERIC, "C");
    
    load_config();
    
    float x = 0;
    float y = 0;
    
    renderer myrenderer;
    
    auto & win = myrenderer.win;
    glfwSetScrollCallback(win, myScrollEventCallback);
    glfwSetKeyCallback(win, myKeyEventCallback);
    glfwSetErrorCallback(error_callback);
    
    std::vector<std::wstring> mydir;
    
    int index = 0;
    
    std::wstring directory(arg);
    fs::path file(arg);
    std::wstring filename = file.filename();
    bool from_filename;
    if(looks_like_image_filename(directory))
    {
        directory = fs::path(arg).parent_path().wstring();
        if(directory == L"") directory = L".";
        from_filename = true;
    }
    
    for(auto& p : fs::directory_iterator(directory))
    {
        // There's something VERY DEEPLY BROKEN in mingw's implementation of std::experimental::filesystem w/r/t unicode filenames/paths.
        // This is the only way I could get my test case to work. No, really, it is.
        // Fucking fuck the C++ """ecosystem""", if you can even call something this destitute and broken by such a pleasant name.
        std::wstring path = ((p.path().root_path().wstring())+(p.path().relative_path().wstring()));
        
        if(!looks_like_image_filename(path))
            continue;
        else
        {
            if(from_filename)
            {
                if(p.path().filename() == filename)
                    from_filename = false;
                else
                    index++;
            }
            mydir.push_back(path);
        }
    }
    if(index == int(mydir.size())) index = 0;
    
    auto myimage = myrenderer.load_texture(mydir[index].data());
    if(!myimage) return 0;
    
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
        
        static int lastPgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        static int lastPgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        
        int pgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        int pgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        
        if(pgUp and !lastPgUp and index > 0)
        {
            puts("entering A");
            repeat:
            index = std::max(index-1, 0);
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
            if(reset_position_on_new_page)
            {
                reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y, !pgup_to_bottom);
            }
        }
        if(pgDn and !lastPgDn and index < int(mydir.size()-1))
        {
            //puts("entering B");
            repeat2:
            index = std::min(index+1, int(mydir.size()-1));
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
            if(reset_position_on_new_page)
            {
                reset_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
            }
        }
        
        lastPgUp = pgUp;
        lastPgDn = pgDn;
        
        
        float motionscale;
        if(xscale > yscale)
            motionscale = myrenderer.w/1000.0;
        else
            motionscale = myrenderer.h/1000.0;
        
        
        washolding = false;
        if(glfwGetKey(win, GLFW_KEY_UP))
        {
            y -= speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_DOWN))
        {
            y += speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_LEFT))
        {
            x -= speed*delta*motionscale;
            washolding = true;
        }
        if(glfwGetKey(win, GLFW_KEY_RIGHT))
        {
            x += speed*delta*motionscale;
            washolding = true;
        }
        
        scrollMutex.lock();
            if(xscale > yscale)
                y -= scroll*scrollspeed*motionscale;
            else
                x -= scroll*scrollspeed*motionscale*(invert_x?-1:1);
            scroll = 0;
        scrollMutex.unlock();
        
        myrenderer.cycle_start();
        
        limit_position(myrenderer.w, myrenderer.h, myimage->w, myimage->h, xscale, yscale, scale, x, y);
        
        myrenderer.draw_texture(myimage, -round(x), -round(y), 0.2, scale, scale);
        
        myrenderer.downscaling = scale < 1;
        myrenderer.infoscale = (scale>1)?(scale):(1);
        myrenderer.cycle_end();
        
        if(delta < throttle)
            std::this_thread::sleep_for(std::chrono::duration<float>(throttle-delta));
    }
    glfwDestroyWindow(win);
    
    return 0;
}
