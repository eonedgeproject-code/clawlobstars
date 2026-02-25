# Changelog

All notable changes to ClawLobstars will be documented in this file.

## [0.1.0-dev] - 2025-02-25

### Added
- **Test Suite** (`tests/test_all.c`) — 70+ unit tests covering all 13 modules
- **Benchmark Tool** (`bench/benchmark.c`) — Performance measurement for all core operations
- **CHANGELOG.md** — This file

### Improved
- Build system: Added `make test` and `make bench` targets
- CI-ready test infrastructure with pass/fail exit codes

### Module Test Coverage
| Module | Tests |
|--------|-------|
| Memory Interface | 10 |
| Perception Engine | 5 |
| Cognitive System | 5 |
| Planning & Strategy | 9 |
| Action Executor | 6 |
| Knowledge Graph | 8 |
| Communication Bus | 6 |
| Multi-Agent Ops | 7 |
| Security Layer | 11 |
| Training Pipeline | 10 |
| Resource Management | 7 |
| Agent Integration | 6 |

---

## [0.1.0] - 2025-02-25

### Initial Release
- 13 fully operational modules
- Pure C99 implementation
- POSIX-compatible build system
- Cross-compilation support (ARM, RISC-V)
- Full integration demo (`examples/main.c`)
- Landing page (`index.html`)
- Technical whitepaper (`WHITEPAPER.md`)
