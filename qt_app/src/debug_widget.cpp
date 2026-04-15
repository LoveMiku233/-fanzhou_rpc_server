/**
 * @file debug_widget.cpp
 * @brief 调试页面实现
 */

#include "debug_widget.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QScrollArea>
#include <QScroller>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QFontMetrics>
#include <QScrollBar>

using namespace UIConstants;

DebugWidget::DebugWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , filePathEdit_(nullptr)
    , refreshButton_(nullptr)
    , loadFileButton_(nullptr)
    , saveConfigButton_(nullptr)
    , tabs_(nullptr)
    , configEditor_(nullptr)
    , deviceEditor_(nullptr)
    , groupEditor_(nullptr)
    , sysEditor_(nullptr)
    , localFileEditor_(nullptr)
    , rpcMethodEdit_(nullptr)
    , rpcParamsEditor_(nullptr)
    , rpcSendButton_(nullptr)
    , rpcResultEditor_(nullptr)
{
    setupUi();
}

void DebugWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(PAGE_SPACING);

    QLabel *title = new QLabel(QStringLiteral("调试中心"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50;").arg(FONT_SIZE_TITLE));
    mainLayout->addWidget(title);

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(CARD_SPACING);

    refreshButton_ = new QPushButton(QStringLiteral("刷新运行态"), this);
    refreshButton_->setMinimumHeight(BTN_HEIGHT);
    connect(refreshButton_, &QPushButton::clicked, this, &DebugWidget::onRefreshClicked);
    toolbar->addWidget(refreshButton_);

    saveConfigButton_ = new QPushButton(QStringLiteral("保存运行配置"), this);
    saveConfigButton_->setMinimumHeight(BTN_HEIGHT);
    connect(saveConfigButton_, &QPushButton::clicked, this, &DebugWidget::onSaveRuntimeConfigClicked);
    toolbar->addWidget(saveConfigButton_);

    filePathEdit_ = new QLineEdit(QStringLiteral("core.json"), this);
    filePathEdit_->setMinimumHeight(INPUT_HEIGHT);
    filePathEdit_->setPlaceholderText(QStringLiteral("本地配置文件路径"));
    toolbar->addWidget(filePathEdit_, 1);

    loadFileButton_ = new QPushButton(QStringLiteral("读取本地文件"), this);
    loadFileButton_->setMinimumHeight(BTN_HEIGHT);
    connect(loadFileButton_, &QPushButton::clicked, this, &DebugWidget::onLoadLocalFileClicked);
    toolbar->addWidget(loadFileButton_);

    mainLayout->addLayout(toolbar);

    statusLabel_ = new QLabel(QStringLiteral("等待操作"), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "padding: 4px 8px; border-radius: 8px; background: #e9eef1; color: #455a64;"));
    mainLayout->addWidget(statusLabel_);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->setTabPosition(QTabWidget::North);

    auto makeEditor = [this]() {
        QPlainTextEdit *editor = new QPlainTextEdit(this);
        editor->setReadOnly(true);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        editor->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #162027; color: #d8e3e7; "
            "border: 1px solid #2f3a40; border-radius: 8px; padding: 6px; }"));
        return editor;
    };

    configEditor_ = makeEditor();
    deviceEditor_ = makeEditor();
    groupEditor_ = makeEditor();
    sysEditor_ = makeEditor();
    localFileEditor_ = makeEditor();

    tabs_->addTab(configEditor_, QStringLiteral("config.get"));
    tabs_->addTab(deviceEditor_, QStringLiteral("device.list"));
    tabs_->addTab(groupEditor_, QStringLiteral("group.list"));
    tabs_->addTab(sysEditor_, QStringLiteral("sys.info"));
    tabs_->addTab(localFileEditor_, QStringLiteral("本地文件"));

    mainLayout->addWidget(tabs_);

    QGroupBox *rpcBox = new QGroupBox(QStringLiteral("手动RPC调用"), this);
    QVBoxLayout *rpcLayout = new QVBoxLayout(rpcBox);
    rpcLayout->setSpacing(CARD_SPACING);

    QHBoxLayout *rpcTop = new QHBoxLayout();
    rpcMethodEdit_ = new QLineEdit(QStringLiteral("config.get"), rpcBox);
    rpcMethodEdit_->setMinimumHeight(INPUT_HEIGHT);
    rpcMethodEdit_->setPlaceholderText(QStringLiteral("method, 例如: relay.statusAll"));
    rpcTop->addWidget(rpcMethodEdit_, 1);

    rpcSendButton_ = new QPushButton(QStringLiteral("发送"), rpcBox);
    rpcSendButton_->setMinimumHeight(BTN_HEIGHT);
    connect(rpcSendButton_, &QPushButton::clicked, this, &DebugWidget::onSendRpcClicked);
    rpcTop->addWidget(rpcSendButton_);
    rpcLayout->addLayout(rpcTop);

    rpcParamsEditor_ = new QPlainTextEdit(rpcBox);
    rpcParamsEditor_->setPlaceholderText(QStringLiteral("{\n  \"node\": 1\n}"));
    rpcParamsEditor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rpcParamsEditor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rpcParamsEditor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rpcParamsEditor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #111920; color: #d8e3e7; border: 1px solid #2f3a40; border-radius: 8px; }"));
    rpcParamsEditor_->setPlainText(QStringLiteral("{}"));
    updateEditorHeight(rpcParamsEditor_);
    rpcLayout->addWidget(rpcParamsEditor_);

    rpcResultEditor_ = new QPlainTextEdit(rpcBox);
    rpcResultEditor_->setReadOnly(true);
    rpcResultEditor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rpcResultEditor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rpcResultEditor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rpcResultEditor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #162027; color: #d8e3e7; border: 1px solid #2f3a40; border-radius: 8px; }"));
    rpcResultEditor_->setPlainText(QStringLiteral("waiting..."));
    updateEditorHeight(rpcResultEditor_);
    rpcLayout->addWidget(rpcResultEditor_);

    mainLayout->addWidget(rpcBox);
    mainLayout->addStretch();
}

void DebugWidget::setTextForEditor(QPlainTextEdit *editor, const QJsonValue &value)
{
    if (!editor) {
        return;
    }
    if (value.isObject()) {
        editor->setPlainText(QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented)));
    } else if (value.isArray()) {
        editor->setPlainText(QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented)));
    } else {
        editor->setPlainText(QStringLiteral("null"));
    }
    updateEditorHeight(editor);
}

void DebugWidget::updateEditorHeight(QPlainTextEdit *editor)
{
    if (!editor) {
        return;
    }
    const int blocks = qMax(1, editor->document()->blockCount());
    const int lineHeight = QFontMetrics(editor->font()).lineSpacing();
    const int target = qMax(120, blocks * lineHeight + 24);
    editor->setFixedHeight(target);
}

void DebugWidget::setStatus(const QString &text, const QString &level)
{
    if (!statusLabel_) {
        return;
    }

    QString style = QStringLiteral("padding: 4px 8px; border-radius: 8px;");
    if (level == QStringLiteral("ERROR")) {
        style += QStringLiteral("background: #fbe9e7; color: #b23b35;");
    } else if (level == QStringLiteral("WARN")) {
        style += QStringLiteral("background: #fff8e1; color: #8d6e2f;");
    } else {
        style += QStringLiteral("background: #e9eef1; color: #455a64;");
    }
    statusLabel_->setStyleSheet(style);
    statusLabel_->setText(QStringLiteral("%1  %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(text));
}

void DebugWidget::onRefreshClicked()
{
    refreshAll();
}

void DebugWidget::refreshAll()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        setStatus(QStringLiteral("未连接服务器"), QStringLiteral("WARN"));
        return;
    }

    setStatus(QStringLiteral("正在刷新运行态..."));

    rpcClient_->callAsync(QStringLiteral("config.get"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                configEditor_->setPlainText(QStringLiteral("error: %1").arg(error.value(QStringLiteral("message")).toString()));
                updateEditorHeight(configEditor_);
                return;
            }
            setTextForEditor(configEditor_, result);
        }, 2500);

    rpcClient_->callAsync(QStringLiteral("device.list"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                deviceEditor_->setPlainText(QStringLiteral("error: %1").arg(error.value(QStringLiteral("message")).toString()));
                updateEditorHeight(deviceEditor_);
                return;
            }
            setTextForEditor(deviceEditor_, result);
        }, 2500);

    rpcClient_->callAsync(QStringLiteral("group.list"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                groupEditor_->setPlainText(QStringLiteral("error: %1").arg(error.value(QStringLiteral("message")).toString()));
                updateEditorHeight(groupEditor_);
                return;
            }
            setTextForEditor(groupEditor_, result);
        }, 2500);

    rpcClient_->callAsync(QStringLiteral("sys.info"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                sysEditor_->setPlainText(QStringLiteral("error: %1").arg(error.value(QStringLiteral("message")).toString()));
                updateEditorHeight(sysEditor_);
                setStatus(QStringLiteral("刷新完成(部分失败)"), QStringLiteral("WARN"));
                return;
            }
            setTextForEditor(sysEditor_, result);
            setStatus(QStringLiteral("刷新完成"));
        }, 2500);
}

void DebugWidget::onLoadLocalFileClicked()
{
    const QString path = filePathEdit_ ? filePathEdit_->text().trimmed() : QString();
    if (path.isEmpty()) {
        setStatus(QStringLiteral("文件路径为空"), QStringLiteral("WARN"));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        localFileEditor_->setPlainText(QStringLiteral("open file failed: %1").arg(file.errorString()));
        updateEditorHeight(localFileEditor_);
        setStatus(QStringLiteral("读取本地文件失败"), QStringLiteral("ERROR"));
        return;
    }

    const QByteArray raw = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isObject()) {
        localFileEditor_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    } else if (doc.isArray()) {
        localFileEditor_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    } else {
        localFileEditor_->setPlainText(QString::fromUtf8(raw));
    }
    updateEditorHeight(localFileEditor_);

    tabs_->setCurrentWidget(localFileEditor_);
    setStatus(QStringLiteral("已读取本地文件: %1").arg(path));
}

void DebugWidget::onSaveRuntimeConfigClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        setStatus(QStringLiteral("未连接服务器"), QStringLiteral("WARN"));
        return;
    }

    QJsonObject params;
    const QString path = filePathEdit_ ? filePathEdit_->text().trimmed() : QString();
    if (!path.isEmpty()) {
        params.insert(QStringLiteral("path"), path);
    }

    rpcClient_->callAsync(QStringLiteral("config.save"), params, this,
        [this, path](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                setStatus(QStringLiteral("保存失败: %1").arg(error.value(QStringLiteral("message")).toString()),
                    QStringLiteral("ERROR"));
                return;
            }
            if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
                setStatus(QStringLiteral("运行配置已保存%1")
                    .arg(path.isEmpty() ? QString() : QStringLiteral(" -> %1").arg(path)));
                emit logMessage(QStringLiteral("Debug: config.save success"));
                return;
            }
            setStatus(QStringLiteral("保存失败: 返回异常"), QStringLiteral("ERROR"));
        }, 3000);
}

void DebugWidget::onSendRpcClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        setStatus(QStringLiteral("未连接服务器"), QStringLiteral("WARN"));
        return;
    }

    const QString method = rpcMethodEdit_ ? rpcMethodEdit_->text().trimmed() : QString();
    if (method.isEmpty()) {
        setStatus(QStringLiteral("method 不能为空"), QStringLiteral("WARN"));
        return;
    }

    QJsonObject params;
    const QString paramsText = rpcParamsEditor_ ? rpcParamsEditor_->toPlainText().trimmed() : QString();
    if (!paramsText.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(paramsText.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            setStatus(QStringLiteral("params 必须是合法JSON对象: %1").arg(parseError.errorString()),
                      QStringLiteral("ERROR"));
            return;
        }
        params = doc.object();
    }

    setStatus(QStringLiteral("发送RPC: %1").arg(method));
    if (rpcResultEditor_) {
        rpcResultEditor_->setPlainText(QStringLiteral("waiting..."));
        updateEditorHeight(rpcResultEditor_);
    }

    rpcClient_->callAsync(method, params, this,
        [this, method](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                if (rpcResultEditor_) {
                    rpcResultEditor_->setPlainText(
                        QStringLiteral("error: %1").arg(error.value(QStringLiteral("message")).toString()));
                    updateEditorHeight(rpcResultEditor_);
                }
                setStatus(QStringLiteral("RPC失败: %1").arg(method), QStringLiteral("ERROR"));
                return;
            }
            if (rpcResultEditor_) {
                if (result.isObject()) {
                    rpcResultEditor_->setPlainText(QString::fromUtf8(
                        QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented)));
                } else if (result.isArray()) {
                    rpcResultEditor_->setPlainText(QString::fromUtf8(
                        QJsonDocument(result.toArray()).toJson(QJsonDocument::Indented)));
                } else {
                    rpcResultEditor_->setPlainText(QStringLiteral("ok"));
                }
                updateEditorHeight(rpcResultEditor_);
            }
            setStatus(QStringLiteral("RPC完成: %1").arg(method));
        }, 3500);
}
