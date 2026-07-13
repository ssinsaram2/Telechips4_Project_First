#!/bin/sh
# start_system.sh — 실행 순서를 자동화한다.
#   1) can0 설정  2) 런타임 정리  3) central_app 기동  4) Qt UI 기동
#
# 사용 전 환경에 맞게 아래 변수를 export 하거나 이 파일 상단을 수정:
#   CENTRAL_APP_PATH  (기본 /opt/central/central_app)
#   UI_APP_PATH       (기본 /opt/central/school_bus_ui)
#   RPI5_AI_HOST, RPI5_AI_CONTROL_PORT(5020), OBJECT_CAMERA_PORT(5010)
#
#   [MOD] Telegram 수신자 분리 — 1차=운전자, 2차=경찰
#   CHILD_ALERT_LEVEL1_CMD="/opt/central/send_child_alert_telegram.sh driver"
#   CHILD_ALERT_LEVEL2_CMD="/opt/central/send_child_alert_telegram.sh police"
#   TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID_DRIVER, TELEGRAM_CHAT_ID_POLICE
#   level2 기본값은 /root/send_child_alert_telegram.sh police가 있으면 이를 우선 사용한다.
#
#   [MOD] 타이머 (실설계값 / 시연 단축)
#   CHILD_ALERT_ESCALATION_SEC     90  (1차→2차, 시연 15~30)
#   CHILD_ACK_COOLDOWN_SEC         90  (확인 후 재감지 유예, 시연 10~15)
#   * Telegram은 운전자 1회 + 경찰 1회만 발송 (반복 없음)
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"

CENTRAL_APP_PATH="${CENTRAL_APP_PATH:-/opt/central/central_app}"
UI_APP_PATH="${UI_APP_PATH:-/opt/central/school_bus_ui}"
LOG_DIR="${LOG_DIR:-/tmp/school_bus_logs}"
mkdir -p "$LOG_DIR"

echo "[start] 1/4 can0 setup"
"$DIR/can_setup.sh"

echo "[start] 2/4 runtime clean"
"$DIR/clean_runtime.sh"

echo "[start] 3/4 central_app"
if [ ! -x "$CENTRAL_APP_PATH" ]; then
    echo "[start] $CENTRAL_APP_PATH 없음 — CENTRAL_APP_PATH를 확인하세요." >&2
    exit 1
fi
if [ -z "${CHILD_ALERT_LEVEL2_CMD:-}" ]; then
    if [ -x /root/send_child_alert_telegram.sh ]; then
        export CHILD_ALERT_LEVEL2_CMD="/root/send_child_alert_telegram.sh police"
    else
        export CHILD_ALERT_LEVEL2_CMD="$DIR/../rpi4_central/scripts/send_child_alert_telegram.sh police"
    fi
fi
nohup "$CENTRAL_APP_PATH" >>"$LOG_DIR/central.log" 2>&1 &
sleep 0.5
if ! pidof central_app >/dev/null; then
    echo "[start] central_app 기동 실패 — $LOG_DIR/central.log 확인" >&2
    exit 1
fi
echo "[start] central_app pid=$(pidof central_app)"

echo "[start] 4/4 Qt UI"
# central은 이미 떠 있으므로 UI의 자동 기동 기능은 끈다.
export UI_START_CENTRAL=0
if [ -x "$UI_APP_PATH" ]; then
    nohup "$UI_APP_PATH" >>"$LOG_DIR/ui.log" 2>&1 &
    sleep 0.5
    echo "[start] school_bus_ui pid=$(pidof school_bus_ui || echo '?')"
else
    echo "[start] UI 바이너리($UI_APP_PATH) 없음 — UI는 수동 실행하세요."
fi

echo "[start] done. logs: $LOG_DIR/{central,ui}.log"
echo "        tail -f $LOG_DIR/central.log"
