# Red Eclipse Engine Refactor Plan

## Goals

- **Encapsulate engine state:** Replace global variables with classes and namespaces.
- **Organize by feature:** Structure `src` by subsystem, using lowercase names without underscores.
- **Modularize code:** Split large files (e.g., `engine.h`, `cube.h`) into smaller, focused modules.
- **Use engine containers:** Prefer engine-provided containers and extend them where necessary.
- **Enforce standards:** Include only required headers per file; minimize dependencies.

## 1. Encapsulation & State Management

- **Audit global state:** Identify all global variables and functions in engine and game code.
- **Encapsulate:** Move global state into subsystem classes or namespaces. Each subsystem should own and manage its own state.
- **Group related logic:** Use classes or namespaces for related state and logic (e.g., `render`, `network`, `game`, `input`).
- **RAII:** Use Resource Acquisition Is Initialization for resource management—acquire in constructors, release in destructors (memory, files, sockets, etc.).
- **Ownership:** Prefer explicit ownership and dependency injection over singletons.
- **Containers:** Replace global containers with member variables in subsystem classes.
- **Utilities:** Move static utility functions into appropriate namespaces or static class methods.
- **Macros:** Minimize global macros for state.
- **Headers:** Only include headers needed for each source file; avoid umbrella headers.

**Example:**

- Move `vector<gameentity *> entities` from global scope to an `EntitySystem` class.
- Replace `extern` variables with class members, accessed via subsystem instances.
- Use RAII: wrap file handles or OpenGL resources in classes that acquire/release them in constructors/destructors.
- Include only specific headers needed (e.g., `vec.h`, `entities.h`, `renderer.h`).

## 2. Directory & File Structure

- **Organize by subsystem:**
    - `src/render/` — renderer, shaders, materials, postprocessing
    - `src/network/` — enet integration, protocol, client/server logic
    - `src/game/` — entities, player, weapons, game logic
    - `src/input/` — input handling, keybinds, mouse
    - `src/ui/` — menus, HUD, CubeScript UI
    - `src/audio/` — OpenAL, sound, music
    - `src/core/` — engine core, main loop, config, system
    - `src/util/` — string, math primitives, file IO, logging, misc utilities
- **Naming:** Use lowercase, no underscores for files and directories.
- **Headers and sources:** Place together (e.g., `src/util/fileio.h`, `src/util/fileio.cpp`).
- **Includes:** Only include headers needed for each file; avoid umbrella headers.
- **External code:** Do not modify `src/lib`, `src/include`, `src/support`, `src/steam`, or `src/enet`.

## 3. Modularization & Refactoring Patterns

- **Split large files:** Break up monolithic files (e.g., `engine.cpp`, `game.cpp`, `rendergl.cpp`, `renderlights.cpp`).
- **Remove umbrella headers:** Eliminate umbrella and precompiled headers (e.g., `engine.h`, `cube.h`).
- **Create focused modules:** Group new files/headers by subsystem or feature (e.g., `vec.h`, `entities.h`, `renderer.h`).
- **Minimal includes:** Each source file should include only what it needs. Prefer forward declarations in headers.
- **Move definitions:** Place related constants, macros, and types in the appropriate headers.
- **VAR/FVAR/SVAR/ENUM:** Declare in headers in own namespace; use macros to determine extern/instantiation by file context.
- **Update build system:** Remove umbrella header references; use only new, granular modules.
- **Header guards:** Use include guards or `#pragma once` in every header.
- **Encapsulation:** All state in classes/namespaces; avoid global/static file-scope variables for shared state.
- **Single responsibility:** Each file/class should focus on one thing; avoid dependencies on unrelated subsystems.
- **RAII:** Use constructors/destructors for resource management; no manual global init/cleanup functions.
- **Lambdas:** Convert inline macros and small reusable code blocks to C++11 lambdas where possible. Use static functions for larger utilities.
- **Modern iteration:** Convert iteration macros (e.g., `loopi`, `loopv`) to C++11 range-based for loops or class iterators where possible.
- **Macro use:** Reserve macros for engine compatibility or performance-critical cases only.
- **Iterators:** Implement C++11-style iterators for all engine containers to enable safe, efficient iteration.

## 4. Coding Standards Enforcement

- **Containers:** Use engine containers (`vector<T>`, `string`, `hashtable<K,V>`).
- **Naming:** Classes: PascalCase. Namespaces, functions, variables: lowercase, no underscores. Constants: UPPERCASE.
- **Formatting:** Opening braces on new lines, 4-space indent, lines <200 chars.
- **Error handling:** Use `conoutf` for errors/warnings, `ASSERT` for debug, bounds checking for runtime.
- **Enums:** Use `enum.h` macros for CubeScript-accessible enums.
- **Input validation:** Always validate input, especially from network or scripts.
- **Includes:** Never use umbrella headers (e.g., `engine.h`, `cube.h`); include only what is needed for each file.

## 5. Risks & Considerations

- Large-scale refactor may introduce regressions; incremental changes and thorough testing are critical.
- Some global state may be required for legacy CubeScript/engine integration — minimize and document these cases.
- Avoid breaking external API/ABI for existing user generated content unless necessary.
