# 🦞 CLAWLOBSTARS

### Autonomous AI Agent Framework — Pure C99

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C99](https://img.shields.io/badge/C99-Compliant-blue.svg)]()
[![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/Tests-91%20Passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/Platform-x86%20|%20ARM%20|%20RISC--V-orange.svg)]()

---

**ClawLobstars** is a lightweight AI agent framework built entirely in C99. Designed for edge computing, embedded systems, and autonomous operations where every byte matters.

Zero dependencies. Sub-microsecond inference. Runs on anything with a C compiler.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                     AGENT CORE                        │
│              lifecycle · state · step loop             │
├─────────────┬─────────────┬──────────┬───────────────┤
│ PERCEPTION  │  COGNITIVE   │ PLANNING │    ACTION     │
│   ENGINE    │   SYSTEM     │ STRATEGY │   EXECUTOR    │
│ sensors     │ 4 model types│ DAG tasks│ handlers      │
│ events      │ rule/nn/tree │ goals    │ rollback      │
│ polling     │ bayesian     │ deps     │ timeouts      │
├─────────────┼─────────────┼──────────┼───────────────┤
│   MEMORY    │  KNOWLEDGE   │  MULTI   │COMMUNICATION  │
│  INTERFACE  │   GRAPH      │  AGENT   │     BUS       │
│ hash KV     │ embeddings   │ peers    │ pub/sub       │
│ TTL/prune   │ BFS paths    │ voting   │ ring buffer   │
│ FNV-1a      │ similarity   │ collab   │ broadcast     │
├─────────────┼─────────────┼──────────┼───────────────┤
│  SECURITY   │  TRAINING    │ RESOURCE │INFRASTRUCTURE │
│ RBAC/auth   │ replay buf   │ /proc    │ logging       │
│ encryption  │ ε-greedy     │ health   │ diagnostics   │
│ audit log   │ TD-learning  │ recovery │ config        │
└─────────────┴─────────────┴──────────┴───────────────┘
```

13 modular subsystems. Each module is independently testable and swappable.

---

## Quick Start

```bash
git clone https://github.com/eonedgeproject-code/clawlobstars.git
cd clawlobstars
make build
./build/bin/cls_example
```

## Usage

```c
#include "cls_framework.h"

int main(void) {
    cls_config_t cfg = CLS_CONFIG_DEFAULT;
    cfg.agent_id    = 1;
    cfg.agent_name  = "tactical-01";
    cfg.memory_size = 1024 * 256;
    cfg.security_level = CLS_SEC_HIGH;

    cls_agent_t agent;
    cls_agent_init(&agent, &cfg);

    for (int i = 0; i < 100; i++) {
        cls_agent_step(&agent);
    }

    cls_agent_shutdown(&agent);
    cls_agent_destroy(&agent);
    return 0;
}
```

---

## Modules

| # | Module | Description |
|---|--------|-------------|
| 1 | **Agent Core** | Lifecycle management, state machine, step loop |
| 2 | **Memory Interface** | Hash table KV store with TTL, FNV-1a, 256 buckets |
| 3 | **Perception Engine** | Sensor polling, event detection, threshold triggers |
| 4 | **Cognitive System** | 4 inference models: rule-based, neural net, decision tree, Bayesian |
| 5 | **Planning & Strategy** | DAG-based task scheduling, goal management, dependency resolution |
| 6 | **Action Executor** | Handler registration, rollback support, timeout monitoring |
| 7 | **Knowledge Graph** | Node/edge store, cosine similarity search, BFS pathfinding |
| 8 | **Communication Bus** | Pub/sub messaging, ring buffer queue, broadcast/unicast |
| 9 | **Multi-Agent Ops** | Peer discovery, collaboration proposals, voting consensus |
| 10 | **Security Layer** | RBAC, auth tokens, XOR cipher, input validation, audit log |
| 11 | **Training Pipeline** | Experience replay, ε-greedy exploration, TD-learning, snapshots |
| 12 | **Resource Management** | /proc monitoring, health checks, auto-recovery actions |
| 13 | **Infrastructure** | Logging, diagnostics, configuration management |

---

## Performance

Benchmarked on x86_64 (GCC -O2):

| Operation | Latency | Throughput |
|-----------|---------|------------|
| `memory_store` | 0.10 µs | 10M ops/s |
| `infer_rule_based` | 0.10 µs | 9.6M ops/s |
| `knowledge_search` | 53.63 µs | 18K ops/s |
| `agent_step` | 0.34 µs | 2.9M ops/s |

91 unit tests — all passing.

---

## Project Structure

```
clawlobstars/
├── src/
│   ├── include/              # All headers (14 files)
│   │   ├── cls_framework.h   # Master include
│   │   ├── cls_agent.h       # Agent core
│   │   ├── cls_memory.h      # Memory interface
│   │   ├── cls_perception.h  # Perception engine
│   │   ├── cls_cognitive.h   # Cognitive system
│   │   ├── cls_planning.h    # Planning & strategy
│   │   ├── cls_action.h      # Action executor
│   │   ├── cls_knowledge.h   # Knowledge graph
│   │   ├── cls_comm.h        # Communication bus
│   │   ├── cls_multiagent.h  # Multi-agent ops
│   │   ├── cls_security.h    # Security layer
│   │   ├── cls_training.h    # Training pipeline
│   │   └── cls_resource.h    # Resource management
│   ├── core/                 # Agent core
│   ├── memory/               # Memory interface
│   ├── perception/           # Perception engine
│   ├── cognitive/            # Cognitive system
│   ├── planning/             # Planning & strategy
│   ├── action/               # Action executor
│   ├── knowledge/            # Knowledge graph
│   ├── comm/                 # Communication bus
│   ├── multiagent/           # Multi-agent operations
│   ├── security/             # Security layer
│   ├── training/             # Training pipeline
│   ├── resource/             # Resource management
│   ├── solana/               # Solana integration ⛓️
│   └── token/                # $CLAW token engine 🪙
├── examples/
│   ├── main.c                # Full 13-module demo
│   ├── solana_agent.c        # Solana wallet/DeFi demo
│   └── token_demo.c          # $CLAW tokenomics demo
├── tests/
│   └── test_all.c            # 91 unit tests
├── bench/
│   └── benchmark.c           # Performance benchmarks
├── Makefile
├── README.md
├── WHITEPAPER.md
├── CHANGELOG.md
├── LICENSE
└── index.html
```

---

## Build Options

```bash
make build              # Default build (O2)
make lib                # Static library only
make example            # Example binary only
make test               # Run 91 unit tests
make bench              # Run benchmarks
make solana             # Build Solana agent demo
make token              # Build token demo
make build OPT=O3       # Aggressive optimization
make build DEBUG=1      # Debug symbols + CLS_DEBUG
make arm                # Cross-compile ARM Cortex-M4
make riscv              # Cross-compile RISC-V
make clean              # Remove build artifacts
```

---

## Branches

| Branch | Description |
|--------|-------------|
| `main` | Stable — 13 core modules, docs, landing page |
| `dev` | Testing — 91 unit tests, benchmarks, changelog |
| `feature/solana-agent` | Solana integration — wallet, RPC, DeFi, watchers |
| `feature/token-integration` | $CLAW token engine — staking, governance, licensing |

---

## Solana Integration

Available on `feature/solana-agent` branch:

- Ed25519 keypair generation & Base58 encoding
- Wallet management with balance tracking
- Transaction builder (multi-instruction support)
- Pre-built instructions: SOL transfer, SPL token, create ATA, memo
- DeFi operations: price feeds, swap quotes, AMM execution
- On-chain watchers with callbacks (balance, token, price)
- ATA derivation (PDA simulation)

```bash
git checkout feature/solana-agent
make solana
./build/bin/cls_solana_agent
```

## $CLAW Token

Available on `feature/token-integration` branch:

- **Supply:** 1,000,000,000 CLAW (9 decimals)
- **Staking:** 4 tiers — Scout (12% APY) → Admiral (22% APY)
- **Governance:** Stake-weighted voting, 10% quorum
- **Revenue:** 70% stakers / 20% treasury / 10% burned
- **Agent Licensing:** Tier-gated module access
- **Vesting:** Linear, cliff, stepped schedules

```bash
git checkout feature/token-integration
make token
./build/bin/cls_token_demo
```

---

## Specs

| Spec | Value |
|------|-------|
| Language | Pure C (C99) |
| Dependencies | None (libc only) |
| Min RAM | < 512KB |
| Library Size | ~81KB static |
| Binary Size | ~73KB example |
| Source | 25 files, ~4,700 LOC |
| Inference | Sub-microsecond |
| Platforms | x86, ARM, RISC-V, MIPS |
| Concurrency | POSIX threads |
| License | GPL-3.0 |

---

## Applications

- **Edge AI** — Intelligence at the sensor level, zero cloud dependency
- **Autonomous Robotics** — Adaptive task execution on embedded MCUs
- **IoT Networks** — Anomaly detection on microcontrollers
- **Trading Systems** — Microsecond-latency market analysis
- **DeFi Agents** — Autonomous on-chain operations via Solana
- **Medical Devices** — Deterministic AI for adaptive implants

---

## Documentation

- [Technical Whitepaper](WHITEPAPER.md) — Full architecture deep dive
- [Changelog](CHANGELOG.md) — Version history
- [Landing Page](index.html) — Project overview

## Contributing

Contributions welcome. Open an issue or submit a PR.

```bash
git checkout -b feature/your-feature
make test    # ensure all 91 tests pass
git push origin feature/your-feature
```

## License

GPL-3.0 — See [LICENSE](LICENSE)

---

**CLAWLOBSTARS** — *Pure C. Zero Dependencies. Autonomous Intelligence.*

[GitHub](https://github.com/eonedgeproject-code/clawlobstars) · [Whitepaper](WHITEPAPER.md)
