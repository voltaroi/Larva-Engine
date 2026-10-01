# Larva Engine

**Larva Engine** is a C++ game engine based on OpenGL. Its goal is to facilitate the creation of massively online 3D games on Windows.

## Modules
| Module      | Technology   | Status          |
|-------------|--------------|-----------------|
| Rendering   | C++ / OpenGL | 🛠️ In progress |
| Audio       | OpenAL       | 🛠️ In progress |
| Physics     | C++          | 🛠️ In progress |
| Multiplayer | C++          | 🛠️ In progress |
| Editor      | C++          | 🛠️ In progress |
## Engine layout
| Folder | Contents |
|--------|----------|
| `Engine/Core` | `MathUtils` (angles, clamp, damping), `Random` (deterministic generator), `Spline` (centripetal Catmull-Rom, constant-step resampling), `ConfigFile` (key/value settings file) |
| `Engine/Physics` | `Collision2D` (circle vs segment / oriented box, spatial grid of segments) |
| `Engine/Audio` | `AudioDevice` (OpenAL context, buffers, looping 3D sources, one-shot pool), `Synth` (procedural sound: filters, tones, engine loop, rumble, impact) |
| `Engine/Graphics` | `Model`, `Camera`, `UI`, `UIWidgets` (immediate-mode buttons and sliders), `ShaderProgram` (GLSL from pak or disk with a shared prelude), `RenderTarget` (offscreen HDR / depth framebuffers), `ParticleSystem` (CPU particles + sorted billboard data), `MeshBuilder` (CPU mesh building and static batching) |
| `Engine/Network` | `Client` (line protocol, optional message queue), `Server`, `LocalServer` (launch the game server from the client to host a match) |

New projects created with `create_project.bat` compile all of these folders.
