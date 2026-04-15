/**
 * @file debug_widget.h
 * @brief 调试页面 - 运行配置与状态查看
 */

#ifndef DEBUG_WIDGET_H
#define DEBUG_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QJsonValue>

class RpcClient;

class DebugWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DebugWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshAll();

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

private slots:
    void onRefreshClicked();
    void onLoadLocalFileClicked();
    void onSaveRuntimeConfigClicked();
    void onSendRpcClicked();

private:
    void setupUi();
    void setTextForEditor(QPlainTextEdit *editor, const QJsonValue &value);
    void updateEditorHeight(QPlainTextEdit *editor);
    void setStatus(const QString &text, const QString &level = QStringLiteral("INFO"));

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    QLineEdit *filePathEdit_;
    QPushButton *refreshButton_;
    QPushButton *loadFileButton_;
    QPushButton *saveConfigButton_;
    QTabWidget *tabs_;
    QPlainTextEdit *configEditor_;
    QPlainTextEdit *deviceEditor_;
    QPlainTextEdit *groupEditor_;
    QPlainTextEdit *sysEditor_;
    QPlainTextEdit *localFileEditor_;
    QLineEdit *rpcMethodEdit_;
    QPlainTextEdit *rpcParamsEditor_;
    QPushButton *rpcSendButton_;
    QPlainTextEdit *rpcResultEditor_;
};

#endif // DEBUG_WIDGET_H
