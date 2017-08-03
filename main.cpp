#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image_wrapper.h"

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include <iostream>

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

struct renderer {
    // TODO: FIXME: add a real reference counter
    struct texture {
        int w, h, n;
        GLuint texid;
        texture(const unsigned char * data, int w, int h)
        {
            this->w = w;
            this->h = h;
            this->n = 3;
            
            checkerr(__LINE__);
            glActiveTexture(GL_TEXTURE0);
            
            checkerr(__LINE__);
            
            glGenTextures(1, &texid);
            glBindTexture(GL_TEXTURE_2D, texid);
            printf("Actual size: %dx%d\n", this->w, this->h);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, this->w, this->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            
            checkerr(__LINE__);
            puts("Generating mipmaps");
            glGenerateMipmap(GL_TEXTURE_2D);
            puts("Done generating mipmaps");
            checkerr(__LINE__);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            checkerr(__LINE__);
        }
    };
    texture * lastTexture = nullptr;
    texture * load_texture(const wchar_t * filename)
    {
        
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
            varying out vec2 myTexCoord;\n\
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
    
    GLFWwindow * win;
    postprogram * copy;
    renderer()
    {
        glfwSwapInterval(1);
        
        if(!glfwInit()) puts("glfw failed to init"), exit(0);
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        win = glfwCreateWindow(1111*2/3-1, 600, "Hello, World!", NULL, NULL);
        
        if(!win) puts("glfw failed to init"), exit(0);
        glfwMakeContextCurrent(win);
        
        if(gl3wInit()) puts("gl3w failed to init"), exit(0);
        
        
        //glfwSwapBuffers(win);
        glfwGetFramebufferSize(win, &w, &h);
        
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
        {
            puts(message);
        }, nullptr);
        
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        
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
        varying out vec2 myTexCoord;\n\
        void main()\n\
        {\n\
            gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0) * translation * projection;\n\
            myTexCoord = aTex;\n\
        }\n"
        ;
        
        const char * fshadersource = 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        uniform vec2 mySize;\n\
        uniform vec2 myScale;\n\
        varying vec2 myTexCoord;\n\
        #define M_PI 3.1415926435\n\
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
        vec2 basicRoundedCoord(int a, int b)\n\
        {\n\
            return vec2((floor(myTexCoord.x*(mySize.x))+a)/(mySize.x),\n\
                        (floor(myTexCoord.y*(mySize.y))+b)/(mySize.y));\n\
        }\n\
        vec4 basicRoundedPixel(int a, int b)\n\
        {\n\
            return texture2D(mytexture, basicRoundedCoord(a, b));\n\
        }\n\
        vec2 downscalingPhase()\n\
        {\n\
            return vec2(mod(myTexCoord.x*(mySize.x), 1),\n\
                        mod(myTexCoord.y*(mySize.y), 1));\n\
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
        float sinc(float x)\n\
        {\n\
            if(x == 0) return 1;\n\
            else return sin(x*M_PI)/(x*M_PI);\n\
        }\n\
        float samplewindow(float x, float diameter)\n\
        {\n\
            //if(x < -1 || x > 1) return 0;\n\
            //return cos(x*M_PI)*0.5+0.5;\n\
            //return cos(x*M_PI*3/4)*cos(x*M_PI/4);\n\
            return sinc(x) * cos(x*M_PI/diameter);\n\
        }\n\
        vec4 supersamplerow(int y, float i)\n\
        {\n\
            int radius = 3;\n\
            int low = int(ceil(-radius/myScale.x + i));\n\
            int high = int(floor(radius/myScale.x + i));\n\
            vec4 c = vec4(0);\n\
            float sampleWeight = 0;\n\
            for(int j = low; j <= high; j++)\n\
            {\n\
                float thisWeight = samplewindow((j-i)*myScale.x, radius*2);\n\
                sampleWeight += thisWeight;\n\
                c += basicRoundedPixel(j, y)*thisWeight;\n\
            }\n\
            c /= sampleWeight;\n\
            return c;\n\
        }\n\
        vec4 supersamplegrid(float ix, float i)\n\
        {\n\
            int radius = 3;\n\
            int low = int(ceil(-radius/myScale.y + i));\n\
            int high = int(floor(radius/myScale.y + i));\n\
            vec4 c = vec4(0);\n\
            float sampleWeight = 0;\n\
            for(int j = low; j <= high; j++)\n\
            {\n\
                float thisWeight = samplewindow((j-i)*myScale.y, radius*2);\n\
                sampleWeight += thisWeight;\n\
                c += supersamplerow(j, ix)*thisWeight;\n\
            }\n\
            c /= sampleWeight;\n\
            return c;\n\
        }\n\
        void main()\n\
        {\n\
            if(myScale.x < 1 || myScale.y < 1)\n\
            {\n\
                vec2 phase = downscalingPhase();\n\
                gl_FragColor =  supersamplegrid(phase.x, phase.y);\n\
            }\n\
            else\n\
            {\n\
                vec2 phase = interpolationPhase();\n\
                vec4 c = hermitegrid(phase.x, phase.y);\n\
                gl_FragColor = c;\n\
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
        
        checkerr(__LINE__);
        
        glDeleteShader(fshader);
        glDeleteShader(vshader);
        
        
        checkerr(__LINE__);
        
        // FBO program
        
        copy = new postprogram("copy", 
        "#version 330 core\n\
        uniform sampler2D mytexture;\n\
        varying vec2 myTexCoord;\n\
        void main()\n\
        {\n\
            gl_FragColor = texture2D(mytexture, myTexCoord);\n\
        }\n");
        
        glUseProgram(copy->program);
        checkerr(__LINE__);
        glUniform1i(glGetUniformLocation(copy->program, "mytexture"), 0);
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBOtexture1, 0);
        checkerr(__LINE__);
        
        glBindTexture(GL_TEXTURE_2D, FBOtexture2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, FBOtexture2, 0);
        checkerr(__LINE__);
        
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
            0.0f, -2.0f/h, 0.0f,1.0f,
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
        glBindTexture(GL_TEXTURE_2D, texture->texid);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,  GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

std::mutex scrollMutex;
float scroll = 0;

void myScrollEventCallback(GLFWwindow * win, double x, double y)
{
    scrollMutex.lock();
        scroll += y;
    scrollMutex.unlock();
}

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

// Unicode file path handling on windows is complete horseshit. Sorry. This should be relatively easy to change if you're on a reasonable OS.
int wmain (int argc, wchar_t **argv)
{
    wchar_t * arg;
    if(argc > 1) arg = argv[1];
    else return 0;
    
    float x = 0;
    float y = 0;
    
    renderer myrenderer;
    
    auto & win = myrenderer.win;
    glfwSetScrollCallback(win, myScrollEventCallback);
    glfwSetErrorCallback(error_callback);
    
    std::vector<std::wstring> mydir;
    
    int index = 0;
    
    std::wstring directory = std::wstring(arg);
    bool from_filename = false;
    if(looks_like_image_filename(directory))
    {
        directory = fs::path(arg).parent_path().wstring();
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
                if(p.path().compare(std::wstring(arg)) != 0)
                    index++;
                else
                    from_filename = false;
            }
            mydir.push_back(path);
        }
    }
    if(index == int(mydir.size())) index = 0;
    
    auto myimage = myrenderer.load_texture(mydir[index].data());
    
    if(!myimage) return 0;
    
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
        
        static int lastPgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        static int lastPgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        
        int pgUp = glfwGetKey(win, GLFW_KEY_PAGE_UP);
        int pgDn = glfwGetKey(win, GLFW_KEY_PAGE_DOWN);
        
        constexpr bool reset_position_on_new_page = true;        
        constexpr bool invert_x = true;
        
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
                y = 0;
                if(!invert_x)
                    x = 0;
                else
                {
                    float xscale = float(myrenderer.w)/float(myimage->w);
                    float yscale = float(myrenderer.h)/float(myimage->h);
                    float scale = std::max(xscale, yscale);
                    x = myimage->w*scale - myrenderer.w;
                }
            }
                
        }
        if(pgDn and !lastPgDn and index < int(mydir.size()-1))
        {
            puts("entering B");
            repeat2:
            index = std::min(index+1, int(mydir.size()-1));
            myimage = myrenderer.load_texture(mydir[index].data());
            if(!myimage and index < int(mydir.size()-1))
            {
                puts("looping B");
                goto repeat2;
            }
            else if(!myimage)
            {
                index = 0;
                myimage = myrenderer.load_texture(mydir[0].data());
            }
            if(reset_position_on_new_page)
            {
                y = 0;
                if(!invert_x)
                    x = 0;
                else
                {
                    float xscale = float(myrenderer.w)/float(myimage->w);
                    float yscale = float(myrenderer.h)/float(myimage->h);
                    float scale = std::max(xscale, yscale);
                    x = myimage->w*scale - myrenderer.w;
                }
            }
        }
        
        lastPgUp = pgUp;
        lastPgDn = pgDn;
        
        const float speed = 2000;
        const float scrollspeed = 100;
        
        float xscale = float(myrenderer.w)/float(myimage->w);
        float yscale = float(myrenderer.h)/float(myimage->h);
        float scale = std::max(xscale, yscale);
        static float lastscale = scale;
        
        float motionscale;
        if(xscale > yscale)
            motionscale = myrenderer.w/1000.0;
        else
            motionscale = myrenderer.h/1000.0;
        
        if(scale != lastscale)
        {
            x /= lastscale;
            x *= scale;
            y /= lastscale;
            y *= scale;
        }
        
        lastscale = scale;
        
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
        
        if(xscale > yscale)
        {
            if(!invert_x)
                x = 0;
            else
                x = myimage->w*scale - myrenderer.w;
            float upperlimit = 0;
            float lowerlimit = myimage->h*scale - myrenderer.h;
            if(y < upperlimit) y = upperlimit;
            if(y > lowerlimit) y = lowerlimit;
        }
        else
        {
            y = 0;
            float upperlimit = 0;
            float lowerlimit = myimage->w*scale - myrenderer.w;
            if(x < upperlimit) x = upperlimit;
            if(x > lowerlimit) x = lowerlimit;
        }
        myrenderer.draw_texture(myimage, -x, -y, 0.2, scale, scale);
        
        myrenderer.cycle_end();
        
        constexpr float throttle = 0.004;
        if(delta < throttle)
            std::this_thread::sleep_for(std::chrono::duration<float>(throttle-delta));
    }
    glfwDestroyWindow(win);
    
    return 0;
}
