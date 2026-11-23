#!/bin/bash
# Regression test: stream events that arrive while the daemon is still querying
# the previous one must not be lost.
#
# Bursts of very short playback streams make the sound server send "new" and
# "remove" events in quick succession. If the "remove" event arrives while the
# query for the "new" one is still running and gets dropped, the daemon reports
# RUNNING (and, without --dry-print-*, holds the idle inhibitor) after every
# stream is gone, until some unrelated stream event comes along.
#
# Runs against a private sound server with a null sink: PipeWire with
# pipewire-pulse and WirePlumber when installed (the race shows there: about 8%
# of checks on the unfixed loop), otherwise PulseAudio (answers too fast to hit
# the race reliably, so the test is much weaker there). Needs pacat and pactl;
# without a server it is skipped (exit 77).
#
# Usage: lost-stream-events.sh /path/to/sway-audio-idle-inhibit
# Environment: TRIALS (default 40), DAEMONS (default 6), SETTLE (tenths of a
# second each daemon gets to report NOT RUNNING after a burst, default 30),
# SERVER (pipewire or pulseaudio, default: pipewire if installed).

set -u
BIN=${1:?usage: $0 /path/to/sway-audio-idle-inhibit}
TRIALS=${TRIALS:-40}
DAEMONS=${DAEMONS:-6}
SETTLE=${SETTLE:-30}

skip() { echo "SKIP: $*"; exit 77; }
command -v pacat >/dev/null || skip "pacat not installed"
command -v pactl >/dev/null || skip "pactl not installed"
if [ -z "${SERVER:-}" ]; then
	if command -v pipewire-pulse >/dev/null && command -v wireplumber >/dev/null; then
		SERVER=pipewire
	elif command -v pulseaudio >/dev/null; then
		SERVER=pulseaudio
	else
		skip "neither pipewire-pulse nor pulseaudio installed"
	fi
fi

DIR=$(mktemp -d "${TMPDIR:-/tmp}/saii-test.XXXXXX") || exit 1
PIDS=()
cleanup() {
	for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done
	for p in "${PIDS[@]}"; do wait "$p" 2>/dev/null; done
	rm -rf "$DIR"
}
trap cleanup EXIT

# A private server: its own runtime and state dirs, no autospawn.
mkdir -p "$DIR/run/pulse" "$DIR/home" && chmod 700 "$DIR/run" "$DIR/run/pulse"
export XDG_RUNTIME_DIR=$DIR/run HOME=$DIR/home PULSE_RUNTIME_PATH=$DIR/run/pulse
export XDG_CONFIG_HOME=$DIR/home XDG_STATE_HOME=$DIR/home
export PULSE_STATE_PATH=$DIR/home PULSE_CONFIG_PATH=$DIR/home
printf 'autospawn = no\n' >"$DIR/client.conf"
export PULSE_CLIENTCONFIG=$DIR/client.conf
unset PULSE_SERVER PIPEWIRE_REMOTE DBUS_SESSION_BUS_ADDRESS
if [ "$SERVER" = pipewire ]; then
	if command -v dbus-daemon >/dev/null; then
		dbus-daemon --session --address="unix:path=$DIR/run/bus" --nofork --nopidfile >"$DIR/dbus.log" 2>&1 &
		PIDS+=($!)
		export DBUS_SESSION_BUS_ADDRESS=unix:path=$DIR/run/bus
	fi
	pipewire >"$DIR/pipewire.log" 2>&1 &
	PIDS+=($!)
	for _ in $(seq 50); do [ -S "$DIR/run/pipewire-0" ] && break; sleep 0.1; done
	wireplumber >"$DIR/wireplumber.log" 2>&1 &
	PIDS+=($!)
	pipewire-pulse >"$DIR/pipewire-pulse.log" 2>&1 &
	PIDS+=($!)
	export PULSE_SERVER=unix:$DIR/run/pulse/native
else
	pulseaudio -n --daemonize=no --exit-idle-time=-1 --use-pid-file=no \
		--disable-shm=yes --enable-memfd=no --system=no \
		--load="module-native-protocol-unix socket=$DIR/run/pulse/native auth-anonymous=1" \
		--load="module-null-sink sink_name=nul" >"$DIR/pulseaudio.log" 2>&1 &
	PIDS+=($!)
	export PULSE_SERVER=unix:$DIR/run/pulse/native
fi
for _ in $(seq 100); do
	pactl info >/dev/null 2>&1 && break
	sleep 0.1
done
pactl info >/dev/null 2>&1 || { cat "$DIR"/*.log; skip "$SERVER did not start"; }
if [ "$SERVER" = pipewire ]; then
	pactl load-module module-null-sink sink_name=nul >/dev/null || { echo "FAIL: no null sink"; exit 1; }
	for _ in $(seq 50); do pactl list short sinks | grep -qw nul && break; sleep 0.1; done
fi
echo "server: $(pactl info | sed -n 's/^Server Name: //p')"

last() { tail -n 1 "$DIR/d$1.out" 2>/dev/null; }

# Several daemons watch the same server, so each burst is checked several times.
for i in $(seq "$DAEMONS"); do
	"$BIN" --dry-print-both >"$DIR/d$i.out" 2>"$DIR/d$i.err" &
	PIDS+=($!)
done
for i in $(seq "$DAEMONS"); do
	for _ in $(seq 50); do [ "$(last "$i")" = "NOT RUNNING" ] && break; sleep 0.1; done
	if [ "$(last "$i")" != "NOT RUNNING" ]; then
		echo "FAIL: daemon $i did not report NOT RUNNING at start"
		cat "$DIR/d$i.err"
		exit 1
	fi
done

stuck=0
for t in $(seq "$TRIALS"); do
	# 1 to 8 concurrent streams of 10 ms of silence each
	n=$((1 + RANDOM % 8))
	burst=()
	for _ in $(seq "$n"); do
		head -c 882 /dev/zero |
			pacat --playback --raw --format=s16le --rate=44100 --channels=1 -d nul &
		burst+=($!)
	done
	for p in "${burst[@]}"; do wait "$p"; done
	# Every stream is gone now: each daemon must say so within SETTLE.
	for i in $(seq "$DAEMONS"); do
		for _ in $(seq "$SETTLE"); do
			[ "$(last "$i")" = "NOT RUNNING" ] && break
			sleep 0.1
		done
		if [ "$(last "$i")" != "NOT RUNNING" ]; then
			stuck=$((stuck + 1))
			echo "trial $t ($n streams): daemon $i still reports '$(last "$i")'"
		fi
	done
done

leftover=$(pactl list short sink-inputs | wc -l)
echo "$TRIALS bursts x $DAEMONS daemons: $stuck stuck at RUNNING; sink inputs left: $leftover"
[ "$leftover" -eq 0 ] || { echo "FAIL: streams left on the server"; exit 1; }
[ "$stuck" -eq 0 ] || { echo "FAIL: lost stream events"; exit 1; }
echo PASS
