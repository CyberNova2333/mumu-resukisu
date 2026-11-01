#!/system/bin/sh
set -C
umask 077
[ ! -e /data/adb/resukisu-migration-test/service-ran ] && [ ! -L /data/adb/resukisu-migration-test/service-ran ] || exit 1
IFS= read -r boot_id < /proc/sys/kernel/random/boot_id || exit 1
printf '%s\n' "$boot_id" > /data/adb/resukisu-migration-test/service-ran
