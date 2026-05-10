#pragma once

#include <Windows.h>
#include <string>
#include "ConveyorProtocol.h"

// SerialComm.h
// RS232 serial communication class
// Raspberry Pi <-> Windows PC (MFC)

class CSerialComm
{
public:
    CSerialComm();
    ~CSerialComm();

    // Open / Close
    bool Open(const std::wstring& portName, DWORD baudRate = DEFAULT_BAUD_RATE);
    void Close();
    bool IsOpen() const;

    // Send / Receive (9-byte packet unit)
    bool SendPacket(const BYTE* pPacket, int nLen);
    int  ReadPacket(BYTE* pPacket, int nLen);

    // Control command helpers
    bool SendConveyorControl(BYTE onOff);
    bool SendSpeedSet(BYTE step);
    bool SendFanControl(BYTE onOff);
    bool SendEmergencyStop();
    bool SendDefectServo();

    // Packet validation
    static bool ValidatePacket(const BYTE* pPacket, int nLen);

    std::wstring GetLastErrorStr() const { return m_strLastError; }

private:
    HANDLE          m_hPort;
    std::wstring    m_strLastError;

    void BuildPacket(BYTE* pOut, BYTE cmd, BYTE data);
    void SetError(const std::wstring& msg);
};
