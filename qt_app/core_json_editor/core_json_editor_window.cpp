#include "core_json_editor_window.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

static QString channelsToBindingCsv(const QJsonArray &channels);
static QJsonArray bindingCsvToChannels(const QString &text, QString *error);

namespace {
struct OptionItem {
    int value;
    const char *label;
};

const OptionItem kDeviceTypeOptions[] = {
    {1, "RelayGd427"},
    {11, "ActuatorGeneric"},
    {21, "SensorModbusGeneric"},
    {22, "SensorModbusTemp"},
    {23, "SensorModbusHumidity"},
    {24, "SensorModbusSoil"},
    {25, "SensorModbusCO2"},
    {26, "SensorModbusLight"},
    {51, "SensorCanGeneric"},
    {52, "SensorCanTemp"},
    {53, "SensorCanHumidity"},
    {81, "SensorUartGeneric"},
    {82, "SensorUartGps"},
    {83, "SensorUartPm25"},
};

const OptionItem kCommTypeOptions[] = {
    {1, "Serial"},
    {2, "CAN"},
    {3, "Modbus"},
    {4, "UART"},
    {5, "TCP Client"},
};

const char *const kBusOptions[] = {
    "can0",
    "tcp-client",
    "/dev/ttyS1",
    "/dev/ttyS2",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
};

QTableWidgetItem *makeItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

QComboBox *makeIntCombo(const OptionItem *items, int count, int value, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    for (int i = 0; i < count; ++i) {
        combo->addItem(QStringLiteral("%1 (%2)").arg(items[i].label).arg(items[i].value), items[i].value);
    }
    int idx = combo->findData(value);
    if (idx < 0) {
        idx = 0;
    }
    combo->setCurrentIndex(idx);
    return combo;
}

QComboBox *makeBoolCombo(bool value, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->addItem(QStringLiteral("true"), true);
    combo->addItem(QStringLiteral("false"), false);
    combo->setCurrentIndex(value ? 0 : 1);
    return combo;
}

QComboBox *makeEditableTextCombo(const char *const *items, int count, const QString &value, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    for (int i = 0; i < count; ++i) {
        combo->addItem(QString::fromLatin1(items[i]));
    }
    int idx = combo->findText(value);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else {
        combo->setEditText(value);
    }
    return combo;
}

QString tableCellText(const QTableWidget *table, int row, int col)
{
    if (auto *combo = qobject_cast<QComboBox *>(table->cellWidget(row, col))) {
        const QVariant data = combo->currentData();
        if (data.isValid()) {
            return data.toString();
        }
        return combo->currentText();
    }
    const QTableWidgetItem *item = table->item(row, col);
    return item ? item->text() : QString();
}

QString tableCellDisplayText(const QTableWidget *table, int row, int col)
{
    if (auto *combo = qobject_cast<QComboBox *>(table->cellWidget(row, col))) {
        return combo->currentText();
    }
    const QTableWidgetItem *item = table->item(row, col);
    return item ? item->text() : QString();
}

}

CoreJsonEditorWindow::CoreJsonEditorWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    createMenu();
    newConfig();
    resize(1400, 860);
}

void CoreJsonEditorWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    auto *leftPanel = new QWidget(splitter);
    auto *rightPanel = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    auto *rightLayout = new QVBoxLayout(rightPanel);

    auto *mainBox = new QGroupBox(QStringLiteral("Main"), leftPanel);
    auto *mainForm = new QFormLayout(mainBox);
    deviceIdEdit_ = new QLineEdit(mainBox);
    rpcPortSpin_ = new QSpinBox(mainBox);
    rpcPortSpin_->setRange(1, 65535);
    mainForm->addRow(QStringLiteral("deviceId"), deviceIdEdit_);
    mainForm->addRow(QStringLiteral("rpcPort"), rpcPortSpin_);
    leftLayout->addWidget(mainBox);

    auto *canBox = new QGroupBox(QStringLiteral("CAN"), leftPanel);
    auto *canForm = new QFormLayout(canBox);
    canIfnameEdit_ = new QLineEdit(canBox);
    canBitrateSpin_ = new QSpinBox(canBox);
    canBitrateSpin_->setRange(1, 10000000);
    canFdCheck_ = new QCheckBox(QStringLiteral("canFd"), canBox);
    tripleSamplingCheck_ = new QCheckBox(QStringLiteral("tripleSampling"), canBox);
    isFakeCanCheck_ = new QCheckBox(QStringLiteral("is_fake"), canBox);
    fakeCanEdit_ = new QLineEdit(canBox);
    fakeCanBaudSpin_ = new QSpinBox(canBox);
    fakeCanBaudSpin_->setRange(1, 10000000);
    canForm->addRow(QStringLiteral("ifname"), canIfnameEdit_);
    canForm->addRow(QStringLiteral("bitrate"), canBitrateSpin_);
    canForm->addRow(canFdCheck_);
    canForm->addRow(tripleSamplingCheck_);
    canForm->addRow(isFakeCanCheck_);
    canForm->addRow(QStringLiteral("fake_can"), fakeCanEdit_);
    canForm->addRow(QStringLiteral("fake_can_baudrate"), fakeCanBaudSpin_);
    leftLayout->addWidget(canBox);

    auto *devHeader = new QHBoxLayout();
    devHeader->addWidget(new QLabel(QStringLiteral("Devices"), leftPanel));
    auto *addDevBtn = new QPushButton(QStringLiteral("+"), leftPanel);
    auto *delDevBtn = new QPushButton(QStringLiteral("-"), leftPanel);
    addDevBtn->setFixedWidth(28);
    delDevBtn->setFixedWidth(28);
    devHeader->addStretch();
    devHeader->addWidget(addDevBtn);
    devHeader->addWidget(delDevBtn);
    leftLayout->addLayout(devHeader);

    devicesTable_ = new QTableWidget(leftPanel);
    devicesTable_->setColumnCount(6);
    devicesTable_->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("nodeId") << QStringLiteral("name")
                      << QStringLiteral("type") << QStringLiteral("commType")
                      << QStringLiteral("bus") << QStringLiteral("params(JSON)"));
    devicesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    devicesTable_->horizontalHeader()->setStretchLastSection(true);
    devicesTable_->verticalHeader()->setVisible(false);
    leftLayout->addWidget(devicesTable_, 1);

    auto *grpHeader = new QHBoxLayout();
    grpHeader->addWidget(new QLabel(QStringLiteral("Groups"), rightPanel));
    auto *addGrpBtn = new QPushButton(QStringLiteral("+"), rightPanel);
    auto *delGrpBtn = new QPushButton(QStringLiteral("-"), rightPanel);
    addGrpBtn->setFixedWidth(28);
    delGrpBtn->setFixedWidth(28);
    grpHeader->addStretch();
    grpHeader->addWidget(addGrpBtn);
    grpHeader->addWidget(delGrpBtn);
    rightLayout->addLayout(grpHeader);

    groupsTable_ = new QTableWidget(rightPanel);
    groupsTable_->setColumnCount(5);
    groupsTable_->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("groupId") << QStringLiteral("name")
                      << QStringLiteral("enabled") << QStringLiteral("devices(csv)")
                      << QStringLiteral("bindings(csv node:ch)"));
    groupsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    groupsTable_->horizontalHeader()->setStretchLastSection(true);
    groupsTable_->verticalHeader()->setVisible(false);
    rightLayout->addWidget(groupsTable_, 1);

    rightLayout->addWidget(new QLabel(QStringLiteral("JSON Preview (read-only)"), rightPanel));
    previewEdit_ = new QPlainTextEdit(rightPanel);
    previewEdit_->setReadOnly(true);
    rightLayout->addWidget(previewEdit_, 1);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setSizes(QList<int>() << 700 << 700);
    mainLayout->addWidget(splitter);
    setCentralWidget(central);

    auto markDirty = [this]() {
        dirty_ = true;
        updatePreview();
    };
    connect(deviceIdEdit_, &QLineEdit::textChanged, this, markDirty);
    connect(rpcPortSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(canIfnameEdit_, &QLineEdit::textChanged, this, markDirty);
    connect(canBitrateSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(canFdCheck_, &QCheckBox::toggled, this, markDirty);
    connect(tripleSamplingCheck_, &QCheckBox::toggled, this, markDirty);
    connect(isFakeCanCheck_, &QCheckBox::toggled, this, markDirty);
    connect(fakeCanEdit_, &QLineEdit::textChanged, this, markDirty);
    connect(fakeCanBaudSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(devicesTable_, &QTableWidget::itemChanged, this, markDirty);
    connect(groupsTable_, &QTableWidget::itemChanged, this, markDirty);

    connect(addDevBtn, &QPushButton::clicked, this, &CoreJsonEditorWindow::addDeviceRow);
    connect(delDevBtn, &QPushButton::clicked, this, &CoreJsonEditorWindow::removeDeviceRow);
    connect(addGrpBtn, &QPushButton::clicked, this, &CoreJsonEditorWindow::addGroupRow);
    connect(delGrpBtn, &QPushButton::clicked, this, &CoreJsonEditorWindow::removeGroupRow);
}

void CoreJsonEditorWindow::createMenu()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    auto *newAct = fileMenu->addAction(QStringLiteral("New"));
    auto *openAct = fileMenu->addAction(QStringLiteral("Open..."));
    auto *saveAct = fileMenu->addAction(QStringLiteral("Save"));
    auto *saveAsAct = fileMenu->addAction(QStringLiteral("Save As..."));
    fileMenu->addSeparator();
    auto *exitAct = fileMenu->addAction(QStringLiteral("Exit"));

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    auto *validateAct = toolsMenu->addAction(QStringLiteral("Validate"));

    connect(newAct, &QAction::triggered, this, &CoreJsonEditorWindow::newConfig);
    connect(openAct, &QAction::triggered, this, &CoreJsonEditorWindow::openConfig);
    connect(saveAct, &QAction::triggered, this, &CoreJsonEditorWindow::saveConfig);
    connect(saveAsAct, &QAction::triggered, this, &CoreJsonEditorWindow::saveConfigAs);
    connect(validateAct, &QAction::triggered, this, &CoreJsonEditorWindow::validateConfig);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
}

void CoreJsonEditorWindow::setStatus(const QString &text)
{
    statusBar()->showMessage(text, 3000);
    setWindowTitle(QStringLiteral("Core JSON Editor - %1%2")
                       .arg(currentPath_.isEmpty() ? QStringLiteral("(new)") : currentPath_)
                       .arg(dirty_ ? QStringLiteral(" *") : QString()));
}

bool CoreJsonEditorWindow::maybeSave()
{
    if (!dirty_) {
        return true;
    }
    const auto ret = QMessageBox::question(
        this, QStringLiteral("Unsaved changes"),
        QStringLiteral("Configuration has unsaved changes. Save now?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel) {
        return false;
    }
    if (ret == QMessageBox::Save) {
        return saveConfig();
    }
    return true;
}

void CoreJsonEditorWindow::newConfig()
{
    if (!maybeSave()) {
        return;
    }
    QJsonObject root;
    root.insert(QStringLiteral("main"), QJsonObject{
                                         {QStringLiteral("deviceId"), QStringLiteral("NULL")},
                                         {QStringLiteral("rpcPort"), 12345}});
    root.insert(QStringLiteral("can"), QJsonObject{
                                        {QStringLiteral("ifname"), QStringLiteral("can0")},
                                        {QStringLiteral("bitrate"), 125000},
                                        {QStringLiteral("tripleSampling"), true},
                                        {QStringLiteral("canFd"), false},
                                        {QStringLiteral("is_fake"), false},
                                        {QStringLiteral("fake_can"), QStringLiteral("/dev/ttyUSB1")},
                                        {QStringLiteral("fake_can_baudrate"), 115200}});
    root.insert(QStringLiteral("devices"), QJsonArray{});
    root.insert(QStringLiteral("groups"), QJsonArray{});
    loadFromJson(root);
    currentPath_.clear();
    dirty_ = false;
    setStatus(QStringLiteral("Created new config"));
}

void CoreJsonEditorWindow::openConfig()
{
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open core.json"),
                                                      currentPath_.isEmpty() ? QStringLiteral(".") : currentPath_,
                                                      QStringLiteral("JSON (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    loadFromFile(path);
}

bool CoreJsonEditorWindow::saveConfig()
{
    if (currentPath_.isEmpty()) {
        return saveConfigAs();
    }
    return saveToFile(currentPath_);
}

bool CoreJsonEditorWindow::saveConfigAs()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save core.json"),
                                                      currentPath_.isEmpty() ? QStringLiteral("core.json") : currentPath_,
                                                      QStringLiteral("JSON (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        return false;
    }
    return saveToFile(path);
}

void CoreJsonEditorWindow::validateConfig()
{
    QStringList errors;
    buildJson(&errors);
    if (errors.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Validate"), QStringLiteral("Validation passed."));
    } else {
        QMessageBox::warning(this, QStringLiteral("Validate"),
                             QStringLiteral("Validation failed:\n- %1").arg(errors.join(QStringLiteral("\n- "))));
    }
    updatePreview();
}

void CoreJsonEditorWindow::addDeviceRow()
{
    const int row = devicesTable_->rowCount();
    devicesTable_->insertRow(row);
    devicesTable_->setItem(row, 0, makeItem(QString::number(row + 1)));
    devicesTable_->setItem(row, 1, makeItem(QStringLiteral("device%1").arg(row + 1, 2, 10, QLatin1Char('0'))));
    auto *typeCombo = makeIntCombo(kDeviceTypeOptions,
                                   static_cast<int>(sizeof(kDeviceTypeOptions) / sizeof(kDeviceTypeOptions[0])),
                                   1, devicesTable_);
    auto *commCombo = makeIntCombo(kCommTypeOptions,
                                   static_cast<int>(sizeof(kCommTypeOptions) / sizeof(kCommTypeOptions[0])),
                                   5, devicesTable_);
    auto *busCombo = makeEditableTextCombo(
        kBusOptions, static_cast<int>(sizeof(kBusOptions) / sizeof(kBusOptions[0])),
        QStringLiteral("tcp-client"), devicesTable_);
    devicesTable_->setCellWidget(row, 2, typeCombo);
    devicesTable_->setCellWidget(row, 3, commCombo);
    devicesTable_->setCellWidget(row, 4, busCombo);
    devicesTable_->setItem(row, 5, makeItem(QStringLiteral("{\"channels\":4,\"enabled\":true}")));
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        dirty_ = true;
        updatePreview();
    });
    connect(commCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        dirty_ = true;
        updatePreview();
    });
    connect(busCombo, &QComboBox::editTextChanged, this, [this]() {
        dirty_ = true;
        updatePreview();
    });
}

void CoreJsonEditorWindow::removeDeviceRow()
{
    const int row = devicesTable_->currentRow();
    if (row >= 0) {
        devicesTable_->removeRow(row);
        dirty_ = true;
        updatePreview();
    }
}

void CoreJsonEditorWindow::addGroupRow()
{
    const int row = groupsTable_->rowCount();
    groupsTable_->insertRow(row);
    groupsTable_->setItem(row, 0, makeItem(QString::number(row + 1)));
    groupsTable_->setItem(row, 1, makeItem(QStringLiteral("group%1").arg(row + 1)));
    auto *enabledCombo = makeBoolCombo(true, groupsTable_);
    groupsTable_->setCellWidget(row, 2, enabledCombo);
    groupsTable_->setItem(row, 3, makeItem(QStringLiteral("")));
    groupsTable_->setItem(row, 4, makeItem(QStringLiteral("")));
    connect(enabledCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        dirty_ = true;
        updatePreview();
    });
}

void CoreJsonEditorWindow::removeGroupRow()
{
    const int row = groupsTable_->currentRow();
    if (row >= 0) {
        groupsTable_->removeRow(row);
        dirty_ = true;
        updatePreview();
    }
}

bool CoreJsonEditorWindow::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("Open failed"),
                              QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, QStringLiteral("Parse failed"),
                              QStringLiteral("Invalid JSON: %1").arg(err.errorString()));
        return false;
    }
    loadFromJson(doc.object());
    currentPath_ = path;
    dirty_ = false;
    setStatus(QStringLiteral("Loaded: %1").arg(path));
    return true;
}

bool CoreJsonEditorWindow::saveToFile(const QString &path)
{
    QStringList errors;
    const QJsonObject root = buildJson(&errors);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Save blocked"),
                             QStringLiteral("Cannot save due to validation errors:\n- %1")
                                 .arg(errors.join(QStringLiteral("\n- "))));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("Save failed"),
                              QStringLiteral("Cannot write file: %1").arg(path));
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    currentPath_ = path;
    originalRoot_ = root;
    dirty_ = false;
    setStatus(QStringLiteral("Saved: %1").arg(path));
    updatePreview();
    return true;
}

void CoreJsonEditorWindow::loadFromJson(const QJsonObject &root)
{
    originalRoot_ = root;

    const QJsonObject main = root.value(QStringLiteral("main")).toObject();
    const QJsonObject can = root.value(QStringLiteral("can")).toObject();

    deviceIdEdit_->setText(main.value(QStringLiteral("deviceId")).toString(
        main.value(QStringLiteral("DeviceId")).toString(QStringLiteral("NULL"))));
    rpcPortSpin_->setValue(main.value(QStringLiteral("rpcPort")).toInt(12345));

    canIfnameEdit_->setText(can.value(QStringLiteral("ifname")).toString(
        can.value(QStringLiteral("interface")).toString(QStringLiteral("can0"))));
    canBitrateSpin_->setValue(can.value(QStringLiteral("bitrate")).toInt(125000));
    canFdCheck_->setChecked(can.value(QStringLiteral("canFd")).toBool(false));
    tripleSamplingCheck_->setChecked(can.value(QStringLiteral("tripleSampling")).toBool(true));
    isFakeCanCheck_->setChecked(can.value(QStringLiteral("is_fake")).toBool(false));
    fakeCanEdit_->setText(can.value(QStringLiteral("fake_can")).toString(QStringLiteral("/dev/ttyUSB1")));
    fakeCanBaudSpin_->setValue(can.value(QStringLiteral("fake_can_baudrate")).toInt(115200));

    devicesTable_->setRowCount(0);
    const QJsonArray devices = root.value(QStringLiteral("devices")).toArray();
    for (const QJsonValue &v : devices) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject dev = v.toObject();
        const int row = devicesTable_->rowCount();
        devicesTable_->insertRow(row);
        devicesTable_->setItem(row, 0, makeItem(QString::number(dev.value(QStringLiteral("nodeId")).toInt())));
        devicesTable_->setItem(row, 1, makeItem(dev.value(QStringLiteral("name")).toString()));
        auto *typeCombo = makeIntCombo(kDeviceTypeOptions,
                                       static_cast<int>(sizeof(kDeviceTypeOptions) / sizeof(kDeviceTypeOptions[0])),
                                       dev.value(QStringLiteral("type")).toInt(), devicesTable_);
        auto *commCombo = makeIntCombo(kCommTypeOptions,
                                       static_cast<int>(sizeof(kCommTypeOptions) / sizeof(kCommTypeOptions[0])),
                                       dev.value(QStringLiteral("commType")).toInt(), devicesTable_);
        auto *busCombo = makeEditableTextCombo(
            kBusOptions, static_cast<int>(sizeof(kBusOptions) / sizeof(kBusOptions[0])),
            dev.value(QStringLiteral("bus")).toString(), devicesTable_);
        devicesTable_->setCellWidget(row, 2, typeCombo);
        devicesTable_->setCellWidget(row, 3, commCombo);
        devicesTable_->setCellWidget(row, 4, busCombo);
        const QJsonObject params = dev.value(QStringLiteral("params")).toObject();
        devicesTable_->setItem(row, 5, makeItem(QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            dirty_ = true;
            updatePreview();
        });
        connect(commCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            dirty_ = true;
            updatePreview();
        });
        connect(busCombo, &QComboBox::editTextChanged, this, [this]() {
            dirty_ = true;
            updatePreview();
        });
    }

    groupsTable_->setRowCount(0);
    const QJsonArray groups = root.value(QStringLiteral("groups")).toArray();
    for (const QJsonValue &v : groups) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject g = v.toObject();
        const int row = groupsTable_->rowCount();
        groupsTable_->insertRow(row);
        groupsTable_->setItem(row, 0, makeItem(QString::number(g.value(QStringLiteral("groupId")).toInt())));
        groupsTable_->setItem(row, 1, makeItem(g.value(QStringLiteral("name")).toString()));
        auto *enabledCombo = makeBoolCombo(g.value(QStringLiteral("enabled")).toBool(true), groupsTable_);
        groupsTable_->setCellWidget(row, 2, enabledCombo);
        groupsTable_->setItem(row, 3, makeItem(intArrayToCsv(g.value(QStringLiteral("devices")).toArray())));
        groupsTable_->setItem(row, 4, makeItem(channelsToBindingCsv(g.value(QStringLiteral("channels")).toArray())));
        connect(enabledCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            dirty_ = true;
            updatePreview();
        });
    }

    dirty_ = false;
    updatePreview();
    setStatus(QStringLiteral("Ready"));
}

void CoreJsonEditorWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

QJsonObject CoreJsonEditorWindow::buildJson(QStringList *errors) const
{
    QJsonObject root = originalRoot_;

    QJsonObject main = root.value(QStringLiteral("main")).toObject();
    main.insert(QStringLiteral("deviceId"), deviceIdEdit_->text().trimmed());
    main.insert(QStringLiteral("rpcPort"), rpcPortSpin_->value());
    root.insert(QStringLiteral("main"), main);

    QJsonObject can = root.value(QStringLiteral("can")).toObject();
    can.insert(QStringLiteral("ifname"), canIfnameEdit_->text().trimmed());
    can.insert(QStringLiteral("bitrate"), canBitrateSpin_->value());
    can.insert(QStringLiteral("canFd"), canFdCheck_->isChecked());
    can.insert(QStringLiteral("tripleSampling"), tripleSamplingCheck_->isChecked());
    can.insert(QStringLiteral("is_fake"), isFakeCanCheck_->isChecked());
    can.insert(QStringLiteral("fake_can"), fakeCanEdit_->text().trimmed());
    can.insert(QStringLiteral("fake_can_baudrate"), fakeCanBaudSpin_->value());
    root.insert(QStringLiteral("can"), can);

    QJsonArray devices;
    QSet<int> deviceNodeSet;
    for (int row = 0; row < devicesTable_->rowCount(); ++row) {
        const QString nodeText = devicesTable_->item(row, 0) ? devicesTable_->item(row, 0)->text().trimmed() : QString();
        const QString nameText = devicesTable_->item(row, 1) ? devicesTable_->item(row, 1)->text().trimmed() : QString();
        const QString typeText = tableCellText(devicesTable_, row, 2).trimmed();
        const QString commText = tableCellText(devicesTable_, row, 3).trimmed();
        const QString busText = tableCellText(devicesTable_, row, 4).trimmed();
        const QString paramsText = devicesTable_->item(row, 5) ? devicesTable_->item(row, 5)->text().trimmed() : QString();

        bool okNode = false;
        const int nodeId = nodeText.toInt(&okNode);
        bool okType = false;
        const int type = typeText.toInt(&okType);
        bool okComm = false;
        const int commType = commText.toInt(&okComm);
        if (!okNode || nodeId < 1 || nodeId > 255) {
            if (errors) errors->append(QStringLiteral("devices[%1].nodeId invalid (1..255)").arg(row));
            continue;
        }
        if (deviceNodeSet.contains(nodeId)) {
            if (errors) errors->append(QStringLiteral("devices[%1].nodeId duplicated: %2").arg(row).arg(nodeId));
            continue;
        }
        if (!okType) {
            if (errors) errors->append(QStringLiteral("devices[%1].type invalid").arg(row));
            continue;
        }
        if (!okComm) {
            if (errors) errors->append(QStringLiteral("devices[%1].commType invalid").arg(row));
            continue;
        }
        if (nameText.isEmpty()) {
            if (errors) errors->append(QStringLiteral("devices[%1].name empty").arg(row));
            continue;
        }

        QJsonObject paramsObj;
        if (!paramsText.isEmpty()) {
            QJsonParseError pErr;
            const QJsonDocument pDoc = QJsonDocument::fromJson(paramsText.toUtf8(), &pErr);
            if (pErr.error != QJsonParseError::NoError || !pDoc.isObject()) {
                if (errors) errors->append(QStringLiteral("devices[%1].params invalid JSON object").arg(row));
                continue;
            }
            paramsObj = pDoc.object();
        }

        QJsonObject dev;
        dev.insert(QStringLiteral("nodeId"), nodeId);
        dev.insert(QStringLiteral("name"), nameText);
        dev.insert(QStringLiteral("type"), type);
        dev.insert(QStringLiteral("commType"), commType);
        dev.insert(QStringLiteral("bus"), busText);
        dev.insert(QStringLiteral("params"), paramsObj);
        devices.append(dev);
        deviceNodeSet.insert(nodeId);
    }
    root.insert(QStringLiteral("devices"), devices);

    QJsonArray groups;
    QSet<int> groupIdSet;
    for (int row = 0; row < groupsTable_->rowCount(); ++row) {
        const QString idText = groupsTable_->item(row, 0) ? groupsTable_->item(row, 0)->text().trimmed() : QString();
        const QString nameText = groupsTable_->item(row, 1) ? groupsTable_->item(row, 1)->text().trimmed() : QString();
        const QString enabledText = tableCellDisplayText(groupsTable_, row, 2).trimmed().toLower();
        const QString devicesText = groupsTable_->item(row, 3) ? groupsTable_->item(row, 3)->text().trimmed() : QString();
        const QString channelsText = groupsTable_->item(row, 4) ? groupsTable_->item(row, 4)->text().trimmed() : QString();

        bool okId = false;
        const int groupId = idText.toInt(&okId);
        if (!okId || groupId <= 0) {
            if (errors) errors->append(QStringLiteral("groups[%1].groupId invalid (>0)").arg(row));
            continue;
        }
        if (groupIdSet.contains(groupId)) {
            if (errors) errors->append(QStringLiteral("groups[%1].groupId duplicated: %2").arg(row).arg(groupId));
            continue;
        }
        if (nameText.isEmpty()) {
            if (errors) errors->append(QStringLiteral("groups[%1].name empty").arg(row));
            continue;
        }

        QString csvErr;
        const QJsonArray devArr = csvToIntArray(devicesText, &csvErr);
        if (!csvErr.isEmpty()) {
            if (errors) {
                errors->append(
                    QStringLiteral("groups[%1].devices: %2").arg(QString::number(row), csvErr));
            }
            continue;
        }
        for (const QJsonValue &v : devArr) {
            const int node = v.toInt(-1);
            if (!deviceNodeSet.contains(node) && errors) {
                errors->append(QStringLiteral("groups[%1].devices contains unknown nodeId: %2")
                                   .arg(row)
                                   .arg(node));
            }
        }

        QString bindErr;
        QJsonArray channelsArr = bindingCsvToChannels(channelsText, &bindErr);
        if (!bindErr.isEmpty()) {
            if (errors) {
                errors->append(QStringLiteral("groups[%1].bindings: %2").arg(row).arg(bindErr));
            }
            continue;
        }
        for (const QJsonValue &v : channelsArr) {
            const int key = v.toInt(-1);
            const int node = key / 256;
            if (!deviceNodeSet.contains(node) && errors) {
                errors->append(QStringLiteral("groups[%1].bindings contains unknown nodeId: %2")
                                   .arg(row)
                                   .arg(node));
            }
        }

        const bool enabled = (enabledText == QStringLiteral("true") || enabledText == QStringLiteral("1"));
        QJsonObject g;
        g.insert(QStringLiteral("groupId"), groupId);
        g.insert(QStringLiteral("name"), nameText);
        g.insert(QStringLiteral("enabled"), enabled);
        g.insert(QStringLiteral("devices"), devArr);
        g.insert(QStringLiteral("channels"), channelsArr);
        groups.append(g);
        groupIdSet.insert(groupId);
    }
    root.insert(QStringLiteral("groups"), groups);
    return root;
}

void CoreJsonEditorWindow::updatePreview()
{
    QStringList errors;
    const QJsonObject obj = buildJson(&errors);
    QString preview = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    if (!errors.isEmpty()) {
        preview.prepend(QStringLiteral("// Validation errors:\n// - %1\n\n")
                            .arg(errors.join(QStringLiteral("\n// - "))));
    }
    previewEdit_->setPlainText(preview);
    setStatus(QStringLiteral("Ready"));
}

QString CoreJsonEditorWindow::intArrayToCsv(const QJsonArray &arr)
{
    QStringList out;
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        out.append(QString::number(v.toInt()));
    }
    return out.join(QStringLiteral(","));
}

static QString channelsToBindingCsv(const QJsonArray &channels)
{
    QStringList out;
    for (const QJsonValue &v : channels) {
        const int key = v.toInt(-1);
        if (key < 0) {
            continue;
        }
        const int node = key / 256;
        const int ch = key % 256;
        if (node >= 1 && node <= 255 && ch >= 0 && ch <= 3) {
            out.append(QStringLiteral("%1:%2").arg(node).arg(ch));
        }
    }
    return out.join(QStringLiteral(","));
}

static QJsonArray bindingCsvToChannels(const QString &text, QString *error = nullptr)
{
    QJsonArray arr;
    if (text.trimmed().isEmpty()) {
        return arr;
    }
    const QStringList parts = text.split(',', QString::SkipEmptyParts);
    for (const QString &raw : parts) {
        const QString token = raw.trimmed();
        const int idx = token.indexOf(':');
        if (idx <= 0 || idx >= token.size() - 1) {
            if (error) {
                *error = QStringLiteral("invalid binding '%1', expected node:channel").arg(token);
            }
            return QJsonArray();
        }
        bool okNode = false;
        bool okCh = false;
        const int node = token.left(idx).trimmed().toInt(&okNode);
        const int ch = token.mid(idx + 1).trimmed().toInt(&okCh);
        if (!okNode || node < 1 || node > 255) {
            if (error) {
                *error = QStringLiteral("invalid node in binding '%1' (1..255)").arg(token);
            }
            return QJsonArray();
        }
        if (!okCh || ch < 0 || ch > 3) {
            if (error) {
                *error = QStringLiteral("invalid channel in binding '%1' (0..3)").arg(token);
            }
            return QJsonArray();
        }
        arr.append(node * 256 + ch);
    }
    return arr;
}

QJsonArray CoreJsonEditorWindow::csvToIntArray(const QString &text, QString *error)
{
    QJsonArray arr;
    if (text.trimmed().isEmpty()) {
        return arr;
    }

    const QStringList parts = text.split(',', QString::SkipEmptyParts);
    for (const QString &raw : parts) {
        bool ok = false;
        const int val = raw.trimmed().toInt(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("contains non-integer value: %1").arg(raw.trimmed());
            }
            return QJsonArray();
        }
        arr.append(val);
    }
    return arr;
}
