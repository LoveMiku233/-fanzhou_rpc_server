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
// 主窗口尺寸（完美适配1024x600屏幕）
constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 600;

// 侧边栏宽度 (传统模式)
constexpr int SIDEBAR_WIDTH = 85;

// 旋钮菜单区域宽度 (新模式)
constexpr int KNOB_MENU_WIDTH = 100;

// 内容区域可用宽度 (旋钮菜单模式)
constexpr int CONTENT_WIDTH_KNOB = WINDOW_WIDTH - KNOB_MENU_WIDTH;

// 内容区域可用宽度 (传统模式)
constexpr int CONTENT_WIDTH = WINDOW_WIDTH - SIDEBAR_WIDTH;

// ==================== 边距和间距 ====================
// 页面边距
constexpr int PAGE_MARGIN = 8;
constexpr int PAGE_SPACING = 8;

// 卡片边距
constexpr int CARD_MARGIN = 8;
constexpr int CARD_SPACING = 6;

// 对话框边距
constexpr int DIALOG_MARGIN = 16;
constexpr int DIALOG_SPACING = 10;

// ==================== 控件高度（统一, 触屏友好） ====================
// 小按钮高度（图标按钮等）
constexpr int BTN_HEIGHT_SMALL = 34;

// 标准按钮高度
constexpr int BTN_HEIGHT = 40;

// 大按钮高度（主要操作）
constexpr int BTN_HEIGHT_LARGE = 50;

// 紧急/急停按钮高度（需要更突出的显示）
constexpr int BTN_HEIGHT_EMERGENCY = 56;

// 侧边栏菜单按钮高度（包含图标和文字两行）
constexpr int MENU_BTN_HEIGHT = 52;

// 输入框高度
constexpr int INPUT_HEIGHT = 38;

// ==================== 控件最小宽度 ====================
// 小按钮最小宽度
constexpr int BTN_MIN_WIDTH_SMALL = 55;

// 标准按钮最小宽度
constexpr int BTN_MIN_WIDTH = 80;

// 大按钮最小宽度
constexpr int BTN_MIN_WIDTH_LARGE = 100;

// 输入框最小宽度
constexpr int INPUT_MIN_WIDTH = 90;

// ==================== 对话框尺寸（适配1024x600） ====================
// 小对话框（如确认框）
constexpr int DIALOG_WIDTH_SMALL = 320;
constexpr int DIALOG_HEIGHT_SMALL = 220;

// 标准对话框
constexpr int DIALOG_WIDTH = 520;
constexpr int DIALOG_HEIGHT = 420;

// 大对话框（如策略编辑）
constexpr int DIALOG_WIDTH_LARGE = 600;
constexpr int DIALOG_HEIGHT_LARGE = 500;

// ==================== 卡片尺寸 ====================
// 卡片最小高度
constexpr int CARD_MIN_HEIGHT = 96;

// 卡片最大宽度（两列布局时）
constexpr int CARD_MAX_WIDTH = (CONTENT_WIDTH - PAGE_MARGIN * 2 - PAGE_SPACING) / 2;

// ==================== 字体大小（触屏友好） ====================
// 页面标题
constexpr int FONT_SIZE_TITLE = 18;

// 卡片标题
constexpr int FONT_SIZE_CARD_TITLE = 14;

// 正文
constexpr int FONT_SIZE_BODY = 13;

// 小字（提示、状态）
constexpr int FONT_SIZE_SMALL = 11;

// 大数值显示（如传感器数值）
constexpr int FONT_SIZE_VALUE = 32;

// ==================== 表格尺寸 ====================
// 表格最小高度
constexpr int TABLE_MIN_HEIGHT = 90;

// 表格最大高度
constexpr int TABLE_MAX_HEIGHT = 160;

// 表格行高
constexpr int TABLE_ROW_HEIGHT = 32;

// ==================== 滚动区域 ====================
// 滚动条宽度
constexpr int SCROLLBAR_WIDTH = 14;

// ==================== 圆角 ====================
// 按钮圆角
constexpr int BORDER_RADIUS_BTN = 10;

// 卡片圆角
constexpr int BORDER_RADIUS_CARD = 14;

// 对话框圆角
constexpr int BORDER_RADIUS_DIALOG = 14;

// 输入框圆角
constexpr int BORDER_RADIUS_INPUT = 10;

// ==================== 颜色主题 - 现代大棚农业主题 ====================
namespace Colors {
    // 主色调 - 农业绿色系（代表植物、生长）
    constexpr char Primary[] = "#27ae60";
    constexpr char PrimaryDark[] = "#1e8449";
    constexpr char PrimaryLight[] = "#58d68d";

    // 辅助色 - 土壤棕色系（代表土地）
    constexpr char Secondary[] = "#8b5a2b";
    constexpr char SecondaryDark[] = "#654321";
    constexpr char SecondaryLight[] = "#a0522d";

    // 强调色 - 阳光橙色（代表阳光、温暖）
    constexpr char Accent[] = "#f39c12";
    constexpr char AccentDark[] = "#d68910";
    constexpr char AccentLight[] = "#f7dc6f";

    // 成功/在线 - 深绿色
    constexpr char Success[] = "#27ae60";
    constexpr char SuccessDark[] = "#1e8449";
    constexpr char SuccessLight[] = "#58d68d";

    // 警告 - 橙色系
    constexpr char Warning[] = "#f39c12";
    constexpr char WarningDark[] = "#d68910";
    constexpr char WarningLight[] = "#f7dc6f";

    // 危险/错误 - 红色系
    constexpr char Danger[] = "#e74c3c";
    constexpr char DangerDark[] = "#c0392b";
    constexpr char DangerLight[] = "#ec7063";

    // 中性色 - 自然灰色系
    constexpr char TextPrimary[] = "#2d3436";
    constexpr char TextSecondary[] = "#636e72";
    constexpr char TextMuted[] = "#b2bec3";

    // 背景色 - 柔和的自然色调
    constexpr char Background[] = "#f5f6fa";
    constexpr char Surface[] = "#ffffff";
    constexpr char Border[] = "#dfe6e9";

    // 侧边栏 - 深林绿色系
    constexpr char SidebarStart[] = "#1e3d2f";
    constexpr char SidebarEnd[] = "#0f261a";

    // ========== 新增: 现代半透明主题颜色 ==========
    // 毛玻璃效果背景色
    constexpr char GlassBgLight[] = "rgba(255, 255, 255, 0.75)";
    constexpr char GlassBgDark[] = "rgba(30, 40, 50, 0.85)";

    // 旋钮菜单主题色
    constexpr char KnobPrimary[] = "#f39c12";
    constexpr char KnobPrimaryGlow[] = "rgba(243, 156, 18, 0.4)";
    constexpr char KnobBgStart[] = "#2c3e50";
    constexpr char KnobBgEnd[] = "#1a252f";

    // 现代渐变背景
    constexpr char GradientStart[] = "#1a1a2e";
    constexpr char GradientMid[] = "#16213e";
    constexpr char GradientEnd[] = "#0f3460";

    // 卡片半透明
    constexpr char CardBgTransparent[] = "rgba(255, 255, 255, 0.85)";
    constexpr char CardBorderTransparent[] = "rgba(255, 255, 255, 0.3)";

    // 文字半透明
    constexpr char TextWhiteTransparent[] = "rgba(255, 255, 255, 0.9)";
    constexpr char TextWhiteMuted[] = "rgba(255, 255, 255, 0.6)";
}

} // namespace UIConstants

#endif // STYLE_CONSTANTS_H
