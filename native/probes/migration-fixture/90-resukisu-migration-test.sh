#!/system/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# This file is part of MuMu ReSukiSU and is licensed under GNU AGPLv3 with Additional Terms (see LICENSE and ADDITIONAL_TERMS.zh-CN.md; ref: ADDITIONAL_TERMS.en.md).
set -C
umask 077
[ ! -e /data/adb/resukisu-migration-test/service-ran ] && [ ! -L /data/adb/resukisu-migration-test/service-ran ] || exit 1
IFS= read -r boot_id < /proc/sys/kernel/random/boot_id || exit 1
printf '%s\n' "$boot_id" > /data/adb/resukisu-migration-test/service-ran
