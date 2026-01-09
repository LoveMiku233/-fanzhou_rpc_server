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
    setMinimumWidth(320);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 连接设置组
    QGroupBox *groupBox = new QGroupBox(QStringLiteral("服务器设置"), this);
    QFormLayout *formLayout = new QFormLayout(groupBox);
    formLayout->setSpacing(8);
    formLayout->setContentsMargins(10, 14, 10, 10);

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(QStringLiteral("192.168.1.100"));
    hostEdit_->setText(QStringLiteral("127.0.0.1"));
    hostEdit_->setMinimumHeight(32);
    formLayout->addRow(QStringLiteral("服务器:"), hostEdit_);

    portSpinBox_ = new QSpinBox(this);
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(12345);
    portSpinBox_->setMinimumHeight(32);
    formLayout->addRow(QStringLiteral("端口:"), portSpinBox_);

    mainLayout->addWidget(groupBox);

    // 说明
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：连接到RPC服务器，默认端口12345"),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: #666; padding: 2px; font-size: 10px;"));
    mainLayout->addWidget(helpLabel);

    mainLayout->addStretch();

    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();

    QPushButton *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setMinimumHeight(32);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    QPushButton *okButton = new QPushButton(QStringLiteral("确定"), this);
    okButton->setDefault(true);
    okButton->setProperty("type", QStringLiteral("success"));
    okButton->setMinimumHeight(32);
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
