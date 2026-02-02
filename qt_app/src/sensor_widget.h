/**
 * @file sensor_widget.h
 * @brief 传感器数据监控页面 - 增强版
 */

#ifndef SENSOR_WIDGET_H
#define SENSOR_WIDGET_H

#include <QFrame>
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QPropertyAnimation>

class RpcClient;
class QGridLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;

/**
 * @brief 传感器数据卡片 - 增强版
 */
class SensorCard : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(qreal valueOpacity READ valueOpacity WRITE setValueOpacity)

public:
    explicit SensorCard(int nodeId, const QString &name, const QString &typeName, QWidget *parent = nullptr);
    ~SensorCard();
    
    int nodeId() const { return nodeId_; }
    void updateValue(double value, const QString &unit, bool valid, const QString &status = QString());
    void updateData(const QJsonObject &data);
    void updateParams(const QJsonObject &params);
    void setUpdating(bool updating);
    
    void setValueOpacity(qreal opacity);
    qreal valueOpacity() const { return m_valueOpacity; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setupUi();
    void updateValueStyle();
    
    int nodeId_;
    QString name_;
    QString typeName_;
    qreal m_valueOpacity;
    bool m_updating;
    double m_lastValue;
    
    // UI组件
    QLabel *nameLabel_;
    QLabel *typeLabel_;
    QLabel *valueLabel_;
    QLabel *unitLabel_;
    QLabel *statusLabel_;
    QLabel *detailLabel_;
    QLabel *paramsLabel_;
    QWidget *valueContainer_;
    
    QPropertyAnimation *m_valueAnimation;
};

/**
 * @brief 传感器监控页面 - 增强版
 */
class SensorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SensorWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

public slots:
    void refreshSensorList();

private slots:
    void onRefreshClicked();
    void onAutoRefreshToggled(bool checked);
    void onAutoRefreshTimeout();
    void onClearAllClicked();

private:
    void setupUi();
    void updateSensorCards(const QJsonArray &sensors);
    void clearSensorCards();
    void fetchSensorData(int nodeId);
    void fetchSensorParams(int nodeId);
    void updateSensorCardData(int nodeId, const QJsonObject &data);
    void updateSensorCardParams(int nodeId, const QJsonObject &params);
    
    QString getSensorTypeIcon(const QString &typeName) const;
    QString getSensorUnit(const QString &typeName) const;
    QColor getSensorColor(const QString &typeName) const;

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    QPushButton *autoRefreshBtn_;
    QPushButton *refreshBtn_;
    QLabel *lastUpdateLabel_;
    QTimer *refreshTimer_;
    
    QWidget *cardsContainer_;
    QGridLayout *cardsLayout_;
    QList<SensorCard*> sensorCards_;
    
    QJsonArray sensorsCache_;
    bool autoRefresh_;
    int refreshCount_;
};

#endif // SENSOR_WIDGET_H
