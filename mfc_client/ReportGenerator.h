#pragma once
// ================================================================
// ReportGenerator.h — 일일 운영 PDF 리포트 자동 생성
// ================================================================

#include "DatabaseManager.h"

class CReportGenerator
{
public:
    CReportGenerator();
    ~CReportGenerator();

    void SetDBManager(CDatabaseManager* pDB) { m_pDB = pDB; }
    void SetOperatorName(const CString& name) { m_strOperator = name; }
    void SetOutputFolder(const CString& folder) { m_strOutputFolder = folder; }

    CString GenerateDailyReport();
    CString GenerateDailyReport(const CString& dateStr);

    CString GetLastError() const { return m_strLastError; }

private:
    CDatabaseManager*   m_pDB;
    CString             m_strOperator;
    CString             m_strOutputFolder;
    CString             m_strLastError;
};
