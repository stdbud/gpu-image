#pragma once

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "budUtils.hpp"
#include "budImage.hpp"

namespace bud {

namespace gl {

class ImageGL final : public Imagef {
public:
    explicit ImageGL(const int width, const int height, const int nrChannels)
        : Imagef(width, height, nrChannels),
          m_program(0),
          m_pipeline(0),
          m_srcTexture(0),
          m_dstTexture(0) {}

    void compute() override
    {
        loadGL();
        createPipeline();
        createImageTextures();
        dispatch();
        checkAnswer();
        cleanup();
    }
private:
    static void loadGL()
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(800, 600, "bud", nullptr, nullptr);
        if (!window) checkErrorCode<bool, true>(false, "failed to create window!");
        glfwMakeContextCurrent(window);

        bool loaded = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        checkErrorCode<bool, true>(loaded, "failed to load gl!");
    }

    void createPipeline()
    {
        const std::string shaderSource = readCodeFromFile("image.comp");
        const char* source = shaderSource.c_str();
        m_program = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, &source);

        glGenProgramPipelines(1, &m_pipeline);
        glUseProgramStages(m_pipeline, GL_COMPUTE_SHADER_BIT, m_program);
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to create pipeline!");

        // another usage
        // GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        // glShaderSource(shader, 1, &source, nullptr);
        // glCompileShader(shader);
        // GLint success;
        // glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        // checkErrorCode<GLint, GL_TRUE>(success, "failed to compile shader!");

        // m_program = glCreateProgram();
        // glAttachShader(m_program, shader);
        // glLinkProgram(m_program);
        // glGetProgramiv(m_program, GL_LINK_STATUS, &success);
        // checkErrorCode<GLint, GL_TRUE>(success, "failed to link program!");

        // glDeleteShader(shader);
    }

    void createImageTextures()
    {
        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glGenTextures(1, &m_srcTexture);
        glBindTexture(GL_TEXTURE_2D, m_srcTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_width, m_height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, m_data.data());
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to create image texture!");

        glGenTextures(1, &m_dstTexture);
        glBindTexture(GL_TEXTURE_2D, m_dstTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_width, m_height);
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to create image texture!");

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void dispatch()
    {
        // glUseProgram(m_program);
        glBindProgramPipeline(m_pipeline);

        glBindImageTexture(0, m_srcTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(1, m_dstTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to bind image t exture!");

        glDispatchCompute(m_width, m_height, 1);
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to dispatch compute!");

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glFinish();
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to finish!");
    }

    void checkAnswer()
    {
        std::vector<float> got(m_data.size());
        glBindTexture(GL_TEXTURE_2D, m_dstTexture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, got.data());
        checkErrorCode<GLenum, GL_NO_ERROR>(glGetError(), "failed to read pixels!");

        bool valid = validateImageData(got);
        checkErrorCode<bool, true>(valid, "failed to validate image data!");

        std::cout << "OpenGL pass!" << std::endl;
    }

    void cleanup()
    {
        glDeleteTextures(1, &m_srcTexture);
        glDeleteTextures(1, &m_dstTexture);
        glDeleteProgramPipelines(1, &m_pipeline);
        glDeleteProgram(m_program);
        glfwTerminate();
    }

    GLuint m_program;
    GLuint m_pipeline;
    GLuint m_srcTexture;
    GLuint m_dstTexture;
};

}

}
