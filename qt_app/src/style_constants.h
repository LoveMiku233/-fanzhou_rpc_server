/**
 * @file style_constants.h
 * @brief UI常量定义 - 1024x600低分辨率触摸屏优化
 *
 * 定义统一的UI尺寸常量，确保所有控件大小一致
 * 适配7寸1024x600触摸屏
 */

#ifndef STYLE_CONSTANTS_H
#define STYLE_CONSTANTS_H

namespace UIConstants {

// ==================== 布局常量 ====================
// 主窗口最大尺寸（适配1024x600屏幕，减去40px任务栏高度）
constexpr int MAX_WINDOW_WIDTH = 1024;
constexpr int MAX_WINDOW_HEIGHT = 560;

// 侧边栏宽度
constexpr int SIDEBAR_WIDTH = 80;

// 内容区域可用宽度
constexpr int CONTENT_WIDTH = MAX_WINDOW_WIDTH - SIDEBAR_WIDTH;

// ==================== 边距和间距 ====================
// 页面边距
constexpr int PAGE_MARGIN = 10;
constexpr int PAGE_SPACING = 10;

// 卡片边距
constexpr int CARD_MARGIN = 8;
constexpr int CARD_SPACING = 6;

// 对话框边距
constexpr int DIALOG_MARGIN = 12;
constexpr int DIALOG_SPACING = 8;

// ==================== 控件高度（统一） ====================
// 小按钮高度（图标按钮等）
constexpr int BTN_HEIGHT_SMALL = 32;

// 标准按钮高度
constexpr int BTN_HEIGHT = 36;

// 大按钮高度（主要操作）
constexpr int BTN_HEIGHT_LARGE = 40;

// 输入框高度
constexpr int INPUT_HEIGHT = 36;

// ==================== 控件最小宽度 ====================
// 小按钮最小宽度
constexpr int BTN_MIN_WIDTH_SMALL = 40;

// 标准按钮最小宽度
constexpr int BTN_MIN_WIDTH = 60;

// 输入框最小宽度
constexpr int INPUT_MIN_WIDTH = 80;

// ==================== 对话框尺寸 ====================
// 小对话框（如确认框）
constexpr int DIALOG_WIDTH_SMALL = 300;
constexpr int DIALOG_HEIGHT_SMALL = 200;

// 标准对话框
constexpr int DIALOG_WIDTH = 480;
constexpr int DIALOG_HEIGHT = 400;

// 大对话框（如策略编辑）
constexpr int DIALOG_WIDTH_LARGE = 560;
constexpr int DIALOG_HEIGHT_LARGE = 480;

// ==================== 卡片尺寸 ====================
// 卡片最小高度
constexpr int CARD_MIN_HEIGHT = 100;

// 卡片最大宽度（两列布局时）
constexpr int CARD_MAX_WIDTH = (CONTENT_WIDTH - PAGE_MARGIN * 2 - PAGE_SPACING) / 2;

// ==================== 字体大小 ====================
// 页面标题
constexpr int FONT_SIZE_TITLE = 18;

// 卡片标题
constexpr int FONT_SIZE_CARD_TITLE = 14;

// 正文
constexpr int FONT_SIZE_BODY = 12;

// 小字（提示、状态）
constexpr int FONT_SIZE_SMALL = 11;

// 大数值显示（如传感器数值）
constexpr int FONT_SIZE_VALUE = 28;

// ==================== 表格尺寸 ====================
// 表格最小高度
constexpr int TABLE_MIN_HEIGHT = 80;

// 表格最大高度
constexpr int TABLE_MAX_HEIGHT = 140;

// 表格行高
constexpr int TABLE_ROW_HEIGHT = 28;

// ==================== 滚动区域 ====================
// 滚动条宽度
constexpr int SCROLLBAR_WIDTH = 10;

// ==================== 圆角 ====================
// 按钮圆角
constexpr int BORDER_RADIUS_BTN = 6;

// 卡片圆角
constexpr int BORDER_RADIUS_CARD = 10;

// 对话框圆角
constexpr int BORDER_RADIUS_DIALOG = 12;

// 输入框圆角
constexpr int BORDER_RADIUS_INPUT = 6;

} // namespace UIConstants

#endif // STYLE_CONSTANTS_H
