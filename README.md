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
| `Engine/Scene` | `Level` (level data and `.lvl` text format, no OpenGL: usable by the server), `LevelScene` (loads a level from `game.pak` or disk, draws it, collision boxes, raycast) |
| `Engine/Network` | `Client` (line protocol, optional message queue), `Server`, `LocalServer` (launch the game server from the client to host a match) |

New projects created with `create_project.bat` compile all of these folders.

## Level editor
Build it with `build_editor.bat` (add `run` to launch it), then start `Release/Editor/larva-editor.exe`.

- Choose a project, then a level (or create one). Levels are saved in `projects/<name>/Assets/Levels/*.lvl`.
- `larva-editor.exe <project> <level>` opens a level directly.
- Viewport: ZQSD (AZERTY) / WASD (QWERTY) to move the camera, hold right mouse to look around (A/E or Q/E down/up), wheel to zoom (or change speed while looking), middle mouse to pan, Shift to go faster.
- Click to select, gizmo `W` move, `E` rotate, `R` scale, `Space` cycles. Alt + drag duplicates.
- `F` focus, `End` drop to floor, `Del` delete, `Ctrl+D` duplicate, `Ctrl+C`/`Ctrl+V`, `Ctrl+Z`/`Ctrl+Y`, `Ctrl+S` save.
- Content panel: click a model or actor to add it in front of the camera, or drag it into the viewport.

`build_client.bat` packs the levels into `game.pak`. In game:

```cpp
#include "Engine/Scene/LevelScene.h"

LevelScene level;
level.load("Levels/Main.lvl");                 // game.pak first, then disk
const LevelEntity *start = level.level.playerStart();
std::vector<AABB> walls = level.collisionBoxes();
LevelEntity *door = level.level.find("Door");  // entities are plain data, editable at runtime

// each frame, after placing the camera:
level.render();                                // or level.draw() inside your own shadow / main passes
```

Projects created before the editor need `"Engine/Scene/*"` in the `sourceFiles` of their client config.
