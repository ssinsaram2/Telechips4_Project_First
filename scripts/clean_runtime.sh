#!/bin/sh
# clean_runtime.sh — 테스트 시작 전 런타임 완전 초기화.
#   1) 프로세스 종료 (stop_system.sh)
#   2) 잔여 메시지큐 삭제 (reset_ipc.sh)
#   3) 잔여 PID 파일 삭제
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/stop_system.sh"
"$DIR/reset_ipc.sh"

PIDFILE="${CENTRAL_PID_FILE:-/tmp/central_app.pid}"
if [ -e "$PIDFILE" ]; then
    rm -f "$PIDFILE" && echo "[clean] removed $PIDFILE"
fi

echo "[clean] runtime clean complete"
