#pragma once

#include <cstdlib>

/// Debug-only programmer invariant checks. Elided when `MARBLE_DEBUG` is unset or 0
/// (see `docs/decisions/ADR-0007-software-engineering-invariants.md`).
#if defined(MARBLE_DEBUG) && MARBLE_DEBUG
#include <cassert>
#define MARBLE_ASSERT(expr) assert(expr)
#else
#define MARBLE_ASSERT(expr) ((void)0)
#endif

/// Marks a path that must not execute (e.g. non-enum switch default after covered cases).
#if defined(_MSC_VER)
#define MARBLE_UNREACHABLE() __assume(0)
#elif defined(__GNUC__) || defined(__clang__)
#define MARBLE_UNREACHABLE() __builtin_unreachable()
#else
#define MARBLE_UNREACHABLE() std::abort()
#endif
