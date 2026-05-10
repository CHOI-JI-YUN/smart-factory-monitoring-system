# routes/admin.py
"""관리자 웹 페이지 라우트 + 회원 관리 API"""

from flask import Blueprint, render_template, session, redirect, url_for, request, jsonify
from functools import wraps
from datetime import datetime

from models import db, Member

admin_bp = Blueprint('admin', __name__)


def login_required(f):
    """로그인 확인 데코레이터"""
    @wraps(f)
    def decorated(*args, **kwargs):
        if not session.get('user_id'):
            return redirect(url_for('auth.login_page'))
        return f(*args, **kwargs)
    return decorated


from flask import flash

def admin_required(f):
    """관리자 권한 확인 데코레이터"""
    @wraps(f)
    def decorated(*args, **kwargs):
        if session.get('role') != 'admin':
            return render_template(
                'access_denied.html',
                user_name=session.get('user_name', ''),
                user_id=session.get('user_id', '')
            )
        return f(*args, **kwargs)
    return decorated


@admin_bp.route('/')
def index():
    return redirect(url_for('auth.login_page'))


@admin_bp.route('/dashboard')
@login_required
def dashboard():
    """대시보드 (메인 화면)"""
    return render_template(
        'dashboard.html',
        user_name=session.get('user_name', ''),
        user_id=session.get('user_id', ''),
        role=session.get('role', '')
    )


@admin_bp.route('/defect')
@login_required
def defect():
    """불량 확인 페이지"""
    return render_template(
        'defect.html',
        user_name=session.get('user_name', ''),
        user_id=session.get('user_id', ''),
        role=session.get('role', '')
    )


@admin_bp.route('/members')
@login_required
@admin_required
def members():
    """회원 관리 페이지"""
    return render_template(
        'members.html',
        user_name=session.get('user_name', ''),
        user_id=session.get('user_id', ''),
        role=session.get('role', '')
    )


@admin_bp.route('/monitor')
@login_required
def monitor():
    """모니터링 페이지 (C# 영상 스트림)"""
    return render_template(
        'monitor.html',
        user_name=session.get('user_name', ''),
        user_id=session.get('user_id', ''),
        role=session.get('role', '')
    )


@admin_bp.route('/chat')
@login_required
def chat():
    """채팅 페이지"""
    return render_template(
        'chat.html',
        user_name=session.get('user_name', ''),
        user_id=session.get('user_id', ''),
        role=session.get('role', '')
    )


# ============================================================
# 회원 관리 API
# ============================================================

@admin_bp.route('/api/members', methods=['GET'])
@login_required
def list_all_members():
    """회원 목록 조회 (채팅에서도 사용)"""
    members = Member.query.order_by(Member.created_at.asc()).all()
    return jsonify([m.to_dict() for m in members])


@admin_bp.route('/api/member/add', methods=['POST'])
@login_required
@admin_required
def add_member():
    """회원 추가"""
    data = request.get_json() or {}

    member_id = (data.get('id') or '').strip()
    password = (data.get('password') or '').strip()
    name = (data.get('name') or '').strip()
    birth_str = (data.get('birth') or '').strip()
    position = (data.get('position') or '').strip()

    if not member_id:
        return jsonify({'error': '아이디를 입력하세요.'}), 400

    if not password:
        return jsonify({'error': '비밀번호를 입력하세요.'}), 400

    if Member.query.get(member_id):
        return jsonify({'error': '이미 존재하는 아이디입니다.'}), 400

    birth = None
    if birth_str:
        try:
            birth = datetime.strptime(birth_str, '%Y-%m-%d').date()
        except ValueError:
            return jsonify({'error': '생년월일 형식은 YYYY-MM-DD 이어야 합니다.'}), 400

    member = Member(
        id=member_id,
        password=password,
        name=name,
        birth=birth,
        position=position,
        role='user'
    )

    db.session.add(member)
    db.session.commit()

    return jsonify({
        'message': '회원 추가 완료',
        'member': member.to_dict()
    })


@admin_bp.route('/api/member/update', methods=['POST'])
@login_required
@admin_required
def update_member():
    """회원 수정 (아이디 변경 포함)"""
    data = request.get_json() or {}

    old_id = (data.get('old_id') or '').strip()
    new_id = (data.get('id') or '').strip()
    name = (data.get('name') or '').strip()
    birth_str = (data.get('birth') or '').strip()
    position = (data.get('position') or '').strip()
    password = (data.get('password') or '').strip()

    if not old_id:
        return jsonify({'error': '기존 아이디(old_id)가 필요합니다.'}), 400

    if not new_id:
        return jsonify({'error': '새 아이디를 입력하세요.'}), 400

    member = Member.query.get(old_id)
    if not member:
        return jsonify({'error': '회원이 존재하지 않습니다.'}), 404

    birth = None
    if birth_str:
        try:
            birth = datetime.strptime(birth_str, '%Y-%m-%d').date()
        except ValueError:
            return jsonify({'error': '생년월일 형식은 YYYY-MM-DD 이어야 합니다.'}), 400

    if old_id != new_id:
        if Member.query.get(new_id):
            return jsonify({'error': '이미 존재하는 아이디입니다.'}), 400

        new_member = Member(
            id=new_id,
            password=password if password else member.password,
            name=name,
            birth=birth,
            position=position,
            role=member.role,
            created_at=member.created_at
        )

        db.session.add(new_member)
        db.session.delete(member)
        db.session.commit()

        return jsonify({
            'message': '회원 수정 완료',
            'member': new_member.to_dict()
        })

    member.name = name
    member.birth = birth
    member.position = position

    if password:
        member.password = password

    db.session.commit()

    return jsonify({
        'message': '회원 수정 완료',
        'member': member.to_dict()
    })


@admin_bp.route('/api/member/delete', methods=['POST'])
@login_required
@admin_required
def delete_member():
    """회원 삭제"""
    data = request.get_json() or {}
    member_id = (data.get('id') or '').strip()

    if not member_id:
        return jsonify({'error': '아이디가 필요합니다.'}), 400

    member = Member.query.get(member_id)
    if not member:
        return jsonify({'error': '회원이 존재하지 않습니다.'}), 404

    db.session.delete(member)
    db.session.commit()

    return jsonify({'message': '회원 삭제 완료'})