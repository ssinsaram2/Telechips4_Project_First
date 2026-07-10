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

#define OP_PARKING 0U
#define OP_RUNNING 1U
#define OP_ALARM   2U

#define CHILD_ALERT_DEFAULT_ESCALATION_SEC 30U
#define CHILD_MONITOR_DEFAULT_DURATION_SEC 1200U
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
static uint64_t g_childMonitoringStartedSec = 0U;
static uint32_t g_childMonitoringDurationSec = CHILD_MONITOR_DEFAULT_DURATION_SEC;
static bool g_childAlarmLatched = false;
static bool g_childLevel2Sent = false;
static uint64_t g_childAlarmStartedSec = 0U;
static uint32_t g_childAlertEscalationSec = CHILD_ALERT_DEFAULT_ESCALATION_SEC;

static bool g_pirDetected = false;
static bool g_cameraDetected = false;


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
    g_childMonitoringStartedSec = monotonic_seconds();
    g_childAlarmLatched = false;
    g_childLevel2Sent = false;
    g_pirDetected = false;
    g_cameraDetected = false;
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

    send_ignition_operation(IGNITION_OFF, OP_ALARM);
    can_send_onoff(CAN_ID_BUZZER_CONTROL, true);
    can_send_onoff(CAN_ID_EXT_LED_CONTROL, true);
    send_window_target(30U);
    send_ui(IPC_MSG_CHILD_DETECTED_WARNING, (int32_t)source, 1, 0, 0);
    printf("[central] child alarm level1 latched source=%u\n", source);
}

static void start_child_alarm_level2(const char *reason)
{
    const char *command;
    int rc;

    if (!g_childAlarmLatched || g_childLevel2Sent)
    {
        return;
    }

    g_childLevel2Sent = true;
    send_ui(IPC_MSG_CHILD_DETECTED_WARNING, 0, 2, 0, 0);
    printf("[central] child alarm level2: %s\n", reason);

    command = getenv("CHILD_ALERT_LEVEL2_CMD");
    if (command == NULL || command[0] == '\0')
    {
        printf("[central] CHILD_ALERT_LEVEL2_CMD unset; Telegram command skipped\n");
        return;
    }

    rc = system(command);
    printf("[central] CHILD_ALERT_LEVEL2_CMD rc=%d\n", rc);
}

static void clear_child_alarm(void)
{
    g_childAlarmLatched = false;
    g_childLevel2Sent = false;
    g_pirDetected = false;
    g_cameraDetected = false;
    send_ignition_operation(IGNITION_OFF, OP_PARKING);
    can_send_onoff(CAN_ID_EXT_LED_CONTROL, false);
    send_window_target(0U);
    send_object_detected(false, 0U);
    update_buzzer();
    send_ui(IPC_MSG_SCREEN_OFF, 0, 0, 0, 0);
    printf("[central] child alarm cleared by UI\n");
}

static void update_child_sensor(uint32_t source, bool detected)
{
    if (source == CHILD_SENSOR_CAMERA)
    {
        g_cameraDetected = detected;
        send_object_detected(detected, detected ? 100U : 0U);
    }
    else
    {
        g_pirDetected = detected;
    }

    if (g_pirDetected || g_cameraDetected)
    {
        start_child_alarm_level1(source);
    }
}

static void central_tick(void)
{
    uint64_t now = monotonic_seconds();

    if (g_childAlarmLatched &&
        !g_childLevel2Sent &&
        now >= g_childAlarmStartedSec &&
        now - g_childAlarmStartedSec >= g_childAlertEscalationSec)
    {
        start_child_alarm_level2("timeout without UI clear");
    }

    if (g_childMonitoringActive &&
        !g_childAlarmLatched &&
        now >= g_childMonitoringStartedSec &&
        now - g_childMonitoringStartedSec >= g_childMonitoringDurationSec)
    {
        printf("[central] residual monitoring timeout; keep black screen and stop object model\n");
        g_childMonitoringActive = false;
        rpi5_send("OBJECT_OFF");
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
    g_referenceSeatbeltBitmap = g_currentSeatbeltBitmap & ACTIVE_SEAT_MASK;
    send_ignition_operation(IGNITION_ON, OP_RUNNING);
    send_ui(IPC_MSG_SHOW_OPERATION_SCREEN, 0, 0, 0, 0);
    send_seatbelt_ui();
}

static void enter_passenger_transfer(void)
{
    g_state = STATE_PASSENGER_TRANSFER;
    g_seatbeltAlarmActive = false;
    update_buzzer();
    send_ignition_operation(IGNITION_ON, OP_RUNNING);
    send_ui(IPC_MSG_SHOW_BOARDING_SCREEN, 0, 0, 0, 0);
    send_seatbelt_ui();
}

static void enter_ignition_off_from_drive(void)
{
    g_state = STATE_IGNITION_OFF;
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

    g_childAlertEscalationSec = env_u32("CHILD_ALERT_ESCALATION_SEC",
                                        CHILD_ALERT_DEFAULT_ESCALATION_SEC);
    g_childMonitoringDurationSec = env_u32("CHILD_MONITOR_DURATION_SEC",
                                           CHILD_MONITOR_DEFAULT_DURATION_SEC);

    printf("[central] school bus RPi4 central start\n");

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
    return 0;
}
