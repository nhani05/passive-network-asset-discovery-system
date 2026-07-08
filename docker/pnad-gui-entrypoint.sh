#!/bin/sh
set -eu

prepare_runtime_dirs() {
    mkdir -p /data /out /work/logs
    chown -R asset:asset /data /out /work/logs 2>/dev/null || true
}

restore_runtime_dir_ownership() {
    chown -R asset:asset /data /out /work/logs 2>/dev/null || true
}

run_target() {
    target="${PNAD_ENTRYPOINT_TARGET:-/usr/local/bin/asset-discovery-gui}"
    export HOME=/home/asset

    if [ "${PNAD_ENABLE_LIVE_CAPTURE:-0}" = "1" ]; then
        exec setpriv \
            --reuid=asset \
            --regid=asset \
            --init-groups \
            --inh-caps +net_raw,+net_admin \
            --ambient-caps +net_raw,+net_admin \
            "$target" "$@"
    fi

    exec setpriv --reuid=asset --regid=asset --init-groups "$target" "$@"
}

if [ "$(id -u)" = "0" ]; then
    prepare_runtime_dirs
    run_target "$@"
fi

target="${PNAD_ENTRYPOINT_TARGET:-/usr/local/bin/asset-discovery-gui}"
exec "$target" "$@"
