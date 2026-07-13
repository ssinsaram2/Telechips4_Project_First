#!/bin/sh
# stop_system.sh — UI/central 프로세스를 정상 종료(SIGTERM)시키고,
# 3초 내 종료하지 않으면 SIGKILL 한다. IPC 정리는 하지 않는다
# (정상 종료 시 central_app이 스스로 mq_unlink + PID 파일 삭제).
set -u

stop_proc() {
    NAME="$1"
    PIDS="$(pidof "$NAME" 2>/dev/null || true)"
    if [ -z "$PIDS" ]; then
        echo "[stop] $NAME not running"
        return 0
    fi
    echo "[stop] SIGTERM -> $NAME ($PIDS)"
    kill $PIDS 2>/dev/null || true
    for i in 1 2 3 4 5 6; do
        sleep 0.5
        PIDS="$(pidof "$NAME" 2>/dev/null || true)"
        [ -z "$PIDS" ] && { echo "[stop] $NAME exited"; return 0; }
    done
    echo "[stop] SIGKILL -> $NAME ($PIDS)"
    kill -9 $PIDS 2>/dev/null || true
}

# UI를 먼저 내리고 central을 내린다 (UI가 central을 자동 재기동하지 않도록).
stop_proc school_bus_ui
stop_proc central_app
