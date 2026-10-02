#include <GL/glew.h>
#include "LevelScene.h"
#include "../Graphics/ResourcePak.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

bool LevelScene::load(const std::string &path)
{
    Level loaded;
    std::string text;
    bool ok;
    if (ResourcePak::IsInitialized() && ResourcePak::FileExists(path) && ResourcePak::LoadFileAsString(path, text))
        ok = loaded.loadFromString(text);
    else
        ok = loaded.loadFromFile(path);

    if (!ok)
    {
        error = path + ": " + loaded.lastError();
        std::cerr << "[LevelScene] " << error << std::endl;
        return false;
    }
    error.clear();
    setLevel(loaded);
    return true;
}

void LevelScene::setLevel(const Level &newLevel)
{
    level = newLevel;
    refreshModels();
}

void LevelScene::refreshModels()
{
    for (const LevelEntity &e : level.entities)
    {
        if (e.type != LevelEntity::Type::Mesh || e.model.empty() || models.count(e.model))
            continue;

        ModelSlot &slot = models[e.model];
        auto model = std::make_unique<Model>();
        if (!model->loadFromFile(e.model) || model->meshes.empty())
        {
            std::cerr << "[LevelScene] Cannot load model " << e.model << std::endl;
            continue; // slot vide : pas de nouvel essai à chaque image
        }

        float mn[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
        float mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (const auto &mesh : model->meshes)
        {
            for (const auto &v : mesh.verts)
            {
                const float p[3] = {v.x, v.y, v.z};
                for (int i = 0; i < 3; i++)
                {
                    mn[i] = std::min(mn[i], p[i]);
                    mx[i] = std::max(mx[i], p[i]);
                }
            }
        }
        if (mn[0] <= mx[0])
        {
            for (int i = 0; i < 3; i++)
            {
                slot.boundsMin[i] = mn[i];
                slot.boundsMax[i] = mx[i];
            }
        }
        slot.model = std::move(model);
    }
}

void LevelScene::unloadModels()
{
    models.clear();
}

const LevelScene::ModelSlot *LevelScene::slotFor(const std::string &path) const
{
    auto it = models.find(path);
    return it == models.end() ? nullptr : &it->second;
}

bool LevelScene::isModelLoaded(const std::string &path) const
{
    const ModelSlot *slot = slotFor(path);
    return slot && slot->model;
}

void LevelScene::applyLighting() const
{
    Model::SetLightPosition(level.lightPosition[0], level.lightPosition[1], level.lightPosition[2]);
    Model::SetLightTarget(level.lightTarget[0], level.lightTarget[1], level.lightTarget[2]);
    float dx = level.lightPosition[0] - level.lightTarget[0];
    float dy = level.lightPosition[1] - level.lightTarget[1];
    float dz = level.lightPosition[2] - level.lightTarget[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    Model::SetShadowArea(level.shadowArea, 0.1f, dist + level.shadowArea * 2.0f);
}

void LevelScene::draw()
{
    // Opaques d'abord, puis les entités translucides
    for (int pass = 0; pass < 2; pass++)
    {
        for (const LevelEntity &e : level.entities)
        {
            if (e.type != LevelEntity::Type::Mesh || !e.visible)
                continue;
            bool translucent = e.useColor && e.color[3] < 0.999f;
            if (translucent != (pass == 1))
                continue;

            auto it = models.find(e.model);
            if (it == models.end() || !it->second.model)
                continue;

            Model &m = *it->second.model;
            m.setPosition(e.position[0], e.position[1], e.position[2]);
            m.setRotation(e.rotation[0], e.rotation[1], e.rotation[2]);
            m.setScale(e.scale[0], e.scale[1], e.scale[2]);
            if (e.useColor)
                m.setColorRGBA(e.color[0], e.color[1], e.color[2], e.color[3]);
            else
                m.clearColorOverride();
            m.draw();
        }
    }
}

void LevelScene::render()
{
    applyLighting();

    Model::BeginShadowPass();
    draw();
    Model::EndShadowPass();

    float view[16], projection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    Model::SetFrameUniforms(view, projection);
    draw();
}

bool LevelScene::localBounds(const LevelEntity &entity, float outMin[3], float outMax[3]) const
{
    const ModelSlot *slot = entity.type == LevelEntity::Type::Mesh ? slotFor(entity.model) : nullptr;
    // Taille des entités sans modèle : silhouette de joueur, petit repère, cube unité
    float half[3] = {0.5f, 0.5f, 0.5f};
    if (entity.type == LevelEntity::Type::PlayerStart)
        half[0] = 0.4f, half[1] = 0.9f, half[2] = 0.4f;
    else if (entity.type == LevelEntity::Type::Marker)
        half[0] = half[1] = half[2] = 0.25f;
    for (int i = 0; i < 3; i++)
    {
        outMin[i] = slot && slot->model ? slot->boundsMin[i] : -half[i];
        outMax[i] = slot && slot->model ? slot->boundsMax[i] : half[i];
    }
    return slot && slot->model;
}

// Intersection rayon / boîte (méthode des plans), t d'entrée dans [0, +inf[
static bool rayBox(const glm::vec3 &o, const glm::vec3 &d, const float mn[3], const float mx[3], float &tHit)
{
    float t0 = 0.0f, t1 = FLT_MAX;
    for (int i = 0; i < 3; i++)
    {
        if (std::fabs(d[i]) < 1e-9f)
        {
            if (o[i] < mn[i] || o[i] > mx[i])
                return false;
            continue;
        }
        float inv = 1.0f / d[i];
        float a = (mn[i] - o[i]) * inv, b = (mx[i] - o[i]) * inv;
        if (a > b)
            std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1)
            return false;
    }
    tHit = t0;
    return true;
}

// Möller-Trumbore, faces des deux côtés
static bool rayTriangle(const glm::vec3 &o, const glm::vec3 &d, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, float &t)
{
    glm::vec3 e1 = b - a, e2 = c - a;
    glm::vec3 p = glm::cross(d, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 1e-12f)
        return false;
    float inv = 1.0f / det;
    glm::vec3 s = o - a;
    float u = glm::dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f)
        return false;
    glm::vec3 q = glm::cross(s, e1);
    float v = glm::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f)
        return false;
    t = glm::dot(e2, q) * inv;
    return t >= 0.0f;
}

int LevelScene::raycast(const float origin[3], const float dir[3], float &outDistance, int ignoreEntity, bool includeHelpers) const
{
    int best = -1;
    float bestT = FLT_MAX;
    glm::vec3 wo(origin[0], origin[1], origin[2]);
    glm::vec3 wd(dir[0], dir[1], dir[2]);

    for (int i = 0; i < (int)level.entities.size(); i++)
    {
        const LevelEntity &e = level.entities[i];
        if (i == ignoreEntity || !e.visible)
            continue;
        if (e.type != LevelEntity::Type::Mesh && !includeHelpers)
            continue;

        float mat[16];
        entityMatrix(e, mat);
        glm::mat4 inv = glm::inverse(glm::make_mat4(mat));
        // Le paramètre t est identique en local et en monde (transformation affine)
        glm::vec3 lo = glm::vec3(inv * glm::vec4(wo, 1.0f));
        glm::vec3 ld = glm::vec3(inv * glm::vec4(wd, 0.0f));

        float mn[3], mx[3], t;
        bool hasModel = localBounds(e, mn, mx);
        if (!rayBox(lo, ld, mn, mx, t) || t >= bestT)
            continue;

        if (hasModel)
        {
            const Model &m = *slotFor(e.model)->model;
            float nearest = FLT_MAX;
            for (const auto &mesh : m.meshes)
            {
                for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3)
                {
                    const SimpleVertex &a = mesh.verts[mesh.indices[k]];
                    const SimpleVertex &b = mesh.verts[mesh.indices[k + 1]];
                    const SimpleVertex &c = mesh.verts[mesh.indices[k + 2]];
                    float tt;
                    if (rayTriangle(lo, ld, glm::vec3(a.x, a.y, a.z), glm::vec3(b.x, b.y, b.z), glm::vec3(c.x, c.y, c.z), tt) && tt < nearest)
                        nearest = tt;
                }
            }
            if (nearest == FLT_MAX)
                continue;
            t = nearest;
        }

        if (t < bestT)
        {
            bestT = t;
            best = i;
        }
    }
    if (best >= 0)
        outDistance = bestT;
    return best;
}

void LevelScene::entityMatrix(const LevelEntity &e, float out[16])
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, glm::vec3(e.position[0], e.position[1], e.position[2]));
    m = glm::rotate(m, glm::radians(e.rotation[0]), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(e.rotation[1]), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(e.rotation[2]), glm::vec3(0, 0, 1));
    m = glm::scale(m, glm::vec3(e.scale[0], e.scale[1], e.scale[2]));
    const float *p = glm::value_ptr(m);
    std::copy(p, p + 16, out);
}

AABB LevelScene::worldBounds(const LevelEntity &entity) const
{
    float mn[3], mx[3], mat[16];
    localBounds(entity, mn, mx);
    entityMatrix(entity, mat);
    glm::mat4 m = glm::make_mat4(mat);

    AABB box{FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX};
    for (int c = 0; c < 8; c++)
    {
        glm::vec4 p(c & 1 ? mx[0] : mn[0], c & 2 ? mx[1] : mn[1], c & 4 ? mx[2] : mn[2], 1.0f);
        glm::vec4 w = m * p;
        box.minX = std::min(box.minX, w.x);
        box.maxX = std::max(box.maxX, w.x);
        box.minY = std::min(box.minY, w.y);
        box.maxY = std::max(box.maxY, w.y);
        box.minZ = std::min(box.minZ, w.z);
        box.maxZ = std::max(box.maxZ, w.z);
    }
    return box;
}

std::vector<AABB> LevelScene::collisionBoxes() const
{
    std::vector<AABB> boxes;
    for (const LevelEntity &e : level.entities)
    {
        if (e.collision)
            boxes.push_back(worldBounds(e));
    }
    return boxes;
}
