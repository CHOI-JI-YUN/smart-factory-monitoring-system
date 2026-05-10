// ================================================================
// ReportGenerator.cpp
// ================================================================

#include "pch.h"
#include "ReportGenerator.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdio>
#include <string>
#include <vector>
#include <atlconv.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ─── 헬퍼 구조체/함수 (cpp 내부 전용) ───

struct TempHumiStats
{
    float fTempMin;
    float fTempMax;
    float fTempAvg;
    float fHumiMin;
    float fHumiMax;
    float fHumiAvg;
    int   nSampleCount;
};

static std::string EscapePdf(const std::string& str)
{
    std::string r;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '(' || str[i] == ')' || str[i] == '\\')
            r += '\\';
        r += str[i];
    }
    return r;
}

static std::string CStringToAnsi(const CString& str)
{
    CT2A ansi(str);
    return std::string(ansi);
}

static std::vector<AlertRecord> FilterAlertsByDate(
    CDatabaseManager* pDB, const std::string& dateStr, int limit)
{
    std::vector<AlertRecord> all = pDB->GetAlerts(9999);
    std::vector<AlertRecord> out;
    for (size_t i = 0; i < all.size(); i++)
    {
        // DEFECT 레코드는 리포트에서 제외
        if (all[i].status == "DEFECT")
            continue;
        if (all[i].timestamp.find(dateStr) == 0)
        {
            out.push_back(all[i]);
            if ((int)out.size() >= limit) break;
        }
    }
    return out;
}

static TempHumiStats CalcStats(CDatabaseManager* pDB, const std::string& dateStr)
{
    TempHumiStats st;
    memset(&st, 0, sizeof(st));
    std::vector<AlertRecord> alerts = FilterAlertsByDate(pDB, dateStr, 9999);
    if (alerts.empty()) return st;

    float tMin = 999, tMax = -999, tSum = 0;
    float hMin = 999, hMax = -999, hSum = 0;
    int cnt = 0;
    for (size_t i = 0; i < alerts.size(); i++)
    {
        float t = (float)alerts[i].temperature;
        float h = (float)alerts[i].humidity;
        if (t <= 0 && h <= 0) continue;
        if (t > 0) { if (t < tMin) tMin = t; if (t > tMax) tMax = t; tSum += t; }
        if (h > 0) { if (h < hMin) hMin = h; if (h > hMax) hMax = h; hSum += h; }
        cnt++;
    }
    if (cnt > 0)
    {
        st.fTempMin = tMin; st.fTempMax = tMax; st.fTempAvg = tSum / cnt;
        st.fHumiMin = hMin; st.fHumiMax = hMax; st.fHumiAvg = hSum / cnt;
        st.nSampleCount = cnt;
    }
    return st;
}

// ─── PDF 페이지 컨텐츠 ───

static std::string BuildPage(
    const std::string& dateStr,
    const std::string& operName,
    const AlertSummary& summary,
    const TempHumiStats& stats,
    const std::vector<AlertRecord>& alerts)
{
    std::ostringstream s;
    float L = 50;
    float y;
    char buf[64];

    // ══════════ 헤더 ══════════
    s << "0.137 0.188 0.294 rg\n";
    s << "0 792 595 50 re f\n";
    s << "1 1 1 rg\n";
    s << "BT /F2 18 Tf " << L << " 805 Td "
        << "(CONVEYOR BELT DAILY OPERATION REPORT) Tj ET\n";

    s << "BT /F1 10 Tf " << L << " 775 Td "
        << "(Date: " << dateStr << ") Tj ET\n";
    if (!operName.empty())
        s << "BT /F1 10 Tf 400 775 Td "
        << "(Operator: " << EscapePdf(operName) << ") Tj ET\n";

    s << "0 0 0 RG 0.5 w\n";
    s << L << " 760 m 545 760 l S\n";

    // ══════════ 1. ALERT SUMMARY ══════════
    y = 735;
    s << "0.137 0.188 0.294 rg\n";
    s << "BT /F2 13 Tf " << L << " " << y << " Td "
        << "(1. ALERT SUMMARY) Tj ET\n";

    // 테이블 헤더
    y = 706;
    s << "0.2 0.3 0.5 rg\n";
    s << L << " " << (y + 12) << " 300 -22 re f\n";
    s << "1 1 1 rg\n";
    s << "BT /F2 10 Tf " << (L + 10) << " " << y << " Td (Category) Tj ET\n";
    s << "BT /F2 10 Tf 260 " << y << " Td (Count) Tj ET\n";

    // Total Alerts
    y = 679;
    s << "0.95 0.95 0.95 rg\n";
    s << L << " " << (y + 12) << " 300 -22 re f\n";
    s << "0 0 0 rg\n";
    s << "BT /F1 11 Tf " << (L + 10) << " " << y << " Td (Total Alerts) Tj ET\n";
    s << "BT /F2 11 Tf 260 " << y << " Td (" << summary.totalCount << ") Tj ET\n";

    // WARNING
    y = 652;
    s << "1 1 1 rg\n";
    s << L << " " << (y + 12) << " 300 -22 re f\n";
    s << "0.78 0.47 0 rg\n";
    s << "BT /F1 11 Tf " << (L + 10) << " " << y << " Td (WARNING) Tj ET\n";
    s << "BT /F2 11 Tf 260 " << y << " Td (" << summary.warningCount << ") Tj ET\n";

    // DANGER
    y = 625;
    s << "0.95 0.95 0.95 rg\n";
    s << L << " " << (y + 12) << " 300 -22 re f\n";
    s << "0.86 0.2 0.2 rg\n";
    s << "BT /F1 11 Tf " << (L + 10) << " " << y << " Td (DANGER) Tj ET\n";
    s << "BT /F2 11 Tf 260 " << y << " Td (" << summary.dangerCount << ") Tj ET\n";

    // EMERGENCY STOP
    y = 598;
    s << "1 1 1 rg\n";
    s << L << " " << (y + 12) << " 300 -22 re f\n";
    s << "0.78 0 0 rg\n";
    s << "BT /F1 11 Tf " << (L + 10) << " " << y << " Td (EMERGENCY STOP) Tj ET\n";
    s << "BT /F2 11 Tf 260 " << y << " Td (" << summary.stopCount << ") Tj ET\n";

    // ══════════ 2. TEMPERATURE / HUMIDITY ══════════
    y = 560;
    s << "0.137 0.188 0.294 rg\n";
    s << "BT /F2 13 Tf " << L << " " << y << " Td "
        << "(2. TEMPERATURE / HUMIDITY STATISTICS) Tj ET\n";

    // 테이블 헤더
    y = 533;
    s << "0.2 0.3 0.5 rg\n";
    s << L << " " << (y + 12) << " 450 -22 re f\n";
    s << "1 1 1 rg\n";
    s << "BT /F2 10 Tf " << (L + 10) << " " << y << " Td (Metric) Tj ET\n";
    s << "BT /F2 10 Tf 200 " << y << " Td (Min) Tj ET\n";
    s << "BT /F2 10 Tf 290 " << y << " Td (Max) Tj ET\n";
    s << "BT /F2 10 Tf 380 " << y << " Td (Average) Tj ET\n";

    if (stats.nSampleCount > 0)
    {
        // Temperature
        y = 506;
        s << "0.95 0.95 0.95 rg\n";
        s << L << " " << (y + 12) << " 450 -22 re f\n";
        s << "0 0 0 rg\n";
        s << "BT /F1 10 Tf " << (L + 10) << " " << y << " Td (Temperature) Tj ET\n";
        sprintf_s(buf, "%.1f C", stats.fTempMin);
        s << "BT /F1 10 Tf 200 " << y << " Td (" << buf << ") Tj ET\n";
        sprintf_s(buf, "%.1f C", stats.fTempMax);
        s << "BT /F1 10 Tf 290 " << y << " Td (" << buf << ") Tj ET\n";
        sprintf_s(buf, "%.1f C", stats.fTempAvg);
        s << "BT /F1 10 Tf 380 " << y << " Td (" << buf << ") Tj ET\n";

        // Humidity
        y = 479;
        s << "0.9 0.9 0.92 rg\n";
        s << L << " " << (y + 12) << " 450 -22 re f\n";
        s << "0 0 0 rg\n";
        s << "BT /F1 10 Tf " << (L + 10) << " " << y << " Td (Humidity) Tj ET\n";
        sprintf_s(buf, "%.1f %%", stats.fHumiMin);
        s << "BT /F1 10 Tf 200 " << y << " Td (" << buf << ") Tj ET\n";
        sprintf_s(buf, "%.1f %%", stats.fHumiMax);
        s << "BT /F1 10 Tf 290 " << y << " Td (" << buf << ") Tj ET\n";
        sprintf_s(buf, "%.1f %%", stats.fHumiAvg);
        s << "BT /F1 10 Tf 380 " << y << " Td (" << buf << ") Tj ET\n";

        y = 455;
    }
    else
    {
        y = 510;
        s << "0 0 0 rg\n";
        s << "BT /F1 10 Tf " << (L + 10) << " " << y
            << " Td (No sensor data recorded today.) Tj ET\n";
        y = 490;
    }

    // ══════════ 3. ALERT HISTORY ══════════
    y -= 30;
    s << "0.137 0.188 0.294 rg\n";
    s << "BT /F2 13 Tf " << L << " " << y << " Td "
        << "(3. RECENT ALERT HISTORY) Tj ET\n";

    y -= 28;

    if (alerts.empty())
    {
        s << "0 0 0 rg\n";
        s << "BT /F1 10 Tf " << (L + 10) << " " << y
            << " Td (No alerts recorded today.) Tj ET\n";
    }
    else
    {
        // 테이블 헤더
        s << "0.2 0.3 0.5 rg\n";
        s << L << " " << (y + 11) << " 495 -22 re f\n";
        s << "1 1 1 rg\n";
        s << "BT /F2 9 Tf " << (L + 5) << " " << y << " Td (No.) Tj ET\n";
        s << "BT /F2 9 Tf 100 " << y << " Td (Time) Tj ET\n";
        s << "BT /F2 9 Tf 190 " << y << " Td (Status) Tj ET\n";
        s << "BT /F2 9 Tf 260 " << y << " Td (Temp) Tj ET\n";
        s << "BT /F2 9 Tf 320 " << y << " Td (Humi) Tj ET\n";
        s << "BT /F2 9 Tf 380 " << y << " Td (Description) Tj ET\n";

        y -= 28;

        for (size_t idx = 0; idx < alerts.size(); idx++)
        {
            if (y < 55) break;

            int row = (int)idx + 1;

            if (row % 2 == 0)
            {
                s << "0.95 0.95 0.97 rg\n";
                s << L << " " << (y + 10) << " 495 -19 re f\n";
            }

            if (alerts[idx].status == "DANGER")
                s << "0.86 0.2 0.2 rg\n";
            else if (alerts[idx].status == "WARNING")
                s << "0.6 0.35 0 rg\n";
            else if (alerts[idx].status == "STOP")
                s << "0.78 0 0 rg\n";
            else
                s << "0 0 0 rg\n";

            sprintf_s(buf, "%d", row);
            s << "BT /F1 9 Tf " << (L + 5) << " " << y << " Td (" << buf << ") Tj ET\n";

            std::string timeStr = alerts[idx].timestamp;
            if (timeStr.size() > 11) timeStr = timeStr.substr(11);
            s << "BT /F1 9 Tf 100 " << y << " Td (" << EscapePdf(timeStr) << ") Tj ET\n";
            s << "BT /F2 9 Tf 190 " << y << " Td (" << alerts[idx].status << ") Tj ET\n";

            sprintf_s(buf, "%.1f", alerts[idx].temperature);
            s << "BT /F1 9 Tf 260 " << y << " Td (" << buf << ") Tj ET\n";
            sprintf_s(buf, "%.1f", alerts[idx].humidity);
            s << "BT /F1 9 Tf 320 " << y << " Td (" << buf << ") Tj ET\n";

            std::string desc = alerts[idx].content;
            if (desc.size() > 30) desc = desc.substr(0, 30) + "...";
            s << "0 0 0 rg\n";
            s << "BT /F1 8 Tf 380 " << y << " Td (" << EscapePdf(desc) << ") Tj ET\n";

            y -= 18;
        }
    }

    // ══════════ 푸터 ══════════
    s << "0.5 0.5 0.5 rg\n";
    s << "0.5 0.5 0.5 RG 0.3 w\n";
    s << L << " 38 m 545 38 l S\n";

    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);
    sprintf_s(buf, "Generated: %04d-%02d-%02d %02d:%02d:%02d",
        tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
        tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    s << "BT /F1 8 Tf " << L << " 26 Td (" << buf << ") Tj ET\n";
    s << "BT /F1 8 Tf 380 26 Td (Conveyor Belt Monitoring System) Tj ET\n";

    return s.str();
}

// ─── PDF 파일 쓰기 ───

static bool WritePdfFile(const std::string& path, const std::string& content)
{
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "wb");
    if (!fp) return false;

    long pos = 0;
    const char* hdr = "%PDF-1.4\n";
    fwrite(hdr, 1, strlen(hdr), fp);
    pos = (long)strlen(hdr);

    std::string obj1 = "<< /Type /Catalog /Pages 2 0 R >>";
    std::string obj2 = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
    std::string obj3 =
        "<< /Type /Page /Parent 2 0 R "
        "/MediaBox [0 0 595 842] "
        "/Contents 5 0 R "
        "/Resources << /Font << /F1 4 0 R /F2 6 0 R >> >> >>";
    std::string obj4 =
        "<< /Type /Font /Subtype /Type1 "
        "/BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

    std::ostringstream ss;
    ss << "<< /Length " << content.size() << " >>\nstream\n" << content << "\nendstream";
    std::string obj5 = ss.str();

    std::string obj6 =
        "<< /Type /Font /Subtype /Type1 "
        "/BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";

    std::string objs[6];
    objs[0] = obj1; objs[1] = obj2; objs[2] = obj3;
    objs[3] = obj4; objs[4] = obj5; objs[5] = obj6;
    long offs[6];

    for (int i = 0; i < 6; i++)
    {
        offs[i] = pos;
        char nb[32];
        sprintf_s(nb, "%d 0 obj\n", i + 1);
        fwrite(nb, 1, strlen(nb), fp);
        pos += (long)strlen(nb);
        fwrite(objs[i].c_str(), 1, objs[i].size(), fp);
        pos += (long)objs[i].size();
        const char* eo = "\nendobj\n";
        fwrite(eo, 1, strlen(eo), fp);
        pos += (long)strlen(eo);
    }

    long xrefPos = pos;
    std::ostringstream xr;
    xr << "xref\n0 7\n0000000000 65535 f \n";
    for (int i = 0; i < 6; i++)
        xr << std::setfill('0') << std::setw(10) << offs[i] << " 00000 n \n";
    std::string xrs = xr.str();
    fwrite(xrs.c_str(), 1, xrs.size(), fp);

    std::ostringstream tr;
    tr << "trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF\n";
    std::string trs = tr.str();
    fwrite(trs.c_str(), 1, trs.size(), fp);

    fclose(fp);
    return true;
}

// ─── CReportGenerator 멤버 함수 ───

CReportGenerator::CReportGenerator()
    : m_pDB(nullptr)
{
    m_strOutputFolder = _T(".\\Reports");
}

CReportGenerator::~CReportGenerator() {}

CString CReportGenerator::GenerateDailyReport()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    CString d;
    d.Format(_T("%04d-%02d-%02d"), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return GenerateDailyReport(d);
}

CString CReportGenerator::GenerateDailyReport(const CString& dateStr)
{
    if (!m_pDB)
    {
        m_strLastError = _T("Database pointer is NULL");
        return _T("");
    }
    if (!m_pDB->IsOpen())
    {
        m_strLastError = _T("Database not open (IsOpen=false)");
        return _T("");
    }

    ::CreateDirectory(m_strOutputFolder, NULL);

    CString dp = dateStr;
    dp.Remove(_T('-'));
    CString fn;
    if (m_strOperator.IsEmpty())
        fn.Format(_T("%s\\DailyReport_%s.pdf"), (LPCTSTR)m_strOutputFolder, (LPCTSTR)dp);
    else
        fn.Format(_T("%s\\DailyReport_%s_%s.pdf"), (LPCTSTR)m_strOutputFolder, (LPCTSTR)dp, (LPCTSTR)m_strOperator);

    std::string filePath = CStringToAnsi(fn);
    std::string ds = CStringToAnsi(dateStr);
    std::string op = CStringToAnsi(m_strOperator);

    AlertSummary summary = m_pDB->GetTodaySummary();
    TempHumiStats stats = CalcStats(m_pDB, ds);
    std::vector<AlertRecord> alerts = FilterAlertsByDate(m_pDB, ds, 30);

    std::string page = BuildPage(ds, op, summary, stats, alerts);

    if (!WritePdfFile(filePath, page))
    {
        m_strLastError = _T("Failed to write PDF file");
        return _T("");
    }

    return fn;
}