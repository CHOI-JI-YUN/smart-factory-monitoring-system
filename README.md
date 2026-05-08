# 🏭 스마트 공장 모니터링 시스템

![Backend](https://img.shields.io/badge/Backend-Flask-black)
![Desktop](https://img.shields.io/badge/Desktop-MFC%20%7C%20WinForms-blue)
![AI](https://img.shields.io/badge/AI-YOLO%20%7C%20OpenCV-green)
![Communication](https://img.shields.io/badge/Communication-Socket.IO%20%7C%20TCP/IP%20%7C%20RS232-orange)
![Hardware](https://img.shields.io/badge/Hardware-RaspberryPi-red)
![Database](https://img.shields.io/badge/Database-MySQL-lightgrey)

Flask, MFC, WinForms, YOLO 기반의 생산라인 모니터링 및 실시간 채팅 시스템

---

## 📌 프로젝트 소개
본 프로젝트는 스마트 공장 환경을 가정하여,  
생산라인 영상 기반 불량 검출과 실시간 모니터링, 관리자 간 채팅 기능을 통합한 시스템입니다.

YOLO 기반 객체 검출을 통해 생산라인의 불량을 탐지하고,  
Flask 서버를 중심으로 현장 관리자(MFC), 현장 카메라(WinForms), 전체 관리자(Web) 간 데이터를 실시간으로 처리하도록 구성했습니다.

또한 Socket.IO 기반 실시간 채팅 기능을 구현하여  
현장 관리자와 전체 관리자 간 즉각적인 커뮤니케이션이 가능하도록 설계했습니다.

Raspberry Pi와 GPIO를 활용하여 생산라인 장비 상태를 처리하고,  
RS232 통신 기반 외부 장비 연동 기능도 함께 구현했습니다.

---

## 📅 프로젝트 정보
- 개발 기간: 2026.04.22 ~ 2026.05.04
- 개발 형태: 팀 프로젝트

---

## 🧩 시스템 구성
- Flask API Server
- 현장 관리자 (MFC)
- 현장 카메라 (C# WinForms)
- 전체 관리자 Web Dashboard
- YOLO 기반 불량 검출 시스템
- Raspberry Pi 기반 장비 제어 시스템
- Socket.IO 기반 실시간 채팅 시스템

---

## 🖥 시스템 구성도

![시스템 구성도](images/architecture.png)

---

## 🔄 시스템 흐름
1. 현장 카메라에서 생산라인 영상 수집  
2. YOLO 기반 불량 검출 수행  
3. Flask 서버로 검출 데이터 전송  
4. 관리자 Web Dashboard 실시간 모니터링  
5. MFC 현장 관리자 시스템 상태 확인  
6. Socket.IO 기반 실시간 채팅 수행  
7. Raspberry Pi 및 RS232 기반 장비 연동 처리  

---

## 🛠 기술 스택

| 구분 | 내용 |
|------|------|
| Backend | Flask |
| Desktop | MFC, WinForms |
| AI / Vision | YOLO, OpenCV |
| Communication | Socket.IO, TCP/IP, RS232 |
| Database | MySQL |
| Hardware | Raspberry Pi, GPIO |

---

## 💡 주요 기능

### 🎥 생산라인 모니터링
- 실시간 생산라인 영상 출력
- Flask 기반 영상 스트리밍 처리
- Web Dashboard 상태 모니터링
- 생산라인 상태 데이터 실시간 출력

### 🤖 불량 검출 시스템
- YOLO 기반 객체 검출
- 불량 위치 및 확률 표시
- 검출 데이터 실시간 처리
- OpenCV 기반 영상 처리 기능 구현

### 💬 실시간 채팅 기능
- Socket.IO 기반 관리자 채팅
- 현장 관리자 ↔ 전체 관리자 실시간 통신
- 파일 및 메시지 전송 기능

### 🖥 관리자 시스템
- 생산 상태 실시간 확인
- 불량 검출 결과 조회
- 설비 및 상태 데이터 확인
- 현장 관리자(MFC) 기반 상태 제어

### ⚙ Raspberry Pi 연동
- Raspberry Pi 기반 생산라인 장비 제어
- GPIO 신호 기반 장비 상태 처리
- RS232 통신 기반 외부 장비 연동
- 생산라인 상태 데이터 실시간 처리

---

## 👨‍💻 구현 내용
- Flask 기반 API 서버 구축
- MFC 기반 현장 관리자 프로그램 구현
- WinForms 기반 카메라 및 영상 처리 프로그램 구현
- YOLO 기반 실시간 불량 검출 기능 구현
- OpenCV 기반 영상 분석 처리
- Socket.IO 기반 실시간 채팅 기능 구현
- Raspberry Pi 및 GPIO 기반 장비 제어 기능 구현
- Web Dashboard UI 및 상태 모니터링 구현

---

## 🚀 프로젝트 특징
- AI 기반 불량 검출과 공장 모니터링 시스템 통합 구현
- 실시간 영상 처리와 관리자 채팅 기능 결합
- Flask 중심의 서버-클라이언트 구조 설계
- 현장 관리자 / 전체 관리자 역할 기반 시스템 구성
- Raspberry Pi 및 RS232 기반 하드웨어 연동 구현

---

## 📑 발표 자료
- [발표자료 보기](docs/smart_factory_monitoring_presentation.pdf)

---

## 📷 실행 화면

### 🖥 시스템 모니터링
![모니터링](images/monitoring.png)

---

### 🤖 YOLO 불량 검출
![불량 검출](images/defect_detection.png)

---

### 🖥 현장 관리자 시스템 (MFC)
![현장 관리자](images/mfc_manager.png)

---

### 💬 실시간 채팅
![실시간 채팅](images/chat.png)
