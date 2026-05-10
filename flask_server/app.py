# app.py
"""
스마트 컨베이어 품질 관리 시스템 - Flask 중앙 서버

구조:
  - REST API (포트 5000): 센서/불량/회원 데이터 처리
  - WebSocket (포트 5000): 실시간 알림, 채팅, 센서 데이터 푸시
  - Raw TCP (포트 9000): MFC 에지 컨트롤러 상시 연결

통신 흐름:
  MFC  ──② HTTP POST /api/status──►  Flask  ◄──④ HTTP──  C# (노트북)
  MFC  ◄─③ TCP 9000──►  Flask  ──⑤ WS───►  웹 브라우저 / C#

웹 대시보드에서 확인 가능한 항목:
  - 온도 / 습도  (sensor_update 이벤트)
  - 컨베이어 벨트 ON/OFF + 속도  (conveyor_status 이벤트)
  - MFC 연결 상태  (mfc_connection 이벤트)
"""

import base64
import threading
import os
from datetime import date

from flask import Flask, request, jsonify, send_from_directory
from flask_socketio import SocketIO
from flask_cors import CORS

from config import Config
from models import db, Member


# ============================================================
# 앱 생성
# ============================================================
app = Flask(__name__)
app.config.from_object(Config)

# MySQL 연결 설정
app.config["SQLALCHEMY_DATABASE_URI"] = "mysql+pymysql://root:6698@localhost/smart_conveyor"
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

# ★ 파일 업로드 설정
UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'uploads')
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 최대 16MB

# CORS 허용 (MFC, C#에서 HTTP 요청 가능하도록)
CORS(app)

# DB 초기화
db.init_app(app)

# WebSocket 초기화
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# ============================================================
# Blueprint 등록
# ============================================================
from routes.api import api_bp, set_socketio as api_set_socketio
from routes.auth import auth_bp, set_socketio as auth_set_socketio
from routes.admin import admin_bp

app.register_blueprint(api_bp, url_prefix='/api')
app.register_blueprint(auth_bp)
app.register_blueprint(admin_bp)

# socket 연결
api_set_socketio(socketio)
auth_set_socketio(socketio)

# ============================================================
# 이모티콘 API
# ============================================================
@app.route('/api/emoticons/<category>')
def get_emoticons(category):
    folder = os.path.join('static', 'emo')

    if not os.path.exists(folder):
        return jsonify([])

    files = sorted([
        f for f in os.listdir(folder)
        if f.lower().endswith(('.png', '.jpg', '.webp'))
    ])

    # 👉 3등분
    total = len(files)
    third = total // 3

    if category == 'gamjeong':
        selected = files[:third]
    elif category == 'gubangi':
        selected = files[third:2*third]
    elif category == 'ddangkong':
        selected = files[2*third:]
    else:
        selected = []

    urls = [f'/static/emo/{f}' for f in selected]

    return jsonify(urls)

# API 라우트에 socketio 인스턴스 전달
api_set_socketio(socketio)
auth_set_socketio(socketio)


# ============================================================
# WebSocket 핸들러 등록
# ============================================================
from socket_handlers import register_handlers
register_handlers(socketio, app)


@app.route('/reset-online-users')
def reset_online_users():
    from socket_handlers import online_users, sid_to_user
    online_users.clear()
    sid_to_user.clear()
    return '초기화 완료'


# ============================================================
# /predict — C#에서 분석 완료된 프레임 수신 → 웹 모니터 브로드캐스트
# ============================================================

@app.route('/predict', methods=['POST'])
def predict():
    """
    C# WinForm에서 분석 완료된 프레임 수신
    → Socket.IO로 웹 모니터에 실시간 브로드캐스트
    → C#에 결과 반환
    """
    # ── JSON 방식: C#에서 base64로 보낸 경우 ──
    if request.is_json:
        try:
            data = request.get_json(force=True, silent=True)
            if not data or 'image_data' not in data:
                return jsonify({'success': False, 'error': 'image_data 필드 없음'}), 400

            img_base64 = data['image_data']
            is_defective = data.get('is_defective', False)

            # MFC 모니터링용 프레임 저장
            try:
                from routes.api import store_latest_frame
                store_latest_frame(base64.b64decode(img_base64))
            except Exception:
                pass

            socketio.emit('video_frame', {'frame': img_base64})

            return jsonify({
                'success': True,
                'is_defective': is_defective,
                'image_data': img_base64,
            })
        except Exception as e:
            print(f"[PREDICT][JSON] 오류: {e}")
            return jsonify({'success': False, 'error': str(e)}), 500

    # ── Multipart 방식: C#에서 파일로 보낸 경우 ──
    if 'image' in request.files:
        try:
            image_bytes = request.files['image'].read()
            img_base64 = base64.b64encode(image_bytes).decode('utf-8')

            # MFC 모니터링용 프레임 저장
            try:
                from routes.api import store_latest_frame
                store_latest_frame(image_bytes)
            except Exception:
                pass

            print(f"[PREDICT] emit frame size = {len(img_base64)}")
            socketio.emit('video_frame', {'frame': img_base64})

            return jsonify({
                'success': True,
                'is_defective': False,
                'image_data': img_base64,
            })
        except Exception as e:
            print(f"[PREDICT][MULTIPART] 오류: {e}")
            return jsonify({'success': False, 'error': str(e)}), 500

    return jsonify({'success': False, 'error': 'image 또는 image_data 필요'}), 400


# ============================================================
# Raw TCP 서버 시작 (백그라운드 스레드)
# ============================================================
from tcp_server import start_tcp_server, set_socketio as tcp_set_socketio
tcp_set_socketio(socketio)


# ============================================================
# DB 테이블 생성 + 기본 관리자 계정
# ============================================================
def init_db():
    """DB 테이블 생성 및 기본 데이터 삽입"""
    with app.app_context():
        db.create_all()

        admin = Member.query.get('admin')

        if not admin:
            admin = Member(
                id='admin',
                password='1234',
                name='관리자',
                birth=date(1995, 3, 15),
                position='관리자',
                role='admin'
            )
            db.session.add(admin)
            print("[DB] 기본 관리자 계정 생성 (admin / 1234)")
        else:
            changed = False
            if not admin.name or admin.name != '관리자':
                admin.name = '관리자'
                changed = True
            if not admin.birth:
                admin.birth = date(1995, 3, 15)
                changed = True
            if not admin.position or admin.position != '관리자':
                admin.position = '관리자'
                changed = True
            if getattr(admin, 'role', None) != 'admin':
                admin.role = 'admin'
                changed = True
            if changed:
                print("[DB] 기존 관리자 계정 정보 보정 완료")

        db.session.commit()

        # 서버 시작 시 미처리 출석 기록 정리
        from models import Attendance
        from datetime import datetime
        unfinished = Attendance.query.filter_by(check_out=None).all()
        if unfinished:
            for a in unfinished:
                a.check_out = datetime.now()
            db.session.commit()
            print(f"[DB] 미처리 출석 기록 {len(unfinished)}건 정리 완료")

        print("[DB] 테이블 초기화 완료")


# ============================================================
# 실행
# ============================================================
if __name__ == '__main__':
    # DB 초기화
    init_db()

    # Raw TCP 서버를 백그라운드 스레드로 시작
    tcp_thread = threading.Thread(
        target=start_tcp_server,
        args=(Config.TCP_PORT,),
        daemon=True
    )
    tcp_thread.start()

    print("=" * 50)
    print("  스마트 컨베이어 Flask 서버 시작")
    print(f"  HTTP/WS : http://0.0.0.0:5000")
    print(f"  TCP     : 0.0.0.0:{Config.TCP_PORT}")
    print(f"  /predict: C# 분석 완료 프레임 → Socket.IO 브로드캐스트")
    print("=" * 50)
    print()
    print("  웹 대시보드에서 수신하는 Socket.IO 이벤트:")
    print("    sensor_update    → 온도, 습도")
    print("    conveyor_status  → 컨베이어 ON/OFF, 속도, 팬, 비상정지")
    print("    mfc_connection   → MFC 연결 상태 (true/false)")
    print("=" * 50)

    # Flask + SocketIO 시작
    socketio.run(
        app,
        host='0.0.0.0',
        port=5000,
        debug=False,
        allow_unsafe_werkzeug=True
    )