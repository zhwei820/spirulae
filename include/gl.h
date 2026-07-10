#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"

#include <stdio.h>
#include <string>
#include <vector>


// compile shaders into a program
GLuint createShaderProgram(const char* vs_source, const char* fs_source) {
    // Create the shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    GLint Result = GL_FALSE;
    int InfoLogLength;
    std::string errorMessage;

    bool success = true;

    // Vertex Shader
    glShaderSource(VertexShaderID, 1, &vs_source, NULL);
    glCompileShader(VertexShaderID);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        errorMessage.resize(InfoLogLength + 1);
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &errorMessage[0]);
        printf("Vertex shader compile error.\n%s\n", &errorMessage[0]);
        success = false;
    }

    // Fragment Shader
    glShaderSource(FragmentShaderID, 1, &fs_source, NULL);
    glCompileShader(FragmentShaderID);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        errorMessage.resize(InfoLogLength + 1);
        glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &errorMessage[0]);
        printf("Fragment shader compile error.\n%s\n", &errorMessage[0]);
        success = false;
    }

    // Link the program
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 1) {
        errorMessage.resize(InfoLogLength + 1);
        glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &errorMessage[0]);
        printf("Program linking error.\n%s\n", &errorMessage[0]);
        success = false;
    }

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
    if (!success) {
        glDeleteProgram(ProgramID);
        return -1;
    }
    return ProgramID;
}



class GlBatchEvaluator2 {
    std::string vsSource, fsSource;
    GLuint shaderProgram;
    GLuint framebuffer, texture;
    int textureW, textureH;

public:
    GlBatchEvaluator2(std::string funRaw);
    ~GlBatchEvaluator2();
    void evaluateFunction(size_t pn, const glm::vec2 *points, float *v);
};

GlBatchEvaluator2::GlBatchEvaluator2(std::string funRaw) {
    vsSource = R"(#version 300 es
        precision highp float;
        in vec4 aPosition;
        out vec2 vXy;
        void main() {
            gl_Position = vec4(aPosition.xy, 0.0, 1.0);
            gl_PointSize = 1.0;
            vXy = aPosition.zw;
        }
    )";
    fsSource = R"(#version 300 es
        precision highp float;
        in vec2 vXy;
        out vec4 fragColor;
        )" + funRaw + R"(

        void main() {
            // vec2 xy = texelFetch(coords, ivec2(gl_FragCoord.xy), 0).xy;
            // vec2 xy = gl_FragCoord.xy;
            float result = funRaw(vXy.x, vXy.y);
            fragColor = vec4(result, 0, 0, 1);
        }
    )";
    shaderProgram = createShaderProgram(&vsSource[0], &fsSource[0]);

    textureW = 256;
    textureH = 256;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureW, textureH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Failed to create framebuffer.\n");
        throw "Failed to create framebuffer.";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);
}

GlBatchEvaluator2::~GlBatchEvaluator2() {
    glDeleteShader(shaderProgram);
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}


void GlBatchEvaluator2::evaluateFunction(
    size_t pn, const glm::vec2 *points, float *v) {

    if (shaderProgram == -1) {
        for (int i = 0; i < pn; i++)
            v[i] = 0.0f;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureW, textureH);
    glUseProgram(shaderProgram);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    GLuint vbo;
    glGenBuffers(1, &vbo);

    size_t batch_size = textureW * textureH;
    for (size_t batchi = 0; batchi < pn; batchi += batch_size) {
        size_t batchn = std::min(batch_size, pn - batchi);
    
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        std::vector<glm::vec4> coords(pn);
        for (int i = 0; i < batchn; i++) {
            float x = i % textureW, y = i / textureW;
            coords[i] = glm::vec4(
                (glm::vec2(x, y) + 0.5f) / glm::vec2(textureW, textureH) * 2.0f - 1.0f,
                points[batchi+i]);
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * coords.size(), coords.data(), GL_STATIC_DRAW);

        glUseProgram(shaderProgram);
        GLint posAttrib = glGetAttribLocation(shaderProgram, "aPosition");
        glVertexAttribPointer(posAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posAttrib);

        glDrawArrays(GL_POINTS, 0, batchn);

        std::vector<glm::vec4> pixels(batch_size);
        glReadPixels(0, 0, textureW, textureH, GL_RGBA, GL_FLOAT, pixels.data());
        for (int i = 0; i < batchn; i++)
            v[batchi+i] = pixels[i].x;
        // break;
    }

    glDeleteBuffers(1, &vbo);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);

}




class GlBatchEvaluator3 {
    std::string vsSource, fsSource;
    GLuint shaderProgram;
    GLuint framebuffer, texture;
    int textureW, textureH;
    bool rgba;

public:
    GlBatchEvaluator3(std::string funRaw, bool rgba);
    ~GlBatchEvaluator3();
    void evaluateFunction(size_t pn, const glm::vec3 *points, float *v);
};

GlBatchEvaluator3::GlBatchEvaluator3(std::string funRaw, bool rgba) {
    this->rgba = rgba;
    vsSource = R"(#version 300 es
        precision highp float;
        in vec2 aPosition;
        in vec3 aXyz;
        out vec3 vXyz;
        void main() {
            gl_Position = vec4(aPosition, 0.0, 1.0);
            gl_PointSize = 1.0;
            vXyz = aXyz;
        }
    )";
    const std::string fragColor[2] = {
        "vec4(funRaw(vXyz.x, vXyz.y, vXyz.z), 0, 0, 1)",
        "funColor(vXyz)"
    };
    fsSource = R"(#version 300 es
        precision highp float;
        in vec3 vXyz;
        out vec4 fragColor;
        )" + funRaw + R"(

        void main() {
            fragColor = )" + fragColor[rgba] + R"(;
        }
    )";
    // printf("%s\n", fsSource.c_str());
    shaderProgram = createShaderProgram(&vsSource[0], &fsSource[0]);

    textureW = 256;
    textureH = 256;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureW, textureH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Failed to create framebuffer.\n");
        throw "Failed to create framebuffer.";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);
}

GlBatchEvaluator3::~GlBatchEvaluator3() {
    glDeleteShader(shaderProgram);
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}


void GlBatchEvaluator3::evaluateFunction(
    size_t pn, const glm::vec3 *points, float *v) {

    if (shaderProgram == -1) {
        printf("Warning: no shader program linked\n");
        for (size_t i = 0; i < pn; i++)
            v[i] = 0.0f;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureW, textureH);
    glUseProgram(shaderProgram);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    GLuint vxyz;
    glGenBuffers(1, &vxyz);

    size_t batch_size = textureW * textureH;
    for (size_t batchi = 0; batchi < pn; batchi += batch_size) {
        size_t batchn = std::min(batch_size, pn - batchi);
    
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        std::vector<glm::vec2> coords(batchn);
        for (int i = 0; i < batchn; i++) {
            float x = i % textureW, y = i / textureW;
            coords[i] = (glm::vec2(x, y) + 0.5f) / glm::vec2(textureW, textureH) * 2.0f - 1.0f;
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * batchn, coords.data(), GL_STATIC_DRAW);
        GLint posAttrib = glGetAttribLocation(shaderProgram, "aPosition");
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posAttrib);

        glBindBuffer(GL_ARRAY_BUFFER, vxyz);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * batchn, &points[batchi], GL_STATIC_DRAW);
        GLint xyzAttrib = glGetAttribLocation(shaderProgram, "aXyz");
        glVertexAttribPointer(xyzAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(xyzAttrib);

        glDrawArrays(GL_POINTS, 0, batchn);

        std::vector<glm::vec4> pixels(batch_size);
        glReadPixels(0, 0, textureW, textureH, GL_RGBA, GL_FLOAT, pixels.data());
        if (this->rgba) {
            for (int i = 0; i < 4*batchn; i++)
                v[4*batchi+i] = ((float*)&pixels[0])[i];
        }
        else {
            for (int i = 0; i < batchn; i++)
                v[batchi+i] = pixels[i].x;
        }
    }

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vxyz);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);

}




class GlIntersectionEvaluator3 {
    std::string vsSource, fsSource;
    GLuint shaderProgram;
    GLuint framebuffer, texture;
    int textureW, textureH;

public:
    GlIntersectionEvaluator3(std::string funGlsl);
    ~GlIntersectionEvaluator3();
    void evaluateIntersections(size_t pn,
        const glm::vec3 *p0, const glm::vec3 *p1, glm::vec3 *p);
};

// funGlsl must define `float fun(vec3 p)`;
// finds the zero crossing on each segment (p0, p1), assuming fun(p0) <= 0 < fun(p1)
GlIntersectionEvaluator3::GlIntersectionEvaluator3(std::string funGlsl) {
    vsSource = R"(#version 300 es
        precision highp float;
        in vec2 aPosition;
        in vec3 aP0;
        in vec3 aP1;
        out vec3 vP0;
        out vec3 vP1;
        void main() {
            gl_Position = vec4(aPosition, 0.0, 1.0);
            gl_PointSize = 1.0;
            vP0 = aP0, vP1 = aP1;
        }
    )";
    fsSource = R"(#version 300 es
        precision highp float;
        uniform float ZERO;  // used in loops to reduce compilation time
        in vec3 vP0;
        in vec3 vP1;
        out vec4 fragColor;
        )" + funGlsl + R"(

        void main() {
            float t0 = 0.0, t1 = 1.0;
            for (int i = int(ZERO); i < 16; i++) {
                float tc = 0.5 * (t0 + t1);
                if (fun(mix(vP0, vP1, tc)) <= 0.0) t0 = tc;
                else t1 = tc;
            }
            float t = clamp(0.5 * (t0 + t1), 0.01, 0.99);
            fragColor = vec4(mix(vP0, vP1, t), 1.0);
        }
    )";
    // printf("%s\n", fsSource.c_str());
    shaderProgram = createShaderProgram(&vsSource[0], &fsSource[0]);

    textureW = 256;
    textureH = 256;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureW, textureH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Failed to create framebuffer.\n");
        throw "Failed to create framebuffer.";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);
}

GlIntersectionEvaluator3::~GlIntersectionEvaluator3() {
    glDeleteShader(shaderProgram);
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}


void GlIntersectionEvaluator3::evaluateIntersections(
    size_t pn, const glm::vec3 *p0, const glm::vec3 *p1, glm::vec3 *p) {

    if (shaderProgram == -1) {
        printf("Warning: no shader program linked\n");
        for (size_t i = 0; i < pn; i++)
            p[i] = 0.5f * (p0[i] + p1[i]);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureW, textureH);
    glUseProgram(shaderProgram);
    glUniform1f(glGetUniformLocation(shaderProgram, "ZERO"), 0.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    GLuint vp0, vp1;
    glGenBuffers(1, &vp0);
    glGenBuffers(1, &vp1);

    size_t batch_size = textureW * textureH;
    for (size_t batchi = 0; batchi < pn; batchi += batch_size) {
        size_t batchn = std::min(batch_size, pn - batchi);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        std::vector<glm::vec2> coords(batchn);
        for (int i = 0; i < batchn; i++) {
            float x = i % textureW, y = i / textureW;
            coords[i] = (glm::vec2(x, y) + 0.5f) / glm::vec2(textureW, textureH) * 2.0f - 1.0f;
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * batchn, coords.data(), GL_STATIC_DRAW);
        GLint posAttrib = glGetAttribLocation(shaderProgram, "aPosition");
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posAttrib);

        glBindBuffer(GL_ARRAY_BUFFER, vp0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * batchn, &p0[batchi], GL_STATIC_DRAW);
        GLint p0Attrib = glGetAttribLocation(shaderProgram, "aP0");
        glVertexAttribPointer(p0Attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(p0Attrib);

        glBindBuffer(GL_ARRAY_BUFFER, vp1);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * batchn, &p1[batchi], GL_STATIC_DRAW);
        GLint p1Attrib = glGetAttribLocation(shaderProgram, "aP1");
        glVertexAttribPointer(p1Attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(p1Attrib);

        glDrawArrays(GL_POINTS, 0, batchn);

        std::vector<glm::vec4> pixels(batch_size);
        glReadPixels(0, 0, textureW, textureH, GL_RGBA, GL_FLOAT, pixels.data());
        for (int i = 0; i < batchn; i++)
            p[batchi+i] = glm::vec3(pixels[i]);
    }

    glDisableVertexAttribArray(glGetAttribLocation(shaderProgram, "aPosition"));
    glDisableVertexAttribArray(glGetAttribLocation(shaderProgram, "aP0"));
    glDisableVertexAttribArray(glGetAttribLocation(shaderProgram, "aP1"));
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vp0);
    glDeleteBuffers(1, &vp1);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)NULL);

}
