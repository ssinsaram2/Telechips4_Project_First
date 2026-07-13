#!/bin/sh
# send_child_alert_telegram.sh — 잔류 아동 감지 Telegram 발송
#
# [MOD] 수신자(운전자/경찰)와 경고 단계(1차/2차)를 인자로 분리했다.
#
# 사용법:
#   send_child_alert_telegram.sh driver    # 1차 경고 → 운전자
#   send_child_alert_telegram.sh police    # 2차 경고 → 경찰
#
# 필요 환경변수:
#   TELEGRAM_BOT_TOKEN         공통 봇 토큰
#   TELEGRAM_CHAT_ID_DRIVER    운전자 chat_id (1차)
#   TELEGRAM_CHAT_ID_POLICE    경찰   chat_id (2차)
#   TELEGRAM_CHAT_ID           (fallback) 위 두 개가 없을 때 공용으로 사용
set -eu

ROLE="${1:-driver}"

if [ -z "${TELEGRAM_BOT_TOKEN:-}" ]; then
    echo "[telegram] TELEGRAM_BOT_TOKEN is not set" >&2
    exit 1
fi

case "$ROLE" in
    driver)
        CHAT_ID="${TELEGRAM_CHAT_ID_DRIVER:-${TELEGRAM_CHAT_ID:-}}"
        TEXT="${CHILD_ALERT_TEXT_DRIVER:-[1차 경고] 어린이 통학차량 내부에 잔류 아동이 감지되었습니다. 즉시 차량을 확인하세요.}"
        ;;
    police)
        CHAT_ID="${TELEGRAM_CHAT_ID_POLICE:-${TELEGRAM_CHAT_ID:-}}"
        TEXT="${CHILD_ALERT_TEXT_POLICE:-[2차 경고/긴급] 어린이 통학차량 잔류 아동이 90초간 미조치 상태입니다. 즉시 출동 바랍니다.}"
        ;;
    *)
        echo "[telegram] unknown role: $ROLE (use 'driver' or 'police')" >&2
        exit 1
        ;;
esac

if [ -z "$CHAT_ID" ]; then
    echo "[telegram] chat id for role '$ROLE' is not set" >&2
    exit 1
fi

curl -sS -X POST \
    "https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendMessage" \
    -d "chat_id=${CHAT_ID}" \
    --data-urlencode "text=${TEXT}" >/dev/null

echo "[telegram] child alert sent (role=$ROLE)"
