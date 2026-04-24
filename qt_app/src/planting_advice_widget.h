/**
 * @file planting_advice_widget.h
 * @brief 种植建议页面（含 GPT 聊天分析预留）
 */

#ifndef PLANTING_ADVICE_WIDGET_H
#define PLANTING_ADVICE_WIDGET_H

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QTextEdit;
class QLineEdit;
class QPushButton;

class PlantingAdviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlantingAdviceWidget(QWidget *parent = nullptr);

public slots:
    void refreshSuggestion();

private slots:
    void onGenerateClicked();
    void onReservedChatClicked();

private:
    void setupUi();
    QString detectSeason() const;
    QString buildRecommendation() const;
    QString cropTemplate(const QString &crop) const;

    QComboBox *cropCombo_;
    QComboBox *seasonCombo_;
    QComboBox *weatherCombo_;
    QDoubleSpinBox *tempSpin_;
    QDoubleSpinBox *humiditySpin_;
    QDoubleSpinBox *lightSpin_;
    QTextEdit *recommendationText_;
    QTextEdit *gptOutput_;
    QLineEdit *gptInput_;
    QPushButton *generateButton_;
    QPushButton *gptSendButton_;
};

#endif // PLANTING_ADVICE_WIDGET_H
