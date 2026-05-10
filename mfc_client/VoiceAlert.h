#pragma once
// ================================================================
// VoiceAlert.h — Windows TTS 음성 알림 (SAPI ISpVoice)
// ================================================================

// sapi.h는 .cpp에서만 포함 (VC_EXTRALEAN 충돌 방지)
struct ISpVoice;    // forward declaration

class CVoiceAlert
{
public:
    CVoiceAlert();
    ~CVoiceAlert();

    bool Initialize();
    void Shutdown();

    void SpeakDanger(float temp, float humi);
    void SpeakWarning(float temp, float humi);
    void SpeakEmergency();
    void SpeakCustom(const CString& text);

    void SetEnabled(bool bEnable) { m_bEnabled = bEnable; }
    bool IsEnabled() const { return m_bEnabled; }
    void SetCooldownSec(int sec) { m_nCooldownSec = sec; }

private:
    ISpVoice*   m_pVoice;
    bool        m_bEnabled;
    bool        m_bInitialized;
    int         m_nCooldownSec;
    ULONGLONG   m_dwLastDangerTick;
    ULONGLONG   m_dwLastWarningTick;
    ULONGLONG   m_dwLastEmgTick;

    void SpeakAsync(const CString& text);
    bool CheckCooldown(ULONGLONG& lastTick);
};
