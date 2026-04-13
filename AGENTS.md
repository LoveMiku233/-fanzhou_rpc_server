# Repository Guidelines

## Project Structure & Module Organization
Core server code is in `src/`, organized by layer: `core/` (runtime context and config), `rpc/` (JSON-RPC server/dispatcher/handlers), `comm/` (CAN/serial adapters), `device/` (relay and sensor drivers), `cloud/` (MQTT + FanZhou cloud), plus shared `utils/` and `types/`.  
Main entrypoint is `main.cpp`; Qt build config is `fanzhou_rpc_server.pro`.  
Related apps: `qt_app/` (desktop client) and `test_web/` (web/Tauri debug tool).  
Docs live in `docs/`; example runtime config is `config/config_example.json`.

## Build, Test, and Development Commands
Server build:
```bash
mkdir -p build && cd build
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```
Qt client build:
```bash
cd qt_app && mkdir -p build && cd build
qmake ../qt_app.pro
make -j$(nproc)
```
Tauri debug app:
```bash
cd test_web
cargo tauri dev    # local debug
cargo tauri build  # package app
```
Quick RPC smoke test (server on `12345`):
```bash
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345
```

## Coding Style & Naming Conventions
Use C++14-compatible Qt code (project setting), 4-space indentation, and existing include/order patterns in nearby files.  
Naming: classes `PascalCase`, methods `camelCase`, members `camelCase_`, constants `kPascalCase`, enums `PascalCase`.  
Prefer `QStringLiteral("...")` for static Qt strings.  
Keep modules layered: transport in `comm/*`, device logic in `device/*`, RPC wiring in `src/core/rpc_registry.cpp`.

## Testing Guidelines
There is no dedicated unit-test suite in this repository yet. Treat build + runtime verification as mandatory:
1. Build changed targets without warnings/errors.
2. Run JSON-RPC smoke checks (for example `rpc.ping`, modified methods).
3. For CAN/serial/cloud changes, attach manual verification notes (commands, sample payloads, observed results).
When adding complex logic, prefer adding a small reproducible harness or scripted check in the same PR.

## Commit & Pull Request Guidelines
Recent history uses concise, imperative subjects (`Fix ...`, `Improve ...`) and optional prefixes (`fix:`). Keep subject lines specific to one change.  
PRs should include: scope summary, affected modules (for example `src/rpc`, `src/device/can`), config/protocol changes, and verification evidence (build + runtime checks). Link related issues and include UI screenshots only for `qt_app/` or `test_web/` changes.
