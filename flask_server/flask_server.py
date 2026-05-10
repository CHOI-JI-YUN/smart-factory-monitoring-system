# """
# flask_server.py
# ===============
# 컨베이어 시스템 Flask HTTP + TCP 서버

# 구조:
#   MFC(PC) ──HTTP POST──▶ Flask(192.168.0.30:5000)  : 센서 상태 전송
#   MFC(PC) ──HTTP GET───▶ Flask(192.168.0.30:5000)  : 제어 명령 수신
#   MFC(PC) ──TCP────────▶ Flask(192.168.0.30:9000)  : 실시간 연결

# 실행:
#     pip install flask
#     python flask_server.py
# """

# from flask import Flask, request, jsonify
# import socket
# import threading
# import time
# from datetime import datetime

# app = Flask(__name__)

# # ── 공유 상태 ─────────────────────────────────────────────────
# _lock = threading.Lock()

# system_status = {
#     "temp": 0.0,
#     "humi": 0.0,
#     "distance": 0,
#     "speed": 0,
#     "fan": False,
#     "emg_stop": False,
#     "updated_at": "",
#     "comm_ok": False,
# }

# control_queue = []
# tcp_clients   = []   # 연결된 MFC 소켓 목록

# # ── 연결 상태 플래그 (출력 중복 방지) ─────────────────────────
# _http_was_connected = False   # HTTP 최초 수신 여부
# _http_last_update   = ""      # 마지막 HTTP 수신 시각 (watchdog용)


# def now_str() -> str:
#     return datetime.now().strftime("%H:%M:%S")


# def log(tag: str, msg: str):
#     print(f"[{now_str()}][{tag}] {msg}")


# # ── HTTP API ───────────────────────────────────────────────────

# @app.route("/api/status", methods=["POST"])
# def api_post_status():
#     global _http_was_connected, _http_last_update

#     data = request.get_json(force=True, silent=True)
#     if not data:
#         return jsonify({"result": "error", "msg": "invalid JSON"}), 400

#     with _lock:
#         # ★ 최초 연결 시 1번만 출력
#         if not _http_was_connected:
#             log("HTTP", f"MFC 연결 성공! ({request.remote_addr})")
#             _http_was_connected = True

#         system_status["temp"]       = data.get("temp",     0.0)
#         system_status["humi"]       = data.get("humi",     0.0)
#         system_status["distance"]   = data.get("distance", 0)
#         system_status["speed"]      = data.get("speed",    0)
#         system_status["fan"]        = data.get("fan",      False)
#         system_status["emg_stop"]   = data.get("emg_stop", False)
#         system_status["updated_at"] = now_str()
#         system_status["comm_ok"]    = True

#     # 상태는 매 수신마다 출력 (타임스탬프 포함)
#     cur = system_status["updated_at"]
#     if cur != _http_last_update:
#         _http_last_update = cur
#         log("STATUS",
#             f"온도={system_status['temp']}°C  "
#             f"습도={system_status['humi']}%  "
#             f"거리={system_status['distance']}cm  "
#             f"속도={system_status['speed']}단계  "
#             f"환풍기={'ON' if system_status['fan'] else 'OFF'}")

#     return jsonify({"result": "ok"}), 200


# @app.route("/api/status", methods=["GET"])
# def api_get_status():
#     with _lock:
#         return jsonify(system_status), 200


# @app.route("/api/control", methods=["POST"])
# def api_post_control():
#     data = request.get_json(force=True, silent=True)
#     if not data or "cmd" not in data:
#         return jsonify({"result": "error", "msg": "cmd 필드 없음"}), 400

#     cmd = data["cmd"]
#     valid_cmds = [
#         "conveyor_on", "conveyor_off",
#         "fan_on",      "fan_off",
#         "speed_1",     "speed_2", "speed_3",
#         "emg_stop"
#     ]
#     if cmd not in valid_cmds:
#         return jsonify({"result": "error", "msg": f"알 수 없는 명령: {cmd}"}), 400

#     with _lock:
#         control_queue.append({"cmd": cmd, "time": now_str()})

#     log("CTRL", f"명령 등록: {cmd}")

#     # ★ TCP로 연결된 MFC에도 즉시 전송
#     tcp_cmd = cmd.upper().replace("_", ":") + "\n"
#     tcp_send_all(tcp_cmd)

#     return jsonify({"result": "ok", "cmd": cmd}), 200


# @app.route("/api/control", methods=["GET"])
# def api_get_control():
#     with _lock:
#         if control_queue:
#             return jsonify(control_queue.pop(0)), 200
#         return jsonify({}), 200


# # ── TCP 서버 ───────────────────────────────────────────────────

# def tcp_send_all(msg: str):
#     """연결된 모든 MFC에 메시지 전송"""
#     dead = []
#     with _lock:
#         for conn in tcp_clients:
#             try:
#                 conn.sendall(msg.encode("utf-8"))
#             except Exception:
#                 dead.append(conn)
#         for d in dead:
#             tcp_clients.remove(d)


# def tcp_server_thread():
#     server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#     server.bind(("0.0.0.0", 9000))
#     server.listen(5)
#     log("TCP", "서버 시작 - MFC 연결 대기 중 (port 9000)")

#     while True:
#         try:
#             conn, addr = server.accept()
#             log("TCP", f"MFC 연결 성공! ({addr[0]}:{addr[1]})")
#             with _lock:
#                 tcp_clients.append(conn)
#             t = threading.Thread(target=handle_mfc, args=(conn, addr), daemon=True)
#             t.start()

#             # ★ 연결 직후 PING 전송 (MFC가 PONG 응답하면 연결 확인)
#             try:
#                 conn.sendall(b"PING\n")
#             except Exception:
#                 pass

#         except Exception as e:
#             log("TCP", f"accept 오류: {e}")


# def handle_mfc(conn: socket.socket, addr):
#     buf = ""
#     try:
#         while True:
#             data = conn.recv(1024)
#             if not data:
#                 break
#             buf += data.decode("utf-8", errors="ignore")

#             while "\n" in buf:
#                 line, buf = buf.split("\n", 1)
#                 line = line.strip()
#                 if not line:
#                     continue
#                 if line == "PONG":
#                     log("TCP", f"MFC PONG 수신 - 연결 정상 ({addr[0]})")
#                     continue
#                 if line == "ACK:OK":
#                     continue
#                 log("TCP", f"MFC 수신: {line}")

#     except Exception as e:
#         log("TCP", f"수신 오류: {e}")
#     finally:
#         with _lock:
#             if conn in tcp_clients:
#                 tcp_clients.remove(conn)
#         log("TCP", f"MFC 연결 끊김: {addr[0]}")
#         conn.close()


# # ── PING 스레드 (30초마다 연결 유지) ─────────────────────────

# def ping_thread():
#     while True:
#         time.sleep(30)
#         if tcp_clients:
#             tcp_send_all("PING\n")


# # ── 통신 감시 스레드 ──────────────────────────────────────────

# def comm_watchdog():
#     global _http_was_connected
#     last = ""
#     while True:
#         time.sleep(5)
#         with _lock:
#             cur = system_status["updated_at"]
#             if cur == last and system_status["comm_ok"]:
#                 system_status["comm_ok"] = False
#                 _http_was_connected = False   # 재연결 시 로그 다시 출력
#                 log("WATCHDOG", "MFC HTTP 응답 없음 - 통신 오류")
#             last = cur


# # ── 메인 ──────────────────────────────────────────────────────

# if __name__ == "__main__":
#     threading.Thread(target=tcp_server_thread, daemon=True).start()
#     threading.Thread(target=ping_thread,       daemon=True).start()
#     threading.Thread(target=comm_watchdog,     daemon=True).start()

#     print("=" * 55)
#     print("  컨베이어 시스템 Flask 서버")
#     print("  HTTP : 0.0.0.0:5000")
#     print("  TCP  : 0.0.0.0:9000")
#     print("  MFC → HTTP → 192.168.0.30:5000")
#     print("  MFC → TCP  → 192.168.0.30:9000")
#     print("=" * 55)

#     app.run(host="0.0.0.0", port=5000, debug=False)