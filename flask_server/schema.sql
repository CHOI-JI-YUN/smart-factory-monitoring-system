-- schema.sql
-- MySQL에서 실행: mysql -u root -p < schema.sql

CREATE DATABASE IF NOT EXISTS smart_conveyor
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE smart_conveyor;

-- 회원
CREATE TABLE IF NOT EXISTS members (
    id VARCHAR(50) PRIMARY KEY,
    password VARCHAR(255) NOT NULL,
    name VARCHAR(100) DEFAULT '',
    birth DATE,
    position VARCHAR(100) DEFAULT '',
    role VARCHAR(20) DEFAULT 'user',
    profile_img VARCHAR(300) DEFAULT '',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 출퇴근 기록
CREATE TABLE IF NOT EXISTS attendance (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id VARCHAR(50) NOT NULL,
    check_in DATETIME,
    check_out DATETIME,
    INDEX idx_user_id (user_id)
);

-- 센서 원본 로그
CREATE TABLE IF NOT EXISTS sensor_log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME,
    temperature FLOAT DEFAULT 0.0,
    humidity FLOAT DEFAULT 0.0,
    INDEX idx_timestamp (timestamp)
);

-- 시간별 집계
CREATE TABLE IF NOT EXISTS sensor_hourly (
    id INT AUTO_INCREMENT PRIMARY KEY,
    date DATE,
    hour INT DEFAULT 0,
    avg_temp FLOAT DEFAULT 0.0,
    avg_humi FLOAT DEFAULT 0.0,
    count INT DEFAULT 0
);

-- 불량 검출 이벤트
CREATE TABLE IF NOT EXISTS defect_event (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME,
    defect_type VARCHAR(50) DEFAULT 'unknown',
    defect_count INT DEFAULT 1,
    confidence FLOAT DEFAULT 0.0,
    image_path VARCHAR(300) DEFAULT '',
    ultrasonic_matched BOOLEAN DEFAULT FALSE,
    INDEX idx_timestamp (timestamp)
);

-- 채팅 메시지
CREATE TABLE IF NOT EXISTS chat_message (
    id INT AUTO_INCREMENT PRIMARY KEY,
    sender_id VARCHAR(50) NOT NULL,
    receiver_id VARCHAR(50) NOT NULL,
    message TEXT NOT NULL,
    file_path VARCHAR(500) DEFAULT '',
    file_name VARCHAR(200) DEFAULT '',
    file_type VARCHAR(20) DEFAULT '',
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_deleted BOOLEAN DEFAULT FALSE,
    deleted_at DATETIME,
    deleted_by VARCHAR(50) DEFAULT '',
    deleted_for TEXT DEFAULT '',
    is_deleted_all BOOLEAN DEFAULT FALSE
);

-- 메시지 읽음 처리
CREATE TABLE IF NOT EXISTS message_read (
    id INT AUTO_INCREMENT PRIMARY KEY,
    message_id INT NOT NULL,
    user_id VARCHAR(50) NOT NULL,
    read_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 일일 정상 판정 카운트
CREATE TABLE IF NOT EXISTS daily_inspection (
    id INT AUTO_INCREMENT PRIMARY KEY,
    date DATE NOT NULL,
    normal_count INT DEFAULT 0,
    INDEX idx_date (date)
);

-- 회원 삭제 로그
CREATE TABLE IF NOT EXISTS member_delete_log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    deleted_member_id VARCHAR(50),
    deleted_member_name VARCHAR(50),
    deleted_member_role VARCHAR(20),
    deleted_member_position VARCHAR(100),
    deleted_by VARCHAR(50),
    deleted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    reason VARCHAR(200)
);

-- 비상정지 이력
CREATE TABLE IF NOT EXISTS emergency_log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    triggered_by VARCHAR(50),
    reason VARCHAR(255)
);

-- 기본 관리자 계정
INSERT IGNORE INTO members (id, password, name, position, role)
VALUES ('admin', '1234', '관리자', '관리자', 'admin');
