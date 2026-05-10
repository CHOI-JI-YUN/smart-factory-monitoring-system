# models.py
"""
SQLAlchemy 모델 정의
"""
from datetime import datetime, date
from flask_sqlalchemy import SQLAlchemy

db = SQLAlchemy()


class Member(db.Model):
    __tablename__ = 'members'
    
    id = db.Column(db.String(50), primary_key=True)
    password = db.Column(db.String(200), nullable=False)
    name = db.Column(db.String(100), default='')
    birth = db.Column(db.Date, nullable=True)
    position = db.Column(db.String(100), default='')
    role = db.Column(db.String(20), default='user')  # 'admin' / 'user'
    profile_img = db.Column(db.String(300), default='')
    created_at = db.Column(db.DateTime, default=datetime.now)

    def to_dict(self):
        return {
            'id': self.id,
            'name': self.name,
            'birth': self.birth.strftime('%Y-%m-%d') if self.birth else '',
            'position': self.position,
            'role': self.role,
            'profile_img': self.profile_img or '',
            'created_at': self.created_at.strftime('%Y-%m-%d %H:%M:%S') if self.created_at else '',
        }


class SensorLog(db.Model):
    __tablename__ = 'sensor_log'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    timestamp = db.Column(db.DateTime, default=datetime.now)
    temperature = db.Column(db.Float, default=0.0)
    humidity = db.Column(db.Float, default=0.0)

    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp.strftime('%Y-%m-%d %H:%M:%S') if self.timestamp else '',
            'temperature': self.temperature,
            'humidity': self.humidity,
        }


class SensorHourly(db.Model):
    __tablename__ = 'sensor_hourly'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    date = db.Column(db.Date, default=date.today)
    hour = db.Column(db.Integer, default=0)
    avg_temp = db.Column(db.Float, default=0.0)
    avg_humi = db.Column(db.Float, default=0.0)
    count = db.Column(db.Integer, default=0)

    def to_dict(self):
        return {
            'id': self.id,
            'date': self.date.strftime('%Y-%m-%d') if self.date else '',
            'hour': self.hour,
            'avg_temp': round(self.avg_temp, 1),
            'avg_humi': round(self.avg_humi, 1),
            'count': self.count,
        }


class DefectEvent(db.Model):
    __tablename__ = 'defect_event'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    timestamp = db.Column(db.DateTime, default=datetime.now)
    defect_type = db.Column(db.String(50), default='unknown')
    defect_count = db.Column(db.Integer, default=1)
    confidence = db.Column(db.Float, default=0.0)
    image_path = db.Column(db.String(300), default='')
    ultrasonic_matched = db.Column(db.Boolean, default=False)

    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp.strftime('%Y-%m-%d %H:%M:%S') if self.timestamp else '',
            'defect_type': self.defect_type,
            'defect_count': self.defect_count,
            'confidence': self.confidence,
            'image_path': self.image_path,
            'ultrasonic_matched': self.ultrasonic_matched,
        }


class ChatMessage(db.Model):
    __tablename__ = 'chat_message'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    sender_id = db.Column(db.String(50), nullable=False)
    receiver_id = db.Column(db.String(50), nullable=False)
    message = db.Column(db.Text, nullable=False)
    file_path = db.Column(db.String(500), default='')
    file_name = db.Column(db.String(200), default='')
    file_type = db.Column(db.String(20), default='')  # 'image' / 'file' / ''
    timestamp = db.Column(db.DateTime, default=datetime.now)

    is_deleted = db.Column(db.Boolean, default=False)
    deleted_at = db.Column(db.DateTime, nullable=True)
    deleted_by = db.Column(db.String(50), default='')
    
    deleted_for = db.Column(db.Text, default='')
    is_deleted_all = db.Column(db.Boolean, default=False)

    def to_dict(self):
        return {
            'id': self.id,
            'sender_id': self.sender_id,
            'receiver_id': self.receiver_id,
            'message': self.message,
            'file_path': self.file_path,
            'file_name': self.file_name,
            'file_type': self.file_type,
            'timestamp': self.timestamp.strftime('%Y-%m-%d %H:%M:%S') if self.timestamp else '',
        }


class MessageRead(db.Model):
    __tablename__ = 'message_read'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    message_id = db.Column(db.Integer, nullable=False)
    user_id = db.Column(db.String(50), nullable=False)
    read_at = db.Column(db.DateTime, default=datetime.now)


class DailyInspection(db.Model):
    __tablename__ = 'daily_inspection'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    date = db.Column(db.Date, default=date.today)
    normal_count = db.Column(db.Integer, default=0)

    def to_dict(self):
        return {
            'id': self.id,
            'date': self.date.strftime('%Y-%m-%d') if self.date else '',
            'normal_count': self.normal_count,
        }


class Attendance(db.Model):
    __tablename__ = 'attendance'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    user_id = db.Column(db.String(50), nullable=False)
    check_in = db.Column(db.DateTime, default=datetime.now)
    check_out = db.Column(db.DateTime, nullable=True)

    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'check_in': self.check_in.strftime('%Y-%m-%d %H:%M:%S') if self.check_in else '',
            'check_out': self.check_out.strftime('%Y-%m-%d %H:%M:%S') if self.check_out else '',
        }


class MemberDeleteLog(db.Model):
    __tablename__ = 'member_delete_log'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    deleted_member_id = db.Column(db.String(50))
    deleted_member_name = db.Column(db.String(50))
    deleted_member_role = db.Column(db.String(20))
    deleted_member_position = db.Column(db.String(100))
    deleted_by = db.Column(db.String(50))
    deleted_at = db.Column(db.DateTime, default=datetime.now)
    reason = db.Column(db.String(200))

    def to_dict(self):
        return {
            'id': self.id,
            'deleted_member_id': self.deleted_member_id,
            'deleted_member_name': self.deleted_member_name,
            'deleted_member_role': self.deleted_member_role,
            'deleted_member_position': self.deleted_member_position,
            'deleted_by': self.deleted_by,
            'deleted_at': self.deleted_at.strftime('%Y-%m-%d %H:%M:%S') if self.deleted_at else None,
            'reason': self.reason
        }