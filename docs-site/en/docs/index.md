---
title: Quick Start
description: Drive MuMu emulator instances programmatically with microsecond touch injection and instant screen acquisition.
---

::: warning AI Translation Notice
This document was translated by an AI language model. In case of any discrepancies or ambiguities, please refer to the [original Chinese documentation](/docs/) as the authoritative reference.
:::

# Quick Start

Drive MuMu emulator instances programmatically with microsecond touch injection and instant screen acquisition.

## Core Problems Solved

- **Pain Point 1**: Traditional ADB screencap is far too slow (200ms+) for real-time vision pipelines.
- **Pain Point 2**: ADB input keyevent / tap induces noticeable latency and lacks true multi-touch fidelity.
- **Pain Point 3**: Frequent ADB daemon disconnects during heavy automated batch runs.

---

## Prerequisites

Windows 10/11 x64, MuMu Player 12+, Rust 1.80+ or Visual C++ runtime, pnpm 12.6.0.

---

## Quick Installation & Launch

```bash
# 1. Install project dependencies
pnpm install

# 2. Run local tests and verification
pnpm test
```

---

## Next Steps

- Explore internal mechanics: [Architecture & Design](/en/docs/architecture)
- Comprehensive configuration reference: [Configuration Guide](/en/docs/configuration)
- Troubleshooting & FAQ: [FAQ & Diagnostics](/en/docs/faq)
