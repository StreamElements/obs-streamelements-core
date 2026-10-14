#pragma once

#ifndef STREAMELEMENTSREPORTISSUEDIALOG_H
#define STREAMELEMENTSREPORTISSUEDIALOG_H

#include <QDialog>
#include <memory>

// Forward declarations rather than includes: these headers drag in windows.h
// ahead of winsock2.h on this translation unit's include path.
class SEPerformanceHistoryTracker;
class SEAnalyticsEventsManager;

namespace Ui {
class SEReportIssueDialog;
}

class SEReportIssueDialog : public QDialog
{
    Q_OBJECT

public:
    //
    // Takes the two services it reports with. Neither is reached through a
    // global any more, so a test can construct this dialog with stubs and
    // assert on what it would have submitted.
    //
    SEReportIssueDialog(
	    std::shared_ptr<SEPerformanceHistoryTracker> performanceHistoryTracker,
	    std::shared_ptr<SEAnalyticsEventsManager> analyticsEventsManager,
	    QWidget *parent = 0);
    ~SEReportIssueDialog();

public slots:
	virtual void update();
	virtual void accept();

private:
    std::shared_ptr<SEPerformanceHistoryTracker> m_performanceHistoryTracker;
    std::shared_ptr<SEAnalyticsEventsManager> m_analyticsEventsManager;

    Ui::SEReportIssueDialog *ui;
};

#endif // STREAMELEMENTSREPORTISSUEDIALOG_H
