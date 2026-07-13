#!/bin/sh
# send_child_alert_telegram.sh — 잔류 아동 감지 Telegram 발송
#
# [MOD] 수신자(운전자/경찰)와 경고 단계(1차/2차)를 인자로 분리했다.
#
# 사용법:
#   send_child_alert_telegram.sh driver    # 1차 경고 → 운전자
#   send_child_alert_telegram.sh police    # 2차 경고 → 경찰
#
# 설정 파일:
#   driver: telegram.txt  (GROUP_ID/API_TOKEN)
#   police: police.txt    (CHAT_ID/API_TOKEN)
#
# 환경변수도 계속 지원한다:
#   TELEGRAM_BOT_TOKEN         공통 봇 토큰
#   TELEGRAM_CHAT_ID_DRIVER    운전자 chat_id (1차)
#   TELEGRAM_CHAT_ID_POLICE    경찰   chat_id (2차)
#   TELEGRAM_CHAT_ID           (fallback) 위 두 개가 없을 때 공용으로 사용
set -eu

ROLE="${1:-driver}"
SCRIPT_DIR="$(CDPATH= cd "$(dirname "$0")" && pwd)"
CONFIG_API_TOKEN=""
CONFIG_CHAT_ID=""

strip_quotes() {
    value="$1"
    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    case "$value" in
        \"*\") value="${value#\"}"; value="${value%\"}" ;;
        \'*\') value="${value#\'}"; value="${value%\'}" ;;
    esac
    printf '%s' "$value"
}

read_config_value() {
    key="$1"
    file="$2"
    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            ''|'#'*) continue ;;
        esac
        name="${line%%=*}"
        value="${line#*=}"
        name="$(strip_quotes "$name")"
        if [ "$name" = "$key" ]; then
            strip_quotes "$value"
            return 0
        fi
    done < "$file"
    return 1
}

load_config() {
    role="$1"
    case "$role" in
        police)
            configured="${POLICE_TELEGRAM_CONFIG:-}"
            default_name="police.txt"
            ;;
        *)
            configured="${TELEGRAM_CONFIG:-}"
            default_name="telegram.txt"
            ;;
    esac

    for path in \
        "$configured" \
        "./$default_name" \
        "$SCRIPT_DIR/$default_name" \
        "$SCRIPT_DIR/../$default_name" \
        "$SCRIPT_DIR/../../$default_name" \
        "/root/$default_name" \
        "/opt/central/$default_name"
    do
        [ -n "$path" ] || continue
        [ -r "$path" ] || continue

        CONFIG_API_TOKEN="$(read_config_value API_TOKEN "$path" || true)"
        CONFIG_CHAT_ID="$(read_config_value CHAT_ID "$path" || true)"
        if [ -z "$CONFIG_CHAT_ID" ]; then
            CONFIG_CHAT_ID="$(read_config_value GROUP_ID "$path" || true)"
        fi

        if [ -n "$CONFIG_API_TOKEN" ] && [ -n "$CONFIG_CHAT_ID" ]; then
            echo "[telegram] config loaded from $path"
            return 0
        fi
    done

    return 1
}

load_config "$ROLE" || true

case "$ROLE" in
    driver)
        CHAT_ID="${TELEGRAM_CHAT_ID_DRIVER:-${TELEGRAM_CHAT_ID:-${CONFIG_CHAT_ID:-}}}"
        TEXT="${CHILD_ALERT_TEXT_DRIVER:-[1차 경고] 어린이 통학차량 내부에 잔류 아동이 감지되었습니다. 즉시 차량을 확인하세요.}"
        ;;
    police)
        CHAT_ID="${TELEGRAM_CHAT_ID_POLICE:-${TELEGRAM_CHAT_ID:-${CONFIG_CHAT_ID:-}}}"
        TEXT="${CHILD_ALERT_TEXT_POLICE:-아동용 승하차 차량에 잔류 아동이 감지되었습니다! 해당 차량의 위치로 출동하십시오!}"
        ;;
    *)
        echo "[telegram] unknown role: $ROLE (use 'driver' or 'police')" >&2
        exit 1
        ;;
esac

BOT_TOKEN="${TELEGRAM_BOT_TOKEN:-${CONFIG_API_TOKEN:-}}"

if [ -z "$BOT_TOKEN" ]; then
    echo "[telegram] API_TOKEN is not set and config file is missing" >&2
    exit 1
fi

if [ -z "$CHAT_ID" ]; then
    echo "[telegram] chat id for role '$ROLE' is not set" >&2
    exit 1
fi

curl -sS -X POST \
    "https://api.telegram.org/bot${BOT_TOKEN}/sendMessage" \
    -d "chat_id=${CHAT_ID}" \
    --data-urlencode "text=${TEXT}" >/dev/null

echo "[telegram] child alert sent (role=$ROLE)"
