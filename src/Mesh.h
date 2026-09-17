#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;

    Mesh() = default;

    void Upload(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices) {
        indexCount = (GLsizei)indices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

        glBindVertexArray(0);
    }

    void Draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// ---------- Generatory prymitywow ----------

inline Mesh MakeSphere(float radius, int sectors = 32, int stacks = 24) {
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = glm::pi<float>() / 2 - i * (glm::pi<float>() / stacks); // od +pi/2 do -pi/2
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * glm::pi<float>() / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            Vertex v;
            v.pos = glm::vec3(x, z, y); // Y w gore
            v.normal = glm::normalize(glm::vec3(x, z, y));
            v.uv = glm::vec2((float)j / sectors, (float)i / stacks);
            verts.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                idx.push_back(k1); idx.push_back(k2); idx.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                idx.push_back(k1 + 1); idx.push_back(k2); idx.push_back(k2 + 1);
            }
        }
    }

    Mesh m;
    m.Upload(verts, idx);
    return m;
}

// Walec/stozek uogolniony - dwa promienie (dolny/gorny), dla stozka topRadius = 0
inline Mesh MakeCylinderCone(float bottomRadius, float topRadius, float height, int segments = 24) {
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;

    float halfH = height * 0.5f;
    float slantNormalY = (bottomRadius - topRadius) / height;

    // boczna powierzchnia
    for (int i = 0; i <= segments; ++i) {
        float angle = i * (2 * glm::pi<float>() / segments);
        float c = cosf(angle), s = sinf(angle);

        glm::vec3 nBottom = glm::normalize(glm::vec3(c, slantNormalY, s));

        Vertex vb; vb.pos = glm::vec3(c * bottomRadius, -halfH, s * bottomRadius);
        vb.normal = nBottom; vb.uv = glm::vec2((float)i / segments, 0.0f);
        verts.push_back(vb);

        Vertex vt; vt.pos = glm::vec3(c * topRadius, halfH, s * topRadius);
        vt.normal = nBottom; vt.uv = glm::vec2((float)i / segments, 1.0f);
        verts.push_back(vt);
    }
    for (int i = 0; i < segments; ++i) {
        int base = i * 2;
        idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 2); idx.push_back(base + 1); idx.push_back(base + 3);
    }

    // pokrywy (proste, plaskie normalne) - dolna i gorna
    unsigned int centerBottomIdx = (unsigned int)verts.size();
    Vertex cb; cb.pos = glm::vec3(0, -halfH, 0); cb.normal = glm::vec3(0, -1, 0); cb.uv = glm::vec2(0.5f, 0.5f);
    verts.push_back(cb);
    for (int i = 0; i <= segments; ++i) {
        float angle = i * (2 * glm::pi<float>() / segments);
        Vertex v; v.pos = glm::vec3(cosf(angle) * bottomRadius, -halfH, sinf(angle) * bottomRadius);
        v.normal = glm::vec3(0, -1, 0); v.uv = glm::vec2(cosf(angle) * 0.5f + 0.5f, sinf(angle) * 0.5f + 0.5f);
        verts.push_back(v);
    }
    for (int i = 0; i < segments; ++i) {
        idx.push_back(centerBottomIdx);
        idx.push_back(centerBottomIdx + 1 + i + 1);
        idx.push_back(centerBottomIdx + 1 + i);
    }

    if (topRadius > 0.0001f) {
        unsigned int centerTopIdx = (unsigned int)verts.size();
        Vertex ct; ct.pos = glm::vec3(0, halfH, 0); ct.normal = glm::vec3(0, 1, 0); ct.uv = glm::vec2(0.5f, 0.5f);
        verts.push_back(ct);
        for (int i = 0; i <= segments; ++i) {
            float angle = i * (2 * glm::pi<float>() / segments);
            Vertex v; v.pos = glm::vec3(cosf(angle) * topRadius, halfH, sinf(angle) * topRadius);
            v.normal = glm::vec3(0, 1, 0); v.uv = glm::vec2(cosf(angle) * 0.5f + 0.5f, sinf(angle) * 0.5f + 0.5f);
            verts.push_back(v);
        }
        for (int i = 0; i < segments; ++i) {
            idx.push_back(centerTopIdx);
            idx.push_back(centerTopIdx + 1 + i);
            idx.push_back(centerTopIdx + 1 + i + 1);
        }
    }

    Mesh m;
    m.Upload(verts, idx);
    return m;
}

inline Mesh MakePlane(float size, float uvTiling = 4.0f) {
    float h = size * 0.5f;
    std::vector<Vertex> verts = {
        {{-h, 0,  h}, {0,1,0}, {0, 0}},
        {{ h, 0,  h}, {0,1,0}, {uvTiling, 0}},
        {{ h, 0, -h}, {0,1,0}, {uvTiling, uvTiling}},
        {{-h, 0, -h}, {0,1,0}, {0, uvTiling}},
    };
    std::vector<unsigned int> idx = { 0,1,2, 0,2,3 };
    Mesh m;
    m.Upload(verts, idx);
    return m;
}
