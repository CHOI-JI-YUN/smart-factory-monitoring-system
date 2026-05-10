# routes/api.py
"""
REST API 엔드포인트

② MFC → Flask:  POST /api/status       (센서 + 컨베이어 상태)
                 POST /api/sensor/batch  (센서 배치 업로드)
④ C#  → Flask:  POST /api/defect        (불량 이벤트)
⑤ 브라우저:     GET/POST /api/members   (회원 CRUD)
                GET /api/defect          (불량 이력 조회)
"""

from flask import Blueprint, request, jsonify
from datetime import datetime, date
from models import db, SensorLog, SensorHourly, DefectEvent, Member, ChatMessage, MemberDeleteLog, DailyInspection, MessageRead

api_bp = Blueprint('api', __name__)

# socketio는 app.py에서 주입됨
_socketio = None

def set_socketio(sio):
    global _socketio
    _socketio = sio


# ============================================================
# 최신 프레임 저장 (MFC 모니터링용)
# ============================================================
import threading as _th
_frame_lock = _th.Lock()
_frame_jpeg = None

def store_latest_frame(jpeg_bytes):
    global _frame_jpeg
    with _frame_lock:
        _frame_jpeg = jpeg_bytes

def get_latest_frame():
    with _frame_lock:
        return _frame_jpeg


# ============================================================
# MFC 센서/컨베이어 상태 (공유 딕셔너리)
# ============================================================

_mfc_status = {
    'temp': 0.0,
    'humi': 0.0,
    'distance': 0,
    'speed': 0,
    'fan': False,
    'emg_stop': False,
    'updated_at': '',
    'comm_ok': False,
    'conveyor_on': False,
}


# ============================================================
# ② MFC → Flask: POST /api/status (센서 상태 수신)
# ============================================================

@api_bp.route('/status', methods=['POST'])
def api_post_status():
    """
    MFC에서 센서 상태 수신 → WebSocket 브로드캐스트

    MFC가 3초마다 HTTP POST로 전송하는 데이터:
    {
        "temp": 25.3,
        "humi": 45.2,
        "distance": 10,
        "speed": 2,
        "conveyor_on": true
        "fan": true,
        "emg_stop": false
    }
    """
    data = request.get_json(force=True, silent=True)
    if not data:
        return jsonify({'result': 'error', 'msg': 'invalid JSON'}), 400

    # 상태 업데이트
    _mfc_status['temp']     = data.get('temp', 0.0)
    _mfc_status['humi']     = data.get('humi', 0.0)
    _mfc_status['distance'] = data.get('distance', 0)
    _mfc_status['speed']    = data.get('speed', 0)
    _mfc_status['fan']      = data.get('fan', False)
    _mfc_status['emg_stop'] = data.get('emg_stop', False)
    _mfc_status['updated_at'] = datetime.now().strftime('%H:%M:%S')
    _mfc_status['comm_ok']  = True

    # 컨베이어 ON/OFF 판단 (speed > 0 이면 ON)
    conveyor_on = _mfc_status['speed'] > 0
    _mfc_status['conveyor_on'] = conveyor_on

    # ── WebSocket 브로드캐스트 (웹 대시보드로 실시간 전달) ──
    if _socketio:
        # 1) 온도 + 습도
        _socketio.emit('sensor_update', {
            'temp': _mfc_status['temp'],
            'humidity': _mfc_status['humi'],
            'timestamp': _mfc_status['updated_at']
        })

        # LED 상태 판단 (라즈베리파이 LED와 동기화)
        if _mfc_status['emg_stop']:
            led_state = 'red'       # 비상정지
        elif _mfc_status['speed'] == 0 and data.get('conveyor_on', False):
            led_state = 'yellow'    # IR 검증 중 (물체 감지 → 일시정지)
        elif _mfc_status['speed'] > 0:
            led_state = 'green'     # 정상 가동
        else:
            led_state = 'off'       # 대기

        # 2) 컨베이어 벨트 ON/OFF + 속도 + 팬 + 비상정지 + LED 상태
        _socketio.emit('conveyor_status', {
            'status': 'on' if conveyor_on else 'off',
            'speed': _mfc_status['speed'],
            'fan': _mfc_status['fan'],
            'emg_stop': _mfc_status['emg_stop'],
            'led_state': led_state,
            'timestamp': _mfc_status['updated_at']
        })

        # 3) MFC 연결 상태 (HTTP POST가 도착했으므로 연결 OK)
        _socketio.emit('mfc_connection', {
            'connected': True,
            'timestamp': _mfc_status['updated_at']
        })

    # ── DB에 센서 데이터 저장 ──
    try:
        log = SensorLog(
            temperature=_mfc_status['temp'],
            humidity=_mfc_status['humi']
        )
        db.session.add(log)
        db.session.commit()
    except Exception as e:
        print(f"[API] 센서 로그 저장 오류: {e}")
        db.session.rollback()

    return jsonify({'result': 'ok'}), 200


@api_bp.route('/status', methods=['GET'])
def api_get_status():
    """현재 MFC 상태 조회 (웹 대시보드 폴링 / 디버깅용)"""
    from tcp_server import is_mfc_connected
    status = dict(_mfc_status)
    status['mfc_tcp_connected'] = is_mfc_connected()
    return jsonify(status), 200


# ============================================================
# 웹 → MFC 제어 명령
# ============================================================

@api_bp.route('/control', methods=['POST'])
def api_post_control():
    """웹에서 MFC 제어 명령 전송 (TCP를 통해 MFC로)"""
    data = request.get_json(force=True, silent=True)
    if not data or 'cmd' not in data:
        return jsonify({'result': 'error', 'msg': 'cmd 필드 없음'}), 400

    cmd = data['cmd']
    from tcp_server import broadcast_to_mfc
    tcp_cmd = cmd.upper().replace('_', ':') + '\n'
    broadcast_to_mfc(tcp_cmd)

    return jsonify({'result': 'ok', 'cmd': cmd}), 200


@api_bp.route('/control', methods=['GET'])
def api_get_control():
    """MFC 제어 명령 폴링 (현재 미사용, 확장용)"""
    return jsonify({}), 200


# ============================================================
# ② MFC → Flask: POST /api/sensor/batch (센서 배치 업로드)
# ============================================================

@api_bp.route('/sensor/batch', methods=['POST'])
def sensor_batch():
    """MFC에서 센서 데이터 배치 업로드 (10개씩)"""
    data = request.json
    if not data or 'data' not in data:
        return jsonify({'status': 'error', 'message': 'data 필드 필요'}), 400

    items = data['data']
    inserted = 0
    latest = None

    for item in items:
        try:
            ts = datetime.fromisoformat(item['timestamp']) if 'timestamp' in item else datetime.now()
            log = SensorLog(
                timestamp=ts,
                temperature=item.get('temp', 0),
                humidity=item.get('humidity', 0)
            )
            db.session.add(log)
            inserted += 1
            latest = item
        except Exception as e:
            print(f"[API] 센서 데이터 파싱 오류: {e}")
            continue

    db.session.commit()

    # 최신 데이터를 WebSocket으로 브로드캐스트
    if latest and _socketio:
        _socketio.emit('sensor_update', {
            'temp': latest.get('temp', 0),
            'humidity': latest.get('humidity', 0),
            'timestamp': datetime.now().strftime('%H:%M:%S')
        })
        _check_thresholds(latest)

    return jsonify({'status': 'ok', 'inserted': inserted})


def _check_thresholds(sensor_data):
    """센서 임계값 확인 → 초과 시 알림"""
    from flask import current_app
    from tcp_server import send_alert

    temp = sensor_data.get('temp', 0)
    humidity = sensor_data.get('humidity', 0)
    config = current_app.config

    if temp > config.get('TEMP_HIGH', 30.0):
        send_alert('TEMP_HIGH')
        if _socketio:
            _socketio.emit('sensor_alert', {
                'reason': 'temp_high',
                'value': temp,
                'message': f'온도 {temp}℃ - 상한 초과'
            })

    if temp < config.get('TEMP_LOW', 10.0):
        if _socketio:
            _socketio.emit('sensor_alert', {
                'reason': 'temp_low',
                'value': temp,
                'message': f'온도 {temp}℃ - 하한 미달'
            })

    if humidity > config.get('HUMIDITY_HIGH', 70.0):
        send_alert('HUMIDITY_HIGH')
        if _socketio:
            _socketio.emit('sensor_alert', {
                'reason': 'humidity_high',
                'value': humidity,
                'message': f'습도 {humidity}% - 상한 초과'
            })


# ============================================================
# ④ 불량 이벤트 (모니터링 → Flask)
# ============================================================

@api_bp.route('/defect', methods=['POST'])
def new_defect():
    """
    모니터링에서 검사 이벤트 수신
    - 불량: timestamp + defect_type + defect_count
    - 정상: timestamp만 (defect_type 없으면 정상 판정)
    """
    data = request.json
    if not data:
        return jsonify({'status': 'error', 'message': 'JSON 데이터 필요'}), 400

    try:
        ts = datetime.fromisoformat(data['timestamp']) if 'timestamp' in data else datetime.now()
    except Exception:
        ts = datetime.now()

    # ★ defect_type이 없거나 비어있으면 → 정상
    defect_type = data.get('defect_type', '').strip()
    if not defect_type:
        defect_type = 'normal'

    defect_count = data.get('defect_count', 1) if defect_type != 'normal' else 0

    event = DefectEvent(
        timestamp=ts,
        defect_type=defect_type,
        defect_count=defect_count,
        confidence=data.get('confidence', 0),
        image_path=data.get('image_path', ''),
        ultrasonic_matched=data.get('ultrasonic_matched', False)
    )

    try:
        db.session.add(event)
        db.session.commit()
    except Exception as e:
        print(f"[API] defect 저장 오류: {e}")
        db.session.rollback()
        return jsonify({'status': 'error', 'message': str(e)}), 500

    # TCP로 MFC에 서보모터 작동 명령 (불량일 때만)
    if defect_type != 'normal':
        try:
            from tcp_server import send_defect_servo
            send_defect_servo()
        except Exception:
            pass

        # WebSocket으로 웹에 실시간 알림
        if _socketio:
            _socketio.emit('new_defect', {
                'id': event.id,
                'type': event.defect_type,
                'defect_count': event.defect_count,
                'confidence': event.confidence,
                'timestamp': ts.strftime('%H:%M:%S')
            })

        print(f"[API] 불량 검출: {event.defect_type} / 개수: {event.defect_count} / 시간: {ts.strftime('%Y-%m-%d %H:%M:%S')}")
    else:
        # 정상 이벤트도 실시간 갱신
        if _socketio:
            _socketio.emit('normal_event', {
                'id': event.id,
                'timestamp': ts.strftime('%H:%M:%S')
            })

    return jsonify({'status': 'ok', 'id': event.id, 'type': defect_type})


@api_bp.route('/defect', methods=['GET'])
def get_defect():
    """날짜별 불량 이력 조회 (DB에서 실제 데이터)"""
    date_str = request.args.get('date')

    if date_str:
        try:
            target_date = datetime.strptime(date_str, '%Y-%m-%d').date()
        except Exception:
            return jsonify({'status': 'error', 'message': '날짜 형식: YYYY-MM-DD'}), 400
    else:
        target_date = date.today()

    events = DefectEvent.query.filter(
        db.func.date(DefectEvent.timestamp) == target_date
    ).order_by(DefectEvent.timestamp.desc()).all()

    # 총 불량 개수 합산
    total_defect_count = sum(e.defect_count for e in events)

    return jsonify({
        'status': 'ok',
        'date': target_date.strftime('%Y-%m-%d'),
        'total': len(events),
        'total_defect_count': total_defect_count,
        'list': [e.to_dict() for e in events]
    })


@api_bp.route('/defect/summary', methods=['GET'])
def get_defect_summary():
    """날짜별 시간대별 불량 집계 (defect.html 차트/테이블용)"""
    date_str = request.args.get('date')

    if date_str:
        try:
            target_date = datetime.strptime(date_str, '%Y-%m-%d').date()
        except Exception:
            return jsonify({'status': 'error', 'message': '날짜 형식: YYYY-MM-DD'}), 400
    else:
        target_date = date.today()

    try:
        # 해당 날짜의 모든 이벤트 조회
        all_events = DefectEvent.query.filter(
            db.func.date(DefectEvent.timestamp) == target_date
        ).order_by(DefectEvent.timestamp.asc()).all()
    except Exception as e:
        print(f"[API] defect 조회 오류: {e}")
        db.session.rollback()
        return jsonify({'status': 'ok', 'date': target_date.strftime('%Y-%m-%d'),
                        'total_defect': 0, 'total_normal': 0, 'defect_rate': 0,
                        'hourly': [], 'events': []})

    # ★ 정상 / 불량 분리
    defect_events = [e for e in all_events if e.defect_type != 'normal']
    normal_events = [e for e in all_events if e.defect_type == 'normal']
    total_normal = len(normal_events)
    total_defect_count = sum(e.defect_count for e in defect_events)

    # 불량률 계산
    total_inspected = len(defect_events) + total_normal
    defect_rate = round((len(defect_events) / total_inspected * 100), 1) if total_inspected > 0 else 0

    # 시간대별 집계 (0~23시)
    hourly = {}
    for e in defect_events:
        h = e.timestamp.hour
        if h not in hourly:
            hourly[h] = {'hour': h, 'defect_count': 0, 'types': {}}
        hourly[h]['defect_count'] += e.defect_count
        t = e.defect_type or 'unknown'
        hourly[h]['types'][t] = hourly[h]['types'].get(t, 0) + e.defect_count

    # 센서 시간대 평균 (sensor_log에서 직접 계산)
    sensor_map = {}
    try:
        sensor_rows = db.session.query(
            db.func.hour(SensorLog.timestamp).label('hour'),
            db.func.avg(SensorLog.temperature).label('avg_temp'),
            db.func.avg(SensorLog.humidity).label('avg_humi')
        ).filter(
            db.func.date(SensorLog.timestamp) == target_date
        ).group_by(
            db.func.hour(SensorLog.timestamp)
        ).all()

        for row in sensor_rows:
            sensor_map[row.hour] = {
                'avg_temp': round(row.avg_temp, 1) if row.avg_temp else None,
                'avg_humi': round(row.avg_humi, 1) if row.avg_humi else None
            }
    except Exception as e:
        print(f"[API] sensor_log 조회 오류 (무시): {e}")
        db.session.rollback()

    hourly_list = []
    for h in range(24):
        info = hourly.get(h, {'hour': h, 'defect_count': 0, 'types': {}})
        sensor = sensor_map.get(h, {'avg_temp': None, 'avg_humi': None})
        hourly_list.append({
            'hour': h,
            'defect_count': info['defect_count'],
            'types': info['types'],
            'avg_temp': sensor['avg_temp'],
            'avg_humi': sensor['avg_humi'],
        })

    # ★ 시간대별 정상 개수 집계
    hourly_normal = {}
    for e in normal_events:
        h = e.timestamp.hour
        hourly_normal[h] = hourly_normal.get(h, 0) + 1

    total_defect = sum(info['defect_count'] for info in hourly_list)

    return jsonify({
        'status': 'ok',
        'date': target_date.strftime('%Y-%m-%d'),
        'total_defect': total_defect,
        'total_defect_events': len(defect_events),
        'total_normal': total_normal,
        'defect_rate': defect_rate,
        'hourly': hourly_list,
        'hourly_normal': hourly_normal,
        'events': [e.to_dict() for e in defect_events]
    })


# ============================================================
# 정상 판정 카운트 (WinForm → Flask)
# ============================================================

@api_bp.route('/normal', methods=['POST'])
def add_normal():
    """WinForm에서 정상 판정 시 호출 → 일일 정상 카운트 증가"""
    today = date.today()
    try:
        record = DailyInspection.query.filter_by(date=today).first()
        if not record:
            record = DailyInspection(date=today, normal_count=0)
            db.session.add(record)
        record.normal_count += 1
        db.session.commit()

        # WebSocket으로 실시간 알림
        if _socketio:
            _socketio.emit('normal_update', {
                'normal_count': record.normal_count,
                'timestamp': datetime.now().strftime('%H:%M:%S')
            })

        return jsonify({'status': 'ok', 'normal_count': record.normal_count})
    except Exception as e:
        print(f"[API] 정상 카운트 오류: {e}")
        db.session.rollback()
        return jsonify({'status': 'error', 'message': str(e)}), 500


# ============================================================
# 회원 CRUD
# ============================================================

@api_bp.route('/members', methods=['POST'])
def add_member():
    data = request.json
    if not data or 'id' not in data or 'password' not in data:
        return jsonify({'status': 'error', 'message': 'id, password 필수'}), 400

    if Member.query.get(data['id']):
        return jsonify({'status': 'error', 'message': '이미 존재하는 ID'}), 409

    member = Member(
        id=data['id'],
        password=data['password'],
        name=data.get('name', ''),
        birth=datetime.strptime(data['birth'], '%Y-%m-%d').date() if data.get('birth') else None,
        position=data.get('position', '')
    )
    db.session.add(member)
    db.session.commit()
    return jsonify({'status': 'ok', 'member': member.to_dict()})


@api_bp.route('/members/<member_id>', methods=['PUT'])
def update_member(member_id):
    member = Member.query.get(member_id)
    if not member:
        return jsonify({'status': 'error', 'message': '회원 없음'}), 404

    data = request.json
    if data.get('name'):
        member.name = data['name']
    if data.get('birth'):
        member.birth = datetime.strptime(data['birth'], '%Y-%m-%d').date()
    if data.get('position'):
        member.position = data['position']
    if data.get('password'):
        member.password = data['password']

    db.session.commit()
    return jsonify({'status': 'ok', 'member': member.to_dict()})


# @api_bp.route('/members/<member_id>', methods=['DELETE'])
# def delete_member(member_id):
    
#     try:
        
#         # raw SQL로 관련 데이터 + 회원 삭제 (외래키 문제 회피)
#         log = MemberDeleteLog(
#             deleted_member_id=member_id,
#         deleted_member_name='삭제됨',
#         deleted_member_role='user',
#         deleted_member_position='',
#         deleted_by='admin',
#         deleted_at=datetime.now(),
#         reason='삭제'
#     )
#         db.session.add(log)
#         conn = db.engine.raw_connection()
#         cursor = conn.cursor()
#         cursor.execute("DELETE FROM members WHERE id = %s", (member_id,))
#         cursor.execute("DELETE FROM chat_message WHERE sender_id = %s OR receiver_id = %s", (member_id, member_id))
#         cursor.execute("DELETE FROM members WHERE id = %s", (member_id,))
#         conn.commit()
#         cursor.close()
#         conn.close()
#         db.session.commit()

#         # SQLAlchemy 세션 캐시 초기화
#         db.session.expire_all()

#         print(f"[API] 회원 삭제 완료: {member_id}")
#         return jsonify({'status': 'ok', 'deleted': member_id})
#     except Exception as e:
#         print(f"[API] 회원 삭제 오류: {e}")
#         return jsonify({'status': 'error', 'message': str(e)}), 500



# ============================================================
# 채팅 이력
# ============================================================

@api_bp.route('/chat/history', methods=['GET'])
def chat_history():
    limit = request.args.get('limit', 50, type=int)
    messages = ChatMessage.query.order_by(
        ChatMessage.timestamp.desc()
    ).limit(limit).all()

    return jsonify({
        'status': 'ok',
        'messages': [m.to_dict() for m in reversed(messages)]
    })


@api_bp.route('/chat/history/<peer_id>', methods=['GET'])
def chat_history_with_peer(peer_id):
    from flask import session
    from models import ChatMessage, Member, MessageRead

    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    # 날짜 필터 (선택)
    date_str = request.args.get('date', '')

    if peer_id == 'all':
        # 전체공지
        q = ChatMessage.query.filter(ChatMessage.receiver_id == 'all')
    else:
        query = db.or_(
            db.and_(ChatMessage.sender_id == current_user, ChatMessage.receiver_id == peer_id),
            db.and_(ChatMessage.sender_id == peer_id, ChatMessage.receiver_id == current_user)
        )
        q = ChatMessage.query.filter(query)

    if date_str:
        try:
            target_date = datetime.strptime(date_str, '%Y-%m-%d').date()
            q = q.filter(db.func.date(ChatMessage.timestamp) == target_date)
        except:
            pass

    messages = q.order_by(ChatMessage.timestamp.asc()).all()

    # 전체 회원 수 (전체공지 읽음 카운트용)
    total_members = Member.query.count()

    result = []
    for msg in messages:
        # 나에게만 삭제된 메시지는 내 화면에서만 안 보이게
        if msg.deleted_for:
            deleted_users = msg.deleted_for.split(',')
            if current_user in deleted_users:
                continue

        sender = Member.query.get(msg.sender_id)

        # 읽음 상태 계산
        read_count = MessageRead.query.filter_by(message_id=msg.id).count()

        if msg.receiver_id == 'all':
            unread = max(0, total_members - 1 - read_count)
            read_status = str(unread) if unread > 0 else ''
        else:
            if msg.sender_id == current_user:
                is_read = MessageRead.query.filter_by(
                    message_id=msg.id, user_id=peer_id
                ).first() is not None
                read_status = '읽음' if is_read else ''
            else:
                read_status = ''

        # 모두에게 삭제된 메시지는 내용만 바꿔서 보여주기
        if msg.is_deleted_all:
            display_message = '삭제된 메시지입니다'
            display_file_path = ''
            display_file_name = ''
            display_file_type = ''
        else:
            display_message = msg.message
            display_file_path = msg.file_path or ''
            display_file_name = msg.file_name or ''
            display_file_type = msg.file_type or ''

        result.append({
            'id': msg.id,
            'sender_id': msg.sender_id,
            'sender_name': sender.name if sender else msg.sender_id,
            'sender_profile': sender.profile_img if sender and sender.profile_img else '',
            'receiver_id': msg.receiver_id,
            'message': display_message,
            'file_path': display_file_path,
            'file_name': display_file_name,
            'file_type': display_file_type,
            'read_status': read_status,
            'timestamp': msg.timestamp.strftime('%H:%M') if msg.timestamp else '',
            'date_label': f"{msg.timestamp.year}년 {msg.timestamp.month}월 {msg.timestamp.day}일" if msg.timestamp else '',
            'date_key': msg.timestamp.strftime('%Y-%m-%d') if msg.timestamp else ''
            })

    # ★ 상대가 보낸 메시지를 읽음 처리 (이 채팅방을 열었으므로)
    for msg in messages:
        if msg.sender_id != current_user:
            existing = MessageRead.query.filter_by(
                message_id=msg.id, user_id=current_user
            ).first()
            if not existing:
                db.session.add(MessageRead(
                    message_id=msg.id, user_id=current_user
                ))
    try:
        db.session.commit()
    except:
        db.session.rollback()

    # ★ 읽음 처리 후 상대방에게 알림
    if _socketio:
        _socketio.emit('read_receipt', {
            'reader_id': current_user,
            'peer_id': peer_id
        })

    return jsonify({'status': 'ok', 'messages': result})


# ============================================================
# 채팅 파일 업로드
# ============================================================
import os
import uuid

ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif', 'bmp', 'webp', 'pdf', 'doc', 'docx', 'xls', 'xlsx', 'txt', 'zip', 'hwp'}
IMAGE_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif', 'bmp', 'webp'}

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@api_bp.route('/chat/upload', methods=['POST'])
def chat_upload():
    """채팅 파일 업로드 → DB 저장 + WebSocket 알림"""
    from flask import session, current_app

    sender_id = session.get('user_id')
    if not sender_id:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    receiver_id = request.form.get('receiver_id', '').strip()
    if not receiver_id:
        return jsonify({'status': 'error', 'message': '수신자 필요'}), 400

    if 'file' not in request.files:
        return jsonify({'status': 'error', 'message': '파일 없음'}), 400

    file = request.files['file']
    if file.filename == '':
        return jsonify({'status': 'error', 'message': '파일명 없음'}), 400

    if not allowed_file(file.filename):
        return jsonify({'status': 'error', 'message': '허용되지 않는 파일 형식'}), 400

    # 고유 파일명 생성
    ext = file.filename.rsplit('.', 1)[1].lower()
    unique_name = f"{uuid.uuid4().hex}.{ext}"
    save_path = os.path.join(current_app.config['UPLOAD_FOLDER'], unique_name)
    file.save(save_path)

    # 파일 타입 판별
    file_type = 'image' if ext in IMAGE_EXTENSIONS else 'file'
    file_url = f'/static/uploads/{unique_name}'

    # DB 저장
    from models import ChatMessage, Member
    sender = Member.query.get(sender_id)

    chat = ChatMessage(
        sender_id=sender_id,
        receiver_id=receiver_id,
        message=file.filename,  # 원본 파일명을 메시지로
        file_path=file_url,
        file_name=file.filename,
        file_type=file_type,
        timestamp=datetime.now()
    )
    db.session.add(chat)
    db.session.commit()

    # WebSocket 알림
    if _socketio:
        payload = {
            'id': chat.id,
            'sender_id': sender_id,
            'sender_name': sender.name if sender else sender_id,
            'receiver_id': receiver_id,
            'message': file.filename,
            'file_path': file_url,
            'file_name': file.filename,
            'file_type': file_type,
            'timestamp': chat.timestamp.strftime('%H:%M'),
            'date_label': f"{chat.timestamp.year}년 {chat.timestamp.month}월 {chat.timestamp.day}일",
            'date_key': chat.timestamp.strftime('%Y-%m-%d')
            }
        _socketio.emit('receive_message', payload)

    print(f"[CHAT] 파일 전송: {sender_id} → {receiver_id}: {file.filename}")
    return jsonify({
        'status': 'ok',
        'file_path': file_url,
        'file_name': file.filename,
        'file_type': file_type
    })


# ============================================================
# 프로필 이미지 변경
# ============================================================

@api_bp.route('/profile/change', methods=['POST'])
def change_profile():
    from flask import session

    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    data = request.json
    profile_img = data.get('profile_img', '')

    member = Member.query.get(current_user)
    if not member:
        return jsonify({'status': 'error', 'message': '회원 없음'}), 404

    member.profile_img = profile_img
    db.session.commit()

    # ⭐ 여기서 emit
    if _socketio:
        _socketio.emit('profile_updated', {
            'user_id': current_user,
            'profile_img': profile_img
        })

    return jsonify({'status': 'ok', 'profile_img': profile_img})


@api_bp.route('/profile/me', methods=['GET'])
def get_my_profile():
    from flask import session
    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    member = Member.query.get(current_user)
    if not member:
        return jsonify({'status': 'error'}), 404

    return jsonify({
        'status': 'ok',
        'id': member.id,
        'name': member.name,
        'profile_img': member.profile_img or ''
    })


# ============================================================
# 이모티콘 목록
# ============================================================
@api_bp.route('/emoticons/<category>')
def get_emoticons_by_category(category):
    import os

    base_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'static', 'emo', category
    )

    if not os.path.exists(base_path):
        return jsonify([])

    files = sorted(os.listdir(base_path))

    result = [
        f'/static/emo/{category}/{f}'
        for f in files
        if f.lower().endswith(('.png','.jpg','.jpeg','.gif'))
    ]

    return jsonify(result)


# ============================================================
# 프로필 이미지 목록
# ============================================================
@api_bp.route('/profiles')
def get_profiles():
    import os

    base_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'static', 'pro'
    )

    if not os.path.exists(base_path):
        return jsonify({'status': 'ok', 'profiles': []})

    files = sorted(os.listdir(base_path))

    result = [
        f'/static/pro/{f}'
        for f in files
        if f.lower().endswith(('.png', '.jpg', '.jpeg', '.gif'))
    ]

    return jsonify({'status': 'ok', 'profiles': result})


# ============================================================
# 연락처 최신 메시지 시간
# ============================================================
@api_bp.route('/chat/recent_contacts')
def recent_contacts():
    from flask import session
    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error'}), 401

    rows = db.session.execute(db.text("""
        SELECT contact_id, MAX(ts) as last_ts FROM (
            SELECT receiver_id as contact_id, `timestamp` as ts
            FROM chat_message
            WHERE sender_id = :uid AND receiver_id != 'all'
            UNION ALL
            SELECT sender_id as contact_id, `timestamp` as ts
            FROM chat_message
            WHERE receiver_id = :uid AND sender_id != 'system'
        ) AS sub
        GROUP BY contact_id
        ORDER BY last_ts DESC
    """), {'uid': current_user}).fetchall()

    result = {}
    for row in rows:
        result[row[0]] = row[1].isoformat() if hasattr(row[1], 'isoformat') else str(row[1])

    return jsonify({'status': 'ok', 'recent': result})


# ============================================================
# 센서 최신 데이터
# ============================================================


# ============================================================
# 날짜별 센서 이력 조회
# ============================================================

@api_bp.route('/sensor/history', methods=['GET'])
def sensor_history():
    """날짜별 센서 이력 조회 (차트용)"""
    date_str = request.args.get('date', '')
    if not date_str:
        return jsonify({'status': 'error', 'message': 'date 파라미터 필요'}), 400
    try:
        target_date = datetime.strptime(date_str, '%Y-%m-%d').date()
    except:
        return jsonify({'status': 'error', 'message': '날짜 형식: YYYY-MM-DD'}), 400

    logs = SensorLog.query.filter(
        db.func.date(SensorLog.timestamp) == target_date
    ).order_by(SensorLog.timestamp.asc()).all()

    return jsonify({
        'status': 'ok',
        'date': date_str,
        'data': [{
            'temp': log.temperature,
            'humidity': log.humidity,
            'timestamp': log.timestamp.strftime('%H:%M:%S')
        } for log in logs]
    })


@api_bp.route('/sensor/latest', methods=['GET'])
def sensor_latest():
    latest = SensorLog.query.order_by(SensorLog.timestamp.desc()).first()
    if not latest:
        return jsonify({'status': 'ok', 'data': None})
    return jsonify({'status': 'ok', 'data': latest.to_dict()})


# ============================================================
# 현재 접속자 수
# ============================================================

@api_bp.route('/online_count', methods=['GET'])
def online_count():
    try:
        from routes.auth import _get_count
        count = _get_count()
    except Exception:
        from models import Attendance
        count = Attendance.query.filter_by(check_out=None).count()
    return jsonify({'status': 'ok', 'count': count})





# ============================================================
# 채팅 메시지 삭제
# - 나에게만 삭제: deleted_for에 현재 사용자 ID 저장
# - 모두에게 삭제: is_deleted_all=True 처리
# ============================================================

@api_bp.route('/chat/delete_for_me', methods=['POST'])
def chat_delete_for_me():
    from flask import session
    from models import ChatMessage

    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    data = request.get_json()
    msg_id = data.get('msg_id') if data else None

    if not msg_id:
        return jsonify({'status': 'error', 'message': 'msg_id 필요'}), 400

    msg = ChatMessage.query.get(msg_id)
    if not msg:
        return jsonify({'status': 'error', 'message': '메시지 없음'}), 404

    deleted_list = msg.deleted_for.split(',') if msg.deleted_for else []

    if current_user not in deleted_list:
        deleted_list.append(current_user)

    msg.deleted_for = ','.join(deleted_list)
    db.session.commit()

    return jsonify({'status': 'ok', 'deleted_id': msg_id})


@api_bp.route('/chat/delete_for_all', methods=['POST'])
def chat_delete_for_all():
    from flask import session
    from models import ChatMessage

    current_user = session.get('user_id')
    if not current_user:
        return jsonify({'status': 'error', 'message': '로그인 필요'}), 401

    data = request.get_json()
    msg_id = data.get('msg_id')

    msg = ChatMessage.query.get(msg_id)
    if not msg:
        return jsonify({'status': 'error', 'message': '메시지 없음'}), 404

    # 🔥 여기 추가
    if msg.sender_id != current_user:
        return jsonify({'status': 'error', 'message': '내 메시지만 삭제 가능'}), 403

    # 모두에게 삭제 처리
    msg.is_deleted_all = True
    msg.deleted_at = datetime.now()
    msg.deleted_by = current_user

    db.session.commit()

    return jsonify({'status': 'ok'})


# ============================================================
# GET /api/monitor/frame — MFC에서 최신 JPEG 프레임 가져가기
# ============================================================
@api_bp.route('/monitor/frame', methods=['GET'])
def api_monitor_frame():
    frame = get_latest_frame()
    if frame is None:
        return '', 204
    from flask import Response
    return Response(frame, mimetype='image/jpeg', status=200)