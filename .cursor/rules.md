# Project Rules

**Language:** Modern C++20  
**Build System:** CMake  
**Architecture Goals:**
- SOLID principles
- ECS architecture
- Vulkan renderer
- Cross platform
- Highly modular

**Code Style:**
- No raw new/delete
- Use RAII
- Prefer unique_ptr
- Avoid macros
- Prefer composition over inheritance

**Engine Layers:**

```
engine/
    core
    math
    ecs
    renderer
    platform
```

**Rules:**
- Renderer must not depend on ECS
- ECS must not depend on renderer
- Platform layer must isolate OS code
- Math must contain no external dependencies

This dramatically improves Cursor's suggestions.
