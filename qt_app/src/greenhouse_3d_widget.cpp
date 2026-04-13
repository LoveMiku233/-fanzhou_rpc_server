/**
 * @file greenhouse_3d_widget.cpp
 * @brief 3D大棚可视化控制页面实现
 */

#include "greenhouse_3d_widget.h"

#include "rpc_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

Greenhouse3DWidget::Greenhouse3DWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , titleLabel_(nullptr)
    , hintLabel_(nullptr)
    , statusLabel_(nullptr)
    , refreshTimer_(new QTimer(this))
    , canvasArea_(nullptr)
{
    setAutoFillBackground(false);
    setupUi();

    connect(refreshTimer_, &QTimer::timeout, this, &Greenhouse3DWidget::onRefreshGroups);
    refreshTimer_->start(10000);

    QTimer::singleShot(200, this, &Greenhouse3DWidget::onRefreshGroups);
}

void Greenhouse3DWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 14, 18, 14);
    mainLayout->setSpacing(8);

    titleLabel_ = new QLabel(QStringLiteral("3D大棚联动"), this);
    titleLabel_->setStyleSheet(QStringLiteral(
        "font-size: 22px; font-weight: 700; color: #14304a;"));
    mainLayout->addWidget(titleLabel_);

    hintLabel_ = new QLabel(QStringLiteral("点击设备点位触发分组动作，优先使用 group.controlOptimized（失败自动回退 group.control）。"), this);
    hintLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #2f4f66; background: rgba(255,255,255,0.6);"
        "padding: 6px 10px; border-radius: 8px;"));
    mainLayout->addWidget(hintLabel_);

    statusLabel_ = new QLabel(QStringLiteral("正在加载分组映射..."), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #1f4d2c; background: rgba(210,245,223,0.8);"
        "padding: 5px 10px; border-radius: 6px;"));
    mainLayout->addWidget(statusLabel_);

    canvasArea_ = new QWidget(this);
    canvasArea_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    canvasArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(canvasArea_, 1);

    struct SpotSeed {
        const char *name;
        qreal x;
        qreal y;
    };
    const SpotSeed seeds[] = {
        {"A区", 0.19, 0.34},
        {"B区", 0.33, 0.40},
        {"C区", 0.47, 0.46},
        {"D区", 0.62, 0.52},
        {"E区", 0.74, 0.58},
        {"F区", 0.86, 0.64}
    };

    for (const auto &seed : seeds) {
        Hotspot spot;
        spot.name = QString::fromUtf8(seed.name);
        spot.relativePos = QPointF(seed.x, seed.y);
        spot.groupId = -1;
        spot.button = new QPushButton(this);
        spot.button->setCursor(Qt::PointingHandCursor);
        spot.button->setFixedSize(96, 40);
        spot.button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #fffcf1, stop:1 #ffe2a7);"
            "  color: #3b2f1c; border: 2px solid #d5a44b; border-radius: 12px;"
            "  font-weight: 700; font-size: 12px;"
            "}"
            "QPushButton:hover { background: #ffd480; }"
            "QPushButton:pressed { background: #ffbf4d; }"));
        connect(spot.button, &QPushButton::clicked, this, &Greenhouse3DWidget::onDeviceHotspotClicked);
        hotspots_.append(spot);
    }

    updateHotspotLabels();
    layoutHotspots();
}

void Greenhouse3DWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF full = rect();
    QLinearGradient bgGrad(0, 0, 0, full.height());
    bgGrad.setColorAt(0.0, QColor("#eaf6ff"));
    bgGrad.setColorAt(0.45, QColor("#d4f1de"));
    bgGrad.setColorAt(1.0, QColor("#b8ddb8"));
    p.fillRect(full, bgGrad);

    const qreal topY = 128.0;
    const qreal left = 90.0;
    const qreal right = width() - 90.0;
    const qreal frontY = height() - 84.0;

    const QPointF backL(left + 90.0, topY);
    const QPointF backR(right - 90.0, topY);
    const QPointF frontL(left, frontY);
    const QPointF frontR(right, frontY);

    QPolygonF floorPoly;
    floorPoly << frontL << frontR << backR << backL;
    QLinearGradient floorGrad(frontL, backR);
    floorGrad.setColorAt(0.0, QColor(82, 134, 85, 180));
    floorGrad.setColorAt(1.0, QColor(56, 104, 62, 150));
    p.setPen(Qt::NoPen);
    p.setBrush(floorGrad);
    p.drawPolygon(floorPoly);

    QPen framePen(QColor("#2b495b"), 3.0);
    p.setPen(framePen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(frontL, backL);
    p.drawLine(frontR, backR);
    p.drawLine(backL, backR);
    p.drawLine(frontL, frontR);

    for (int i = 1; i <= 6; ++i) {
        const qreal t = i / 7.0;
        const QPointF l = frontL + t * (backL - frontL);
        const QPointF r = frontR + t * (backR - frontR);
        p.setPen(QPen(QColor(43, 73, 91, 120), 1.6));
        p.drawLine(l, r);
    }

    QPen archPen(QColor("#1f3d4e"), 2.2);
    p.setPen(archPen);
    for (int i = 0; i <= 5; ++i) {
        const qreal t = i / 5.0;
        const QPointF baseL = frontL + t * (backL - frontL);
        const QPointF baseR = frontR + t * (backR - frontR);
        const QPointF apex((baseL.x() + baseR.x()) * 0.5, baseL.y() - (42.0 + (1.0 - t) * 56.0));
        QPainterPath path(baseL);
        path.quadTo(apex, baseR);
        p.drawPath(path);
    }
}

void Greenhouse3DWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutHotspots();
}

void Greenhouse3DWidget::layoutHotspots()
{
    buttonIndexMap_.clear();
    for (int i = 0; i < hotspots_.size(); ++i) {
        Hotspot &spot = hotspots_[i];
        if (!spot.button) {
            continue;
        }
        const int x = static_cast<int>(spot.relativePos.x() * width()) - spot.button->width() / 2;
        const int y = static_cast<int>(spot.relativePos.y() * height()) - spot.button->height() / 2;
        spot.button->move(x, y);
        spot.button->raise();
        buttonIndexMap_.insert(spot.button, i);
    }
}

void Greenhouse3DWidget::updateHotspotLabels()
{
    for (const Hotspot &spot : hotspots_) {
        if (!spot.button) {
            continue;
        }
        if (spot.groupId > 0) {
            spot.button->setText(QStringLiteral("%1\nG%2").arg(spot.name).arg(spot.groupId));
        } else {
            spot.button->setText(QStringLiteral("%1\n未绑定").arg(spot.name));
        }
    }
}

void Greenhouse3DWidget::onRefreshGroups()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("未连接RPC服务器，无法加载分组。"));
        return;
    }

    rpcClient_->callAsync(QStringLiteral("group.list"), QJsonObject(), [this](const QJsonValue &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            statusLabel_->setText(QStringLiteral("分组加载失败：%1").arg(error.value(QStringLiteral("message")).toString()));
            emit logMessage(QStringLiteral("3D大棚分组加载失败"), QStringLiteral("WARN"));
            return;
        }

        const QJsonObject obj = result.toObject();
        const QJsonArray groups = obj.value(QStringLiteral("groups")).toArray();
        for (int i = 0; i < hotspots_.size(); ++i) {
            hotspots_[i].groupId = (i < groups.size())
                                       ? groups[i].toObject().value(QStringLiteral("groupId")).toInt(-1)
                                       : -1;
        }
        updateHotspotLabels();
        statusLabel_->setText(QStringLiteral("已加载 %1 个分组映射，点击点位可启动分组。").arg(groups.size()));
        emit logMessage(QStringLiteral("3D大棚分组映射已更新"));
    }, 3000);
}

void Greenhouse3DWidget::onDeviceHotspotClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn || !buttonIndexMap_.contains(btn)) {
        return;
    }
    triggerGroupStart(buttonIndexMap_.value(btn));
}

void Greenhouse3DWidget::triggerGroupStart(int hotspotIndex)
{
    triggerGroupControl(hotspotIndex, QStringLiteral("fwd"));
}

void Greenhouse3DWidget::triggerGroupControl(int hotspotIndex, const QString &action)
{
    if (hotspotIndex < 0 || hotspotIndex >= hotspots_.size()) {
        return;
    }
    const Hotspot &spot = hotspots_[hotspotIndex];
    if (spot.groupId <= 0) {
        statusLabel_->setText(QStringLiteral("%1 未绑定分组，请先在分组页面创建。").arg(spot.name));
        emit logMessage(QStringLiteral("3D大棚点击失败：未绑定分组"), QStringLiteral("WARN"));
        return;
    }
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("未连接服务器，无法执行分组控制。"));
        return;
    }

    QJsonObject params{
        {QStringLiteral("groupId"), spot.groupId},
        {QStringLiteral("ch"), -1},
        {QStringLiteral("action"), action}
    };

    statusLabel_->setText(QStringLiteral("正在执行 %1 (group %2, action=%3)...")
                              .arg(spot.name)
                              .arg(spot.groupId)
                              .arg(action));

    rpcClient_->callAsync(QStringLiteral("group.controlOptimized"), params, [this, spot, action, params](const QJsonValue &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            // 新接口失败时回退到兼容接口
            rpcClient_->callAsync(QStringLiteral("group.control"), params, [this, spot, action](const QJsonValue &fallbackResult, const QJsonObject &fallbackError) {
                if (!fallbackError.isEmpty()) {
                    const QString msg = fallbackError.value(QStringLiteral("message")).toString();
                    statusLabel_->setText(QStringLiteral("%1 执行失败：%2").arg(spot.name, msg));
                    emit logMessage(QStringLiteral("3D大棚分组控制失败（optimized+fallback）：%1").arg(msg), QStringLiteral("WARN"));
                    return;
                }

                const QJsonObject obj = fallbackResult.toObject();
                if (obj.value(QStringLiteral("ok")).toBool()) {
                    statusLabel_->setText(QStringLiteral("%1 已执行（group %2, action=%3）")
                                              .arg(spot.name)
                                              .arg(spot.groupId)
                                              .arg(action));
                    emit logMessage(QStringLiteral("3D大棚分组控制成功（fallback）：group %1").arg(spot.groupId));
                } else {
                    const QString err = obj.value(QStringLiteral("error")).toString();
                    statusLabel_->setText(QStringLiteral("%1 执行失败：%2").arg(spot.name, err));
                    emit logMessage(QStringLiteral("3D大棚分组控制失败（fallback）：%1").arg(err), QStringLiteral("WARN"));
                }
            }, 3000);
            return;
        }

        const QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            statusLabel_->setText(QStringLiteral("%1 已执行（group %2, action=%3）")
                                      .arg(spot.name)
                                      .arg(spot.groupId)
                                      .arg(action));
            emit logMessage(QStringLiteral("3D大棚分组控制成功（optimized）：group %1").arg(spot.groupId));
        } else {
            const QString err = obj.value(QStringLiteral("error")).toString();
            statusLabel_->setText(QStringLiteral("%1 执行失败：%2").arg(spot.name, err));
            emit logMessage(QStringLiteral("3D大棚分组控制失败（optimized）：%1").arg(err), QStringLiteral("WARN"));
        }
    }, 3000);
}
