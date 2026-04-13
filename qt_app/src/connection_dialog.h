/**
 * @file connection_dialog.h
 * @brief 连接设置对话框头文件
 */

#ifndef CONNECTION_DIALOG_H
#define CONNECTION_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

/**
 * @brief 连接设置对话框
 */
class ConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    QString host() const;
    quint16 port() const;

    void setHost(const QString &host);
    void setPort(quint16 port);

private:
    void setupUi();

    QLineEdit *hostEdit_;
    QSpinBox *portSpinBox_;
};

#endif // CONNECTION_DIALOG_H
