# socket_handlers.py
from datetime import datetime
import threading

# sid -> user_id
_sid_map = {}
# 대기 중인 로그아웃 타이머 {user_id: Timer}
_pending_logout = {}
# 미읽은 메시지 수 {receiver_id: {sender_id: count}}
_unread = {}
_lock = threading.Lock()

LOGOUT_DELAY = 3  # 초 (새로고침 시간보다 길게)


def get_sids_for_user(user_id):
    """특정 유저의 모든 sid 목록 반환 (auth.py에서 호출)"""
    with _lock:
        return [sid for sid, uid in _sid_map.items() if uid == user_id]


def remove_user_sids(user_id):
    """특정 유저의 모든 sid를 _sid_map에서 제거 (auth.py에서 호출)"""
    with _lock:
        # 대기 중인 로그아웃 타이머도 취소
        if user_id in _pending_logout:
            _pending_logout[user_id].cancel()
            del _pending_logout[user_id]
        sids_to_remove = [sid for sid, uid in _sid_map.items() if uid == user_id]
        for sid in sids_to_remove:
            del _sid_map[sid]
        return sids_to_remove


def register_handlers(socketio, app):

    @socketio.on('connect')
    def on_connect():
        print("[WS] 클라이언트 연결됨")

    @socketio.on('register_user')
    def on_register_user(data):
        from flask import request as req
        user_id = data.get('user_id', '').strip()
        if not user_id:
            return

        with _lock:
            if user_id in _pending_logout:
                _pending_logout[user_id].cancel()
                del _pending_logout[user_id]
                print(f"[WS] 로그아웃 취소 (재연결): {user_id}")

            for old_sid in [s for s, u in list(_sid_map.items()) if u == user_id]:
                del _sid_map[old_sid]
            _sid_map[req.sid] = user_id

        print(f"[WS] 등록: {user_id}")
        with app.app_context():
            try:
                from routes.auth import _get_count
                count = _get_count()
            except Exception:
                from models import Attendance
                count = Attendance.query.filter_by(check_out=None).count()
        socketio.emit('online_count', {'count': count})
        
        # 미읽은 메시지 정보 전송
        if user_id in _unread and _unread[user_id]:
            from flask import request as req2
            socketio.emit('unread_update', {
                'target_user': user_id,
                'unread': _unread[user_id]
            })
        socketio.emit('online_users', {'users': list(set(_sid_map.values()))})

    @socketio.on('disconnect')
    def on_disconnect():
        from flask import request as req
        with _lock:
            user_id = _sid_map.pop(req.sid, None)

        if user_id:
            print(f"[WS] disconnect: {user_id} → {LOGOUT_DELAY}초 후 로그아웃")
            t = threading.Timer(LOGOUT_DELAY, _do_logout, args=[user_id, app, socketio])
            with _lock:
                _pending_logout[user_id] = t
            t.start()

    @socketio.on('heartbeat')
    def on_heartbeat(data):
        pass

    @socketio.on('mark_read')
    def on_mark_read(data):
        from flask import request as req
        from models import db, ChatMessage, MessageRead
        with _lock:
            user_id = _sid_map.get(req.sid)
        sender_id = data.get('sender_id', '')

        if not user_id or not sender_id:
            return

        # 1) 메모리 unread 카운트 정리
        if user_id in _unread:
            _unread[user_id].pop(sender_id, None)

        # 2) DB에 읽음 기록 저장 (아직 안 읽은 메시지만)
        try:
            with app.app_context():
                if sender_id == 'all':
                    unread_msgs = ChatMessage.query.filter(
                        ChatMessage.receiver_id == 'all',
                        ChatMessage.sender_id != user_id
                    ).all()
                else:
                    unread_msgs = ChatMessage.query.filter(
                        ChatMessage.sender_id == sender_id,
                        ChatMessage.receiver_id == user_id
                    ).all()

                added = 0
                for msg in unread_msgs:
                    existing = MessageRead.query.filter_by(
                        message_id=msg.id, user_id=user_id
                    ).first()
                    if not existing:
                        db.session.add(MessageRead(
                            message_id=msg.id, user_id=user_id
                        ))
                        added += 1

                if added > 0:
                    db.session.commit()

                # 3) 상대방에게 실시간 read_receipt 전송
                socketio.emit('read_receipt', {
                    'reader_id': user_id,
                    'peer_id': sender_id
                })
        except Exception as e:
            print(f"[WS] mark_read 오류: {e}")
            try:
                db.session.rollback()
            except:
                pass

    @socketio.on('emergency_stop')
    def on_emergency_stop():
        from flask import request as req
        from models import db, ChatMessage, Member

        print("[WS] 웹 관리자 비상정지!")
        from tcp_server import broadcast_to_mfc
        sent = broadcast_to_mfc("EMERGENCY:STOP\n")

        # 누가 눌렀는지 확인
        with _lock:
            user_id = _sid_map.get(req.sid, 'system')
        
        user_name = user_id
        try:
            with app.app_context():
                member = Member.query.get(user_id)
                if member:
                    user_name = member.name

                now = datetime.now()
                time_str = now.strftime('%Y-%m-%d %H:%M:%S')

                if sent > 0:
                    msg_text = f"🚨 비상정지 발생! ({time_str}) - {user_name}({user_id})"
                else:
                    msg_text = f"⚠️ 비상정지 실패! ({time_str}) - {user_name}({user_id})"

                chat = ChatMessage(
                    sender_id='system',
                    receiver_id='all',
                    message=msg_text,
                    timestamp=now
                )
                db.session.add(chat)
                db.session.commit()

                socketio.emit('receive_message', {
                    'sender_id': 'system',
                    'sender_name': '시스템',
                    'receiver_id': 'all',
                    'message': msg_text,
                    'timestamp': now.strftime('%H:%M')
                })
        except Exception as e:
            print(f"[WS] 비상정지 공지 오류: {e}")

        socketio.emit('emergency_triggered', {
            'by': 'web_admin',
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        })

    @socketio.on('conveyor_on')
    def on_conveyor_on():
        from tcp_server import broadcast_to_mfc
        broadcast_to_mfc("CONVEYOR:ON\n")
        socketio.emit('conveyor_status', {
            'status': 'on',
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        })

    @socketio.on('conveyor_off')
    def on_conveyor_off():
        from tcp_server import broadcast_to_mfc
        broadcast_to_mfc("CONVEYOR:OFF\n")
        socketio.emit('conveyor_status', {
            'status': 'off',
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        })

    @socketio.on('emergency_release')
    def on_emergency_release():
        from tcp_server import broadcast_to_mfc
        broadcast_to_mfc("EMERGENCY:RELEASE\n")
        socketio.emit('emergency_released', {
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        })

    @socketio.on('chat_message')
    def on_chat_message(data):
        from flask import request as req
        from models import db, ChatMessage, Member
        
        with _lock:
            sender_id = _sid_map.get(req.sid)

        receiver_id = data.get('receiver_id', '').strip()
        message = data.get('message', '').strip()

        if not sender_id or not receiver_id or not message:
            print(f"[CHAT] 무시: sender={sender_id}, receiver={receiver_id}, msg={message}")
            return

        try:
            with app.app_context():
                sender = Member.query.get(sender_id)

                chat = ChatMessage(
                    sender_id=sender_id,
                    receiver_id=receiver_id,
                    message=message,
                    timestamp=datetime.now()
                )

                db.session.add(chat)
                db.session.commit()

                payload = {
                    'id': chat.id,
                    'sender_id': sender_id,
                    'sender_name': sender.name if sender else sender_id,
                    'sender_profile': sender.profile_img if sender and sender.profile_img else '',
                    'receiver_id': receiver_id,
                    'message': message,
                    'timestamp': chat.timestamp.strftime('%H:%M')
                }

            socketio.emit('receive_message', payload)
            
            # 미읽은 수 업데이트
            if receiver_id not in _unread:
                _unread[receiver_id] = {}
            _unread[receiver_id][sender_id] = _unread[receiver_id].get(sender_id, 0) + 1
            
            # 수신자에게 미읽은 수 전송
            socketio.emit('unread_update', {
                'target_user': receiver_id,
                'unread': _unread.get(receiver_id, {})
            })
            print(f"[CHAT] 메시지 전송: {sender_id} → {receiver_id}: {message}")
        except Exception as e:
            print(f"[CHAT] 오류: {e}")

    @socketio.on('request_status')
    def on_request_status():
        from models import SensorLog
        from tcp_server import is_mfc_connected
        with app.app_context():
            latest = SensorLog.query.order_by(SensorLog.timestamp.desc()).first()
        socketio.emit('status_response', {
            'mfc_connected': is_mfc_connected(),
            'latest_sensor': latest.to_dict() if latest else None,
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        })


def _do_logout(user_id, app, socketio):
    with _lock:
        if user_id in _pending_logout:
            del _pending_logout[user_id]

    # 같은 유저가 다른 sid로 아직 연결 중이면 로그아웃하지 않음
    with _lock:
        still_connected = any(uid == user_id for uid in _sid_map.values())
    if still_connected:
        print(f"[WS] 로그아웃 취소 (다른 탭 연결 중): {user_id}")
        return

    try:
        with app.app_context():
            from models import db, Attendance

            # attendance 정리
            rows = Attendance.query.filter_by(user_id=user_id, check_out=None).all()
            for row in rows:
                row.check_out = datetime.now()
            if rows:
                db.session.commit()

            # 메모리 세션 해제 + 카운트 갱신
            try:
                from routes.auth import unregister_session, _get_count
                unregister_session(user_id)
                count = _get_count()
            except Exception:
                count = Attendance.query.filter_by(check_out=None).count()

            socketio.emit('online_count', {'count': count})
            socketio.emit('online_users', {'users': list(set(_sid_map.values()))})
            print(f"[WS] 로그아웃 완료: {user_id} → {count}명")
    except Exception as e:
        print(f"[WS] 로그아웃 오류: {e}")