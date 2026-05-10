#pragma once

// ConveyorProtocol.h
// Common protocol header for Conveyor System
// RS232 / TCP / HTTP shared definitions

#define PACKET_SIZE     9

#define STX             0x02
#define ETX             0x03

// Packet byte index
#define IDX_STX         0
#define IDX_CMD         1
#define IDX_LEN         2
#define IDX_TEMP        3
#define IDX_HUMI        4
#define IDX_ULTR        5
#define IDX_MOTR        6
#define IDX_CS          7
#define IDX_ETX         8

// CMD: status report  (Raspberry Pi -> PC)
#define CMD_STATUS_REPORT       0x10

// CMD: control        (PC -> Raspberry Pi)
#define CMD_CONVEYOR_CONTROL    0x20
#define CMD_SPEED_SET           0x21
#define CMD_FAN_CONTROL         0x22
#define CMD_EMG_STOP            0x23
#define CMD_DEFECT_SERVO        0x24    // Defect detected -> activate servo to reject

// CMD: event          (Raspberry Pi -> PC)
#define CMD_ALERT_DEFECT        0x40
#define CMD_ALERT_WARNING       0x41

// CMD: response
#define CMD_ACK                 0x70
#define CMD_NACK                0x71

// Motor state values
#define MOTOR_OFF               0x00
#define MOTOR_SPEED_1           0x01
#define MOTOR_SPEED_2           0x02
#define MOTOR_SPEED_3           0x03
#define MOTOR_EMG_STOP          0xFF

// ON / OFF values
#define CTRL_OFF                0x00
#define CTRL_ON                 0x01

// Servo action values
#define SERVO_REJECT            0x01    // Activate servo to reject defect item

// Checksum: XOR of bytes IDX_CMD..IDX_MOTR
#define CALC_CS(pkt) \
    ((pkt)[IDX_CMD] ^ (pkt)[IDX_LEN] ^ (pkt)[IDX_TEMP] ^ \
     (pkt)[IDX_HUMI] ^ (pkt)[IDX_ULTR] ^ (pkt)[IDX_MOTR])

// Flask server on PC - set PC IP here
// (Change to actual IP when connecting from Raspberry Pi)
#define TCP_FLASK_HOST          "192.168.0.30"   // PC IP (Flask server)
#define TCP_FLASK_PORT          9000             // TCP listen port in flask_server.py

// HTTP / Flask endpoint
#define HTTP_FLASK_HOST         "192.168.0.30"   // PC IP
#define HTTP_FLASK_PORT         5000
#define HTTP_ENDPOINT_STATUS    "/api/status"
#define HTTP_ENDPOINT_CONTROL   "/api/control"

// Timer IDs
#define TIMER_SERIAL_READ       1001
#define TIMER_COMM_WATCHDOG     1002
#define TIMER_EMG_AUTO_RELEASE  1003
#define SERIAL_POLL_MS          100
#define COMM_TIMEOUT_MS         3000
#define EMG_AUTO_RELEASE_MS     60000   // 1 minute auto-release for emergency stop

// WM_APP offset for TCP receive notification
#define WM_TCP_RECEIVED         (WM_APP + 100)

// Serial port default settings
#define DEFAULT_SERIAL_PORT     L"COM4"
#define DEFAULT_BAUD_RATE       9600

// Communication status enum
enum eCommStatus
{
    COMM_OK = 0,
    COMM_ERROR = 1,
    COMM_TIMEOUT = 2
};