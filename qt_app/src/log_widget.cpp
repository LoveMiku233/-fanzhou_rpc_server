/**
 * @file log_widget.cpp
 * @brief 日志页面实现（美化版）
 */

#include "log_widget.h"

#include <QVBoxLayout>
#include <QScroller>
#include <QDebug>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QTextCursor>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QScrollBar>

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
    qDebug() << "[LOG_WIDGET] 日志页面初始化完成";
}

void LogWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 页面标题 - 美化
    QLabel *titleLabel = new QLabel(QStringLiteral("[志] 系统日志"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 26px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏 - 美化
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    QLabel *filterLabel = new QLabel(QStringLiteral("[筛] 筛选:"), this);
    filterLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: #5d6d7e;"));
    toolbarLayout->addWidget(filterLabel);

    filterCombo_ = new QComboBox(this);
    filterCombo_->addItem(QStringLiteral("[全] 全部"), QStringLiteral("ALL"));
    filterCombo_->addItem(QStringLiteral("[信] 信息"), QStringLiteral("INFO"));
    filterCombo_->addItem(QStringLiteral("[警] 警告"), QStringLiteral("WARN"));
    filterCombo_->addItem(QStringLiteral("[错] 错误"), QStringLiteral("ERROR"));
    filterCombo_->setMinimumWidth(120);
    filterCombo_->setMinimumHeight(36);
    filterCombo_->setStyleSheet(QStringLiteral(
        "QComboBox { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 14px; }"
        "QComboBox:focus { border-color: #3498db; }"
        "QComboBox::drop-down { border: none; width: 30px; }"));
    toolbarLayout->addWidget(filterCombo_);

    toolbarLayout->addStretch();

    countLabel_ = new QLabel(QStringLiteral("[统] 共: 0 | 警: 0 | 错: 0"), this);
    countLabel_->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: 13px; padding: 8px 14px; "
        "background-color: #f8f9fa; border-radius: 8px; font-weight: 500;"));
    toolbarLayout->addWidget(countLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 日志显示区域 - 美化（仿终端风格）
    logTextEdit_ = new QTextEdit(this);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMinimumHeight(250);
    logTextEdit_->setStyleSheet(QStringLiteral(
        "QTextEdit { "
        "  background-color: #1e1e1e; "
        "  color: #d4d4d4; "
        "  font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "  font-size: 12px; "
        "  border: 2px solid #3c3c3c; "
        "  border-radius: 10px; "
        "  padding: 10px; "
        "}"
        "QScrollBar:vertical { width: 12px; background: #2d2d2d; border-radius: 6px; margin: 4px; }"
        "QScrollBar::handle:vertical { background: #4a4a4a; border-radius: 6px; min-height: 40px; }"
        "QScrollBar::handle:vertical:hover { background: #5a5a5a; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
    
    // 启用触控滚动
    QScroller::grabGesture(logTextEdit_->viewport(), QScroller::LeftMouseButtonGesture);
    
    mainLayout->addWidget(logTextEdit_, 1);

    // 按钮区域 - 美化
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    clearButton_ = new QPushButton(QStringLiteral("[清] 清空日志"), this);
    clearButton_->setMinimumHeight(44);
    clearButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #7f8c8d; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #6c7a7d; }"
        "QPushButton:pressed { background-color: #5a6268; }"));
    connect(clearButton_, &QPushButton::clicked, this, &LogWidget::clearLogs);
    buttonLayout->addWidget(clearButton_);

    exportButton_ = new QPushButton(QStringLiteral("[出] 导出日志"), this);
    exportButton_->setMinimumHeight(44);
    exportButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #1c5a8a; }"));
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
                QStringLiteral("[OK] 日志已导出到: %1").arg(fileName));
            qDebug() << "[LOG_WIDGET] 日志已导出:" << fileName;
        }
    }
}

void LogWidget::appendLog(const QString &message, const QString &level)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    QString colorCode;
    QString bgColor;
    QString levelText;
    QString icon;

    if (level == QStringLiteral("WARN") || level == QStringLiteral("WARNING")) {
        colorCode = QStringLiteral("#f39c12");
        bgColor = QStringLiteral("#3d2914");
        levelText = QStringLiteral("警告");
        icon = QStringLiteral("[警]");
        warningCount_++;
    } else if (level == QStringLiteral("ERROR")) {
        colorCode = QStringLiteral("#e74c3c");
        bgColor = QStringLiteral("#3d1f1f");
        levelText = QStringLiteral("错误");
        icon = QStringLiteral("[错]");
        errorCount_++;
        emit newAlertMessage(message);
    } else {
        colorCode = QStringLiteral("#3498db");
        bgColor = QStringLiteral("#1a2f3f");
        levelText = QStringLiteral("信息");
        icon = QStringLiteral("[信]");
    }

    totalCount_++;

    QString formattedMessage = QStringLiteral(
        "<div style='margin: 2px 0; padding: 4px 8px; border-radius: 4px; background-color: %1;'>"
        "<span style='color: #7f8c8d; font-size: 10px;'>[%2]</span> "
        "<span style='color: %3; font-weight: bold;'>[%4 %5]</span> "
        "<span style='color: #d4d4d4;'>%6</span>"
        "</div>")
        .arg(bgColor, timestamp, colorCode, icon, levelText, message.toHtmlEscaped());

    logTextEdit_->append(formattedMessage);

    // 滚动到底部
    QScrollBar *sb = logTextEdit_->verticalScrollBar();
    sb->setValue(sb->maximum());

    // 更新计数
    countLabel_->setText(QStringLiteral("[统] 共: %1 | 警: %2 | 错: %3")
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
    countLabel_->setText(QStringLiteral("[统] 共: 0 | 警: 0 | 错: 0"));
    qDebug() << "[LOG_WIDGET] 日志已清空";
}
