#include "ipc_contract.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <mqueue.h>
#include <net/if.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CAN_ID_POWER_BUTTON_INPUT       0x012U
#define CAN_ID_PIR_DETECTED             0x022U
#define CAN_ID_SEATBELT_STATUS          0x032U
#define CAN_ID_ENV_SENSOR_VALUE         0x041U
#define CAN_ID_DUST_SENSOR_VALUE        0x051U
#define CAN_ID_FW_STATUS                0x061U
#define CAN_ID_WINDOW_CONTROL           0x063U
#define CAN_ID_TEMPERATURE_TARGET_VALUE 0x073U
#define CAN_ID_IGNITION_STATE           0x083U
#define CAN_ID_SEAT_TARGET_VALUE        0x093U
#define CAN_ID_EXT_LED_CONTROL          0x0A3U
#define CAN_ID_INTERIOR_LIGHT_CONTROL   0x0B3U
#define CAN_ID_BUZZER_CONTROL           0x0C3U
#define CAN_ID_OBJECT_DETECTED          0x0D3U

#define WAIT_IGNITION_DEFAULT_TIMEOUT_SEC 30U

#define CAN_VALUE_OFF 0U
#define CAN_VALUE_ON  1U

#define IGNITION_OFF 0U
#define IGNITION_DISPLAY 1U
#define IGNITION_ON 2U

/*
 * [MOD] 0x083 data[1] 운행 상태 정의 — RPi4/RTOS/F/W 3보드 공통 규약 (canonical).
 *   0 = OP_PARKING  : 시동 OFF / 주차 (잔류 감시 대기 포함)
 *   1 = OP_RUNNING  : 운행 중 (OPERATIONAL)
 *   2 = OP_ALARM    : 잔류 감지 경보 활성 (RPi4가 확정한 경우에만 송신)
 *   3 = (reserved)
 *   4 = OP_BOARDING : 승하차 중 (PASSENGER_TRANSFER)
 * F/W fw_common.h의 OperationState_t, RTOS main.c의 op 분기와 반드시 일치해야 한다.
 */
#define OP_PARKING  0U
#define OP_RUNNING  1U
#define OP_ALARM    2U
#define OP_BOARDING 4U

/*
 * [MOD] 30초 Telegram 에스컬레이션 상수 (요구사항 18-14).
 * 테스트 시 환경변수 CHILD_ALERT_ESCALATION_SEC로 짧게(예: 5) 조절 가능.
 */
#define CHILD_ALERT_DEFAULT_ESCALATION_SEC 30U
/*
 * [MOD-이력] 무감지 20분 자동 종료 로직은 폐기됨 (2026-07-13).
 * 감시 세션 종료는 central_tick()의 내부 타이머가 아니라
 * enter_driver_auth() / enter_operational_from_wait() —
 * 즉 "다음 운행 시작"에서만 발생한다 (Euro NCAP CPD §3.3.6.1.3과 동일 경계).
 * 이유: 통학버스는 정차 시간이 길고, 정확히 무감지 구간 이후에 아동이
 * 스스로 탑승하는 경우(CPD Scenario 3, §8.5.3)를 놓치면 안 되기 때문.
 */
/* [MOD] 경고 해제 직후 재감지 무시 시간 — PIR이 계속 true여도 이 시간 동안은
 * 새 경고를 latch하지 않는다 (요구사항 9-12 권장 정책). */
/*
 * [MOD] 해제 버튼 = "차량 확인 완료" (2026-07-12 최종 확정).
 * 근거: Euro NCAP CPD Protocol v1.2 §4.3.8 — 에스컬레이션 경고 취소 후
 * 90초 뒤 재확인하여, 아동이 여전히 감지되고 차량이 잠겨 있으면 문이
 * 열리거나 개입이 시작될 때까지 새 경고 사이클을 반복해야 한다.
 * 즉 경고 해제 ≠ 감지 기능 OFF. 해제는 현재 경고 사건의 처리 완료만
 * 의미하며 감시는 유지되고, 유예(재확인 주기) 후 감지가 지속/재발하면
 * 새 1차 경고부터 다시 시작한다.
 * 실설계값 90초(§4.3.8), 시연 기본 30초 — 타이머 상수 치환 원칙.
 * 환경변수 CHILD_ACK_COOLDOWN_SEC로 조절 (시연 단축 시 10 권장).
 * (센서 안정화 대기 조건은 채택하지 않음 — 아동이 계속 감지되는 최악
 *  케이스에서 영구 침묵하는 결함이 있고 §4.3.8에도 반함)
 */
#define CHILD_ACK_COOLDOWN_DEFAULT_SEC   30U
/* [MOD] can0가 늦게 올라온 경우 소켓 재시도 주기 */
#define CAN_RETRY_INTERVAL_SEC             2U
/*
 * [MOD] 잔류 감지 모드 F/W 제어 기준값 (로그/판단 표시용).
 * 실제 팬/LED 실행 판단은 F/W(fw_control.c)가 동일 기준으로 수행한다:
 *   FAN_PARK_TEMP_L1/L2/L3 = 25.0/28.0/30.0°C, LIGHT_THRESHOLD = 700.0 lux
 * 값을 바꿀 때는 F/W fw_common.h / fw_control.c와 함께 바꿀 것.
 */
#define ROA_FAN_TEMP_THRESHOLD_X10   250   /* 25.0°C 이상이면 팬 가동 시작 */
#define ROA_LED_LUX_THRESHOLD_X10   7000   /* 700.0 lux 미만이면 내부 LED ON */
#define CHILD_SENSOR_PIR 1U
#define CHILD_SENSOR_CAMERA 2U
#define ACTIVE_SEAT_MASK 0x03U
#define MAIN_LOOP_SLEEP_US 10000U
#define MAX_CAN_FRAMES_PER_LOOP 32
#define MAX_OBJECT_CAMERA_PER_LOOP 16

typedef enum
{
    STATE_IGNITION_OFF = 0,
    STATE_DRIVER_AUTH,
    STATE_WAIT_IGNITION,
    STATE_OPERATIONAL,
    STATE_PASSENGER_TRANSFER
} SystemState;

typedef struct
{
    uint32_t driverId;
    uint8_t seatPosition;
    uint8_t temperatureTarget;
} DriverProfile;

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} CanMessage;

static volatile sig_atomic_t g_running = 1;
static int g_canSocket = -1;
static int g_objectCameraSocket = -1;
static mqd_t g_mqCentralToQt = (mqd_t)-1;
static mqd_t g_mqQtToCentral = (mqd_t)-1;

static SystemState g_state = STATE_IGNITION_OFF;
static DriverProfile g_profile = {1U, 40U, 25U};
static bool g_hasProfile = false;

static uint32_t g_currentSeatbeltBitmap = 0U;
static uint32_t g_referenceSeatbeltBitmap = 0U;
static bool g_seatbeltAlarmActive = false;

static bool g_childMonitoringActive = false;
/* (g_childMonitoringStartedSec / g_childMonitoringDurationSec 제거됨 —
 * 감시는 다음 운행 시작까지 계속되며 내부 타이머로 종료되지 않는다) */
static bool g_childAlarmLatched = false;
static bool g_childLevel2Sent = false;
static uint64_t g_childAlarmStartedSec = 0U;
static uint32_t g_childAlertEscalationSec = CHILD_ALERT_DEFAULT_ESCALATION_SEC;

/*
 * [MOD-이력] g_childEverEscalated(재감지 시 2차부터 재개) 제거됨.
 * 확정 정책: 확인 버튼 → ACK 쿨다운 → 재감지 시 항상 "1차 경고부터" 재시작.
 * (1차 = 운전자 Telegram, 2차 = 경찰 Telegram)
 */

/*
 * [MOD-이력] 2차 경고 반복 발송(1분마다 20분간, CPD §4.3.4.2) 제거됨.
 * 확정 정책: 운전자 1회(1차) + 경찰 1회(2차) 발송으로 끝낸다.
 * 경고 장치(부저/외부LED/창문/팬/LED)는 확인 버튼 전까지 계속 유지되므로
 * "경고 지속" 요구 자체는 만족하고, Telegram 메시지만 1회씩 보낸다.
 */

static bool g_pirDetected = false;
static bool g_cameraDetected = false;

/* [MOD] 잔류 감지 재무장(re-arm) 억제: 해제 직후 잠깐은 새 경고를 latch하지 않음 */
/* [MOD] ACK cooldown: 확인 완료 후 이 시각까지는 새 경고를 latch하지 않는다.
 * 감시(monitoring)는 계속 살아 있으며, 유예 만료 후 감지가 지속/재발하면
 * 새 1차 경고가 발생한다. */
static uint32_t g_childAckCooldownSec = CHILD_ACK_COOLDOWN_DEFAULT_SEC;
static uint64_t g_childAckCooldownUntilSec = 0U;

/* [MOD] 마지막으로 수신한 환경 센서값 캐시 (0x041) — 잔류 경보 시 판단 로그용 */
static int16_t  g_lastTempX10 = 0;
static uint16_t g_lastLuxX10 = 0U;
static uint8_t  g_lastEnvValid = 0U;

/* [MOD] can0가 늦게 올라오는 경우를 위한 재시도 타임스탬프 */
static uint64_t g_canLastInitTrySec = 0U;

/* [MOD] 중복 실행 방지 PID 파일 */
#define CENTRAL_PID_FILE_DEFAULT "/tmp/central_app.pid"
static int g_pidFileFd = -1;

/* [MOD] CAN 원시 프레임 디버그 로그 (CAN_DEBUG_LOG=1 일 때) */
static bool g_canDebugLog = false;


static const char *queue_name(const char *envName, const char *defaultName)
{
    const char *value = getenv(envName);
    if (value == NULL || value[0] == '\0')
    {
        return defaultName;
    }
    return value;
}

static uint64_t monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0U;
    }
    return (uint64_t)ts.tv_sec;
}

static uint64_t monotonic_milliseconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0U;
    }

    return ((uint64_t)ts.tv_sec * 1000U) + ((uint64_t)ts.tv_nsec / 1000000U);
}



static uint32_t env_u32(const char *name, uint32_t defaultValue)
{
    const char *value = getenv(name);
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0')
    {
        return defaultValue;
    }

    parsed = strtoul(value, &end, 10);
    if (end == value || parsed == 0UL || parsed > 86400UL)
    {
        fprintf(stderr, "[central] invalid %s=%s, using %u\n", name, value, defaultValue);
        return defaultValue;
    }
    return (uint32_t)parsed;
}

static void handle_signal(int signum)
{
    (void)signum;
    g_running = 0;
}

/* [MOD] 상태 이름 로그 헬퍼 (요구사항 13 테스트 로그) */
static const char *state_name(SystemState s)
{
    switch (s)
    {
        case STATE_IGNITION_OFF:       return "IGNITION_OFF";
        case STATE_DRIVER_AUTH:        return "DRIVER_AUTH";
        case STATE_WAIT_IGNITION:      return "WAIT_IGNITION";
        case STATE_OPERATIONAL:        return "OPERATIONAL";
        case STATE_PASSENGER_TRANSFER: return "PASSENGER_TRANSFER";
        default:                       return "UNKNOWN";
    }
}

/*
 * [MOD] 중복 실행 방지: PID 파일을 O_EXCL로 생성한다.
 * 이미 존재하면 기록된 PID가 살아 있는지 kill(pid, 0)로 확인하고,
 * 죽은 프로세스의 잔여 파일이면 지우고 다시 시도한다 (stale pid file 처리).
 */
static bool pid_file_acquire(void)
{
    const char *path = getenv("CENTRAL_PID_FILE");
    char buf[32];
    int len;

    if (path == NULL || path[0] == '\0')
    {
        path = CENTRAL_PID_FILE_DEFAULT;
    }

    for (int attempt = 0; attempt < 2; attempt++)
    {
        g_pidFileFd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (g_pidFileFd >= 0)
        {
            len = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
            if (len > 0)
            {
                (void)!write(g_pidFileFd, buf, (size_t)len);
            }
            return true;
        }

        if (errno != EEXIST)
        {
            perror("[central] pid file open");
            return true; /* 파일 시스템 문제로 실행 자체를 막지는 않는다 */
        }

        /* 기존 파일이 있음 → PID가 살아 있는지 확인 */
        {
            FILE *fp = fopen(path, "r");
            long oldPid = 0;
            if (fp != NULL)
            {
                if (fscanf(fp, "%ld", &oldPid) != 1)
                {
                    oldPid = 0;
                }
                fclose(fp);
            }

            if (oldPid > 0 && kill((pid_t)oldPid, 0) == 0)
            {
                fprintf(stderr,
                        "[central] already running (pid=%ld, pidfile=%s). "
                        "stop_system.sh로 종료 후 재실행하세요.\n",
                        oldPid, path);
                return false;
            }

            /* stale pid file → 삭제 후 재시도 */
            printf("[central] removing stale pid file %s (pid=%ld dead)\n", path, oldPid);
            unlink(path);
        }
    }

    return true;
}

static void pid_file_release(void)
{
    const char *path = getenv("CENTRAL_PID_FILE");
    if (path == NULL || path[0] == '\0')
    {
        path = CENTRAL_PID_FILE_DEFAULT;
    }
    if (g_pidFileFd >= 0)
    {
        close(g_pidFileFd);
        g_pidFileFd = -1;
        unlink(path);
    }
}

static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static bool ipc_init(void)
{
    struct mq_attr attr;
    const char *centralToQtName = queue_name(IPC_CENTRAL_TO_QT_QUEUE_ENV,
                                             IPC_CENTRAL_TO_QT_QUEUE_DEFAULT);
    const char *qtToCentralName = queue_name(IPC_QT_TO_CENTRAL_QUEUE_ENV,
                                             IPC_QT_TO_CENTRAL_QUEUE_DEFAULT);

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = IPC_QUEUE_MAX_MESSAGES_DEFAULT;
    attr.mq_msgsize = sizeof(IpcMessage);

    g_mqCentralToQt = mq_open(centralToQtName, O_CREAT | O_RDWR | O_NONBLOCK, 0644, &attr);
    if (g_mqCentralToQt == (mqd_t)-1)
    {
        perror("[central] mq_open central_to_qt");
        return false;
    }

    g_mqQtToCentral = mq_open(qtToCentralName, O_CREAT | O_RDWR | O_NONBLOCK, 0644, &attr);
    if (g_mqQtToCentral == (mqd_t)-1)
    {
        perror("[central] mq_open qt_to_central");
        return false;
    }

    /*
     * [MOD] 시작 시 잔여 메시지 배출(drain).
     * 이전 실행이 비정상 종료되면 mqueue 커널 객체에 옛 메시지가 남아
     * 다음 실행의 초기 상태를 오염시킨다 (예: 부팅하자마자 오래된 버튼 이벤트 처리).
     * unlink 대신 drain을 쓰는 이유: UI가 이미 큐 디스크립터를 잡고 있는 상태에서
     * unlink+재생성하면 UI는 고아가 된 옛 큐를 계속 보게 되어
     * "보내는데 화면이 안 바뀌는" 문제가 재발한다.
     */
    {
        IpcMessage drainMsg;
        int drained = 0;
        while (mq_receive(g_mqQtToCentral, (char *)&drainMsg, sizeof(drainMsg), NULL) > 0)
        {
            drained++;
        }
        while (mq_receive(g_mqCentralToQt, (char *)&drainMsg, sizeof(drainMsg), NULL) > 0)
        {
            drained++;
        }
        if (drained > 0)
        {
            printf("[central] drained %d stale IPC message(s)\n", drained);
        }
    }

    printf("[central] IPC ready: %s / %s\n", centralToQtName, qtToCentralName);
    return true;
}

static void ipc_close(void)
{
    if (g_mqCentralToQt != (mqd_t)-1)
    {
        mq_close(g_mqCentralToQt);
        g_mqCentralToQt = (mqd_t)-1;
    }
    if (g_mqQtToCentral != (mqd_t)-1)
    {
        mq_close(g_mqQtToCentral);
        g_mqQtToCentral = (mqd_t)-1;
    }

    /*
     * [MOD] 정상 종료 시 큐 소유자(central)가 unlink까지 수행한다.
     * CENTRAL_KEEP_QUEUES=1이면 unlink를 생략 (UI를 계속 띄워둔 채
     * central만 재시작하는 개발 상황용).
     */
    {
        const char *keep = getenv("CENTRAL_KEEP_QUEUES");
        if (keep == NULL || strcmp(keep, "1") != 0)
        {
            mq_unlink(queue_name(IPC_CENTRAL_TO_QT_QUEUE_ENV, IPC_CENTRAL_TO_QT_QUEUE_DEFAULT));
            mq_unlink(queue_name(IPC_QT_TO_CENTRAL_QUEUE_ENV, IPC_QT_TO_CENTRAL_QUEUE_DEFAULT));
            printf("[central] IPC queues unlinked\n");
        }
    }
}

static void send_ui(IpcMessageType type, int32_t d0, int32_t d1, int32_t d2, int32_t d3)
{
    IpcMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = (int32_t)type;
    msg.data[0] = d0;
    msg.data[1] = d1;
    msg.data[2] = d2;
    msg.data[3] = d3;


    if (g_mqCentralToQt == (mqd_t)-1)
    {
        printf("[central] send UI failed: queue not open type=%d data=[%d,%d,%d,%d]\n",
               (int)type,
               (int)d0,
               (int)d1,
               (int)d2,
               (int)d3);
        fflush(stdout);
        return;
    }

    if (mq_send(g_mqCentralToQt, (const char *)&msg, sizeof(msg), 0) != 0)
    {
        printf("[central] send UI failed: type=%d data=[%d,%d,%d,%d] errno=%d (%s)\n",
               (int)type,
               (int)d0,
               (int)d1,
               (int)d2,
               (int)d3,
               errno,
               strerror(errno));
        fflush(stdout);


        if (errno != EAGAIN)
        {
            perror("[central] mq_send UI");
        }

        
    }
    else
    {
        printf("[central] send UI success: type=%d data=[%d,%d,%d,%d]\n",
               (int)type,
               (int)d0,
               (int)d1,
               (int)d2,
               (int)d3);
        fflush(stdout);
    }
}

static bool can_init(void)
{
    struct sockaddr_can addr;
    struct ifreq ifr;
    const char *ifName = getenv("CAN_INTERFACE");
    if (ifName == NULL || ifName[0] == '\0')
    {
        ifName = "can0";
    }

    g_canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (g_canSocket < 0)
    {
        perror("[central] CAN socket");
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(g_canSocket, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("[central] CAN ioctl");
        close(g_canSocket);
        g_canSocket = -1;
        return false;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(g_canSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("[central] CAN bind");
        close(g_canSocket);
        g_canSocket = -1;
        return false;
    }

    if (set_nonblock(g_canSocket) != 0)
    {
        perror("[central] CAN nonblock");
    }

    printf("[central] CAN ready on %s\n", ifName);
    return true;
}

static void can_close(void)
{
    if (g_canSocket >= 0)
    {
        close(g_canSocket);
        g_canSocket = -1;
    }
}

static bool can_send(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    struct can_frame frame;
    ssize_t written;

    if (g_canSocket < 0 || dlc > 8U)
    {
        return false;
    }

    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.can_dlc = dlc;
    if (data != NULL && dlc > 0U)
    {
        memcpy(frame.data, data, dlc);
    }

    written = write(g_canSocket, &frame, sizeof(frame));
    if (written != (ssize_t)sizeof(frame))
    {
        if (errno != ENOBUFS && errno != EAGAIN)
        {
            perror("[central] CAN write");
        }
        return false;
    }

    return true;
}

static bool can_receive(CanMessage *out)
{
    struct can_frame frame;
    ssize_t received;

    if (g_canSocket < 0 || out == NULL)
    {
        return false;
    }

    received = read(g_canSocket, &frame, sizeof(frame));
    if (received < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("[central] CAN read");
        }
        return false;
    }
    if (received != (ssize_t)sizeof(frame) || (frame.can_id & CAN_EFF_FLAG) != 0U)
    {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->id = frame.can_id & CAN_SFF_MASK;
    out->dlc = frame.can_dlc;
    memcpy(out->data, frame.data, sizeof(out->data));
    return true;
}

static void can_send_onoff(uint32_t id, bool on)
{
    uint8_t data[1];
    data[0] = on ? CAN_VALUE_ON : CAN_VALUE_OFF;
    (void)can_send(id, 1U, data);
}

static void send_ignition_operation(uint8_t ignition, uint8_t operation)
{
    uint8_t data[2];
    data[0] = ignition;
    data[1] = operation;
    (void)can_send(CAN_ID_IGNITION_STATE, 2U, data);
}

static void send_window_target(uint8_t percent)
{
    uint8_t data[1];
    data[0] = percent;
    (void)can_send(CAN_ID_WINDOW_CONTROL, 1U, data);
}

static void send_temperature_target(uint8_t tempC)
{
    uint8_t data[4];
    uint16_t targetX10 = (uint16_t)tempC * 10U;
    data[0] = (uint8_t)(targetX10 & 0xFFU);
    data[1] = (uint8_t)((targetX10 >> 8) & 0xFFU);
    data[2] = 0U; /* AUTO */
    data[3] = 0U; /* manual level */
    (void)can_send(CAN_ID_TEMPERATURE_TARGET_VALUE, 4U, data);
}

static void send_seat_target(uint8_t duty)
{
    uint8_t data[5];
    data[0] = 1U;       /* forward */
    data[1] = duty;     /* PWM duty */
    data[2] = 0xE8U;    /* 1000 ms little-endian */
    data[3] = 0x03U;
    data[4] = 0x00U;
    (void)can_send(CAN_ID_SEAT_TARGET_VALUE, 5U, data);
}

static void send_interior_light_auto(void)
{
    uint8_t data[5];
    uint16_t thresholdX10 = 7000U;
    data[0] = 1U; /* AUTO */
    data[1] = 0U; /* off command ignored in auto */
    data[2] = 100U;
    data[3] = (uint8_t)(thresholdX10 & 0xFFU);
    data[4] = (uint8_t)((thresholdX10 >> 8) & 0xFFU);
    (void)can_send(CAN_ID_INTERIOR_LIGHT_CONTROL, 5U, data);
}

static void send_object_detected(bool detected, uint8_t confidence)
{
    uint8_t data[2];
    data[0] = detected ? CAN_VALUE_ON : CAN_VALUE_OFF;
    data[1] = confidence;
    (void)can_send(CAN_ID_OBJECT_DETECTED, 2U, data);
}

static bool udp_send_text(const char *host, const char *port, const char *text)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    int fd;
    int rc;
    bool ok = false;

    if (host == NULL || host[0] == '\0' || port == NULL || text == NULL)
    {
        return false;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    rc = getaddrinfo(host, port, &hints, &result);
    if (rc != 0)
    {
        fprintf(stderr, "[central] RPi5 getaddrinfo failed: %s\n", gai_strerror(rc));
        return false;
    }

    fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd >= 0)
    {
        ok = sendto(fd, text, strlen(text), 0, result->ai_addr, result->ai_addrlen) >= 0;
        close(fd);
    }

    freeaddrinfo(result);
    return ok;
}

static void rpi5_send(const char *command)
{
    const char *host = getenv("RPI5_AI_HOST");
    const char *port = getenv("RPI5_AI_CONTROL_PORT");

    if (port == NULL || port[0] == '\0')
    {
        port = "5020";
    }
    if (host == NULL || host[0] == '\0')
    {
        printf("[central] RPI5_AI_HOST unset; skip RPi5 command %s\n", command);
        return;
    }

    printf("[central] RPi5 command: %s -> %s:%s\n", command, host, port);
    (void)udp_send_text(host, port, command);
}

static bool object_camera_init(void)
{
    struct sockaddr_in addr;
    uint16_t port = (uint16_t)env_u32("OBJECT_CAMERA_PORT", 5010U);
    int reuse = 1;

    g_objectCameraSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_objectCameraSocket < 0)
    {
        perror("[central] object camera socket");
        return false;
    }

    (void)setsockopt(g_objectCameraSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(g_objectCameraSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("[central] object camera bind");
        close(g_objectCameraSocket);
        g_objectCameraSocket = -1;
        return false;
    }

    if (set_nonblock(g_objectCameraSocket) != 0)
    {
        perror("[central] object camera nonblock");
    }

    printf("[central] object camera UDP listening on %u\n", port);
    return true;
}

static void object_camera_close(void)
{
    if (g_objectCameraSocket >= 0)
    {
        close(g_objectCameraSocket);
        g_objectCameraSocket = -1;
    }
}

static bool parse_bool_payload(const char *payload, bool *detected)
{
    char normalized[128];
    size_t out = 0U;
    size_t i;

    if (payload == NULL || detected == NULL)
    {
        return false;
    }

    for (i = 0U; payload[i] != '\0' && out + 1U < sizeof(normalized); i++)
    {
        char ch = payload[i];
        if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t')
        {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = (char)(ch - 'A' + 'a');
        }
        normalized[out++] = ch;
    }
    normalized[out] = '\0';

    if (strcmp(normalized, "1") == 0 ||
        strcmp(normalized, "true") == 0 ||
        strcmp(normalized, "person=1") == 0 ||
        strcmp(normalized, "person=true") == 0 ||
        strcmp(normalized, "detected=1") == 0 ||
        strcmp(normalized, "detected=true") == 0)
    {
        *detected = true;
        return true;
    }
    if (strcmp(normalized, "0") == 0 ||
        strcmp(normalized, "false") == 0 ||
        strcmp(normalized, "person=0") == 0 ||
        strcmp(normalized, "person=false") == 0 ||
        strcmp(normalized, "detected=0") == 0 ||
        strcmp(normalized, "detected=false") == 0)
    {
        *detected = false;
        return true;
    }

    for (i = 0U; normalized[i] != '\0'; i++)
    {
        if (normalized[i] == '1')
        {
            *detected = true;
            return true;
        }
        if (normalized[i] == '0')
        {
            *detected = false;
            return true;
        }
    }

    return false;
}

static bool object_camera_poll(bool *detected)
{
    char buffer[128];
    ssize_t received;

    if (g_objectCameraSocket < 0 || detected == NULL)
    {
        return false;
    }

    received = recv(g_objectCameraSocket, buffer, sizeof(buffer) - 1U, 0);
    if (received < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("[central] object camera recv");
        }
        return false;
    }

    buffer[received] = '\0';
    if (!parse_bool_payload(buffer, detected))
    {
        printf("[central] ignored object camera payload: %s\n", buffer);
        return false;
    }

    printf("[central] object camera detected=%d payload=%s\n", *detected ? 1 : 0, buffer);
    return true;
}

static void update_buzzer(void)
{
    can_send_onoff(CAN_ID_BUZZER_CONTROL, g_childAlarmLatched || g_seatbeltAlarmActive);
}

static void send_seatbelt_ui(void)
{
    uint32_t current = g_currentSeatbeltBitmap & ACTIVE_SEAT_MASK;
    uint32_t blue;
    uint32_t red;
    uint32_t gray;

    if (g_state == STATE_PASSENGER_TRANSFER)
    {
        blue = current;
        red = 0U;
        gray = (~current) & ACTIVE_SEAT_MASK;
    }
    else
    {
        blue = g_referenceSeatbeltBitmap & current & ACTIVE_SEAT_MASK;
        red = g_referenceSeatbeltBitmap & (~current) & ACTIVE_SEAT_MASK;
        gray = (~g_referenceSeatbeltBitmap) & ACTIVE_SEAT_MASK;
    }

    send_ui(IPC_MSG_SEATBELT_UPDATE,
            (int32_t)current,
            (int32_t)blue,
            (int32_t)red,
            (int32_t)gray);

    g_seatbeltAlarmActive = (g_state == STATE_OPERATIONAL && red != 0U);
    update_buzzer();
}

static void handle_seatbelt_bitmap(uint32_t bitmap)
{
    g_currentSeatbeltBitmap = bitmap & ACTIVE_SEAT_MASK;
    send_seatbelt_ui();
}

static void apply_profile(void)
{
    if (!g_hasProfile)
    {
        return;
    }

    send_temperature_target(g_profile.temperatureTarget);
    send_seat_target(g_profile.seatPosition);
    send_interior_light_auto();
    send_ui(IPC_MSG_PROFILE_APPLIED,
            (int32_t)g_profile.driverId,
            (int32_t)g_profile.seatPosition,
            (int32_t)g_profile.temperatureTarget,
            0);
}

static void load_profile(uint32_t driverId)
{
    /*
     * Demo profile table. Replace with JSON loading later if needed.
     * seatPosition is used as seat motor PWM duty, temperatureTarget is °C.
     */
    if (driverId == 2U)
    {
        g_profile.driverId = 2U;
        g_profile.seatPosition = 45U;
        g_profile.temperatureTarget = 24U;
    }
    else if (driverId == 3U)
    {
        g_profile.driverId = 3U;
        g_profile.seatPosition = 35U;
        g_profile.temperatureTarget = 26U;
    }
    else
    {
        g_profile.driverId = driverId == 0U ? 1U : driverId;
        g_profile.seatPosition = 40U;
        g_profile.temperatureTarget = 25U;
    }
    g_hasProfile = true;
}

static void start_child_monitoring(void)
{
    g_childMonitoringActive = true;
    g_childAlarmLatched = false;
    g_childLevel2Sent = false;
    g_pirDetected = false;
    g_cameraDetected = false;
    g_childAckCooldownUntilSec = 0U; /* [MOD] 새 세션은 이전 cooldown을 승계하지 않는다 */
    rpi5_send("FACE_OFF");
    rpi5_send("OBJECT_ON");
    printf("[central] residual child monitoring started\n");
}

static void stop_child_monitoring(void)
{
    if (g_childMonitoringActive || g_childAlarmLatched)
    {
        printf("[central] residual child monitoring stopped\n");
    }

    /*
     * [MOD] 경보가 latch된 채로 감시를 끝내는 경로(예: 경보 중 전원 버튼 →
     * DRIVER_AUTH 진입)에서 F/W의 안전 latch(팬/내부 LED)가 남지 않도록
     * OP_ALARM 해제를 명시적으로 브로드캐스트한다.
     */
    if (g_childAlarmLatched)
    {
        send_ignition_operation(IGNITION_OFF, OP_PARKING);
    }

    g_childMonitoringActive = false;
    g_childAlarmLatched = false;
    g_childLevel2Sent = false;
    g_pirDetected = false;
    g_cameraDetected = false;
    rpi5_send("OBJECT_OFF");
    send_object_detected(false, 0U);
    can_send_onoff(CAN_ID_EXT_LED_CONTROL, false);
    send_window_target(0U);
    update_buzzer();
}

/*
 * [MOD] Telegram 발송 공통 헬퍼.
 * env_name으로 지정된 환경변수의 명령을 실행한다.
 *   CHILD_ALERT_LEVEL1_CMD : 1차 경고 → 운전자 chat_id
 *   CHILD_ALERT_LEVEL2_CMD : 2차 경고 → 경찰 chat_id
 * 두 변수는 서로 다른 스크립트(또는 같은 스크립트 + 다른 TELEGRAM_CHAT_ID)를
 * 가리켜야 한다. 미설정 시 발송을 건너뛰고 로그만 남긴다(경고 장치는 정상 동작).
 */
static void notify_child_alert(const char *env_name, const char *label)
{
    const char *command = getenv(env_name);
    int rc;

    if (command == NULL || command[0] == '\0')
    {
        printf("[central] %s unset; Telegram skipped (%s)\n", env_name, label);
        fflush(stdout);
        return;
    }

    rc = system(command);
    printf("[central] Telegram %s via %s rc=%d\n", label, env_name, rc);
    fflush(stdout);
}

static void start_child_alarm_level1(uint32_t source)
{
    if (!g_childMonitoringActive)
    {
        printf("[central] child detection ignored outside residual monitoring\n");
        return;
    }

    if (g_childAlarmLatched)
    {
        return;
    }

    g_childAlarmLatched = true;
    g_childLevel2Sent = false;
    g_childAlarmStartedSec = monotonic_seconds();

    /*
     * [MOD] 잔류 감지 확정 → 제어 명령 송신 (RPi4가 유일한 판단 주체).
     *  - 0x083 [IGNITION_OFF, OP_ALARM] : RTOS는 상태 추종, F/W는 이 명령으로
     *    안전 latch를 켜고 자체 센서 기준(온도 25/28/30°C, 조도 700lux)으로
     *    팬 단계 / 내부 LED를 구동한다. (경보 시작/해제 권한은 RPi4에만 있음)
     *  - 0x0C3 부저 ON, 0x0A3 외부 LED ON, 0x063 창문 30% 개방 : RTOS 실행
     */
    send_ignition_operation(IGNITION_OFF, OP_ALARM);
    can_send_onoff(CAN_ID_BUZZER_CONTROL, true);
    can_send_onoff(CAN_ID_EXT_LED_CONTROL, true);
    send_window_target(30U);
    send_ui(IPC_MSG_CHILD_DETECTED_WARNING, (int32_t)source, 1, 0, 0);
    printf("[central] child alarm level1 latched source=%u (1=PIR,2=CAMERA)\n", source);

    /* [MOD] 판단 근거 로그: F/W가 실행할 팬/LED 예상 동작을 함께 남긴다. */
    if (g_lastEnvValid != 0U)
    {
        printf("[central] env at alarm: temp=%d.%d C (fan %s expected), lux=%u.%u (LED %s expected)\n",
               g_lastTempX10 / 10, (g_lastTempX10 < 0 ? -g_lastTempX10 : g_lastTempX10) % 10,
               (g_lastTempX10 >= ROA_FAN_TEMP_THRESHOLD_X10) ? "ON" : "OFF",
               g_lastLuxX10 / 10U, g_lastLuxX10 % 10U,
               (g_lastLuxX10 < ROA_LED_LUX_THRESHOLD_X10) ? "ON" : "OFF");
    }
    else
    {
        printf("[central] env at alarm: no valid data yet (F/W fail-safe: fan L1 / LED ON)\n");
    }
    fflush(stdout);

    /*
     * [MOD] 1차 경고 = 운전자에게 Telegram 발송 (즉시).
     * 2차(90초 미조치)는 경찰에게 발송한다 — start_child_alarm_level2() 참조.
     * 수신자 분리는 CHILD_ALERT_LEVEL1_CMD / CHILD_ALERT_LEVEL2_CMD 환경변수로
     * 서로 다른 chat_id를 쓰는 스크립트를 지정해 구현한다.
     */
    notify_child_alert("CHILD_ALERT_LEVEL1_CMD", "level1 -> driver");
}

static void start_child_alarm_level2(const char *reason)
{
    if (!g_childAlarmLatched || g_childLevel2Sent)
    {
        return;
    }

    g_childLevel2Sent = true;
    send_ui(IPC_MSG_CHILD_DETECTED_WARNING, 0, 2, 0, 0);
    printf("[central] child alarm level2: %s\n", reason);

    /* [MOD] 2차 경고 = 경찰에게 Telegram 1회 발송 (반복 없음).
     * g_childLevel2Sent 플래그로 같은 사건에서 재발송이 불가능하다. */
    notify_child_alert("CHILD_ALERT_LEVEL2_CMD", "level2 -> police");
}

static void clear_child_alarm(void)
{
    if (!g_childAlarmLatched)
    {
        printf("[central] ignore stale child alarm clear: state=%d\n", (int)g_state);
        fflush(stdout);
        return;
    }

    /*
     * [MOD] "차량 확인 완료" 처리 (최종 정책, 2026-07-12):
     *  - 현재 경고 사건만 종료: 부저/외부LED/창문/F/W latch/화면 OFF,
     *    1·2차 타이머와 Telegram 플래그 리셋
     *  - 감시는 유지 (stop_child_monitoring을 부르지 않음, RPi5 OBJECT_ON 유지)
     *  - ACK cooldown 시작: 확인자가 차량에서 나갈 시간 동안 재경고 억제
     *  - cooldown 종료 후 감지가 지속/재발하면 새 1차 경고부터 다시 시작
     *  - 감시 세션 자체의 초기화는 다음 운행 사이클에서만 수행
     */
    g_childAlarmLatched = false;
    g_childLevel2Sent = false;
    g_pirDetected = false;
    g_cameraDetected = false;
    g_childAckCooldownUntilSec = monotonic_seconds() + g_childAckCooldownSec;

    if (g_state == STATE_IGNITION_OFF)
    {
        send_ignition_operation(IGNITION_OFF, OP_PARKING); /* F/W 팬/LED latch 해제 */
        send_ui(IPC_MSG_SCREEN_OFF, 0, 0, 0, 0);
    }
    else
    {
        printf("[central] child alarm clear without screen-off: state=%d\n", (int)g_state);
        fflush(stdout);
    }

    can_send_onoff(CAN_ID_EXT_LED_CONTROL, false);
    send_window_target(0U);
    send_object_detected(false, 0U);
    update_buzzer();
    printf("[central] vehicle-check confirmed: alarm event closed, "
           "monitoring continues, cooldown %us\n", g_childAckCooldownSec);
}

static void update_child_sensor(uint32_t source, bool detected)
{
    /*
     * [MOD] 잔류 감지 OR 판단은 RPi4 단일 지점에서만 수행한다 (요구사항 5/9-7).
     *   residual_detected = pir_detected || ai_object_detected
     * - RTOS는 PIR 원시값을 0x022로 계속 브로드캐스트하므로, 감시 모드가 아닐 때는
     *   값만 저장하고 판단/로그를 하지 않는다 (기존 코드는 10Hz로 무시 로그를 쏟아냄).
     * - 해제 직후 재무장 억제 시간 동안은 새 경고를 latch하지 않는다.
     */
    bool residualDetected;

    if (source == CHILD_SENSOR_CAMERA)
    {
        if (detected != g_cameraDetected)
        {
            printf("[central] AI object detected=%d\n", detected ? 1 : 0);
        }
        g_cameraDetected = detected;
        send_object_detected(detected, detected ? 100U : 0U);
    }
    else
    {
        if (detected != g_pirDetected)
        {
            printf("[central] PIR detected=%d\n", detected ? 1 : 0);
        }
        g_pirDetected = detected;
    }

    if (!g_childMonitoringActive)
    {
        return;
    }

    residualDetected = g_pirDetected || g_cameraDetected;

    if (residualDetected)
    {
        /* [MOD] ACK cooldown 구간에는 새 경고를 latch하지 않는다
         * (확인자의 움직임 오탐 방지). 만료 후에는 감지가 지속되든
         * 재발하든 새 1차 경고로 처리한다. */
        if (monotonic_seconds() < g_childAckCooldownUntilSec)
        {
            return;
        }
        printf("[central] residual_detected = PIR(%d) || AI(%d) => 1\n",
               g_pirDetected ? 1 : 0, g_cameraDetected ? 1 : 0);
        start_child_alarm_level1(source);
    }
}

static void central_tick(void)
{
    uint64_t now = monotonic_seconds();

    /*
     * [MOD] can0가 central보다 늦게 올라오는 경우 재시도.
     * 기존 코드는 시작 시 can_init() 1회 실패하면 영원히 CAN 없이 동작했다.
     */
    if (g_canSocket < 0 &&
        (g_canLastInitTrySec == 0U || now - g_canLastInitTrySec >= CAN_RETRY_INTERVAL_SEC))
    {
        g_canLastInitTrySec = now;
        if (can_init())
        {
            printf("[central] CAN socket recovered\n");
            /* 재연결 직후 현재 상태를 다시 브로드캐스트해 보드 상태를 동기화 */
            if (g_state == STATE_OPERATIONAL)
            {
                send_ignition_operation(IGNITION_ON, OP_RUNNING);
            }
            else if (g_state == STATE_PASSENGER_TRANSFER)
            {
                send_ignition_operation(IGNITION_ON, OP_BOARDING);
            }
            else
            {
                send_ignition_operation(IGNITION_OFF, OP_PARKING);
            }
        }
    }

    if (g_childAlarmLatched &&
        !g_childLevel2Sent &&
        now >= g_childAlarmStartedSec &&
        now - g_childAlarmStartedSec >= g_childAlertEscalationSec)
    {
        start_child_alarm_level2("timeout without UI clear");
    }
}

static void enter_driver_auth(void)
{
    printf("[central] enter_driver_auth\n");
    fflush(stdout);

    stop_child_monitoring();

    g_state = STATE_DRIVER_AUTH;

    rpi5_send("OBJECT_OFF");
    rpi5_send("FACE_ON");

    printf("[central] send UI auth screen\n");
    fflush(stdout);

    send_ui(IPC_MSG_SHOW_AUTH_SCREEN, 0, 0, 0, 0);
}

static void enter_wait_ignition(uint32_t driverId)
{
    g_state = STATE_WAIT_IGNITION;
    printf("[central] state -> WAIT_IGNITION (driverId=%u)\n", driverId); /* [MOD] log */
    load_profile(driverId);
    rpi5_send("FACE_OFF");
    apply_profile();
    send_ui(IPC_MSG_SHOW_READY_SCREEN,
            (int32_t)g_profile.driverId,
            (int32_t)g_profile.seatPosition,
            (int32_t)g_profile.temperatureTarget,
            0);
    send_ignition_operation(IGNITION_DISPLAY, OP_PARKING);
}

static void enter_operational_from_wait(void)
{
    g_state = STATE_OPERATIONAL;
    printf("[central] state -> OPERATIONAL (ignition ON)\n"); /* [MOD] log */
    stop_child_monitoring();
    g_referenceSeatbeltBitmap = 0U;
    g_currentSeatbeltBitmap = 0U;
    g_seatbeltAlarmActive = false;
    rpi5_send("ALL_OFF");
    send_ignition_operation(IGNITION_ON, OP_RUNNING);
    apply_profile();
    send_ui(IPC_MSG_SHOW_OPERATION_SCREEN, 0, 0, 0, 0);
    send_seatbelt_ui();
}

static void enter_operational_from_transfer(void)
{
    g_state = STATE_OPERATIONAL;
    printf("[central] state -> OPERATIONAL (boarding done, reference seats=0x%02X)\n",
           g_currentSeatbeltBitmap & ACTIVE_SEAT_MASK); /* [MOD] log */
    g_referenceSeatbeltBitmap = g_currentSeatbeltBitmap & ACTIVE_SEAT_MASK;
    send_ignition_operation(IGNITION_ON, OP_RUNNING);
    send_ui(IPC_MSG_SHOW_OPERATION_SCREEN, 0, 0, 0, 0);
    send_seatbelt_ui();
}

static void enter_passenger_transfer(void)
{
    g_state = STATE_PASSENGER_TRANSFER;
    printf("[central] state -> PASSENGER_TRANSFER\n"); /* [MOD] log */
    g_seatbeltAlarmActive = false;
    update_buzzer();
    /*
     * [MOD] 승하차 중에는 OP_BOARDING(4)을 송신한다.
     * RTOS는 op=4에서 STOPPED_BOARDING 상태로 전환하며 안전벨트 경고 플래그를
     * 클리어하고, F/W는 OP_STOPPED_BOARDING에서 좌석/팬 수동 제어를 허용한다.
     * (기존 코드는 OP_RUNNING을 보내 RTOS의 op=4 분기가 죽은 코드였음)
     */
    send_ignition_operation(IGNITION_ON, OP_BOARDING);
    send_ui(IPC_MSG_SHOW_BOARDING_SCREEN, 0, 0, 0, 0);
    send_seatbelt_ui();
}

static void enter_ignition_off_from_drive(void)
{
    g_state = STATE_IGNITION_OFF;
    printf("[central] state -> IGNITION_OFF (+ residual monitoring)\n"); /* [MOD] log */
    g_referenceSeatbeltBitmap = 0U;
    g_currentSeatbeltBitmap = 0U;
    g_seatbeltAlarmActive = false;
    update_buzzer();
    send_ignition_operation(IGNITION_OFF, OP_PARKING);
    send_ui(IPC_MSG_SCREEN_OFF, 1, 0, 0, 0);
    start_child_monitoring();
}

static void handle_ignition_button(void)
{
    switch (g_state)
    {
        case STATE_IGNITION_OFF:
            enter_driver_auth();
            break;

        case STATE_DRIVER_AUTH:
            printf("[central] ignition button ignored while authenticating\n");
            break;

        case STATE_WAIT_IGNITION:
            enter_operational_from_wait();
            break;

        case STATE_OPERATIONAL:
            enter_ignition_off_from_drive();
            break;

        case STATE_PASSENGER_TRANSFER:
            printf("[central] ignition button ignored during passenger transfer; press boarding first\n");
            break;
    }
}

static void handle_boarding_button(void)
{
    if (g_state == STATE_OPERATIONAL)
    {
        enter_passenger_transfer();
    }
    else if (g_state == STATE_PASSENGER_TRANSFER)
    {
        enter_operational_from_transfer();
    }
}

static void handle_auth_fail(void)
{
    if (g_state == STATE_DRIVER_AUTH)
    {
        send_ui(IPC_MSG_AUTH_FAIL, 0, 0, 0, 0);
    }
}

static void process_ipc_message(const IpcMessage *msg)
{
    if (msg == NULL)
    {
        return;
    }

    switch ((IpcMessageType)msg->type)
    {
        case IPC_MSG_BOARDING_BUTTON:
            handle_boarding_button();
            break;

        case IPC_MSG_DRIVER_ID_RECEIVED:
        case IPC_MSG_FACE_AUTH_SUCCESS:
            if (g_state == STATE_DRIVER_AUTH)
            {
                enter_wait_ignition((uint32_t)msg->data[0]);
            }
            break;

        case IPC_MSG_FACE_AUTH_FAIL:
            handle_auth_fail();
            break;

        case IPC_MSG_ALARM_ACK:
            clear_child_alarm();
            break;

        default:
            printf("[central] ignored IPC type=%d\n", msg->type);
            break;
    }
}

static void process_ipc_messages(void)
{
    IpcMessage msg;
    ssize_t received;

    if (g_mqQtToCentral == (mqd_t)-1)
    {
        return;
    }

    while ((received = mq_receive(g_mqQtToCentral, (char *)&msg, sizeof(msg), NULL)) > 0)
    {
        if (received == (ssize_t)sizeof(msg))
        {
            process_ipc_message(&msg);
        }
    }

    if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("[central] mq_receive");
    }
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_i16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

static void process_power_frame(const CanMessage *frame)
{
    static bool wasPressed = false;
    static uint64_t lastAcceptedMs = 0U;

    bool pressed;
    uint64_t nowMs;

    if (frame->dlc < 1U)
    {
        return;
    }

    pressed = (frame->data[0] != 0U);

    /*
     * RTOS 0x012:
     *   data[0] = 1 : button pressed
     *   data[0] = 0 : button released
     *
     * RPi4 Central only treats rising edge, 0 -> 1, as one power-button event.
     */
    if (pressed && !wasPressed)
    {
        nowMs = monotonic_milliseconds();

        if (lastAcceptedMs == 0U || (nowMs - lastAcceptedMs) >= 200U)
        {
            lastAcceptedMs = nowMs;
            printf("[central] power button pressed\n");
            handle_ignition_button();
        }
        else
        {
            printf("[central] power button bounce ignored\n");
        }
    }

    wasPressed = pressed;
}

static void process_pir_frame(const CanMessage *frame)
{
    if (frame->dlc < 1U)
    {
        return;
    }

    if (frame->data[0] == 2U)
    {
        clear_child_alarm();
    }
    else
    {
        update_child_sensor(CHILD_SENSOR_PIR, frame->data[0] != 0U);
    }
}

static void process_seatbelt_frame(const CanMessage *frame)
{
    uint32_t bitmap = 0U;

    if (frame->dlc >= 1U && frame->data[0] == 1U)
    {
        bitmap |= 0x01U;
    }
    if (frame->dlc >= 2U && frame->data[1] == 1U)
    {
        bitmap |= 0x02U;
    }

    handle_seatbelt_bitmap(bitmap);
}

static void process_env_frame(const CanMessage *frame)
{
    int16_t tempX10;
    uint16_t humidityX10;
    uint16_t luxX10;
    uint8_t valid;

    if (frame->dlc < 7U)
    {
        return;
    }

    tempX10 = read_i16_le(&frame->data[0]);
    humidityX10 = read_u16_le(&frame->data[2]);
    luxX10 = read_u16_le(&frame->data[4]);
    valid = frame->data[6];

    /* [MOD] 잔류 경보 시 팬/LED 판단 로그를 위해 마지막 환경값을 캐시한다. */
    g_lastTempX10 = tempX10;
    g_lastLuxX10 = luxX10;
    g_lastEnvValid = valid;

    send_ui(IPC_MSG_ENV_UPDATE, tempX10, humidityX10, luxX10, valid);
}

static void process_dust_frame(const CanMessage *frame)
{
    uint16_t pm1;
    uint16_t pm25;
    uint16_t pm10;
    uint8_t valid = 0U;

    if (frame->dlc < 6U)
    {
        return;
    }

    pm1 = read_u16_le(&frame->data[0]);
    pm25 = read_u16_le(&frame->data[2]);
    pm10 = read_u16_le(&frame->data[4]);
    if (frame->dlc >= 7U)
    {
        valid = frame->data[6];
    }
    send_ui(IPC_MSG_DUST_UPDATE, pm1, pm25, pm10, valid);
}

static void process_fw_status_frame(const CanMessage *frame)
{
    uint8_t fan = frame->dlc >= 1U ? frame->data[0] : 0U;
    uint8_t led = frame->dlc >= 2U ? frame->data[1] : 0U;
    uint8_t ignition = frame->dlc >= 3U ? frame->data[2] : 0U;
    uint8_t operation = frame->dlc >= 4U ? frame->data[3] : 0U;
    uint8_t pir = frame->dlc >= 5U ? frame->data[4] : 0U;
    uint8_t valid = frame->dlc >= 6U ? frame->data[5] : 0U;
    uint8_t alert = frame->dlc >= 7U ? frame->data[6] : 0U;

    send_ui(IPC_MSG_FW_STATUS_UPDATE,
            fan,
            led,
            (int32_t)(((uint32_t)ignition << 8) | operation),
            (int32_t)(((uint32_t)pir << 16) | ((uint32_t)valid << 8) | alert));
}

static void process_can_frame(const CanMessage *frame)
{
    /* [MOD] CAN_DEBUG_LOG=1 이면 수신 프레임 원시 데이터를 로그로 남긴다.
     * (요구사항 3-8: can0 실시간 수신 확인 — UI 센서 패널 + 본 로그로 대응) */
    if (g_canDebugLog)
    {
        printf("[can-rx] id=0x%03X dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\n",
               frame->id, frame->dlc,
               frame->data[0], frame->data[1], frame->data[2], frame->data[3],
               frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
    }

    switch (frame->id)
    {
        case CAN_ID_POWER_BUTTON_INPUT:
            process_power_frame(frame);
            break;
        case CAN_ID_PIR_DETECTED:
            process_pir_frame(frame);
            break;
        case CAN_ID_SEATBELT_STATUS:
            process_seatbelt_frame(frame);
            break;
        case CAN_ID_ENV_SENSOR_VALUE:
            process_env_frame(frame);
            break;
        case CAN_ID_DUST_SENSOR_VALUE:
            process_dust_frame(frame);
            break;
        case CAN_ID_FW_STATUS:
            process_fw_status_frame(frame);
            break;
        default:
            break;
    }
}

static void process_can_messages(void)
{
    int i;
    for (i = 0; i < MAX_CAN_FRAMES_PER_LOOP; i++)
    {
        CanMessage frame;
        if (!can_receive(&frame))
        {
            break;
        }
        process_can_frame(&frame);
    }
}

static void process_object_camera_messages(void)
{
    int i;
    for (i = 0; i < MAX_OBJECT_CAMERA_PER_LOOP; i++)
    {
        bool detected = false;
        if (!object_camera_poll(&detected))
        {
            break;
        }
        update_child_sensor(CHILD_SENSOR_CAMERA, detected);
    }
}

int main(void)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* [MOD] 중복 실행 방지 (요구사항 12-1) */
    if (!pid_file_acquire())
    {
        return 1;
    }

    g_childAlertEscalationSec = env_u32("CHILD_ALERT_ESCALATION_SEC",
                                        CHILD_ALERT_DEFAULT_ESCALATION_SEC);
    /* [MOD] ACK cooldown 시간 (기본 30초, CHILD_ACK_COOLDOWN_SEC로 조절) */
    g_childAckCooldownSec = env_u32("CHILD_ACK_COOLDOWN_SEC",
                                    CHILD_ACK_COOLDOWN_DEFAULT_SEC);
    {
        const char *dbg = getenv("CAN_DEBUG_LOG");
        g_canDebugLog = (dbg != NULL && strcmp(dbg, "1") == 0);
    }

    printf("[central] school bus RPi4 central start (state=%s)\n", state_name(g_state));

    (void)ipc_init();
    (void)can_init();
    (void)object_camera_init();

    rpi5_send("ALL_OFF");
    send_ignition_operation(IGNITION_OFF, OP_PARKING);
    send_ui(IPC_MSG_SCREEN_OFF, 0, 0, 0, 0);

    while (g_running)
    {
        process_ipc_messages();
        process_can_messages();
        process_object_camera_messages();
        central_tick();
        usleep(MAIN_LOOP_SLEEP_US);
    }

    printf("[central] stopping\n");
    send_ignition_operation(IGNITION_OFF, OP_PARKING);
    can_send_onoff(CAN_ID_BUZZER_CONTROL, false);
    can_send_onoff(CAN_ID_EXT_LED_CONTROL, false);
    rpi5_send("ALL_OFF");
    object_camera_close();
    can_close();
    ipc_close();
    pid_file_release(); /* [MOD] PID 파일 정리 */
    printf("[central] shutdown complete\n");
    return 0;
}
