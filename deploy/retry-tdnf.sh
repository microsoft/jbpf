#!/bin/sh

set -u

max_attempts="${TDNF_MAX_ATTEMPTS:-5}"
attempt=1

while :; do
    tdnf "$@"
    status=$?
    if [ "$status" -eq 0 ]; then
        exit 0
    fi
    if [ "$attempt" -ge "$max_attempts" ]; then
        exit "$status"
    fi

    echo "tdnf failed (attempt $attempt/$max_attempts); refreshing metadata and retrying"
    tdnf makecache --refresh || true
    sleep $((attempt * 5))
    attempt=$((attempt + 1))
done