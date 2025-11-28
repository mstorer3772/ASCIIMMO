# ASCIIMMO

ASCIIMMO — an ASCII-art based MMO prototype (sword & sorcery tropes).

This repo contains an initial scaffold:

- A deterministic ASCII world generator in C++ (`src/worldgen.cpp`) that produces a repeatable map from a numeric seed.
- A C++ server skeleton (`src/main.cpp`) which can run the generator on the CLI; HTTP & networking endpoints are marked TODOs.
- A tiny browser client (`client/index.html` + `client/app.js`) that expects an HTTP endpoint `/world?seed=<seed>` (server HTTP stub to be implemented).
- `CMakeLists.txt` to build the server.

Goals for next steps:
- Add a lightweight HTTP/WebSocket library (e.g., `cpp-httplib`, `Boost.Beast`, or `uWebSockets`) to serve the generator and handle player connections.
- Split services into microservices: auth, world, session/actor, persistence.
- Add a GitHub repository and CI.

Quick start (local, no HTTP server):

1. Build the CLI generator:

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

2. Run the generator to print a map to stdout:

```bash
./asciimmo-server --seed 12345 --width 80 --height 24
```

3. To use the browser client before HTTP is implemented, run the generator and save the map to `client/world.txt`, then serve `client/` with a static server, e.g.: 

```bash
./asciimmo-server --seed 12345 > client/world.txt
cd client
python3 -m http.server 8000
# open http://localhost:8000 in your browser
```

Development notes:
- The generator is deterministic using `std::mt19937_64` seeded with the provided seed.
- The browser client will attempt to fetch `/world?seed=`; when the server HTTP endpoint is implemented it can be used directly.

If you'd like, I can:
- Implement a basic HTTP endpoint using `cpp-httplib` so the client works immediately.
- Add WebSocket support for realtime updates.
- Create the GitHub repo and push this scaffold.
