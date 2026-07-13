#!/bin/sh
# reset_ipc.sh — 잔여 POSIX 메시지큐를 강제 삭제한다.
# central_app이 비정상 종료(kill -9, 전원 차단)하면 /dev/mqueue에 큐가 남아
# 다음 실행에서 오래된 메시지가 재생되는 문제를 막는다.
# 주의: central_app / school_bus_ui가 떠 있는 상태에서 실행하면
#       그 프로세스들은 고아 큐를 계속 쥐게 되므로, 반드시 stop_system.sh 이후 실행.
set -u

Q1="${CENTRAL_TO_QT_QUEUE:-/central_to_qt}"
Q2="${QT_TO_CENTRAL_QUEUE:-/qt_to_central}"

MQ_DIR=/dev/mqueue
if [ ! -d "$MQ_DIR" ]; then
    mkdir -p "$MQ_DIR" 2>/dev/null || true
    mount -t mqueue none "$MQ_DIR" 2>/dev/null || true
fi

for q in "$Q1" "$Q2"; do
    name="${q#/}"
    if [ -e "$MQ_DIR/$name" ]; then
        rm -f "$MQ_DIR/$name" && echo "[reset_ipc] removed $q"
    else
        echo "[reset_ipc] $q not present (ok)"
    fi
done
