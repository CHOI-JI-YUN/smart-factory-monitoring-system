// ================================================================
// VoiceAlert.cpp — Windows TTS 음성 알림 구현
// ================================================================

#include "pch.h"

// SAPI 헤더는 반드시 pch.h 이후에 포함
#include <sapi.h>
#pragma comment(lib, "sapi.lib")

#include "VoiceAlert.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CVoiceAlert::CVoiceAlert()
    : m_pVoice(nullptr)
    , m_bEnabled(true)
    , m_bInitialized(false)
    , m_nCooldownSec(10)
    , m_dwLastDangerTick(0)
    , m_dwLastWarningTick(0)
    , m_dwLastEmgTick(0)
{
}

CVoiceAlert::~CVoiceAlert()
{
    Shutdown();
}

bool CVoiceAlert::Initialize()
{
    if (m_bInitialized) return true;

    HRESULT hr = CoCreateInstance(
        CLSID_SpVoice, nullptr, CLSCTX_ALL,
        IID_ISpVoice, (void**)&m_pVoice);

    if (FAILED(hr) || !m_pVoice)
    {
        m_pVoice = nullptr;
        return false;
    }

    m_pVoice->SetRate(1);
    m_pVoice->SetVolume(100);

    m_bInitialized = true;
    return true;
}

void CVoiceAlert::Shutdown()
{
    if (m_pVoice)
    {
        m_pVoice->Release();
        m_pVoice = nullptr;
    }
    m_bInitialized = false;
}

bool CVoiceAlert::CheckCooldown(ULONGLONG& lastTick)
{
    ULONGLONG now = GetTickCount64();
    ULONGLONG cooldownMs = (ULONGLONG)m_nCooldownSec * 1000;

    if (now - lastTick < cooldownMs)
        return false;

    lastTick = now;
    return true;
}

void CVoiceAlert::SpeakAsync(const CString& text)
{
    if (!m_bEnabled || !m_pVoice) return;

    m_pVoice->Speak(text, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

void CVoiceAlert::SpeakDanger(float temp, float humi)
{
    if (!CheckCooldown(m_dwLastDangerTick)) return;

    if (temp > 0 && humi > 0)
        SpeakAsync(_T("위험. 온도 습도 초과."));
    else if (temp > 0)
        SpeakAsync(_T("위험. 온도 초과."));
    else
        SpeakAsync(_T("위험. 습도 초과."));
}

void CVoiceAlert::SpeakWarning(float temp, float humi)
{
    if (!CheckCooldown(m_dwLastWarningTick)) return;

    if (temp > 0 && humi > 0)
        SpeakAsync(_T("경고. 온도 습도 주의."));
    else if (temp > 0)
        SpeakAsync(_T("경고. 온도 주의."));
    else
        SpeakAsync(_T("경고. 습도 주의."));
}

void CVoiceAlert::SpeakEmergency()
{
    if (!CheckCooldown(m_dwLastEmgTick)) return;

    SpeakAsync(_T("비상 정지."));
}

void CVoiceAlert::SpeakCustom(const CString& text)
{
    if (!m_bEnabled || !m_pVoice) return;
    SpeakAsync(text);
}