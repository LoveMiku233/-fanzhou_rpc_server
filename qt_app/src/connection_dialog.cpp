/**
 * @file connection_dialog.cpp
 * @brief 连接设置对话框实现
 */

#include "connection_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
    , hostEdit_(nullptr)
    , portSpinBox_(nullptr)
{
    setupUi();
}

void ConnectionDialog::setupUi()
{
    setWindowTitle(QStringLiteral("连接设置"));
    setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 连接设置组
    QGroupBox *groupBox = new QGroupBox(QStringLiteral("服务器设置"), this);
    QFormLayout *formLayout = new QFormLayout(groupBox);

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(QStringLiteral("例如: 192.168.1.100"));
    hostEdit_->setText(QStringLiteral("127.0.0.1"));
    formLayout->addRow(QStringLiteral("服务器地址:"), hostEdit_);

    portSpinBox_ = new QSpinBox(this);
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(12345);
    formLayout->addRow(QStringLiteral("端口:"), portSpinBox_);

    mainLayout->addWidget(groupBox);

    // 说明
    QLabel *helpLabel = new QLabel(
        QStringLiteral("💡 连接到运行泛舟RPC服务器的设备。\n"
                       "默认端口为 12345。"),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 12px;"));
    mainLayout->addWidget(helpLabel);

    mainLayout->addStretch();

    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    QPushButton *okButton = new QPushButton(QStringLiteral("确定"), this);
    okButton->setDefault(true);
    okButton->setProperty("type", QStringLiteral("success"));
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okButton);

    mainLayout->addLayout(buttonLayout);
}

QString ConnectionDialog::host() const
{
    return hostEdit_->text().trimmed();
}

quint16 ConnectionDialog::port() const
{
    return static_cast<quint16>(portSpinBox_->value());
}

void ConnectionDialog::setHost(const QString &host)
{
    hostEdit_->setText(host);
}

void ConnectionDialog::setPort(quint16 port)
{
    portSpinBox_->setValue(port);
}
