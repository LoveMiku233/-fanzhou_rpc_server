/**
 * @file planting_advice_widget.cpp
 * @brief 种植建议页面（固定规则版）
 */

#include "planting_advice_widget.h"

#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

PlantingAdviceWidget::PlantingAdviceWidget(QWidget *parent)
    : QWidget(parent)
    , cropCombo_(nullptr)
    , seasonCombo_(nullptr)
    , weatherCombo_(nullptr)
    , tempSpin_(nullptr)
    , humiditySpin_(nullptr)
    , lightSpin_(nullptr)
    , recommendationText_(nullptr)
    , gptOutput_(nullptr)
    , gptInput_(nullptr)
    , generateButton_(nullptr)
    , gptSendButton_(nullptr)
{
    setupUi();
    refreshSuggestion();
}

void PlantingAdviceWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(10);

    QLabel *title = new QLabel(QStringLiteral("种植建议"), this);
    title->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 900; color: #17364a;"));
    mainLayout->addWidget(title);

    QFrame *paramsCard = new QFrame(this);
    paramsCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #f5f9fc; border: 1px solid #bfd7e5; border-radius: 12px; }"
        "QLabel { color: #17364a; font-size: 12px; font-weight: 700; }"
        "QComboBox, QDoubleSpinBox { min-height: 32px; background: #ffffff; border: 1px solid #b8d1df; border-radius: 8px; padding: 2px 8px; }"));
    QGridLayout *paramsLayout = new QGridLayout(paramsCard);
    paramsLayout->setContentsMargins(12, 12, 12, 12);
    paramsLayout->setHorizontalSpacing(12);
    paramsLayout->setVerticalSpacing(10);

    cropCombo_ = new QComboBox(paramsCard);
    cropCombo_->addItems(QStringList{
        QStringLiteral("大葱"), QStringLiteral("草莓"), QStringLiteral("番茄"), QStringLiteral("黄瓜"),
        QStringLiteral("辣椒"), QStringLiteral("茄子"), QStringLiteral("生菜"), QStringLiteral("菠菜"),
        QStringLiteral("小白菜"), QStringLiteral("芹菜"), QStringLiteral("韭菜"), QStringLiteral("香菜")
    });

    seasonCombo_ = new QComboBox(paramsCard);
    seasonCombo_->addItems(QStringList{
        QStringLiteral("春季"), QStringLiteral("夏季"), QStringLiteral("秋季"), QStringLiteral("冬季")
    });
    seasonCombo_->setCurrentText(detectSeason());

    weatherCombo_ = new QComboBox(paramsCard);
    weatherCombo_->addItems(QStringList{
        QStringLiteral("晴"), QStringLiteral("多云"), QStringLiteral("阴"), QStringLiteral("雨"), QStringLiteral("大风")
    });

    tempSpin_ = new QDoubleSpinBox(paramsCard);
    tempSpin_->setRange(-10.0, 50.0);
    tempSpin_->setDecimals(1);
    tempSpin_->setSuffix(QStringLiteral(" °C"));
    tempSpin_->setValue(24.5);

    humiditySpin_ = new QDoubleSpinBox(paramsCard);
    humiditySpin_->setRange(0.0, 100.0);
    humiditySpin_->setDecimals(1);
    humiditySpin_->setSuffix(QStringLiteral(" %"));
    humiditySpin_->setValue(66.0);

    lightSpin_ = new QDoubleSpinBox(paramsCard);
    lightSpin_->setRange(0.0, 120000.0);
    lightSpin_->setDecimals(0);
    lightSpin_->setSuffix(QStringLiteral(" lux"));
    lightSpin_->setValue(18000.0);

    paramsLayout->addWidget(new QLabel(QStringLiteral("当前作物"), paramsCard), 0, 0);
    paramsLayout->addWidget(cropCombo_, 0, 1);
    paramsLayout->addWidget(new QLabel(QStringLiteral("季节"), paramsCard), 0, 2);
    paramsLayout->addWidget(seasonCombo_, 0, 3);
    paramsLayout->addWidget(new QLabel(QStringLiteral("天气"), paramsCard), 0, 4);
    paramsLayout->addWidget(weatherCombo_, 0, 5);

    paramsLayout->addWidget(new QLabel(QStringLiteral("温度"), paramsCard), 1, 0);
    paramsLayout->addWidget(tempSpin_, 1, 1);
    paramsLayout->addWidget(new QLabel(QStringLiteral("空气湿度"), paramsCard), 1, 2);
    paramsLayout->addWidget(humiditySpin_, 1, 3);
    paramsLayout->addWidget(new QLabel(QStringLiteral("光照"), paramsCard), 1, 4);
    paramsLayout->addWidget(lightSpin_, 1, 5);

    generateButton_ = new QPushButton(QStringLiteral("生成固定建议"), paramsCard);
    generateButton_->setStyleSheet(QStringLiteral(
        "QPushButton { min-height: 34px; border-radius: 10px; background: #1b5f7f; color: white; font-size: 13px; font-weight: 800; }"
        "QPushButton:hover { background: #174d67; }"));
    connect(generateButton_, &QPushButton::clicked, this, &PlantingAdviceWidget::onGenerateClicked);
    paramsLayout->addWidget(generateButton_, 2, 0, 1, 6);
    mainLayout->addWidget(paramsCard);

    QFrame *resultCard = new QFrame(this);
    resultCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #f7fbff; border: 1px solid #bfd7e5; border-radius: 12px; }"
        "QLabel { color: #17364a; font-size: 14px; font-weight: 900; }"));
    QVBoxLayout *resultLayout = new QVBoxLayout(resultCard);
    resultLayout->setContentsMargins(12, 10, 12, 10);
    resultLayout->setSpacing(8);
    resultLayout->addWidget(new QLabel(QStringLiteral("推荐动作"), resultCard));

    recommendationText_ = new QTextEdit(resultCard);
    recommendationText_->setReadOnly(true);
    recommendationText_->setMinimumHeight(170);
    recommendationText_->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #f4f9fd; border: 1px solid #c6dce8; border-radius: 8px; padding: 8px; color: #1f3340; font-size: 12px; }"));
    resultLayout->addWidget(recommendationText_);
    mainLayout->addWidget(resultCard, 1);

    QFrame *chatCard = new QFrame(this);
    chatCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #f7fbff; border: 1px dashed #7aa3ba; border-radius: 12px; }"
        "QLabel { color: #17364a; font-size: 13px; font-weight: 800; }"));
    QVBoxLayout *chatLayout = new QVBoxLayout(chatCard);
    chatLayout->setContentsMargins(12, 10, 12, 10);
    chatLayout->setSpacing(8);
    chatLayout->addWidget(new QLabel(QStringLiteral("GPT 聊天分析（预留）"), chatCard));

    gptOutput_ = new QTextEdit(chatCard);
    gptOutput_->setReadOnly(true);
    gptOutput_->setMinimumHeight(90);
    gptOutput_->setText(QStringLiteral("当前版本为本地固定建议。此区域预留给后续 GPT 接口接入（联网分析、历史数据关联、病害风险评估）。"));
    gptOutput_->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #f3faff; border: 1px solid #c7dae7; border-radius: 8px; padding: 8px; color: #244355; }"));
    chatLayout->addWidget(gptOutput_);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    gptInput_ = new QLineEdit(chatCard);
    gptInput_->setPlaceholderText(QStringLiteral("例如：草莓当前阶段如何控湿防灰霉？"));
    gptSendButton_ = new QPushButton(QStringLiteral("发送（预留）"), chatCard);
    gptSendButton_->setEnabled(true);
    connect(gptSendButton_, &QPushButton::clicked, this, &PlantingAdviceWidget::onReservedChatClicked);
    inputLayout->addWidget(gptInput_, 1);
    inputLayout->addWidget(gptSendButton_);
    chatLayout->addLayout(inputLayout);
    mainLayout->addWidget(chatCard);
}

QString PlantingAdviceWidget::detectSeason() const
{
    const int month = QDate::currentDate().month();
    if (month >= 3 && month <= 5) {
        return QStringLiteral("春季");
    }
    if (month >= 6 && month <= 8) {
        return QStringLiteral("夏季");
    }
    if (month >= 9 && month <= 11) {
        return QStringLiteral("秋季");
    }
    return QStringLiteral("冬季");
}

QString PlantingAdviceWidget::cropTemplate(const QString &crop) const
{
    if (crop == QStringLiteral("草莓")) {
        return QStringLiteral("草莓建议：重点控湿防灰霉，夜间加强通风，开花坐果期维持稳定水肥。");
    }
    if (crop == QStringLiteral("大葱")) {
        return QStringLiteral("大葱建议：保持中等偏干环境，避免连续高湿，追肥以氮钾平衡为主。");
    }
    if (crop == QStringLiteral("番茄")) {
        return QStringLiteral("番茄建议：优先防裂果和徒长，温差控制在 8~10°C，及时整枝打杈。");
    }
    if (crop == QStringLiteral("黄瓜")) {
        return QStringLiteral("黄瓜建议：生长期需水量高，保持稳定湿度并防止根系长期渍水。");
    }
    if (crop == QStringLiteral("辣椒")) {
        return QStringLiteral("辣椒建议：前期控旺促根，中后期稳温稳湿防落花落果。");
    }
    if (crop == QStringLiteral("生菜")) {
        return QStringLiteral("生菜建议：偏凉环境更利于品质，防止高温导致抽薹。");
    }
    return QStringLiteral("%1建议：按照温湿光条件做分时段通风、补光和水肥管理。").arg(crop);
}

QString PlantingAdviceWidget::buildRecommendation() const
{
    const QString crop = cropCombo_->currentText();
    const QString season = seasonCombo_->currentText();
    const QString weather = weatherCombo_->currentText();
    const double temp = tempSpin_->value();
    const double humidity = humiditySpin_->value();
    const double light = lightSpin_->value();

    QStringList lines;
    lines << QStringLiteral("作物：%1 | 季节：%2 | 天气：%3").arg(crop, season, weather);
    lines << cropTemplate(crop);

    if (temp < 12.0) {
        lines << QStringLiteral("温度偏低：建议白天适当闭棚保温，夜间优先内保温，避免温度突降。");
    } else if (temp > 32.0) {
        lines << QStringLiteral("温度偏高：建议外遮阳放下，联动风机和湿帘降温，错峰浇水。");
    } else {
        lines << QStringLiteral("温度适宜：维持当前放风节奏，按作物生长期微调阈值。");
    }

    if (humidity > 85.0) {
        lines << QStringLiteral("湿度偏高：增加通风换气，早晨先排湿后再升温，降低病害风险。");
    } else if (humidity < 45.0) {
        lines << QStringLiteral("湿度偏低：减少长时间大风量通风，必要时短时喷雾补湿。");
    } else {
        lines << QStringLiteral("湿度处于常用区间：可维持现有风机/卷膜节奏。");
    }

    if (light < 8000.0) {
        lines << QStringLiteral("光照偏弱：建议延后遮阳动作，必要时补光 1~2 小时。");
    } else if (light > 50000.0) {
        lines << QStringLiteral("光照较强：中午建议阶段性遮阳，防叶片灼伤和棚内过热。");
    } else {
        lines << QStringLiteral("光照适中：可优先保证通风，维持光合效率。");
    }

    if (season == QStringLiteral("夏季") || weather == QStringLiteral("大风")) {
        lines << QStringLiteral("季节/天气提醒：注意卷膜和遮阳动作节奏，避免同板多路同时大负载动作。");
    }
    if (weather == QStringLiteral("雨")) {
        lines << QStringLiteral("降雨提醒：优先防潮控湿，减少夜间灌溉，检查棚体密封。");
    }

    lines << QStringLiteral("说明：当前建议为固定规则模板，后续可切换为 GPT + 历史传感器数据联合分析。");
    return lines.join(QStringLiteral("\n"));
}

void PlantingAdviceWidget::onGenerateClicked()
{
    refreshSuggestion();
}

void PlantingAdviceWidget::refreshSuggestion()
{
    if (recommendationText_) {
        recommendationText_->setText(buildRecommendation());
    }
}

void PlantingAdviceWidget::onReservedChatClicked()
{
    const QString question = gptInput_->text().trimmed();
    if (question.isEmpty()) {
        gptOutput_->append(QStringLiteral("\n[预留接口] 请输入问题后再发送。"));
        return;
    }
    gptOutput_->append(QStringLiteral("\n用户：%1").arg(question));
    gptOutput_->append(QStringLiteral("系统：当前版本未接入 GPT 接口，这里仅为预留展示。"));
    gptInput_->clear();
}
