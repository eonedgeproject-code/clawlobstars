# Changelog

All notable changes to the ClawLobstars framework.

---

## [0.4.0] — 2026-02-28

### Added — 3 New Modules (16 → 19 total)
- **Config** (`cls_config`): Runtime configuration management, key-value store, environment variable loading, configuration profiles, hot-reload watchers, freeze/validate support
- **Logging** (`cls_logging`): Structured logging system, 6 log levels, multiple output sinks, log formatting, module-scoped filtering
- **Diagnostics** (`cls_diagnostics`): Health probe system with configurable intervals, metrics collection (counters, gauges, timers), watchdog timer, snapshot history ring buffer, diagnostic dump reports
- **Plugin** (`cls_plugin`): Dynamic module system with 7 plugin types, lifecycle hooks (init/start/tick/stop/destroy), dependency resolution, slot management, per-plugin performance tracking

### Added — Community Infrastructure
- `CONTRIBUTING.md` with code style guide, commit conventions, PR process
- `.github/ISSUE_TEMPLATE/bug_report.md` for structured bug reports
- `.github/ISSUE_TEMPLATE/feature_request.md` for feature requests

### Fixed
- **memory**: `used` size tracker not updated on realloc when updating existing keys
- **memory**: added max data size guard to prevent oversized allocations (cap at capacity/2)
- **security**: stale data in audit ring buffer when entries wrap around (now memset before write)
- **planning**: `cls_plan_add_task()` now validates dependency IDs exist and prevents self-dependency cycles
- **training**: reward values now clipped to [-100, 100] range to prevent NaN/inf propagation in TD-learning
- Removed broken empty directory artifact in `src/`
- README now reflects correct module count (19) and repository URL
- Project structure in README matches actual directory layout
- Architecture diagram updated to show all 19 modules

### Changed
- README fully rewritten with complete module table, updated specs, contributor links
- CHANGELOG versioning corrected
- Makefile updated with new module source paths

---

## [0.3.0] — 2026-02-28

### Added — 3 New Modules (13 → 16 total)
- **Telemetry** (`cls_telemetry`): Performance counters (monotonic, gauge, histogram), event tracing with severity levels, trace spans for profiling, snapshot API
- **Scheduler** (`cls_scheduler`): Priority task queue (6 levels: IDLE to REALTIME), recurring jobs with max runs, deferred execution with delay, deadline management, automatic retry with backoff
- **Network** (`cls_network`): Protocol abstraction (TCP, UDP, WebSocket, RPC, P2P), endpoint management with connection state, message routing with weighted load balancing, packet priority queuing, latency tracking with EMA

### Fixed
- Version mismatch: framework header was stuck at 0.1.0 while Makefile said 0.2.0
- Missing null checks in security encrypt/decrypt buffers (potential segfault)
- Uninitialized buffer in memory retrieve demo (undefined behavior)
- Makefile version string now matches framework version
- Cleanup order in main.c fixed to prevent use-after-free scenarios

### Changed
- Updated `examples/main.c` to demonstrate all 16 modules with proper error handling
- Integration loop now includes scheduler tick and telemetry tracking
- Framework header includes all 16 module headers
- Makefile SRCS updated with 3 new module source files

---

## [0.2.0] — 2025-02-25

### Added — Solana Integration (`feature/solana-agent`)
- Ed25519 keypair generation with Base58 encoding
- Wallet management with balance and portfolio tracking
- Transaction builder with multi-instruction support
- Pre-built instructions: SOL transfer, SPL token, create ATA, memo
- DeFi operations: price feeds, swap quotes, Raydium AMM
- On-chain watchers with callbacks (balance, token, price)
- ATA derivation (PDA simulation)
- Solana agent example (`examples/solana_agent.c`)

### Added — $CLAW Token Engine (`feature/token-integration`)
- Token economics: 1B supply, 6 allocation pools
- 4-tier staking system (Scout → Admiral, 12-22% APY)
- Governance: proposals, stake-weighted voting, 10% quorum
- Revenue distribution: 70% stakers / 20% treasury / 10% burn
- Agent licensing: tier-gated module access
- Vesting engine: linear, cliff, stepped schedules
- Token demo (`examples/token_demo.c`)

### Added — Testing & Benchmarks (`dev`)
- 91 unit tests across all 13 modules
- Performance benchmark tool
- `make test` and `make bench` targets

### Changed
- Updated Makefile with all build targets (test, bench, solana, token)
- Polished README with complete module listing and branch docs
- Fixed all GitHub URLs in landing page and README
- Added LICENSE (GPL-3.0) and CHANGELOG

---

## [0.1.0] — 2025-02-25

### Added — Core Framework (13 Modules)
- **Agent Core** — Lifecycle management, state machine, step loop
- **Memory Interface** — Hash table KV store, TTL pruning, FNV-1a hash
- **Perception Engine** — Sensor polling, event detection, thresholds
- **Cognitive System** — 4 model types: rule-based, neural net, decision tree, Bayesian
- **Planning & Strategy** — DAG-based task scheduling, goal management, dependency resolution
- **Action Executor** — Handler registration, rollback support, timeout monitoring
- **Knowledge Graph** — Node/edge management, cosine similarity, BFS pathfinding
- **Communication Bus** — Pub/sub messaging, ring buffer queue, broadcast/unicast
- **Multi-Agent Ops** — Peer discovery, collaboration proposals, voting consensus
- **Security Layer** — RBAC, auth tokens, XOR cipher, input validation, audit log
- **Training Pipeline** — Experience replay, ε-greedy exploration, TD-learning, snapshots
- **Resource Management** — /proc monitoring, health checks, auto-recovery
- **Infrastructure** — Logging, diagnostics, configuration

### Added — Build System
- Makefile with cross-compilation (ARM Cortex-M4, RISC-V)
- Zero-warning compilation (C99 strict)
- Dependency tracking

### Added — Documentation
- Technical whitepaper (WHITEPAPER.md)
- Landing page (index.html) — military/tactical theme
- README with architecture overview
