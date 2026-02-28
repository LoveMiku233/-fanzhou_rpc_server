/**
 * @file style_constants.h
 * @brief UI样式常量定义
 *
 * 定义全局UI布局和颜色常量，匹配深色主题设计。
 * 目标分辨率：1024x600
 */

#ifndef STYLE_CONSTANTS_H
#define STYLE_CONSTANTS_H

#include <QString>

namespace Style {

// ── 屏幕与布局 ──────────────────────────────
const int kScreenWidth  = 1024;
const int kScreenHeight = 600;
const int kSidebarWidth = 80;
const int kHeaderHeight = 44;
const int kPageMargin   = 12;
const int kCardSpacing  = 10;
const int kCardRadius   = 10;

// ── 字体大小 ─────────────────────────────────
const int kFontTiny   = 10;
const int kFontSmall  = 11;
const int kFontNormal = 12;
const int kFontMedium = 14;
const int kFontLarge  = 16;
const int kFontXLarge = 20;
const int kFontTitle  = 24;

// ── 按钮尺寸 ─────────────────────────────────
const int kBtnHeightSmall  = 28;
const int kBtnHeightNormal = 34;
const int kBtnHeightLarge  = 40;

// ── 深色主题色板 ─────────────────────────────
// 基础背景色
const char kColorBgDark[]       = "#0f172a";   // 最深背景
const char kColorBgPanel[]      = "#1e293b";   // 面板背景
const char kColorBgCard[]       = "#283548";   // 卡片背景
const char kColorBgInput[]      = "#1e293b";   // 输入框背景
const char kColorBgSidebar[]    = "#1a2332";   // 侧边栏背景

// 边框色
const char kColorBorder[]       = "#334155";   // 默认边框
const char kColorBorderLight[]  = "#475569";   // 浅边框
const char kColorBorderFocus[]  = "#0ea5e9";   // 聚焦边框

// 文本色
const char kColorTextPrimary[]  = "#e2e8f0";   // 主文本
const char kColorTextSecondary[]= "#94a3b8";   // 次要文本
const char kColorTextMuted[]    = "#64748b";   // 弱化文本
const char kColorTextWhite[]    = "#ffffff";   // 白色文本

// 主题强调色
const char kColorAccentBlue[]   = "#0ea5e9";   // 蓝色强调
const char kColorAccentCyan[]   = "#38bdf8";   // 青色强调
const char kColorGradientStart[]= "#0ea5e9";   // 渐变起始
const char kColorGradientEnd[]  = "#2563eb";   // 渐变结束

// 状态色
const char kColorSuccess[]      = "#10b981";   // 成功/运行
const char kColorWarning[]      = "#f59e0b";   // 警告/手动
const char kColorDanger[]       = "#ef4444";   // 危险/故障
const char kColorInfo[]         = "#3b82f6";   // 信息

// 数据色
const char kColorOrange[]       = "#fb923c";   // 温度
const char kColorBlue[]         = "#60a5fa";   // 湿度
const char kColorPurple[]       = "#a78bfa";   // CO2
const char kColorYellow[]       = "#facc15";   // 光照
const char kColorEmerald[]      = "#34d399";   // 土壤/正常

// ── 页面索引 ─────────────────────────────────
enum PageIndex {
    PageDashboard = 0,
    PageDeviceControl,
    PageScenes,
    PageAlarms,
    PageSensors,
    PageSettings,
    PageCount
};

} // namespace Style

#endif // STYLE_CONSTANTS_H
