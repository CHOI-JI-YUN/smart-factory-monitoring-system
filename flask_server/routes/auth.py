# routes/auth.py
"""로그인/로그아웃 처리 + 중복 로그인 방지 (유저 선택 방식)"""

from flask import Blueprint, request, jsonify, session, redirect, url_for, render_template, current_app
from models import db, Member, Attendance
from datetime import datetime

auth_bp = Blueprint('auth', __name__)

_socketio = None

# ── 서버 메모리 기반 로그인 세션 추적 ──
# { user_id: { 'login_time': datetime } }
import threading
_active_sessions = {}
_session_lock = threading.Lock()


def set_socketio(sio):
    global _socketio
    _socketio = sio


def _get_count():
    """현재 로그인 중인 수 (메모리 기반)"""
    with _session_lock:
        return len(_active_sessions)


def _broadcast():
    if _socketio:
        _socketio.emit('online_count', {'count': _get_count()})


def _checkout(user_id):
    """해당 유저의 미완료 attendance 정리"""
    rows = Attendance.query.filter_by(user_id=user_id, check_out=None).all()
    for row in rows:
        row.check_out = datetime.now()
    if rows:
        db.session.commit()
        print(f"[AUTH] check_out 처리: {user_id} ({len(rows)}건)")


def register_session(user_id):
    """로그인 세션 등록 (메모리)"""
    with _session_lock:
        _active_sessions[user_id] = {
            'login_time': datetime.now()
        }


def unregister_session(user_id):
    """로그인 세션 해제 (메모리)"""
    with _session_lock:
        _active_sessions.pop(user_id, None)


def is_user_online(user_id):
    """유저가 현재 로그인 중인지 확인 (메모리 + WebSocket + DB 3중 체크)"""
    # 1) 메모리에 세션이 있는지
    with _session_lock:
        if user_id not in _active_sessions:
            return False

    # 2) WebSocket에 실제 연결이 있는지
    try:
        from socket_handlers import get_sids_for_user
        sids = get_sids_for_user(user_id)
        if sids:
            return True
    except Exception:
        pass

    # 3) attendance DB에 check_out=NULL이 있는지
    active = Attendance.query.filter_by(user_id=user_id, check_out=None).first()
    if active:
        return True

    # 메모리에는 있는데 실제 연결도 없고 DB도 없으면 정리
    unregister_session(user_id)
    return False


def _force_logout_via_socket(user_id):
    """WebSocket을 통해 기존 세션에 강제 로그아웃 이벤트 전송"""
    if not _socketio:
        return

    try:
        from socket_handlers import get_sids_for_user, remove_user_sids
        old_sids = get_sids_for_user(user_id)
        for sid in old_sids:
            _socketio.emit('force_logout', {
                'message': '다른 기기에서 로그인되어 현재 세션이 종료됩니다.'
            }, to=sid)
            print(f"[AUTH] 강제 로그아웃 전송: {user_id} (sid={sid})")
        remove_user_sids(user_id)
    except Exception as e:
        print(f"[AUTH] 강제 로그아웃 소켓 처리 오류: {e}")


@auth_bp.route('/login', methods=['GET'])
def login_page():
    """로그인 페이지"""
    user_id = session.get('user_id')
    if user_id:
        # 다른 브라우저/탭에서 이 유저가 활성 중이면 세션만 지우고 attendance는 건드리지 않음
        try:
            from socket_handlers import get_sids_for_user
            other_active = len(get_sids_for_user(user_id)) > 0
        except Exception:
            other_active = False

        if not other_active:
            _checkout(user_id)
            unregister_session(user_id)
            _broadcast()
    session.clear()
    return render_template('login.html')


@auth_bp.route('/api/login', methods=['POST'])
def login():
    """
    로그인 API (중복 로그인 방지 - 유저 선택 방식)

    1차 요청: force 없음 → 중복이면 409 + duplicate_login 상태 반환
    2차 요청: force=true  → 기존 세션 강제 로그아웃 후 로그인 허용
    """
    data = request.json
    if not data:
        return jsonify({'status': 'error', 'message': 'JSON 데이터 필요'}), 400

    user_id = data.get('id', '').strip()
    password = data.get('password', '').strip()
    force = data.get('force', False)

    member = Member.query.get(user_id)
    if not member or member.password != password:
        return jsonify({'status': 'error', 'message': '아이디 또는 비밀번호 불일치'}), 401

    # ── 중복 로그인 방지 ──
    if is_user_online(member.id):
        if not force:
            # 1차: 중복 알림 → 유저에게 선택권 제공
            print(f"[AUTH] 중복 로그인 감지: {member.id} (유저 선택 대기)")
            return jsonify({
                'status': 'duplicate_login',
                'message': '이미 다른 기기에서 로그인 중입니다.'
            }), 409
        else:
            # 2차: 유저가 "강제 로그인" 선택 → 기존 세션 끊기
            print(f"[AUTH] 강제 로그인 요청: {member.id} → 기존 세션 종료")
            _force_logout_via_socket(member.id)
            _checkout(member.id)
            unregister_session(member.id)

    # 세션 설정
    session['user_id'] = member.id
    session['user_name'] = member.name
    session['user_position'] = member.position
    session['role'] = member.role

    # 메모리에 활성 세션 등록
    register_session(member.id)

    # attendance 기록
    db.session.add(Attendance(
        user_id=member.id,
        check_in=datetime.now(),
        check_out=None
    ))
    db.session.commit()

    _broadcast()
    print(f"[AUTH] 로그인: {member.id} → 현재 {_get_count()}명")

    return jsonify({'status': 'ok', 'user': member.to_dict()})


@auth_bp.route('/logout')
def logout():
    """로그아웃"""
    user_id = session.get('user_id')
    if user_id:
        _checkout(user_id)
        unregister_session(user_id)
        _broadcast()
        print(f"[AUTH] 로그아웃: {user_id} → 현재 {_get_count()}명")
    session.clear()
    return redirect(url_for('auth.login_page'))
