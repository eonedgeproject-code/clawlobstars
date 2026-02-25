# Changelog

All notable changes to the ClawLobstars framework.

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
