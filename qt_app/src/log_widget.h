/**
 * @file log_widget.h
 * @brief 日志页面头文件
 */

#ifndef LOG_WIDGET_H
#define LOG_WIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QStringList>

/**
 * @brief 日志页面 - 显示系统日志
 */
class LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);

public slots:
    void appendLog(const QString &message, const QString &level = QStringLiteral("INFO"));
    void appendWarning(const QString &message);
    void appendError(const QString &message);
    void clearLogs();

signals:
    void newAlertMessage(const QString &message);

private slots:
    void onExportClicked();

private:
    void setupUi();

    QTextEdit *logTextEdit_;
    QComboBox *filterCombo_;
    QLabel *countLabel_;
    QPushButton *clearButton_;
    QPushButton *exportButton_;

    int totalCount_;
    int warningCount_;
    int errorCount_;

    static constexpr int kMaxLogEntries = 2000;  ///< 最大日志条数，防止内存增长
};

#endif // LOG_WIDGET_H
