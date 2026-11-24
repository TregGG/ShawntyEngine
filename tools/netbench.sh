#!/bin/bash
# Network benchmark. Needs Linux user namespaces and tc; runs in its own network namespace, so
# it doesn't affect (or count) other traffic. Build first: make BUILD=release
#   tools/netbench.sh [engine binary]
set -u
BIN=$(realpath "${1:-release/framework}")
if [[ -z ${NETBENCH_INNER:-} ]]; then
    export NETBENCH_INNER=1
    exec unshare -rn "$0" "$BIN"
fi
ip link set lo up
WORK=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$WORK"' EXIT
mkdir -p "$WORK"/p/{scenes,textures,spritesheets,animations,objects,prefabs}
cat > "$WORK/p/scenes/main.scene" <<'J'
{"version":1,"scene":{"entities":[{"editorId":"f","name":"F","components":{"transform":{"position":[0,-3],"size":[4000,1]},"collider":{"autoBounds":true},"rigidbody":{"bodyType":"Static"}}}],"relationships":[]}}
J
cd "$WORK"
start() { "$BIN" --server --net-stats --project-dir "$WORK/p" --port 7777 --reload-port 9191 >/dev/null 2>"$WORK/err" & SERVER=$!; sleep 1; }
stop() { kill -9 "$SERVER" 2>/dev/null; wait 2>/dev/null; }

echo "Traffic (both directions) and server CPU"
for n in 1 8 16 32; do
    start
    for i in $(seq $n); do XDG_CACHE_HOME=$WORK/c$i "$BIN" --probe 127.0.0.1:7777 --seconds 10 >/dev/null 2>&1 & done
    sleep 3
    read -r b0 p0 < <(awk '/lo:/{print $2,$3}' /proc/net/dev); c0=$(awk '{print $14+$15}' /proc/$SERVER/stat)
    sleep 5
    read -r b1 p1 < <(awk '/lo:/{print $2,$3}' /proc/net/dev); c1=$(awk '{print $14+$15}' /proc/$SERVER/stat)
    awk -v n=$n -v b=$((b1-b0)) -v p=$((p1-p0)) -v c=$((c1-c0)) \
        'BEGIN{printf "  %2d players: %7.1f KB/s, %5.1f per player, %5.0f packets/s, CPU %.1f%%\n",n,b/5120,b/5120/n,p/5,c/5}'
    stop
done

echo "Inputs missing when the server needed them"
for netem in "delay 0ms" "delay 20ms 4ms" "delay 40ms 8ms" "delay 60ms 8ms" "delay 80ms 10ms" "delay 120ms 15ms" "delay 20ms 4ms loss 3%" "delay 40ms 8ms loss 10%"; do
    tc qdisc replace dev lo root netem $netem || exit 1
    start
    XDG_CACHE_HOME=$WORK/c "$BIN" --probe 127.0.0.1:7777 --seconds 16 > "$WORK/out" 2>&1
    printf "  each way %-24s %-38s %s\n" "$netem" "$(grep inputs "$WORK/err" | tail -n +3 | awk '{split($2,a,"=");split($3,b,"=");u+=a[2];m+=b[2]}END{printf "used=%d missing=%d (%.1f%%)",u,m,100*m/(u+m+1e-9)}')" "$(grep lead "$WORK/out")"
    stop
done
