// ================================================================
// resource.h 에 추가할 내용
// (기존 #define ID_LOGOUT_RESULT 9999 아래에 추가)
// ================================================================

// Settings Dialog
#define IDD_SETTINGS                    136

// Settings Dialog Controls - Communication
#define IDC_STATIC_SECT_COMM            1100
#define IDC_COMBO_SERIAL_PORT           1101
#define IDC_COMBO_BAUD_RATE             1102
#define IDC_EDIT_FLASK_IP               1103
#define IDC_EDIT_TCP_PORT               1104
#define IDC_EDIT_HTTP_PORT              1105

// Settings Dialog Controls - Alert Thresholds
#define IDC_STATIC_SECT_ALERT           1110
#define IDC_SLIDER_TEMP_WARN            1111
#define IDC_SLIDER_TEMP_DANGER          1112
#define IDC_SLIDER_HUMI_WARN            1113
#define IDC_STATIC_VAL_TEMP_WARN        1114
#define IDC_STATIC_VAL_TEMP_DANGER      1115
#define IDC_STATIC_VAL_HUMI_WARN        1116

// Settings Dialog Controls - Emergency
#define IDC_STATIC_SECT_EMG             1120
#define IDC_COMBO_EMG_COOLDOWN          1121

// Settings Button on Main Dialog
#define IDC_BTN_SETTINGS                1130

// Update next values
// _APS_NEXT_RESOURCE_VALUE  137
// _APS_NEXT_CONTROL_VALUE   1131


// ================================================================
// ConveyorSystem.rc 에 추가할 다이얼로그 리소스
// (IDD_ALERT_HISTORY 다이얼로그 END 다음에 추가)
// ================================================================

/*

IDD_SETTINGS DIALOGEX 0, 0, 260, 310
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "System Settings"
FONT 11, "Noto Sans KR Medium", 500, 0, 0x81
BEGIN
    LTEXT           "COMMUNICATION", IDC_STATIC_SECT_COMM, 14, 10, 120, 10
    
    LTEXT           "Serial port", -1, 14, 26, 50, 10
    COMBOBOX        IDC_COMBO_SERIAL_PORT, 14, 37, 108, 100, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP
    
    LTEXT           "Baud rate", -1, 135, 26, 50, 10
    COMBOBOX        IDC_COMBO_BAUD_RATE, 135, 37, 108, 100, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP
    
    LTEXT           "Flask server IP", -1, 14, 57, 60, 10
    EDITTEXT        IDC_EDIT_FLASK_IP, 14, 68, 108, 14, ES_AUTOHSCROLL
    
    LTEXT           "TCP port", -1, 135, 57, 50, 10
    EDITTEXT        IDC_EDIT_TCP_PORT, 135, 68, 50, 14, ES_AUTOHSCROLL | ES_NUMBER

    LTEXT           "HTTP port", -1, 198, 57, 50, 10
    EDITTEXT        IDC_EDIT_HTTP_PORT, 198, 68, 50, 14, ES_AUTOHSCROLL | ES_NUMBER

    LTEXT           "ALERT THRESHOLDS", IDC_STATIC_SECT_ALERT, 14, 94, 120, 10

    LTEXT           "Temperature warning (°C)", -1, 14, 110, 120, 10
    RTEXT           "28.0", IDC_STATIC_VAL_TEMP_WARN, 200, 110, 40, 10
    CONTROL         "", IDC_SLIDER_TEMP_WARN, "msctls_trackbar32", TBS_BOTH | TBS_NOTICKS | WS_TABSTOP, 14, 122, 230, 15

    LTEXT           "Temperature danger (°C)", -1, 14, 142, 120, 10
    RTEXT           "33.0", IDC_STATIC_VAL_TEMP_DANGER, 200, 142, 40, 10
    CONTROL         "", IDC_SLIDER_TEMP_DANGER, "msctls_trackbar32", TBS_BOTH | TBS_NOTICKS | WS_TABSTOP, 14, 154, 230, 15

    LTEXT           "Humidity warning (%)", -1, 14, 174, 120, 10
    RTEXT           "70.0", IDC_STATIC_VAL_HUMI_WARN, 200, 174, 40, 10
    CONTROL         "", IDC_SLIDER_HUMI_WARN, "msctls_trackbar32", TBS_BOTH | TBS_NOTICKS | WS_TABSTOP, 14, 186, 230, 15

    LTEXT           "EMERGENCY", IDC_STATIC_SECT_EMG, 14, 212, 120, 10

    LTEXT           "Cooldown time", -1, 14, 228, 60, 10
    COMBOBOX        IDC_COMBO_EMG_COOLDOWN, 14, 239, 108, 100, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP

    DEFPUSHBUTTON   "Save", IDOK, 185, 280, 60, 18
    PUSHBUTTON      "Cancel", IDCANCEL, 115, 280, 60, 18
END

IDD_SETTINGS AFX_DIALOG_LAYOUT
BEGIN
    0
END

*/


// ================================================================
// IDD_CONVEYOR_SYSTEM_DIALOG 에 Settings 버튼 추가
// (메인 다이얼로그 .rc 에서 END 직전에 추가)
// ================================================================

/*
    PUSHBUTTON      "Settings", IDC_BTN_SETTINGS, 160, 7, 40, 10, BS_FLAT
*/
