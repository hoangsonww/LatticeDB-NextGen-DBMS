# LatticeDB - The Next-Gen, Mergeable Temporal Relational Database 🗂️

*A next-generation, mergeable, temporal, policy-aware relational DBMS with a built-in GUI.*

> [!IMPORTANT]
> **Mission**: Make conflict-free multi-master, time-travel, privacy-preserving analytics, streaming, and vector search **first-class** in a relational DB—without bolted-on sidecars.

<p align="center">
  <img src="docs/logo.png" alt="LatticeDB Logo" width="50%"/>
</p>

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white&style=for-the-badge)](https://isocpp.org/)
[![Bash](https://img.shields.io/badge/Bash-Scripts-4EAA25?logo=gnubash&logoColor=white&style=for-the-badge)](https://www.gnu.org/software/bash/)
[![Python](https://img.shields.io/badge/Python-Helper%20Tools-3776AB?logo=python&logoColor=white&style=for-the-badge)](https://www.python.org/)
[![CMake](https://img.shields.io/badge/CMake-3.15%2B-064F8C?logo=cmake&logoColor=white&style=for-the-badge)](https://cmake.org/)
[![Ninja](https://img.shields.io/badge/Ninja-Build-000000?logo=ninja&logoColor=white&style=for-the-badge)](https://ninja-build.org/)
[![GCC](https://img.shields.io/badge/GCC-Compiler-4EAA25?logo=gnu&logoColor=white&style=for-the-badge)](https://gcc.gnu.org/)
[![Clang/LLVM](https://img.shields.io/badge/Clang-LLVM-262D3A?logo=llvm&logoColor=white&style=for-the-badge)](https://llvm.org/)
[![GitHub Actions](https://img.shields.io/badge/GitHub%20Actions-CI-2088FF?logo=githubactions&logoColor=white&style=for-the-badge)](https://github.com/features/actions)
[![CodeQL](https://img.shields.io/badge/CodeQL-Security%20Scan-2F67A1?logo=github&logoColor=white&style=for-the-badge)](https://codeql.github.com/)
[![Dependabot](https://img.shields.io/badge/Dependabot-Updates-025E8C?logo=dependabot&logoColor=white&style=for-the-badge)](https://github.com/dependabot)
[![AddressSanitizer](https://img.shields.io/badge/Sanitizer-ASan-5E4B8B?logo=llvm&logoColor=white&style=for-the-badge)](https://clang.llvm.org/docs/AddressSanitizer.html)
[![UBSan](https://img.shields.io/badge/Sanitizer-UBSan-5E4B8B?logo=llvm&logoColor=white&style=for-the-badge)](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
[![clang-format](https://img.shields.io/badge/clang--format-Style-262D3A?logo=llvm&logoColor=white&style=for-the-badge)](https://clang.llvm.org/docs/ClangFormat.html)
[![cppcheck](https://img.shields.io/badge/cppcheck-Static%20Analysis-8892BF?logo=cpp&logoColor=white&style=for-the-badge)](http://cppcheck.sourceforge.net/)
[![Docker](https://img.shields.io/badge/Docker-Container-2496ED?logo=docker&logoColor=white&style=for-the-badge)](https://www.docker.com/)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu&logoColor=white&style=for-the-badge)](https://ubuntu.com/)
[![macOS](https://img.shields.io/badge/macOS-Supported-000000?logo=apple&logoColor=white&style=for-the-badge)](https://www.apple.com/macos/)
[![WSL2](https://img.shields.io/badge/WSL2-Dev-0078D6?logo=windows&logoColor=white&style=for-the-badge)](https://learn.microsoft.com/windows/wsl/)
[![Mermaid](https://img.shields.io/badge/Mermaid-Diagrams-1B6AC6?logo=mermaid&logoColor=white&style=for-the-badge)](https://mermaid.js.org/)
[![EditorConfig](https://img.shields.io/badge/EditorConfig-Consistent%20Style-000000?logo=editorconfig&logoColor=white&style=for-the-badge)](https://editorconfig.org/)
[![Vite](https://img.shields.io/badge/Vite-Bundler-646CFF?logo=vite&logoColor=white&style=for-the-badge)](https://vitejs.dev/)
[![React Query](https://img.shields.io/badge/React%20Query-Data%20Fetching-FF4154?logo=reactquery&logoColor=white&style=for-the-badge)](https://tanstack.com/query/latest)
[![Vercel](https://img.shields.io/badge/Vercel-Hosting-000000?logo=vercel&logoColor=white&style=for-the-badge)](https://vercel.com/)
[![React](https://img.shields.io/badge/React-GUI-61DAFB?logo=react&logoColor=black&style=for-the-badge)](https://react.dev/)
[![TypeScript](https://img.shields.io/badge/TypeScript-Frontend-3178C6?logo=typescript&logoColor=white&style=for-the-badge)](https://www.typescriptlang.org/)
[![TailwindCSS](https://img.shields.io/badge/TailwindCSS-Styling-06B6D4?logo=tailwindcss&logoColor=white&style=for-the-badge)](https://tailwindcss.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-3DA639?logo=open-source-initiative&logoColor=white&style=for-the-badge)](LICENSE)

## Table of Contents

- [Why LatticeDB](#why-latticedb)
- [Feature Matrix & How It’s Different](#feature-matrix--how-its-different)
- [Architecture Overview](#architecture-overview)
- [Quick Start](#quick-start)
- [Start with GUI](#start-with-gui)
- [Core Concepts & Examples](#core-concepts--examples)
  - [Mergeable Relational Tables (MRT)](#mergeable-relational-tables-mrt)
  - [Bitemporal Time Travel & Lineage](#bitemporal-time-travel--lineage)
  - [Policy-as-Data & Differential Privacy](#policy-as-data--differential-privacy)
  - [Vectors & Semantic Joins](#vectors--semantic-joins)
  - [Streaming Materialized Views](#streaming-materialized-views)
- [Storage, Transactions & Replication](#storage-transactions--replication)
- [SQL: LatticeSQL Extensions](#sql-latticesql-extensions)
- [Operations & Observability](#operations--observability)
- [Roadmap](#roadmap)
- [Limitations](#limitations)
- [Contributing](#contributing)
- [License](#license)

## Why LatticeDB

LatticeDB is designed from the ground up to make **conflict-free multi-master**, **bitemporal**, **policy-aware**, **privacy-preserving**, **real-time analytics**, **streaming**, and **vector search** first-class citizens in a relational database—without bolting on sidecars, plugins, or external services.

1. **Mergeable Relational Tables (MRT)**: Per-column **CRDT** semantics for conflict-free **active-active** replication and **offline-first** applications. Choose `lww`, `sum_bounded`, `gset`, or custom resolvers.
2. **Bitemporal by default**: Every row has **transaction time** *and* **valid time** plus provenance—no retrofitting needed. Query **as of** a timestamp and ask *why* with lineage.
3. **Policy-as-data**: Row/column masking, role attributes, retention, and **differential privacy** budgets are declared and versioned in the catalog, enforced **inside** the engine.
4. **Unified storage surfaces**: OLTP row pages, OLAP columnar projections, **stream tails**, and **vector indexes** co-exist and are chosen by the optimizer.
5. **WASM extensibility**: Deterministic, capability-scoped WebAssembly UDF/UDTF/UDA, custom codecs, and merge resolvers with fuel/memory limits.
6. **Hybrid concurrency**: MVCC by default; **deterministic lane** for hot-key contention; optional **causal+ snapshots** with bounded staleness hints per query.
7. **Zero-downtime schema evolution**: Versioned schemas, online backfills, and cross-version query rewrite.
8. **Streaming MVs**: Exactly-once, incremental materialized views that can consume internal CDC or external logs.

## Feature Matrix & How It’s Different

LatticeDB focuses on features that major RDBMS generally **don’t provide natively out-of-the-box all together**:

| Capability                                                  | LatticeDB               |                       Typical in PostgreSQL |                   Typical in MySQL |                                                 Typical in SQL Server |
| ----------------------------------------------------------- | ----------------------- | ------------------------------------------: | ---------------------------------: | --------------------------------------------------------------------: |
| **Mergeable tables (CRDTs) for conflict-free multi-master** | **Built-in** per column |           Via external systems/custom logic |  Via external systems/custom logic |                                     Via external systems/custom logic |
| **Bitemporal (valid + transaction time) by default**        | **Built-in**            |              Emulatable via schema/patterns |     Emulatable via schema/patterns | System-versioned temporal tables exist; **valid-time** needs modeling |
| **Policy-as-data (RLS/CLS/DP budgets) in engine**           | **Built-in**            |  RLS exists; DP requires extensions/tooling | RLS via views/plugins; DP external |                                               RLS exists; DP external |
| **Vector search integrated**                                | **Built-in**            |                     Commonly via extensions |      Commonly via plugins/editions |                                             Varies by edition/service |
| **WASM UDF sandbox**                                        | **Built-in**            | Not built-in (extensions/native C required) |                       Not built-in |                                                          Not built-in |
| **Exactly-once streaming MVs**                              | **Built-in**            |               Logical decoding + extensions |          Binlog + external engines |                                                CDC + external engines |
| **Offline-first & causal+ snapshots**                       | **Built-in**            |          External tooling/replication modes |                   External tooling |                                                      External tooling |

> [!IMPORTANT]
> *Notes*: These comparisons refer to **native**, unified features in a single engine. Many incumbents can achieve parts of this via **extensions**, **editions**, or **external services**, but not as a cohesive, first-class design as in LatticeDB.

## Architecture Overview

```mermaid
flowchart TD
    subgraph "Client Layer"
      GUI[Web GUI]
      CLI[CLI Client]
      SDK[SDKs]
    end

    subgraph "Query Engine"
      PARSE[SQL Parser]
      OPT[Query Optimizer]
      EXEC[Executor]
    end

    subgraph "Storage Layer"
      MVCC[MVCC Controller]
      ULS[Universal Log Store]
      IDX[B+ Tree & Vector Indexes]
    end

    subgraph "CRDT Engine"
      LWW[LWW Register]
      GSET[G-Set]
      COUNTER[PN-Counter]
    end

    subgraph "Security & Privacy"
      RLS[Row-Level Security]
      DP[Differential Privacy]
      AUDIT[Audit Logger]
    end

    GUI --> PARSE
    CLI --> PARSE
    SDK --> PARSE
    PARSE --> OPT
    OPT --> EXEC
    EXEC --> MVCC
    MVCC --> ULS
    ULS --> IDX
    EXEC --> RLS
    EXEC --> DP
    EXEC --> AUDIT
    ULS --> LWW
    ULS --> GSET
    ULS --> COUNTER
    F1 --> O3
    F2 --> O3
    F3 --> O3
```

## Quick Start

### Prerequisites

* C++17 toolchain (clang++/g++)
* CMake ≥ 3.15
* Linux/macOS/WSL2 (for now)
* Optional: Python 3.x to run simple workload scripts

### Build from Source

```bash
git clone https://example.com/latticedb.git
cd latticedb
cmake -S . -B build
cmake --build build -j
./build/latticedb  # launches a simple REPL
```

### Hello World (REPL)

```sql
-- Create a mergeable, bitemporal table with vectors
CREATE TABLE people (
  id TEXT PRIMARY KEY,
  name TEXT MERGE lww,
  tags SET<TEXT> MERGE gset,
  credits INT MERGE sum_bounded(0, 1000000),
  profile_vec VECTOR<4>
);

INSERT INTO people (id, name, tags, credits, profile_vec) VALUES
('u1','Ada', {'engineer','math'}, 10, [0.1,0.2,0.3,0.4]),
('u2','Grace', {'engineer'}, 20, [0.4,0.3,0.2,0.1]);

-- Conflict-free merge on upsert
INSERT INTO people (id, credits, tags, name) VALUES
('u1', 15, {'leader'}, 'Ada Lovelace') ON CONFLICT MERGE;

-- Time travel (transaction time)
SELECT * FROM people FOR SYSTEM_TIME AS OF TX 1;

-- Vector similarity filter (brute-force demo)
SELECT id, name FROM people
WHERE DISTANCE(profile_vec, [0.1,0.2,0.29,0.41]) < 0.1;

-- Differential privacy (noisy aggregates)
SET DP_EPSILON = 0.4;
SELECT DP_COUNT(*) FROM people WHERE credits >= 10;

SAVE DATABASE 'snapshot.ldb';
EXIT;
```

> [!NOTE]
> The REPL demonstrates the core LatticeDB concepts end-to-end in a single process. For distributed mode, use the coordinator + shard binaries (see `/cmd`).

## Start with GUI

LatticeDB includes a modern web-based GUI with powerful features:

### GUI Features
- 🎨 **Dark/Light Mode**: Full theme support with persistent settings
- 📝 **SQL Editor**: Monaco-based editor with syntax highlighting
- 📊 **Results Visualization**: Tabular view with export capabilities
- 🕐 **Query History**: Track and replay previous queries
- ⭐ **Favorites**: Save frequently used queries
- 🗂️ **Schema Browser**: Interactive database schema exploration
- 🎯 **Mock Mode**: Try LatticeDB without running the server

### Running the GUI

```bash
# 1) Build and start the HTTP server:
cmake -S . -B build && cmake --build build -j
./build/latticedb_server

# 2) In a new terminal, start the GUI:
cd gui
npm install
npm run dev

# Open http://localhost:5173
```

### Mock Mode (No Server Required)
The GUI can run standalone with sample data:
```bash
cd gui
npm install
npm run dev
# Toggle "Mock Mode" in the UI to use sample data
```

_How the GUI looks..._

<p align="center">
  <img src="docs/gui.png" alt="LatticeDB GUI Screenshot" width="100%"/>
</p>

<p align="center">
  <img src="docs/gui-dark.png" alt="LatticeDB GUI Dark Mode Screenshot" width="100%"/>
</p>

<p align="center">
  <img src="docs/settings.png" alt="LatticeDB GUI Schema Browser Screenshot" width="100%"/>
</p>

## Core Concepts & Examples

### Mergeable Relational Tables (MRT)

* Per-column merge policies: `lww`, `sum_bounded(min,max)`, `gset`, and **custom WASM resolvers**.
* Ideal for **active-active** replication and **edge/offline** writes.

```mermaid
stateDiagram-v2
    [*] --> LocalWrite: Write Operation
    LocalWrite --> CRDTDelta: Generate Delta
    CRDTDelta --> VectorClock: Update Clock
    VectorClock --> Broadcast: Ship to Peers
    Broadcast --> MergeResolve: Apply Policy
    MergeResolve --> LWW: Last-Writer-Wins
    MergeResolve --> Union: Set Union
    MergeResolve --> Custom: Custom Resolver
    LWW --> Apply: Persist
    Union --> Apply: Persist
    Custom --> Apply: Persist
    Apply --> [*]
```

**Example**

```sql
-- Add a custom WASM resolver for notes (pseudo)
CREATE MERGE RESOLVER rev_note LANGUAGE wasm
AS 'wasm://org.example.merges/resolve_rev_note@1.0';

ALTER TABLE tickets
  ALTER COLUMN note SET MERGE USING rev_note;
```

### Bitemporal Time Travel & Lineage

* Every row carries `tx_from/tx_to` and `valid_from/valid_to`.
* Ask: “What did we believe on Aug 10?” vs “What was valid on Aug 10?”

```mermaid
sequenceDiagram
    participant Q as Query
    participant P as Parser
    participant T as Temporal Rewriter
    participant I as Temporal Index
    participant E as Executor
    Q->>P: FOR SYSTEM_TIME AS OF timestamp
    P->>T: Extract temporal predicates
    T->>I: CSN range lookup
    I-->>E: Pruned row versions
    E-->>Q: Snapshot-consistent results
```

**Example**

```sql
-- Snapshot by transaction time
SELECT * FROM orders FOR SYSTEM_TIME AS OF '2025-08-10T13:37:00Z' WHERE id=42;

-- Correct valid time retroactively
UPDATE orders VALID PERIOD ['2025-07-01','2025-07-31') SET status='canceled' WHERE id=42;

-- Why did it change?
SELECT lineage_explain(orders, 42, '2025-08-10T13:37:00Z');
```

### Policy-as-Data & Differential Privacy

* Declarative policies stored in the catalog; enforced in the planner and executor.
* **RLS/CLS**, masking, retention, and **ε-budgeted** differentially private aggregates.

```mermaid
flowchart TD
    U[User/Service] --> AUTH[Authentication]
    AUTH --> CTX[Security Context]
    CTX --> POL[Policy Engine]
    POL --> RLS[Row-Level Security]
    POL --> CLS[Column-Level Security]
    POL --> DP[Differential Privacy]
    RLS --> PLAN[Query Plan]
    CLS --> PLAN
    DP --> BUDGET[Epsilon Budget]
    BUDGET --> PLAN
    PLAN --> EXEC[Executor]
    EXEC --> AUDIT[Audit Logger]
```

**Example**

```sql
CREATE POLICY ssn_mask
ON people AS COLUMN (ssn)
USING MASK WITH (expr => 'CASE WHEN has_role(''auditor'') THEN ssn ELSE sha2(ssn) END');

CREATE POLICY dp_count_sales
ON sales AS DP USING (epsilon => 0.5, sensitivity => 1);

SET DP_EPSILON = 0.5;
SELECT DP_COUNT(*) FROM sales WHERE region='NA';
```

### Vectors & Semantic Joins

* Built-in `VECTOR<D>` columns and ANN indexes (HNSW/IVF plugins).
* Optimizer uses vector distance prefilters before relational joins.

```sql
CREATE TABLE items(
  id UUID PRIMARY KEY,
  title TEXT,
  embedding VECTOR<768> INDEX HNSW (M=32, ef_search=64)
);

SELECT o.id, i.title
FROM orders o
JOIN ANN items ON distance(o.query_vec, items.embedding) < 0.25
WHERE o.status = 'open';
```

### Streaming Materialized Views

* Exactly-once incremental MVs consuming table CDC or external logs (Kafka/Pulsar).
* Backfill and catch-up integrate with temporal indexes.

```mermaid
flowchart LR
    subgraph Sources
        CDC[Table CDC]
        KAFKA[Kafka Stream]
    end
    subgraph Processing
        DECODE[Decoder]
        WINDOW[Window Assignment]
        AGG[Incremental Aggregation]
    end
    subgraph Output
        STATE[State Store]
        MV[Materialized View]
    end
    CDC --> DECODE
    KAFKA --> DECODE
    DECODE --> WINDOW
    WINDOW --> AGG
    AGG --> STATE
    STATE --> MV
```

**Example**

```sql
CREATE MATERIALIZED VIEW revenue_daily
WITH (refresh='continuous', watermark = INTERVAL '1 minute')
AS
SELECT DATE_TRUNC('day', ts) d, SUM(amount) amt
FROM STREAM OF payments
GROUP BY d;

CALL mv.backfill('revenue_daily', source => 'payments_archive', from => '2025-01-01');
```

## Storage, Transactions & Replication

LatticeDB uses a **Unified Log-Structured Storage (ULS)**:

* Append-friendly **row pages** (OLTP), **columnar projections** (OLAP).
* **Temporal pruning** with min/max and validity intervals.
* B+Tree/ART for point/range, inverted indexes for JSON, ANN for vectors.
* **WAL**, checksums, compression, and envelope encryption.

```mermaid
flowchart LR
    subgraph "Storage Engine"
        WAL[Write-Ahead Log]
        MEM[MemTable]
        L0[L0 SSTables]
        L1[L1 SSTables]
        L2[L2 SSTables]
    end
    subgraph "Indexes"
        BT[B+ Tree]
        VEC[Vector HNSW]
        BLOOM[Bloom Filter]
    end
    WAL --> MEM
    MEM --> L0
    L0 --> L1
    L1 --> L2
    L0 --> BT
    L0 --> VEC
    L0 --> BLOOM
```

**Transactions & Consistency**

* MVCC with serializable option.
* **Deterministic lane** batches high-conflict transactions (Calvin-style).
* **Causal+ snapshots** with bounded staleness hints.

```mermaid
sequenceDiagram
    participant C as Client
    participant P as Parser
    participant O as Optimizer
    participant E as Executor
    participant M as MVCC
    participant S as Storage
    participant A as Audit

    C->>P: SQL Query
    P->>O: AST + Policies
    O->>E: Physical Plan
    E->>M: Begin TX (CSN)
    M->>S: Read/Write at CSN
    S->>S: Apply CRDT Merge
    S-->>M: Result Set
    M-->>E: Versioned Data
    E->>A: Log Access
    E-->>C: COMMIT (txid)
```

## SQL: LatticeSQL Extensions

* `MERGE` policies in column definitions (`MERGE lww`, `MERGE sum_bounded(a,b)`, `MERGE gset`).
* `FOR SYSTEM_TIME AS OF` for time travel; `VALID PERIOD [from,to)`.
* `DP_COUNT(*)` and other DP aggregates (with `SET DP_EPSILON`).
* `VECTOR<D>` with `DISTANCE(vec, [..])` predicates.
* Streaming `STREAM OF` sources in `CREATE MATERIALIZED VIEW`.

> [!NOTE]
> LatticeSQL is a strict superset of a familiar ANSI subset—with new **temporal**, **merge**, **vector**, **DP**, and **streaming** constructs.

## Operations & Observability

* **Resource groups** with workload classes (OLTP/Analytics/Vector/Streaming).
* Admission control, plan shaping, and graceful degradation under pressure.
* End-to-end **tracing**, metrics, and **lineage/audit** explorer.

```mermaid
flowchart TD
  IN[Incoming Queries] --> CLASS[Classifier]
  CLASS -->|OLTP| RG1[Low-Latency]
  CLASS -->|Analytics| RG2[Throughput]
  CLASS -->|Vector| RG3[Cache-Heavy]
  CLASS -->|Streaming| RG4[Deadline]
  RG1 --> ADM[Admission Controller]
  RG2 --> ADM
  RG3 --> ADM
  RG4 --> ADM
  ADM --> RUN[Executors]
```

## Implementation Status

### Core Components
- ✅ **SQL Parser**: Full recursive descent parser with LatticeSQL extensions
- ✅ **Storage Engine**: Page-based disk manager with buffer pool management
- ✅ **Index Structures**: B+ tree implementation with iterator support
- ✅ **Query Processing**: Query planner and executor framework
- ✅ **Type System**: Support for basic types, vectors, sets, and CRDTs
- ✅ **Transaction Management**: MVCC foundation with isolation levels
- ✅ **REPL Interface**: Interactive command-line shell (288 lines)
- ✅ **HTTP Server**: REST API bridge for web clients (118 lines)
- ✅ **Web GUI**: Modern React/TypeScript interface with Monaco editor

### Test Coverage
Our comprehensive test suite includes:
- **Temporal Operations**: Time-travel queries, bitemporal support
- **CRDT Merging**: LWW, G-Set, counters, bounded sums
- **Vector Search**: Distance queries, similarity operations
- **Differential Privacy**: DP aggregates with epsilon budgets
- **Transactions**: ACID properties, isolation levels, savepoints
- **Complex Joins**: All join types, multi-table operations
- **Schema Evolution**: Online DDL, constraints, computed columns
- **Streaming**: Materialized views, windowing functions
- **Security**: Row/column-level security, audit logging

Run tests with:
```bash
cd tests
./run_all.sh
```

## Roadmap

* **Phase 1** ✅: MVCC, ULS storage, temporal/lineage, RLS/CLS, streaming MVs.
* **Phase 2** 🚧: MRT CRDT layer + geo multi-master, DP framework, vector indexes.
* **Phase 3**: TEE policy execution, learned index plugin GA, advanced QoS.

## Limitations

* LatticeDB is young. While the architecture targets production, expect:

  * Smaller SQL surface than full SQL:2016 in early builds (expanding quickly).
  * Query optimizer still learning (hybrid rule/cost + runtime feedback).
  * Streaming integrations (Kafka/Pulsar) and vector plugins mature over time.
* **Benchmarks**: Any numbers are **targets**, not guarantees, and vary by workload/hardware.

## Contributing

We ❤️ contributions! Ways to help:

* Tackle issues labeled `good-first-issue` or `help-wanted`.
* Add merge resolvers and UDFs in WASM.
* Extend the optimizer (cardinality models, join ordering, vector pushdowns).
* Improve docs—especially temporal/lineage tutorials.

**Dev setup**

```bash
git clone https://example.com/latticedb.git
cd latticedb
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j
ctest --test-dir build/debug  # if tests are present
```

Please run `clang-format` (or `scripts/format.sh`) before submitting PRs.

## License

Unless stated otherwise in the repository, LatticeDB is released under the **MIT License**. See `LICENSE` for details.

---

## Appendix: Glossary

* **MRT** — Mergeable Relational Table (CRDT-backed).
* **Bitemporal** — Tracks both **transaction time** (what the DB believed) and **valid time** (what was true in the domain).
* **Causal+** — Causal consistency with convergence guarantees.
* **TEE** — Trusted Execution Environment (SGX/SEV-SNP).

### Bonus: ER Model for Governance & Provenance

```mermaid
erDiagram
    TABLE ||--o{ ROW_VERSION : has
    POLICY ||--o{ POLICY_BINDING : applies_to
    ROW_VERSION }o--|| LINEAGE_EVENT : derived_from
    USER ||--o{ QUERY_SESSION : initiates
    QUERY_SESSION ||--o{ AUDIT_LOG : writes
    TABLE ||--o{ INDEX : has
    TABLE ||--o{ MATERIALIZED_VIEW : source

    TABLE {
        uuid table_id PK
        string name
        json schema_versions
        json merge_policy
        string crdt_type
        interval retention_period
    }
    ROW_VERSION {
        uuid row_id
        uuid table_id FK
        bigint csn_min
        bigint csn_max
        timestamptz valid_from
        timestamptz valid_to
        jsonB data
        jsonB provenance
        vector_clock merge_clock
    }
    POLICY {
        uuid policy_id PK
        string name
        string type "RLS|CLS|DP|MASK"
        json spec
        float epsilon_budget
    }
    INDEX {
        uuid index_id PK
        uuid table_id FK
        string type "BTREE|HNSW|BITMAP|BLOOM"
        json config
    }
    MATERIALIZED_VIEW {
        uuid view_id PK
        string name
        string refresh_type
        interval refresh_interval
    }
```

### Why LatticeDB vs. “Big Three”

LatticeDB **natively** combines **CRDT mergeability**, **bitemporal & lineage**, **policy-as-data with differential privacy**, **streaming MVs**, **vector search**, and **WASM extensibility** into the **core** engine—so you can build **offline-tolerant, audited, privacy-preserving, real-time** apps **without stitching together** sidecars, plugins, and external services.

--- 

Thank you for exploring LatticeDB! We’re excited about the future of databases and would love to hear your feedback and contributions.
