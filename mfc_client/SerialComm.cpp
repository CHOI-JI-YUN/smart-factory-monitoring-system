// SerialComm.cpp
// RS232 serial communication implementation

#include "pch.h"
#include "ConveyorProtocol.h"
#include "SerialComm.h"

CSerialComm::CSerialComm()
    : m_hPort(INVALID_HANDLE_VALUE)
{
}

CSerialComm::~CSerialComm()
{
    Close();
}

bool CSerialComm::Open(const std::wstring& portName, DWORD baudRate)
{
    if (IsOpen())
        Close();

    std::wstring path = L"\\\\.\\" + portName;

    m_hPort = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (m_hPort == INVALID_HANDLE_VALUE)
    {
        SetError(L"Port open failed: " + portName);
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(m_hPort, &dcb))
    {
        SetError(L"GetCommState failed");
        Close();
        return false;
    }

    dcb.BaudRate     = baudRate;
    dcb.ByteSize     = 8;
    dcb.Parity       = NOPARITY;
    dcb.StopBits     = ONESTOPBIT;
    dcb.fBinary      = TRUE;
    dcb.fParity      = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_ENABLE;
    dcb.fRtsControl  = RTS_CONTROL_ENABLE;

    if (!SetCommState(m_hPort, &dcb))
    {
        SetError(L"SetCommState failed");
        Close();
        return false;
    }

    // Use different name to avoid Windows macro conflict
    COMMTIMEOUTS commTm = {};
    commTm.ReadIntervalTimeout         = 10;
    commTm.ReadTotalTimeoutMultiplier  = 2;
    commTm.ReadTotalTimeoutConstant    = 50;
    commTm.WriteTotalTimeoutMultiplier = 2;
    commTm.WriteTotalTimeoutConstant   = 50;

    if (!SetCommTimeouts(m_hPort, &commTm))
    {
        SetError(L"SetCommTimeouts failed");
        Close();
        return false;
    }

    PurgeComm(m_hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
    m_strLastError.clear();
    return true;
}

void CSerialComm::Close()
{
    if (m_hPort != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hPort);
        m_hPort = INVALID_HANDLE_VALUE;
    }
}

bool CSerialComm::IsOpen() const
{
    return (m_hPort != INVALID_HANDLE_VALUE);
}

bool CSerialComm::SendPacket(const BYTE* pPacket, int nLen)
{
    if (!IsOpen())
    {
        SetError(L"Port not open");
        return false;
    }

    DWORD dwWritten = 0;
    if (!WriteFile(m_hPort, pPacket, (DWORD)nLen, &dwWritten, nullptr))
    {
        SetError(L"WriteFile failed");
        return false;
    }
    return (dwWritten == (DWORD)nLen);
}

// Returns bytes read, 0 = no data yet, -1 = error
int CSerialComm::ReadPacket(BYTE* pPacket, int nLen)
{
    if (!IsOpen())
    {
        SetError(L"Port not open");
        return -1;
    }

    COMSTAT cs     = {};
    DWORD   dwErrs = 0;
    ClearCommError(m_hPort, &dwErrs, &cs);

    if (cs.cbInQue < (DWORD)nLen)
        return 0;

    DWORD dwRead = 0;
    if (!ReadFile(m_hPort, pPacket, (DWORD)nLen, &dwRead, nullptr))
    {
        SetError(L"ReadFile failed");
        return -1;
    }
    return (int)dwRead;
}

bool CSerialComm::ValidatePacket(const BYTE* pPacket, int nLen)
{
    if (nLen != PACKET_SIZE)          return false;
    if (pPacket[IDX_STX] != STX)     return false;
    if (pPacket[IDX_ETX] != ETX)     return false;

    BYTE cs = CALC_CS(pPacket);
    return (pPacket[IDX_CS] == cs);
}

void CSerialComm::BuildPacket(BYTE* pOut, BYTE cmd, BYTE data)
{
    memset(pOut, 0, PACKET_SIZE);
    pOut[IDX_STX]  = STX;
    pOut[IDX_CMD]  = cmd;
    pOut[IDX_LEN]  = 1;
    pOut[IDX_TEMP] = 0;
    pOut[IDX_HUMI] = 0;
    pOut[IDX_ULTR] = 0;
    pOut[IDX_MOTR] = data;
    pOut[IDX_CS]   = CALC_CS(pOut);
    pOut[IDX_ETX]  = ETX;
}

bool CSerialComm::SendConveyorControl(BYTE onOff)
{
    BYTE pkt[PACKET_SIZE];
    BuildPacket(pkt, CMD_CONVEYOR_CONTROL, onOff);
    return SendPacket(pkt, PACKET_SIZE);
}

bool CSerialComm::SendSpeedSet(BYTE step)
{
    if (step < 1 || step > 3) return false;
    BYTE pkt[PACKET_SIZE];
    BuildPacket(pkt, CMD_SPEED_SET, step);
    return SendPacket(pkt, PACKET_SIZE);
}

bool CSerialComm::SendFanControl(BYTE onOff)
{
    BYTE pkt[PACKET_SIZE];
    BuildPacket(pkt, CMD_FAN_CONTROL, onOff);
    return SendPacket(pkt, PACKET_SIZE);
}

bool CSerialComm::SendEmergencyStop()
{
    BYTE pkt[PACKET_SIZE];
    BuildPacket(pkt, CMD_EMG_STOP, MOTOR_EMG_STOP);
    return SendPacket(pkt, PACKET_SIZE);
}

bool CSerialComm::SendDefectServo()
{
    BYTE pkt[PACKET_SIZE];
    BuildPacket(pkt, CMD_DEFECT_SERVO, SERVO_REJECT);
    return SendPacket(pkt, PACKET_SIZE);
}

void CSerialComm::SetError(const std::wstring& msg)
{
    m_strLastError = msg;
}
