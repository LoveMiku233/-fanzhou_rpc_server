#ifndef CORE_JSON_EDITOR_WINDOW_H
#define CORE_JSON_EDITOR_WINDOW_H

#include <QJsonObject>
#include <QMainWindow>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QTableWidget;
class QPlainTextEdit;
class QCloseEvent;

class CoreJsonEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CoreJsonEditorWindow(QWidget *parent = nullptr);

private slots:
    void newConfig();
    void openConfig();
    bool saveConfig();
    bool saveConfigAs();
    void validateConfig();

    void addDeviceRow();
    void removeDeviceRow();
    void addGroupRow();
    void removeGroupRow();

private:
    void closeEvent(QCloseEvent *event) override;

    void setupUi();
    void createMenu();
    void setStatus(const QString &text);
    bool maybeSave();

    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path);
    void loadFromJson(const QJsonObject &root);
    QJsonObject buildJson(QStringList *errors = nullptr) const;
    void updatePreview();

    static QString intArrayToCsv(const QJsonArray &arr);
    static QJsonArray csvToIntArray(const QString &text, QString *error = nullptr);

    QString currentPath_;
    bool dirty_ = false;
    QJsonObject originalRoot_;

    QLineEdit *deviceIdEdit_ = nullptr;
    QSpinBox *rpcPortSpin_ = nullptr;

    QLineEdit *canIfnameEdit_ = nullptr;
    QSpinBox *canBitrateSpin_ = nullptr;
    QCheckBox *canFdCheck_ = nullptr;
    QCheckBox *tripleSamplingCheck_ = nullptr;
    QCheckBox *isFakeCanCheck_ = nullptr;
    QLineEdit *fakeCanEdit_ = nullptr;
    QSpinBox *fakeCanBaudSpin_ = nullptr;

    QTableWidget *devicesTable_ = nullptr;
    QTableWidget *groupsTable_ = nullptr;
    QPlainTextEdit *previewEdit_ = nullptr;
};

#endif  // CORE_JSON_EDITOR_WINDOW_H
