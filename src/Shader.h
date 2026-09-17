#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
public:
    GLuint ID = 0;

    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vSrc = ReadFile(vertexPath);
        std::string fSrc = ReadFile(fragmentPath);

        GLuint vertex = Compile(GL_VERTEX_SHADER, vSrc.c_str(), vertexPath);
        GLuint fragment = Compile(GL_FRAGMENT_SHADER, fSrc.c_str(), fragmentPath);

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        CheckLinkErrors();

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void Use() const { glUseProgram(ID); }

    void SetMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void SetMat3(const std::string& name, const glm::mat3& mat) const {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void SetVec3(const std::string& name, const glm::vec3& v) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(v));
    }
    void SetFloat(const std::string& name, float v) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), v);
    }
    void SetInt(const std::string& name, int v) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), v);
    }
    void SetBool(const std::string& name, bool v) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)v);
    }

private:
    static std::string ReadFile(const char* path) {
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        std::stringstream ss;
        try {
            file.open(path);
            ss << file.rdbuf();
            file.close();
        } catch (std::ifstream::failure& ex) {
            std::cerr << "BLAD: nie mozna wczytac pliku shadera: " << path << " (" << ex.what() << ")\n";
        }
        return ss.str();
    }

    static GLuint Compile(GLenum type, const char* src, const char* path) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, info);
            std::cerr << "BLAD kompilacji shadera (" << path << "):\n" << info << std::endl;
        }
        return shader;
    }

    void CheckLinkErrors() const {
        GLint success;
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            char info[1024];
            glGetProgramInfoLog(ID, 1024, nullptr, info);
            std::cerr << "BLAD linkowania programu shaderow:\n" << info << std::endl;
        }
    }
};
