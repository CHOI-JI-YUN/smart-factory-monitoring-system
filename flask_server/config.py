# config.py
"""
Flask 서버 설정
"""
import os

BASE_DIR = os.path.abspath(os.path.dirname(__file__))


class Config:
    SECRET_KEY = os.environ.get('SECRET_KEY', 'conveyor-secret-key-2024')

    # MySQL DB
    SQLALCHEMY_DATABASE_URI = "mysql+pymysql://root:6698@localhost/smart_conveyor"

    # TCP 포트 (MFC Raw TCP 연결용)
    TCP_PORT = 9000

    # 센서 임계값
    TEMP_HIGH = 30.0
    TEMP_LOW = 10.0
    HUMIDITY_HIGH = 70.0
    HUMIDITY_LOW = 20.0
