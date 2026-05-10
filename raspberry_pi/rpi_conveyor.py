"""
rpi_conveyor.py
===============
Conveyor System - Raspberry Pi RS232 communication code
MFC(PC) <-> Raspberry Pi

Hardware:
  - RS232 TX/RX  : GPIO14(TX), GPIO15(RX)  via MAX3232 level converter
  - DHT11 sensor : GPIO26 (BCM) - 3pin module
  - DC Motor IN1 : GPIO12 (BCM) - PWM (speed)
  - DC Motor IN2 : GPIO13 (BCM) - Direction fixed LOW
  - Stepper IN1  : GPIO17 (BCM)
  - Stepper IN2  : GPIO18 (BCM)
  - Stepper IN3  : GPIO27 (BCM)
  - Stepper IN4  : GPIO22 (BCM)

Control mapping:
  Conveyor ON/OFF  → DC 모터 ON/OFF
  Speed 1/2/3      → DC 듀티 50/75/100%
  Emergency Stop   → DC 모터 정지
  Fan ON/OFF       → 스텝모터 ON/OFF

DC Motor direction (바꾸려면 두 줄 swap):
  DC_PWM_PIN = 12  ↔  13
  DC_DIR_PIN = 13  ↔  12

Run:
  sudo python3 rpi_conveyor.py
"""

import serial
import time
import threading
import lgpio

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Protocol constants
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
PACKET_SIZE = 9
STX = 0x02
ETX = 0x03

IDX_STX  = 0
IDX_CMD  = 1
IDX_LEN  = 2
IDX_TEMP = 3
IDX_HUMI = 4
IDX_ULTR = 5
IDX_MOTR = 6
IDX_CS   = 7
IDX_ETX  = 8

CMD_STATUS_REPORT    = 0x10
CMD_CONVEYOR_CONTROL = 0x20
CMD_SPEED_SET        = 0x21
CMD_FAN_CONTROL      = 0x22
CMD_EMG_STOP         = 0x23
CMD_ACK  = 0x70
CMD_NACK = 0x71

MOTOR_OFF     = 0x00
MOTOR_SPEED_1 = 0x01
MOTOR_SPEED_2 = 0x02
MOTOR_SPEED_3 = 0x03
MOTOR_EMG     = 0xFF

CTRL_OFF = 0x00
CTRL_ON  = 0x01

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# GPIO settings
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
SERIAL_PORT = "/dev/ttyS0"
BAUD_RATE   = 9600

DHT_PIN = 26

DC_PWM_PIN = 12
DC_DIR_PIN = 13

STEP_IN1 = 17
STEP_IN2 = 18
STEP_IN3 = 27
STEP_IN4 = 22

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Config
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DC_PWM_FREQ = 1000

DC_SPEED_DUTY = {
    MOTOR_SPEED_1: 50,
    MOTOR_SPEED_2: 75,
    MOTOR_SPEED_3: 100,
}

STEP_SEQUENCE = [
    [1, 0, 1, 0],
    [0, 1, 1, 0],
    [0, 1, 0, 1],
    [1, 0, 0, 1],
]

STEP_DELAY = 0.005


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Helper
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
def gpio_safe_free(h, gpio):
    try:
        lgpio.gpio_free(h, gpio)
    except lgpio.error:
        pass

def gpio_safe_claim_output(h, gpio, level=0):
    gpio_safe_free(h, gpio)
    lgpio.gpio_claim_output(h, gpio, level)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Packet utilities
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
def calc_cs(pkt):
    return (pkt[IDX_CMD] ^ pkt[IDX_LEN] ^
            pkt[IDX_TEMP] ^ pkt[IDX_HUMI] ^
            pkt[IDX_ULTR] ^ pkt[IDX_MOTR]) & 0xFF

def build_packet(cmd, temp_byte, humi_byte, ultr_byte, motr):
    pkt = [0] * PACKET_SIZE
    pkt[IDX_STX]  = STX
    pkt[IDX_CMD]  = cmd        & 0xFF
    pkt[IDX_LEN]  = 5
    pkt[IDX_TEMP] = temp_byte  & 0xFF
    pkt[IDX_HUMI] = humi_byte  & 0xFF
    pkt[IDX_ULTR] = ultr_byte  & 0xFF
    pkt[IDX_MOTR] = motr       & 0xFF
    pkt[IDX_CS]   = calc_cs(pkt)
    pkt[IDX_ETX]  = ETX
    return bytes(pkt)

def validate_packet(data):
    if len(data) != PACKET_SIZE:  return False
    if data[IDX_STX] != STX:     return False
    if data[IDX_ETX] != ETX:     return False
    cs = (data[IDX_CMD] ^ data[IDX_LEN] ^
          data[IDX_TEMP] ^ data[IDX_HUMI] ^
          data[IDX_ULTR] ^ data[IDX_MOTR]) & 0xFF
    return data[IDX_CS] == cs

def send_ack(ser, success):
    cmd = CMD_ACK if success else CMD_NACK
    ser.write(build_packet(cmd, 0, 0, 0, 0))


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# DC Motor - conveyor belt (PWM)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
class DCMotor:
    def __init__(self, h, pwm_pin, dir_pin):
        self._h        = h
        self._pwm_pin  = pwm_pin
        self._dir_pin  = dir_pin
        self._speed    = MOTOR_OFF
        self._duty     = 0
        gpio_safe_claim_output(self._h, self._dir_pin, 0)
        gpio_safe_claim_output(self._h, self._pwm_pin, 0)

    def _force_stop(self):
        """PWM 끄고 핀을 강제 LOW — 확실한 정지"""
        lgpio.tx_pwm(self._h, self._pwm_pin, 0, 0)
        time.sleep(0.01)
        gpio_safe_claim_output(self._h, self._pwm_pin, 0)
        lgpio.gpio_write(self._h, self._dir_pin, 0)

    def set_speed(self, speed):
        if speed == MOTOR_EMG or speed == MOTOR_OFF:
            self._speed = MOTOR_OFF
            self._duty  = 0
            self._force_stop()
        else:
            self._speed = speed
            self._duty  = DC_SPEED_DUTY.get(speed, 50)
            lgpio.tx_pwm(self._h, self._pwm_pin, DC_PWM_FREQ, self._duty)

    def get_speed(self):  return self._speed
    def get_duty(self):   return self._duty

    def cleanup(self):
        self._force_stop()
        gpio_safe_free(self._h, self._pwm_pin)
        gpio_safe_free(self._h, self._dir_pin)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Stepper Motor - fan (ON/OFF only)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
class StepMotor:
    def __init__(self, h, pins):
        self._h          = h
        self._pins       = pins
        self._running    = False
        self._step_idx   = 0
        self._lock       = threading.Lock()
        self._stop_event = threading.Event()
        self._thread     = threading.Thread(target=self._run, daemon=True)
        for pin in self._pins:
            gpio_safe_claim_output(self._h, pin, 0)
        self._thread.start()

    def _set_coils(self, pattern):
        for pin, val in zip(self._pins, pattern):
            lgpio.gpio_write(self._h, pin, val)

    def _all_off(self):
        for pin in self._pins:
            lgpio.gpio_write(self._h, pin, 0)

    def _run(self):
        while not self._stop_event.is_set():
            with self._lock:
                running = self._running
            if not running:
                self._all_off()
                time.sleep(0.05)
                continue
            self._set_coils(STEP_SEQUENCE[self._step_idx])
            self._step_idx = (self._step_idx + 1) % len(STEP_SEQUENCE)
            time.sleep(STEP_DELAY)

    def set_on(self, on):
        with self._lock:
            self._running = on

    def is_on(self):
        with self._lock:
            return self._running

    def cleanup(self):
        self._stop_event.set()
        self._thread.join(timeout=2.0)
        self._all_off()
        for pin in self._pins:
            gpio_safe_free(self._h, pin)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# DHT11 reader
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
class DHT11Reader:
    def __init__(self, h, gpio_num):
        self._h    = h
        self._gpio = gpio_num
        self._last_temp = 0.0
        self._last_humi = 0.0

    def _read_raw(self):
        g = self._gpio
        h = self._h
        edges = []
        def on_edge(chip, gpio, level, tick):
            edges.append((level, tick))
        gpio_safe_claim_output(h, g, 1)
        lgpio.gpio_write(h, g, 0)
        time.sleep(0.020)
        lgpio.gpio_write(h, g, 1)
        time.sleep(0.00002)
        gpio_safe_free(h, g)
        lgpio.gpio_claim_alert(h, g, lgpio.BOTH_EDGES, lgpio.SET_PULL_UP)
        cb = lgpio.callback(h, g, lgpio.BOTH_EDGES, on_edge)
        time.sleep(0.1)
        cb.cancel()
        gpio_safe_free(h, g)
        return edges

    def _parse(self, edges):
        high_durations = []
        for i in range(len(edges) - 1):
            if edges[i][0] == 1 and edges[i + 1][0] == 0:
                dur = edges[i + 1][1] - edges[i][1]
                if dur < 0: dur += (1 << 64)
                high_durations.append(dur)
        if len(high_durations) < 38:
            return None, None
        data_pulses = high_durations[-40:] if len(high_durations) >= 40 else high_durations
        threshold = sum(data_pulses) / len(data_pulses)
        bits = [1 if dur > threshold else 0 for dur in data_pulses]
        while len(bits) < 40: bits.insert(0, 0)
        bits = bits[-40:]
        data = []
        byte = 0
        for i, b in enumerate(bits):
            byte = (byte << 1) | b
            if (i + 1) % 8 == 0:
                data.append(byte)
                byte = 0
        if len(data) != 5: return None, None
        if ((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]:
            return None, None
        humi = data[0] + data[1] * 0.1
        temp = data[2] + (data[3] & 0x7F) * 0.1
        if data[3] & 0x80: temp = -temp
        return temp, humi

    def read(self):
        for attempt in range(5):
            try:
                edges = self._read_raw()
                if len(edges) < 2:
                    time.sleep(1.0); continue
                t, h = self._parse(edges)
                if t is not None and h is not None:
                    self._last_temp = t
                    self._last_humi = h
                    return self._last_temp, self._last_humi
                else:
                    print(f"[DHT11] Parse failed (edges={len(edges)}, attempt {attempt+1})")
            except Exception as e:
                print(f"[DHT11] Error: {e}")
            time.sleep(1.0)
        print("[DHT11] Read failed, using last value")
        return self._last_temp, self._last_humi

    def cleanup(self):
        gpio_safe_free(self._h, self._gpio)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Main
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
def main():
    h = lgpio.gpiochip_open(0)

    dht     = DHT11Reader(h, DHT_PIN)
    dc      = DCMotor(h, DC_PWM_PIN, DC_DIR_PIN)
    stepper = StepMotor(h, [STEP_IN1, STEP_IN2, STEP_IN3, STEP_IN4])

    try:
        ser = serial.Serial(
            port=SERIAL_PORT, baudrate=BAUD_RATE,
            bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE, timeout=0.1
        )
        print(f"[SERIAL] {SERIAL_PORT} opened ({BAUD_RATE} bps)")
    except serial.SerialException as e:
        print(f"[ERROR] Serial: {e}")
        dht.cleanup(); dc.cleanup(); stepper.cleanup()
        lgpio.gpiochip_close(h)
        return

    last_report = 0.0
    print("[SYSTEM] Started")
    print(f"[SYSTEM] DC Motor(conveyor): PWM=GPIO{DC_PWM_PIN}, DIR=GPIO{DC_DIR_PIN}")
    print(f"[SYSTEM] Stepper(fan): GPIO {STEP_IN1}/{STEP_IN2}/{STEP_IN3}/{STEP_IN4}")
    print(f"[SYSTEM] DC duty: Speed1={DC_SPEED_DUTY[1]}% Speed2={DC_SPEED_DUTY[2]}% Speed3={DC_SPEED_DUTY[3]}%")

    try:
        while True:
            now = time.time()
            if now - last_report >= 2.0:
                temp, humi = dht.read()
                cur_speed = dc.get_speed()

                temp_int = int(temp) & 0xFF
                humi_int = int(humi) & 0xFF
                temp_dec = int(round(temp * 10)) % 10
                humi_dec = int(round(humi * 10)) % 10
                ultr_packed = ((temp_dec & 0x0F) << 4) | (humi_dec & 0x0F)

                pkt = build_packet(CMD_STATUS_REPORT,
                                   temp_int, humi_int, ultr_packed, cur_speed)
                ser.write(pkt)
                fan_str = "ON" if stepper.is_on() else "OFF"
                print(f"[TX] Temp={temp:.1f}C Humi={humi:.1f}% "
                      f"DC={cur_speed}({dc.get_duty()}%) Fan={fan_str}")
                last_report = now

            if ser.in_waiting >= PACKET_SIZE:
                raw = ser.read(PACKET_SIZE)
                if not validate_packet(raw):
                    print(f"[RX] Packet error: {raw.hex().upper()}")
                    send_ack(ser, False)
                    continue

                cmd  = raw[IDX_CMD]
                data = raw[IDX_MOTR]
                print(f"[RX] CMD=0x{cmd:02X} DATA=0x{data:02X}")

                # ── Conveyor ON/OFF → DC motor ──
                if cmd == CMD_CONVEYOR_CONTROL:
                    if data == CTRL_ON:
                        dc.set_speed(MOTOR_SPEED_1)
                        print(f"[DC] Conveyor ON (duty={dc.get_duty()}%)")
                    else:
                        dc.set_speed(MOTOR_OFF)
                        print("[DC] Conveyor OFF -> STOPPED")
                    send_ack(ser, True)

                # ── Speed 1/2/3 → DC motor duty ──
                elif cmd == CMD_SPEED_SET:
                    if data in (MOTOR_SPEED_1, MOTOR_SPEED_2, MOTOR_SPEED_3):
                        dc.set_speed(data)
                        print(f"[DC] Speed {data} (duty={dc.get_duty()}%)")
                        send_ack(ser, True)
                    else:
                        print(f"[DC] Invalid speed: 0x{data:02X}")
                        send_ack(ser, False)

                # ── Fan ON/OFF → Stepper motor ──
                elif cmd == CMD_FAN_CONTROL:
                    if data == CTRL_ON:
                        stepper.set_on(True)
                        print("[STEPPER] Fan ON")
                    else:
                        stepper.set_on(False)
                        print("[STEPPER] Fan OFF")
                    send_ack(ser, True)

                # ── Emergency Stop → DC motor only ──
                elif cmd == CMD_EMG_STOP:
                    dc.set_speed(MOTOR_EMG)
                    print("[DC] *** EMERGENCY STOP *** -> STOPPED")
                    send_ack(ser, True)

                else:
                    print(f"[RX] Unknown: 0x{cmd:02X}")
                    send_ack(ser, False)

            time.sleep(0.05)

    except KeyboardInterrupt:
        print("\n[SYSTEM] Shutdown (Ctrl+C)")

    finally:
        dc.set_speed(MOTOR_OFF)
        stepper.set_on(False)
        dc.cleanup()
        stepper.cleanup()
        dht.cleanup()
        ser.close()
        lgpio.gpiochip_close(h)
        print("[SYSTEM] Cleanup complete.")


if __name__ == "__main__":
    main()

