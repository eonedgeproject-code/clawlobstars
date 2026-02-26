# Contributing to ClawLobstars

Thanks for your interest in contributing to ClawLobstars. This document covers everything you need to get started.

## Quick Start

```bash
git clone https://github.com/eonedgeproject-code/clawlobstars.git
cd clawlobstars
make build
make test
```

## Project Structure

```
src/
├── include/       # All header files (.h)
├── core/          # Agent lifecycle (cls_agent.c)
├── perception/    # Sensor input processing
├── cognitive/     # Inference engine (rule/nn/tree/bayesian)
├── planning/      # DAG-based task scheduling
├── action/        # Action execution with rollback
├── memory/        # Hash-based key-value store with TTL
├── knowledge/     # Knowledge graph with embeddings
├── comm/          # Pub/sub communication bus
├── multiagent/    # Peer discovery and collaboration
├── security/      # RBAC, encryption, audit logging
├── training/      # Replay buffer and TD-learning
├── resource/      # System monitoring and health
├── logging/       # Structured logging with sinks
├── config/        # Key-value configuration store
├── diagnostics/   # Health probes, metrics, watchdog
├── telemetry/     # Runtime telemetry collection
├── scheduler/     # Job scheduling
└── network/       # Network endpoint management
```

## How to Contribute

### Reporting Bugs

Open an issue using the **Bug Report** template. Include:

- What you expected to happen
- What actually happened
- Steps to reproduce
- Your platform (OS, compiler, architecture)
- Relevant output or logs

### Requesting Features

Open an issue using the **Feature Request** template. Describe:

- The problem you're trying to solve
- Your proposed solution
- Any alternatives you've considered

### Submitting Code

1. Fork the repository
2. Create a branch from `dev` (not `main`)
3. Make your changes
4. Run `make test` and ensure all tests pass
5. Run `make build` with zero warnings
6. Submit a pull request to `dev`

## Code Standards

ClawLobstars is pure C99. Follow these conventions:

**Naming:**
- Functions: `cls_module_action()` (e.g. `cls_memory_store()`)
- Types: `cls_type_name_t` (e.g. `cls_agent_state_t`)
- Constants: `CLS_UPPER_CASE` (e.g. `CLS_ERR_NOMEM`)

**Style:**
- 4-space indentation, no tabs
- Opening brace on same line for functions
- All functions return `cls_status_t` where applicable
- Always check NULL parameters at function entry
- Use `memset` to zero structs on init
- Comment sections with `/* === Section Name === */`

**Memory:**
- No dynamic allocation in hot paths
- Every `malloc`/`calloc` must have a matching `free` in `_destroy()`
- Check all allocation return values
- Zero sensitive data before freeing

**Headers:**
- Include guards: `#ifndef CLS_MODULE_H`
- All public API in headers, all implementation in `.c` files
- Include `cls_framework.h` first

## Commit Messages

```
fix: memory used-size tracking on realloc update
add: diagnostics module with health probes and watchdog
update: planning dependency validation
clean: remove unused variables in cognitive
```

Prefix with: `fix:`, `add:`, `update:`, `clean:`, `docs:`, `test:`

## Testing

Tests live in `tests/test_all.c` on the `dev` branch.

```bash
make test     # Run all tests
make bench    # Run benchmarks
```

When adding a new module, add corresponding tests that cover init/destroy, normal operations, edge cases, and error handling.

## Branches

| Branch | Purpose |
|--------|---------|
| `main` | Stable releases |
| `dev` | Active development, PR target |
| `feature/*` | Feature branches |

## License

By contributing, you agree that your contributions will be licensed under GPL-3.0.

## Questions?

Open an issue or start a discussion. No question is too small.
