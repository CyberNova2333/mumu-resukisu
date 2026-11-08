---
title: Configuration Guide
description: Driver connection settings, shared memory configurations, and capture resolutions.
---

::: warning AI Translation Notice
This document was translated by an AI language model. In case of any discrepancies or ambiguities, please refer to the [original Chinese documentation](/docs/configuration) as the authoritative reference.
:::

# Configuration Guide

### Primary Configuration

```json
{
  "emulator": {
    "install_path": "C:/Program Files/Netease/MuMuPlayer-12.0",
    "instance_index": 0
  },
  "capture": {
    "mode": "shared_memory",
    "fps_limit": 60,
    "pixel_format": "RGBA"
  }
}
```

---

## Production Recommendations

1. **Environment Variable Protection**: Never commit real API keys or sensitive credentials into Git repositories.
2. **Graceful Shutdown & Timeouts**: Set appropriate timeout constraints on long-running processes to prevent orphan executions.
3. **Continuous Monitoring**: Combine console logs and health telemetry to monitor service uptime and operational status.
