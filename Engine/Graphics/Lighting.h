#pragma once
#include "Model.h"
#include <functional>
#include <vector>

// Point lights with real shadows, managed for the game on top of Model's lighting API (environment lighting must be on,
// Model::SetEnvironment). Each frame: BeginFrame(camera), AddLight() for every light, AddCasters() for what may move,
// then EndFrame():
//  - the lights closest to the camera are kept (Model::MAX_POINT_LIGHTS, the farthest fade out near the limit);
//  - the closest lamps that cast shadows get a cube shadow map. What never moves is drawn once per lamp into a cache
//    (a few lamps per frame, by the game's callback); each frame the cache is copied and the casters that moved are
//    drawn on top. Casters standing still for a few frames (a closed door, furniture) join the cache by themselves;
//  - ambient occlusion and bounce light follow the settings (occlusion: only what is drawn between
//    Model::BeginDepthPrepass and EndDepthPrepass).
// A lamp without a map keeps the occlusion grid (Model::SetPointShadowGrid) if the game gave one, otherwise no shadow.
namespace Lighting
{
    struct Light
    {
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float radius = 5.0f;                  // reach (m)
        float color[3] = {1.0f, 1.0f, 1.0f};  // linear HDR, includes intensity
        float dir[3] = {0.0f, -1.0f, 0.0f};   // spot axis
        float cone = -2.0f;                   // cosine of the spot half angle; <= -1: omnidirectional
        // > 0: the light may get a shadow map, and what is closer than this to it does not cast shadows (the fixture
        // around the bulb, the machine it sits in). 0: no shadow map
        float shadowClear = 0.0f;
        // > 0: a lamp that moves every frame (a torch), this number being its identity. Its map has no cache: the faces its
        // cone reaches are redrawn each frame, so a moving spot costs about a static lamp's six faces at most
        int movingId = 0;
    };

    struct Settings
    {
        int shadowQuality = 3;        // 0 none .. 4: lamps with a map (4, 8, 12, 16), resolution and filter
        float softness = 0.45f;       // 0 sharp .. 1 very soft: size of the lamps (8 to 60 cm)
        float shadowRange = 40.0f;    // m from the camera: shadow maps within, the grid beyond
        float verticalWeight = 1.0f;  // > 1: lights above or below the camera count farther (other floors)
        float ambientOcclusion = 1.0f; // 0 off .. 1
        float aoRadius = 0.6f;        // m
        float aoDirect = 0.0f;        // share of the occlusion also applied to direct light
        float bounce = 1.0f;          // bounce light of the lamps, 0 off
    };
    void Configure(const Settings &settings);
    const Settings &GetSettings();

    // One face of a lamp's cube shadow map, for the static draw callback
    struct ShadowView
    {
        float pos[3];
        float radius;
        int face; // 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
    };
    // true when a sphere (center, radius) can be seen by the face: draw what fails it is wasted work
    bool FaceSees(const ShadowView &view, const float center[3], float radius);
    // Draws what never moves (Model::draw / drawInstanced) for one face; the depth shader is already bound
    using StaticDraw = std::function<void(const ShadowView &)>;

    void BeginFrame(const float eye[3]);
    void AddLight(const Light &light);
    // Shadow casters drawn with model.drawInstanced. settled: things that usually stay put (doors, furniture, props):
    // after a few frames without moving they go into the cached maps instead of being redrawn every frame
    void AddCasters(Model &model, const Model::Instance *instances, int count, bool settled);
    // What the static draw callback draws changed (another map, an opened wall...): every cached map is redrawn
    void InvalidateStatic();
    // Sends the lights; with drawStatic, also the shadow maps (none without it: menus, editors)
    void EndFrame(const StaticDraw &drawStatic = nullptr);
    // The lights sent by the last EndFrame, closest first, faded near the limit (for fog, volumetrics...)
    const std::vector<Light> &Lights();
}
