#!/bin/sh
set -eu

if [ -z "${TELEGRAM_BOT_TOKEN:-}" ]; then
    echo "[telegram] TELEGRAM_BOT_TOKEN is not set" >&2
    exit 1
fi

if [ -z "${TELEGRAM_CHAT_ID:-}" ]; then
    echo "[telegram] TELEGRAM_CHAT_ID is not set" >&2
    exit 1
fi

TEXT="${CHILD_ALERT_TEXT:-어린이 통학차량 잔류 감지 2차 경고입니다. 즉시 차량을 확인하세요.}"

curl -sS -X POST \
    "https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendMessage" \
    -d "chat_id=${TELEGRAM_CHAT_ID}" \
    --data-urlencode "text=${TEXT}" >/dev/null

echo "[telegram] level-2 child alert sent"
