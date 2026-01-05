/**
 * @file glass_style.h
 * @brief 极简留白风格样式定义
 * 
 * 定义应用程序的极简留白(Minimalist Whitespace)视觉风格，
 * 采用浅色背景、清晰边框、大量留白的现代简约UI设计。
 * 适配1024x600低分辨率屏幕和Ubuntu系统。
 */

#ifndef GLASS_STYLE_H
#define GLASS_STYLE_H

#include <QString>

/**
 * @brief 极简留白风格命名空间
 * 
 * 包含所有极简风格的颜色、样式模板和辅助函数
 */
namespace GlassStyle {

    // ========== 主色调 ==========
    const QString PRIMARY_COLOR = "#2563eb";        // 主色 - 蓝色
    const QString PRIMARY_LIGHT = "#3b82f6";        // 浅主色
    const QString PRIMARY_DARK = "#1d4ed8";         // 深主色
    
    const QString ACCENT_COLOR = "#7c3aed";         // 强调色 - 紫色
    const QString ACCENT_LIGHT = "#8b5cf6";         // 浅强调色
    
    // ========== 背景色 - 极简浅色主题 ==========
    const QString BG_GRADIENT_START = "#f8fafc";    // 浅灰白
    const QString BG_GRADIENT_MID = "#f1f5f9";      // 柔和灰
    const QString BG_GRADIENT_END = "#e2e8f0";      // 浅灰
    
    // ========== 极简效果色 ==========
    const QString GLASS_BG = "#ffffff";             // 白色背景
    const QString GLASS_BG_HOVER = "#f8fafc";       // 悬停背景
    const QString GLASS_BG_ACTIVE = "#f1f5f9";      // 激活背景
    const QString GLASS_BORDER = "#e2e8f0";         // 边框颜色
    const QString GLASS_BORDER_HOVER = "#cbd5e1";   // 悬停边框
    
    // ========== 文字颜色 ==========
    const QString TEXT_PRIMARY = "#1e293b";         // 主要文字 - 深灰
    const QString TEXT_SECONDARY = "#64748b";       // 次要文字
    const QString TEXT_MUTED = "#94a3b8";           // 淡化文字
    
    // ========== 状态颜色 ==========
    const QString SUCCESS_COLOR = "#16a34a";        // 成功 - 绿色
    const QString WARNING_COLOR = "#ea580c";        // 警告 - 橙色
    const QString ERROR_COLOR = "#dc2626";          // 错误 - 红色
    const QString INFO_COLOR = "#2563eb";           // 信息 - 蓝色
    
    // ========== 阴影效果 ==========
    const QString SHADOW_LIGHT = "rgba(0, 0, 0, 0.05)";
    const QString SHADOW_MEDIUM = "rgba(0, 0, 0, 0.1)";
    const QString SHADOW_DARK = "rgba(0, 0, 0, 0.15)";
    
    // ========== 主窗口背景样式 - 极简浅色主题 ==========
    const QString MAIN_BACKGROUND = R"(
        QMainWindow {
            background: #f8fafc;
        }
        QWidget {
            font-family: "Segoe UI", "Microsoft YaHei", "SimHei", "WenQuanYi Micro Hei", sans-serif;
        }
    )";
    
    // ========== 顶栏样式 - 极简浅色 ==========
    const QString TOPBAR_GLASS = R"(
        QFrame#topBar {
            background: #ffffff;
            border: none;
            border-bottom: 1px solid #e2e8f0;
        }
        QFrame#topBar QLabel {
            color: #1e293b;
            font-size: 14px;
            padding: 4px 8px;
            background: transparent;
            border: none;
        }
        QFrame#topBar QLabel#labTitle {
            color: #1e293b;
            font-size: 18px;
            font-weight: bold;
            background: transparent;
        }
    )";
    
    // ========== 侧边栏样式 - 极简浅色 ==========
    const QString SIDEBAR_GLASS = R"(
        QFrame#menuBar {
            background: #ffffff;
            border: none;
            border-right: 1px solid #e2e8f0;
        }
    )";
    
    // ========== 导航按钮样式 - 极简风格 ==========
    const QString NAV_BUTTON_GLASS = R"(
        QPushButton {
            background: transparent;
            color: #64748b;
            border: none;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 15px;
            font-weight: 500;
            text-align: left;
        }
        QPushButton:hover {
            background: #f1f5f9;
            color: #1e293b;
        }
        QPushButton:pressed {
            background: #e2e8f0;
        }
        QPushButton:checked {
            background: #eff6ff;
            color: #2563eb;
            font-weight: bold;
        }
    )";
    
    // ========== 内容区样式 ==========
    const QString CONTENT_GLASS = R"(
        QStackedWidget#contentStackedWidget {
            background: transparent;
            border: none;
        }
        QStackedWidget#contentStackedWidget > QWidget {
            background: transparent;
        }
    )";
    
    // ========== 页面基础样式 - 极简浅色 ==========
    const QString PAGE_GLASS = R"(
        QWidget {
            background: transparent;
            color: #1e293b;
        }
        QLabel {
            color: #1e293b;
            background: transparent;
        }
    )";
    
    // ========== 面板/卡片样式 - 极简风格 ==========
    const QString GLASS_PANEL = R"(
        QFrame, QGroupBox {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 14px;
            color: #1e293b;
            padding-top: 16px;
            margin-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 16px;
            padding: 0 8px;
            background: transparent;
            color: #1e293b;
        }
    )";
    
    // ========== 按钮样式 - 极简风格 ==========
    const QString GLASS_BUTTON = R"(
        QPushButton {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #f8fafc;
            border: 1px solid #cbd5e1;
        }
        QPushButton:pressed {
            background: #f1f5f9;
        }
        QPushButton:disabled {
            background: #f8fafc;
            color: #94a3b8;
            border: 1px solid #e2e8f0;
        }
    )";
    
    // ========== 主要操作按钮样式 ==========
    const QString GLASS_BUTTON_PRIMARY = R"(
        QPushButton {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #1d4ed8;
        }
        QPushButton:pressed {
            background: #1e40af;
        }
        QPushButton:disabled {
            background: #93c5fd;
            color: #ffffff;
        }
    )";
    
    // ========== 危险操作按钮样式 ==========
    const QString GLASS_BUTTON_DANGER = R"(
        QPushButton {
            background: #dc2626;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #b91c1c;
        }
        QPushButton:pressed {
            background: #991b1b;
        }
    )";
    
    // ========== 输入框样式 - 极简风格 ==========
    const QString GLASS_INPUT = R"(
        QLineEdit, QSpinBox, QDoubleSpinBox {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 14px;
            selection-background-color: #bfdbfe;
        }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border: 1px solid #cbd5e1;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2563eb;
            background: #ffffff;
        }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            background: #f8fafc;
            border: none;
            width: 20px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background: #e2e8f0;
        }
    )";
    
    // ========== 下拉框样式 - 极简风格 ==========
    const QString GLASS_COMBOBOX = R"(
        QComboBox {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 14px;
            min-width: 100px;
        }
        QComboBox:hover {
            border: 1px solid #cbd5e1;
        }
        QComboBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox::drop-down {
            width: 30px;
            border: none;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #64748b;
            margin-right: 10px;
        }
        QComboBox QAbstractItemView {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            selection-background-color: #eff6ff;
            selection-color: #2563eb;
            outline: 0;
            padding: 4px;
        }
        QComboBox QAbstractItemView::item {
            padding: 8px 12px;
            border-radius: 4px;
            margin: 2px;
        }
        QComboBox QAbstractItemView::item:hover {
            background: #f1f5f9;
        }
    )";
    
    // ========== 复选框样式 - 极简风格 ==========
    const QString GLASS_CHECKBOX = R"(
        QCheckBox {
            color: #1e293b;
            font-size: 14px;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 2px solid #cbd5e1;
            background: #ffffff;
        }
        QCheckBox::indicator:hover {
            border: 2px solid #94a3b8;
        }
        QCheckBox::indicator:checked {
            background: #2563eb;
            border: 2px solid #2563eb;
        }
    )";
    
    // ========== 列表样式 - 极简风格 ==========
    const QString GLASS_LIST = R"(
        QListWidget, QListView {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 6px;
            outline: none;
        }
        QListWidget::item, QListView::item {
            background: transparent;
            color: #1e293b;
            border-radius: 6px;
            padding: 10px 14px;
            margin: 2px;
        }
        QListWidget::item:hover, QListView::item:hover {
            background: #f1f5f9;
        }
        QListWidget::item:selected, QListView::item:selected {
            background: #eff6ff;
            color: #2563eb;
        }
    )";
    
    // ========== 标签页样式 - 极简风格 ==========
    const QString GLASS_TAB = R"(
        QTabWidget::pane {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 10px;
        }
        QTabBar::tab {
            background: transparent;
            color: #64748b;
            border: none;
            padding: 10px 20px;
            margin-right: 4px;
            border-radius: 6px 6px 0 0;
            font-size: 14px;
        }
        QTabBar::tab:hover {
            background: #f1f5f9;
            color: #1e293b;
        }
        QTabBar::tab:selected {
            background: #ffffff;
            color: #2563eb;
            font-weight: bold;
            border-bottom: 2px solid #2563eb;
        }
    )";
    
    // ========== 文本编辑器样式 - 极简风格 ==========
    const QString GLASS_TEXTEDIT = R"(
        QTextEdit, QPlainTextEdit {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 12px;
            font-size: 13px;
            font-family: "Consolas", "Monaco", "Microsoft YaHei", monospace;
            selection-background-color: #bfdbfe;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #2563eb;
        }
    )";
    
    // ========== 滚动条样式 - 极简风格 ==========
    const QString GLASS_SCROLLBAR = R"(
        QScrollBar:vertical {
            background: #f8fafc;
            width: 8px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #cbd5e1;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #94a3b8;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #f8fafc;
            height: 8px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background: #cbd5e1;
            border-radius: 4px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #94a3b8;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )";
    
    // ========== 滚动区域样式 ==========
    const QString GLASS_SCROLLAREA = R"(
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }
    )";
    
    // ========== 对话框样式 - 极简风格 ==========
    const QString GLASS_DIALOG = R"(
        QDialog {
            background: #ffffff;
        }
        QDialog QLabel {
            color: #1e293b;
            background: transparent;
        }
        QDialog QFrame#card {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
        }
    )";
    
    // ========== 自定义输入对话框样式 ==========
    const QString GLASS_INPUT_DIALOG = R"(
        QDialog {
            background: #ffffff;
        }
        QDialog QLabel {
            color: #1e293b;
            background: transparent;
            font-size: 14px;
        }
        QDialog QLineEdit {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px 14px;
            font-size: 14px;
            selection-background-color: #bfdbfe;
        }
        QDialog QLineEdit:focus {
            border: 1px solid #2563eb;
        }
        QDialog QSpinBox {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px 14px;
            font-size: 14px;
        }
        QDialog QSpinBox:focus {
            border: 1px solid #2563eb;
        }
        QDialog QSpinBox::up-button, QDialog QSpinBox::down-button {
            background: #f8fafc;
            border: none;
            width: 24px;
        }
        QDialog QSpinBox::up-button:hover, QDialog QSpinBox::down-button:hover {
            background: #e2e8f0;
        }
        QDialog QPushButton {
            background: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 500;
            min-width: 80px;
        }
        QDialog QPushButton:hover {
            background: #f8fafc;
            border: 1px solid #cbd5e1;
        }
        QDialog QPushButton:pressed {
            background: #f1f5f9;
        }
        QDialog QPushButton#primary, QDialog QPushButton:default {
            background: #2563eb;
            color: #ffffff;
            border: none;
        }
        QDialog QPushButton#primary:hover, QDialog QPushButton:default:hover {
            background: #1d4ed8;
        }
        QDialog QPushButton#danger {
            background: #dc2626;
            color: #ffffff;
            border: none;
        }
        QDialog QPushButton#danger:hover {
            background: #b91c1c;
        }
    )";

    // ========== 完整页面样式组合 ==========
    /**
     * @brief 获取完整的页面样式表
     * @return 组合后的样式表字符串
     */
    inline QString getFullPageStyle() {
        QString style;
        style.reserve(8000);  // Pre-allocate for efficiency
        style.append(PAGE_GLASS);
        style.append(GLASS_PANEL);
        style.append(GLASS_BUTTON);
        style.append(GLASS_INPUT);
        style.append(GLASS_COMBOBOX);
        style.append(GLASS_CHECKBOX);
        style.append(GLASS_LIST);
        style.append(GLASS_TAB);
        style.append(GLASS_TEXTEDIT);
        style.append(GLASS_SCROLLBAR);
        style.append(GLASS_SCROLLAREA);
        return style;
    }

}  // namespace GlassStyle

#endif // GLASS_STYLE_H
