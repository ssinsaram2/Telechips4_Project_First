#!/bin/sh
# can_setup.sh — can0 인터페이스를 500 kbit/s로 (재)설정한다.
# RTOS/F/W CAN 타이밍(APB1 42MHz / Prescaler 6 / BS1 11 / BS2 2)과 일치하는
# 500000 bps가 기준값이다. 필요 시 CAN_BITRATE 환경변수로 변경.
set -eu

CAN_IF="${CAN_INTERFACE:-can0}"
BITRATE="${CAN_BITRATE:-500000}"

if ! ip link show "$CAN_IF" >/dev/null 2>&1; then
    echo "[can_setup] $CAN_IF 인터페이스가 없습니다. MCP2515 오버레이/배선을 확인하세요." >&2
    exit 1
fi

# 이미 UP이면 내렸다가 다시 올려 비트레이트를 확정한다 (설정 꼬임 방지).
ip link set "$CAN_IF" down 2>/dev/null || true
ip link set "$CAN_IF" type can bitrate "$BITRATE" restart-ms 100
ip link set "$CAN_IF" up
ip link set "$CAN_IF" txqueuelen 1000

echo "[can_setup] $CAN_IF up @ ${BITRATE}bps"
ip -details link show "$CAN_IF" | sed -n '1,3p'
