#!/bin/sh
set -eu

binary=$1
config=$(mktemp -d)
first=
cleanup()
{
    [ -z "$first" ] || kill "$first" 2>/dev/null || true
    [ -z "$first" ] || wait "$first" 2>/dev/null || true
    rm -rf "$config"
}
trap cleanup EXIT

export QT_QPA_PLATFORM=offscreen XDG_CONFIG_HOME="$config"
"$binary" --autostart >"$config/first.log" 2>&1 &
first=$!

ready=false
for _ in $(seq 1 100); do
    kill -0 "$first"
    if dbus-send --session --print-reply --dest=org.freedesktop.DBus \
        /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner \
        string:com.brightless.Application 2>/dev/null | grep -q 'boolean true'; then
        ready=true
        break
    fi
    sleep 0.05
done
$ready
timeout 20 "$binary" --autostart
timeout 20 "$binary"
kill -0 "$first"
