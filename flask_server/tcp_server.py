# tcp_server.py
"""
Raw TCP 서버 — MFC 에지 컨트롤러와 상시 연결 (포트 9000)

역할:
  - MFC가 TCP 클라이언트로 접속 → 연결 유지
  - Flask → MFC: 명령 전송 (EMERGENCY:STOP, CONVEYOR:ON 등)
  - MFC → Flask: ACK/PONG 수신
  - 주기적 PING으로 연결 상태 확인
  - MFC 연결/해제 시 WebSocket으로 웹 대시보드에 알림
"""

import socket
import threading
import time
import json

# ── 모듈 전역 ──
_socketio = None
_mfc_clients = []          # 연결된 MFC 소켓 리스트
_mfc_clients_lock = threading.Lock()
_mfc_connected = False     # 현재 MFC 연결 여부


def set_socketio(sio):
    global _socketio
    _socketio = sio


def is_mfc_connected():
    """MFC 연결 상태 조회"""
    return _mfc_connected


def broadcast_to_mfc(message):
    """연결된 모든 MFC 클라이언트에 메시지 전송. 성공 수 반환."""
    with _mfc_clients_lock:
        print(f"[TCP] broadcast_to_mfc: 메시지='{message.strip()}', 연결된 클라이언트 수={len(_mfc_clients)}")
        if len(_mfc_clients) == 0:
            print("[TCP] ⚠ MFC 클라이언트가 없습니다! TCP 연결을 확인하세요.")
            return 0
        dead = []
        sent = 0
        for client_sock in _mfc_clients:
            try:
                client_sock.sendall(message.encode('utf-8'))
                print(f"[TCP] ✓ MFC에 전송 성공: {message.strip()}")
                sent += 1
            except Exception as e:
                print(f"[TCP] ✗ MFC 전송 실패: {e}")
                dead.append(client_sock)
        for d in dead:
            _mfc_clients.remove(d)
            try:
                d.close()
            except Exception:
                pass
        return sent


def send_defect_servo():
    """불량 검출 시 MFC에 서보모터 작동 명령"""
    broadcast_to_mfc("DEFECT:SERVO\n")


def send_alert(alert_type):
    """센서 임계값 초과 알림을 MFC에 전송"""
    broadcast_to_mfc(f"ALERT:{alert_type}\n")


def _notify_mfc_status(connected):
    """MFC 연결 상태를 WebSocket으로 웹에 브로드캐스트"""
    global _mfc_connected
    _mfc_connected = connected

    if _socketio:
        _socketio.emit('mfc_connection', {
            'connected': connected,
            'timestamp': time.strftime('%H:%M:%S')
        })
        print(f"[TCP] MFC 연결 상태 → 웹 브로드캐스트: {'연결됨' if connected else '해제됨'}")


def _handle_client(client_sock, addr):
    """개별 MFC 클라이언트 처리 스레드"""
    print(f"[TCP] MFC 연결됨: {addr}")

    with _mfc_clients_lock:
        _mfc_clients.append(client_sock)

    _notify_mfc_status(True)

    buffer = ""
    try:
        while True:
            data = client_sock.recv(1024)
            if not data:
                break

            buffer += data.decode('utf-8', errors='ignore')

            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip()
                if not line:
                    continue

                # PONG 응답 처리 (PING에 대한 응답)
                if line == "PONG":
                    print(f"[TCP] MFC PONG 수신 - 연결 정상")
                    continue

                # ACK 처리
                if line.startswith("ACK:"):
                    print(f"[TCP] ✓ MFC ACK 수신: {line}")
                    continue

                # JSON 상태 데이터 처리 (MFC에서 TCP로 상태 전송 시)
                if line.startswith('{'):
                    try:
                        status = json.loads(line)
                        if _socketio:
                            _socketio.emit('sensor_update', {
                                'temp': status.get('temp', 0),
                                'humidity': status.get('humi', 0),
                                'timestamp': time.strftime('%H:%M:%S')
                            })
                            conveyor_on = status.get('speed', 0) > 0
                            _socketio.emit('conveyor_status', {
                                'status': 'on' if conveyor_on else 'off',
                                'speed': status.get('speed', 0),
                                'fan': status.get('fan', False),
                                'emg_stop': status.get('emg_stop', False),
                                'timestamp': time.strftime('%H:%M:%S')
                            })
                    except json.JSONDecodeError:
                        pass
                    continue

                print(f"[TCP] MFC 수신: {line}")

    except (ConnectionResetError, ConnectionAbortedError, OSError):
        pass
    finally:
        with _mfc_clients_lock:
            if client_sock in _mfc_clients:
                _mfc_clients.remove(client_sock)
        try:
            client_sock.close()
        except Exception:
            pass

        print(f"[TCP] MFC 연결 해제: {addr}")

        # 남은 클라이언트가 없으면 해제 알림
        with _mfc_clients_lock:
            if len(_mfc_clients) == 0:
                _notify_mfc_status(False)


def _ping_loop():
    """30초마다 PING 전송 → MFC가 PONG으로 응답"""
    while True:
        time.sleep(30)
        broadcast_to_mfc("PING\n")


def start_tcp_server(port=9000):
    """TCP 서버 시작 (백그라운드 스레드에서 호출)"""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', port))
    server.listen(5)

    print(f"[TCP] Raw TCP 서버 시작 — 포트 {port}")

    # PING 루프 스레드
    ping_thread = threading.Thread(target=_ping_loop, daemon=True)
    ping_thread.start()

    while True:
        try:
            client_sock, addr = server.accept()
            t = threading.Thread(target=_handle_client, args=(client_sock, addr), daemon=True)
            t.start()
        except Exception as e:
            print(f"[TCP] accept 오류: {e}")
            time.sleep(1)