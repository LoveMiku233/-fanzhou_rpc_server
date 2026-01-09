/**
 * @file log_widget.cpp
 * @brief 日志页面实现
 */

#include "log_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QTextCursor>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
    , logTextEdit_(nullptr)
    , filterCombo_(nullptr)
    , countLabel_(nullptr)
    , clearButton_(nullptr)
    , exportButton_(nullptr)
    , totalCount_(0)
    , warningCount_(0)
    , errorCount_(0)
{
    setupUi();
}

void LogWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("📋 系统日志"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: bold; color: #2c3e50; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    QLabel *filterLabel = new QLabel(QStringLiteral("筛选:"), this);
    toolbarLayout->addWidget(filterLabel);

    filterCombo_ = new QComboBox(this);
    filterCombo_->addItem(QStringLiteral("全部"), QStringLiteral("ALL"));
    filterCombo_->addItem(QStringLiteral("信息"), QStringLiteral("INFO"));
    filterCombo_->addItem(QStringLiteral("警告"), QStringLiteral("WARN"));
    filterCombo_->addItem(QStringLiteral("错误"), QStringLiteral("ERROR"));
    filterCombo_->setMinimumWidth(120);
    toolbarLayout->addWidget(filterCombo_);

    toolbarLayout->addStretch();

    countLabel_ = new QLabel(QStringLiteral("总计: 0 | 警告: 0 | 错误: 0"), this);
    countLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    toolbarLayout->addWidget(countLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 日志显示区域
    logTextEdit_ = new QTextEdit(this);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMinimumHeight(300);
    logTextEdit_->setStyleSheet(QStringLiteral(
        "QTextEdit { "
        "  background-color: #1e1e1e; "
        "  color: #d4d4d4; "
        "  font-family: 'Consolas', 'Monaco', monospace; "
        "  font-size: 13px; "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  padding: 10px; "
        "}"));
    mainLayout->addWidget(logTextEdit_, 1);

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    clearButton_ = new QPushButton(QStringLiteral("🗑️ 清空日志"), this);
    clearButton_->setMinimumHeight(50);
    connect(clearButton_, &QPushButton::clicked, this, &LogWidget::clearLogs);
    buttonLayout->addWidget(clearButton_);

    exportButton_ = new QPushButton(QStringLiteral("💾 导出日志"), this);
    exportButton_->setMinimumHeight(50);
    exportButton_->setProperty("type", QStringLiteral("success"));
    connect(exportButton_, &QPushButton::clicked, this, &LogWidget::onExportClicked);
    buttonLayout->addWidget(exportButton_);

    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

void LogWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出日志"),
        QStringLiteral("log_%1.txt").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))),
        QStringLiteral("文本文件 (*.txt)"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << logTextEdit_->toPlainText();
            file.close();
            QMessageBox::information(this, QStringLiteral("成功"),
                QStringLiteral("日志已导出到: %1").arg(fileName));
        }
    }
}

void LogWidget::appendLog(const QString &message, const QString &level)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    QString colorCode;
    QString levelText;

    if (level == QStringLiteral("WARN") || level == QStringLiteral("WARNING")) {
        colorCode = QStringLiteral("#f39c12");
        levelText = QStringLiteral("⚠️ 警告");
        warningCount_++;
    } else if (level == QStringLiteral("ERROR")) {
        colorCode = QStringLiteral("#e74c3c");
        levelText = QStringLiteral("❌ 错误");
        errorCount_++;
        emit newAlertMessage(message);
    } else {
        colorCode = QStringLiteral("#3498db");
        levelText = QStringLiteral("ℹ️ 信息");
    }

    totalCount_++;

    QString formattedMessage = QStringLiteral(
        "<span style='color: #7f8c8d;'>[%1]</span> "
        "<span style='color: %2;'>[%3]</span> %4")
        .arg(timestamp, colorCode, levelText, message.toHtmlEscaped());

    logTextEdit_->append(formattedMessage);

    // 滚动到底部
    QTextCursor cursor = logTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit_->setTextCursor(cursor);

    // 更新计数
    countLabel_->setText(QStringLiteral("总计: %1 | 警告: %2 | 错误: %3")
        .arg(totalCount_).arg(warningCount_).arg(errorCount_));
}

void LogWidget::appendWarning(const QString &message)
{
    appendLog(message, QStringLiteral("WARN"));
}

void LogWidget::appendError(const QString &message)
{
    appendLog(message, QStringLiteral("ERROR"));
}

void LogWidget::clearLogs()
{
    logTextEdit_->clear();
    totalCount_ = 0;
    warningCount_ = 0;
    errorCount_ = 0;
    countLabel_->setText(QStringLiteral("总计: 0 | 警告: 0 | 错误: 0"));
}
