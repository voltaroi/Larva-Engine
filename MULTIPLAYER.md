**Overview**
- **Purpose:** How to run and debug the project's multiplayer (authoritative server) system.
- **Architecture:** TCP-based authoritative server (20 Hz tick), clients send inputs and perform local prediction; server broadcasts authoritative STATE messages.

**Quick Start (Windows PowerShell)**
- **Build** (use the provided batch files from the repo root):
```
.\buildServer.bat
.\buildGame.bat
```
- **Run server** (adjust path to the built executable):
```
# Example — adjust if your build produces a different path
.\Release\Server\Server.exe
```
- **Run client / engine** (from project root or built folder):
```
.\Release\Game\Game.exe
```

**Connecting clients**
- Start the server first. Clients connect to `127.0.0.1:3000` by default (change in `Game::init` / `Client::connectToServer` if needed).
- Each new client is assigned an id and color by the server (see `ASSIGN` message).

**Wire Protocol (text, newline-delimited)**
- Messages are ASCII lines terminated by `\n`. The client receive thread buffers until `\n` and queues messages for main-thread parsing.
- Server -> Client:
  - `ASSIGN <id> <r> <g> <b>` — tells the client its id and color.
  - `JOIN <id> <r> <g> <b>` — broadcast when a player joins.
  - `EXIST <id> <r> <g> <b> <x> <y>` — existing players sent to a newly connected client.
  - `STATE <id> <x> <y> <lastSeq>` — authoritative position for a player and last processed input seq for that player.
  - `LEAVE <id>` — player disconnected.
- Client -> Server:
  - `HELLO` — initial greeting (server responds with ASSIGN/JOIN/EXIST).
  - `INP <seq> <dx> <dz>` — input message with a sequence number and world-space deltas (floats). Sent ~20 Hz.

**Client-side behavior**
- Clients send inputs at ~20 Hz (server tick rate). Each input carries a sequence number. Clients store pending inputs (seq, dx, dz) and apply them locally for prediction.
- When `STATE` arrives for your player, the client removes pending inputs with seq <= lastSeq and recomputes predicted position by re-applying outstanding deltas.
- The client uses exponential smoothing to interpolate remote players and camera corrections. Tuning constants live in `Game.cpp` and `Camera.cpp` (e.g. `remoteK`, `smoothK`).

**Server-side behavior**
- Simulation runs at a fixed tick (`tickDuration = 50ms` = 20 Hz). Each tick the server applies the last vx/vy assigned from the latest `INP` message for each player and broadcasts `STATE` for all players.
- Colors are assigned deterministically from player id to ensure stable unique colors.

**Important Files**
- `Game/Game.cpp` — client game loop, networking glue, prediction & reconciliation, remote interpolation.
- `Engine/Network/Client/Client.cpp` — TCP client, receive loop (buffers on '\n', pushes to `incomingMessages`).
- `Engine/Network/Server/Server.cpp` — server accept loop, client handler, simulation tick, broadcast.
- `Engine/Graphics/Camera.cpp` — camera code, network smoothing.

**Tuning parameters**
- Server tick: `tickDuration` (currently 50 ms → 20 Hz). Lower values increase bandwidth and CPU but reduce perceived latency.
- Movement speed: server & client should match the per-tick delta (currently set in `Game.cpp` when composing INP). Tune both sides equally.
- Smoothing constants: `remoteK` and camera `smoothK` control responsiveness vs smoothness; higher = snappier, lower = smoother.

**Debugging & Troubleshooting**
- If remote players appear to update only occasionally:
  - Confirm `glutIdleFunc` is installed (the app posts redraws continuously). See `Game/main.cpp`.
  - Confirm the server is sending `STATE` every tick by checking server console logs. You should see STATE lines at ~20 Hz.
  - Check client console: it prints incoming server lines; look for `STATE` messages.
  - Check for firewall blocking port 3000 (use `netstat -ano | findstr 3000`).
- If players share the same color:
  - Server colors are assigned deterministically from id; ensure the code in `Server::clientHandler` hasn't been reverted.
- If movement doesn't match camera direction:
  - The client now sends camera-relative dx/dz. Make sure `Game.cpp` uses `player.getFrontX()` / `getFrontZ()` and the server applies the dx/dz directly.

**Developer tips**
- Keep message parsing on the main thread: the client enqueues received lines in `incomingMessages` and `Game::update()` drains and parses them — this ensures state application happens on the render/update cadence.
- Use the FPS log in `Game/main.cpp` to verify your rendering cadence.
- To change the server address/port, edit `client.connectToServer("127.0.0.1", 3000)` in `Game::init`.

**Example: minimal run sequence**
PowerShell commands (from repo root):
```powershell
# Build the server
.\buildServer.bat

# Run the server (adjust path depending on build output)
.\Release\Server\Server.exe

# In separate windows, run client(s)
.\Release\Game\Game.exe
```

If you want, I can:
- add an on-screen overlay (ping / last state time / queued incoming messages),
- add a short dev script to find the built executables, or
- paste a short checklist for debugging connection problems.

-- Generated by developer assistant
