---
title: FAQ & Troubleshooting
description: Frequently asked questions and performance tuning for MuMu ReSukiSU.
---

::: warning AI Translation Notice
This document was translated by an AI language model. In case of any discrepancies or ambiguities, please refer to the [original Chinese documentation](/docs/faq) as the authoritative reference.
:::

# FAQ & Troubleshooting

Common questions and answers regarding daily usage and integration:

### Q: Why does framebuffer capture report access denied?

**A**: Ensure the driver is launched with appropriate privileges to read emulator shared memory segments, or start MuMu Player first.

### Q: Does it support multiple emulator instances concurrently?

**A**: Yes. Specify distinct `instance_index` values in the configuration to bind independent controller instances.


---

## General Troubleshooting Steps

1. **Verify Runtime Versions**: Ensure Node.js, Python, Rust, or browser versions meet minimum compatibility requirements.
2. **Check Diagnostic Logs**: Review console output and error logs for informative stack traces and line numbers.
3. **Submit Issue Feedback**: If you discover an underlying bug, please open an issue on the GitHub repository with a reproducible test case.
