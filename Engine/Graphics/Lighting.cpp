#include "Lighting.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
    struct Bounds
    {
        float c[3], h[3]; // local box: center, half size
    };

    // A shadow caster of this frame: its instance, and its bounding sphere in the world
    struct Caster
    {
        Model *model;
        Model::Instance inst;
        float c[3], r;
        bool settled; // usually stays put (see AddCasters)
    };

    // One cube shadow map
    struct Slot
    {
        bool used = false, cached = false;
        int dirty = 63; // faces of the map read by the shader that differ from the cache (moving casters were drawn there)
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float radius = 0.0f, clear = 0.0f;
        int seen = 0; // last frame its lamp was chosen
    };

    struct Candidate
    {
        int light; // index in g_sent
    };

    Lighting::Settings g_settings;
    float g_eye[3] = {0.0f, 0.0f, 0.0f};
    std::vector<Lighting::Light> g_lights, g_sent;
    std::vector<float> g_lightDist;
    std::vector<Caster> g_casters;
    std::unordered_map<const Model *, Bounds> g_bounds;
    Slot g_slots[Model::MAX_SHADOW_LAMPS];
    int g_frame = 0, g_lastRes = 0;
    bool g_mapsOk = true;
    bool g_staticDirty = true;
    // Settled casters that have not moved for SETTLE frames are drawn into the caches with the static scene
    const int SETTLE = 8;
    std::unordered_map<unsigned long long, int> g_settleAge;     // caster -> frames without moving
    std::unordered_map<unsigned long long, Caster> g_settled;    // what the caches contain
    std::vector<Caster> g_settledList;

    // Lamps with a map, face resolution, filter, static maps redrawn per frame at most, by quality (0 to 4)
    void tier(int q, int &lamps, int &res, int &filter, int &budget)
    {
        static const int L[5] = {0, 4, 8, 12, 16}, R[5] = {0, 256, 512, 512, 768}, F[5] = {0, 0, 1, 2, 3}, B[5] = {0, 2, 2, 3, 4};
        q = std::max(0, std::min(4, q));
        lamps = L[q], res = R[q], filter = F[q], budget = B[q];
    }

    const Bounds &boundsOf(const Model &m)
    {
        auto it = g_bounds.find(&m);
        if (it != g_bounds.end())
            return it->second;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const Model::Mesh &mesh : m.meshes)
            for (const SimpleVertex &v : mesh.verts)
            {
                const float p[3] = {v.x, v.y, v.z};
                for (int k = 0; k < 3; ++k)
                    lo[k] = std::min(lo[k], p[k]), hi[k] = std::max(hi[k], p[k]);
            }
        Bounds b;
        for (int k = 0; k < 3; ++k)
        {
            const bool any = lo[k] <= hi[k];
            b.c[k] = any ? 0.5f * (lo[k] + hi[k]) : 0.0f;
            b.h[k] = any ? 0.5f * (hi[k] - lo[k]) : 0.5f; // nothing loaded yet: a unit cube
        }
        return g_bounds[&m] = b;
    }

    // Bounding sphere of a transformed local box (the matrix columns are taken as orthogonal: rotation and scale)
    void worldSphere(const Bounds &b, const float *m, float c[3], float &r)
    {
        float r2 = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            c[k] = m[12 + k] + m[k] * b.c[0] + m[4 + k] * b.c[1] + m[8 + k] * b.c[2];
            const float *col = m + 4 * k;
            r2 += b.h[k] * b.h[k] * (col[0] * col[0] + col[1] * col[1] + col[2] * col[2]);
        }
        r = std::sqrt(r2);
    }

    unsigned long long casterKey(const Caster &c)
    {
        unsigned long long h = 1469598103934665603ULL ^ (unsigned long long)(size_t)c.model;
        for (int k = 0; k < 16; ++k)
        {
            const long long q = std::llround(c.inst.model[k] * 2000.0f);
            h = (h ^ (unsigned long long)q) * 1099511628211ULL;
        }
        return h;
    }

    bool inReach(const Slot &s, const float c[3], float r)
    {
        const float dx = c[0] - s.pos[0], dy = c[1] - s.pos[1], dz = c[2] - s.pos[2], reach = s.radius + r;
        return dx * dx + dy * dy + dz * dz < reach * reach;
    }

    // Draws the casters of the list seen by a face, one instanced draw per model
    void drawCasters(const std::vector<const Caster *> &list, const Lighting::ShadowView &view)
    {
        std::vector<Model::Instance> batch;
        Model *current = nullptr;
        auto flush = [&]()
        {
            if (current && !batch.empty())
                current->drawInstanced(batch.data(), (int)batch.size());
            batch.clear();
        };
        for (const Caster *c : list)
        {
            if (!Lighting::FaceSees(view, c->c, c->r))
                continue;
            if (c->model != current)
            {
                flush();
                current = c->model;
            }
            batch.push_back(c->inst);
        }
        flush();
    }

    // Settled casters still for SETTLE frames go to g_settled; the others are returned. A change of g_settled
    // redraws the cached maps it reaches (a door that opens, a plank torn off).
    std::vector<const Caster *> sortCasters()
    {
        std::unordered_map<unsigned long long, int> age;
        std::unordered_map<unsigned long long, Caster> settled;
        std::vector<const Caster *> moving;
        for (const Caster &c : g_casters)
        {
            if (!c.settled)
            {
                moving.push_back(&c);
                continue;
            }
            const unsigned long long key = casterKey(c);
            auto it = g_settleAge.find(key);
            const int a = std::min(SETTLE, (it == g_settleAge.end() ? 0 : it->second) + 1);
            age[key] = a;
            if (a >= SETTLE)
                settled[key] = c;
            else
                moving.push_back(&c);
        }
        g_settleAge.swap(age);
        std::vector<const Caster *> changed;
        for (const auto &kv : settled)
            if (!g_settled.count(kv.first))
                changed.push_back(&kv.second);
        for (const auto &kv : g_settled)
            if (!settled.count(kv.first))
                changed.push_back(&kv.second);
        for (Slot &s : g_slots)
        {
            if (!s.cached)
                continue;
            for (const Caster *c : changed)
                if (inReach(s, c->c, c->r))
                {
                    s.cached = false, s.dirty = 63;
                    break;
                }
        }
        if (!changed.empty())
        {
            g_settled.swap(settled);
            g_settledList.clear();
            for (const auto &kv : g_settled)
                g_settledList.push_back(kv.second);
        }
        return moving;
    }
}

namespace Lighting
{
    void Configure(const Settings &settings)
    {
        g_settings = settings;
        Model::SetAmbientOcclusion(settings.ambientOcclusion, settings.aoRadius, settings.aoDirect);
        Model::SetBounceLight(settings.bounce);
    }

    const Settings &GetSettings()
    {
        return g_settings;
    }

    bool FaceSees(const ShadowView &view, const float center[3], float radius)
    {
        float f[3], u[3], t;
        Model::PointShadowFaceAxes(view.face, f, u, t);
        const float r[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
        const float v[3] = {center[0] - view.pos[0], center[1] - view.pos[1], center[2] - view.pos[2]};
        const float reach = view.radius + radius;
        if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] > reach * reach)
            return false;
        const float fz = v[0] * f[0] + v[1] * f[1] + v[2] * f[2];
        const float fx = v[0] * r[0] + v[1] * r[1] + v[2] * r[2], fy = v[0] * u[0] + v[1] * u[1] + v[2] * u[2];
        const float slack = radius * std::sqrt(1.0f + t * t) + 0.05f;
        return fz + radius >= 0.0f && std::fabs(fx) <= fz * t + slack && std::fabs(fy) <= fz * t + slack;
    }

    void BeginFrame(const float eye[3])
    {
        for (int k = 0; k < 3; ++k)
            g_eye[k] = eye[k];
        g_lights.clear();
        g_lightDist.clear();
        g_casters.clear();
    }

    void AddLight(const Light &light)
    {
        const float dx = light.pos[0] - g_eye[0], dy = light.pos[1] - g_eye[1], dz = light.pos[2] - g_eye[2];
        g_lights.push_back(light);
        g_lightDist.push_back(dx * dx + dz * dz + g_settings.verticalWeight * dy * dy);
    }

    void AddCasters(Model &model, const Model::Instance *instances, int count, bool settled)
    {
        const Bounds &b = boundsOf(model);
        for (int i = 0; i < count; ++i)
        {
            Caster c;
            c.model = &model;
            c.inst = instances[i];
            worldSphere(b, c.inst.model, c.c, c.r);
            c.settled = settled;
            g_casters.push_back(c);
        }
    }

    void InvalidateStatic()
    {
        g_staticDirty = true;
    }

    const std::vector<Light> &Lights()
    {
        return g_sent;
    }

    void EndFrame(const StaticDraw &drawStatic)
    {
        ++g_frame;
        // Closest lights first; past the limit, the farthest kept ones fade out instead of popping
        std::vector<int> order(g_lights.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = (int)i;
        std::sort(order.begin(), order.end(), [](int a, int b) { return g_lightDist[a] < g_lightDist[b]; });
        g_sent.clear();
        const int kept = std::min((int)order.size(), Model::MAX_POINT_LIGHTS);
        const float cut = (int)order.size() > kept ? std::sqrt(g_lightDist[order[kept]]) : -1.0f;
        for (int i = 0; i < kept; ++i)
        {
            Light l = g_lights[order[i]];
            if (cut > 0.0f)
            {
                const float k = std::max(0.0f, std::min(1.0f, (cut - std::sqrt(g_lightDist[order[i]])) / std::max(1.0f, cut * 0.35f)));
                for (float &c : l.color)
                    c *= k;
            }
            g_sent.push_back(l);
        }
        std::vector<float> pos, col, dir;
        for (const Light &l : g_sent)
        {
            pos.insert(pos.end(), {l.pos[0], l.pos[1], l.pos[2], l.radius});
            col.insert(col.end(), {l.color[0], l.color[1], l.color[2]});
            dir.insert(dir.end(), {l.dir[0], l.dir[1], l.dir[2], l.cone});
        }
        Model::SetPointLights((int)g_sent.size(), pos.data(), col.data(), dir.data());

        int lamps, res, filter, budget;
        tier(g_settings.shadowQuality, lamps, res, filter, budget);
        if (!drawStatic || !g_mapsOk || lamps == 0)
        {
            Model::EnablePointShadowMaps(false);
            if (lamps == 0)
                Model::SetupPointShadowMaps(0, 0);
            return;
        }
        if (Model::PointShadowLamps() != lamps || res != g_lastRes)
        {
            for (Slot &s : g_slots)
                s = Slot();
        }
        if (!Model::SetupPointShadowMaps(lamps, res))
        {
            g_mapsOk = false; // graphics card too old: the grid alone
            Model::EnablePointShadowMaps(false);
            return;
        }
        g_lastRes = res;
        if (g_staticDirty)
        {
            g_staticDirty = false;
            for (Slot &s : g_slots)
                s.cached = false, s.dirty = 63;
        }

        // Candidates: bright shadowed lights whose light reaches the shadow range
        std::vector<Candidate> cands;
        const float range = g_settings.shadowRange;
        for (size_t i = 0; i < g_sent.size(); ++i)
        {
            const Light &l = g_sent[i];
            const float dx = l.pos[0] - g_eye[0], dy = l.pos[1] - g_eye[1], dz = l.pos[2] - g_eye[2];
            const float reach = range + l.radius;
            if (l.shadowClear > 0.0f && l.color[0] + l.color[1] + l.color[2] > 0.6f && dx * dx + dy * dy + dz * dz < reach * reach)
                cands.push_back({(int)i});
        }
        // The chosen lamps keep their map; the new ones take those used least recently
        std::vector<int> want;
        for (const Candidate &c : cands)
        {
            if ((int)want.size() >= lamps)
                break;
            const Light &l = g_sent[c.light];
            int found = -1;
            for (int s = 0; s < lamps; ++s)
                if (g_slots[s].used && std::fabs(g_slots[s].pos[0] - l.pos[0]) < 0.01f && std::fabs(g_slots[s].pos[1] - l.pos[1]) < 0.01f &&
                    std::fabs(g_slots[s].pos[2] - l.pos[2]) < 0.01f && std::fabs(g_slots[s].radius - l.radius) < 0.01f)
                    found = s;
            if (found >= 0)
                g_slots[found].seen = g_frame;
            want.push_back(found);
        }
        for (size_t k = 0; k < want.size(); ++k)
        {
            if (want[k] >= 0)
                continue;
            int best = -1;
            for (int s = 0; s < lamps; ++s)
                if (g_slots[s].seen != g_frame && (best < 0 || !g_slots[s].used || (g_slots[best].used && g_slots[s].seen < g_slots[best].seen)))
                    best = s;
            if (best < 0)
                continue;
            const Light &l = g_sent[cands[k].light];
            Slot &s = g_slots[best];
            s = Slot();
            s.used = true, s.radius = l.radius, s.clear = l.shadowClear, s.seen = g_frame;
            for (int a = 0; a < 3; ++a)
                s.pos[a] = l.pos[a];
            want[k] = best;
        }

        const std::vector<const Caster *> moving = sortCasters();
        std::vector<const Caster *> settledFace;
        for (const Caster &c : g_settledList)
            settledFace.push_back(&c);

        // Static scene into the caches: the closest lamps first, a few per frame
        for (size_t k = 0; k < want.size() && budget > 0; ++k)
        {
            const int si = want[k];
            if (si < 0 || g_slots[si].cached)
                continue;
            Slot &s = g_slots[si];
            for (int face = 0; face < 6; ++face)
            {
                const ShadowView view = {{s.pos[0], s.pos[1], s.pos[2]}, s.radius, face};
                Model::BeginPointShadowFace(0, si, face, s.pos, s.radius, s.clear);
                drawStatic(view);
                drawCasters(settledFace, view);
            }
            s.cached = true, s.dirty = 63;
            --budget;
        }
        Model::EndPointShadowFaces();

        // Moving casters: only on the faces where something moves (this frame or the previous one), on top of the cache
        std::vector<int> slotOfLight(Model::MAX_POINT_LIGHTS, -1);
        std::vector<float> slotPos(Model::MAX_SHADOW_LAMPS * 4, 0.0f);
        std::vector<const Caster *> reached, faceList[6];
        for (size_t k = 0; k < want.size(); ++k)
        {
            const int si = want[k];
            if (si < 0 || !g_slots[si].cached)
                continue;
            Slot &s = g_slots[si];
            slotOfLight[cands[k].light] = si;
            slotPos[si * 4 + 0] = s.pos[0], slotPos[si * 4 + 1] = s.pos[1], slotPos[si * 4 + 2] = s.pos[2], slotPos[si * 4 + 3] = s.radius;
            reached.clear();
            for (const Caster *c : moving)
                if (inReach(s, c->c, c->r))
                    reached.push_back(c);
            int movingFaces = 0;
            for (int face = 0; face < 6; ++face)
            {
                faceList[face].clear();
                const ShadowView view = {{s.pos[0], s.pos[1], s.pos[2]}, s.radius, face};
                for (const Caster *c : reached)
                    if (FaceSees(view, c->c, c->r))
                        faceList[face].push_back(c);
                if (!faceList[face].empty())
                    movingFaces |= 1 << face;
            }
            Model::CopyPointShadowCache(si, s.dirty | movingFaces);
            s.dirty = movingFaces;
            for (int face = 0; face < 6; ++face)
            {
                if (faceList[face].empty())
                    continue;
                const ShadowView view = {{s.pos[0], s.pos[1], s.pos[2]}, s.radius, face};
                Model::BeginPointShadowFace(1, si, face, s.pos, s.radius, s.clear);
                drawCasters(faceList[face], view);
            }
        }
        Model::EndPointShadowFaces();
        Model::SetPointShadowRange(range);
        Model::SetPointShadowFilter(filter, 0.08f + 0.52f * g_settings.softness);
        Model::SetPointShadowSlots((int)g_sent.size(), slotOfLight.data(), slotPos.data());
        Model::EnablePointShadowMaps(true);
    }
}
