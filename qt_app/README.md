# 泛舟RPC客户端

基于Qt5.12的GUI客户端，用于连接和控制泛舟RPC服务器。  
界面设计参考 `index3.html`，深色科技主题，目标分辨率 1024×600。

## 功能特性

- 🖥️ **驾驶舱**：气象站数据、棚内环境监测、紧急停止、趋势图
- ⚙️ **设备控制**：分组Tab + 设备卡片（DC电机/AC设备）
- 📋 **场景管理**：自动/手动场景卡片，过滤、激活、执行
- ⚠️ **报警看板**：分级报警列表（紧急/警告/提示），确认处理
- 📊 **传感器**：传感器数据卡片网格，类型颜色编码
- 🔧 **系统设置**：网络配置、MQTT配置、关于系统

## 架构设计

界面与逻辑分离：

```
qt_app/
├── qt_app.pro                  # Qt项目文件
├── README.md                   # 本文档
├── resources/
│   ├── resources.qrc           # 资源文件
│   └── styles/
│       └── dark_theme.qss      # 深色主题样式表
└── src/
    ├── main.cpp                # 程序入口
    ├── mainwindow.h/cpp        # 主窗口（侧边栏 + 顶栏 + 页面栈）
    ├── rpc_client.h/cpp        # RPC通信客户端
    ├── screen_manager.h/cpp    # 屏幕管理器（GPIO背光控制）
    ├── style_constants.h       # UI常量（颜色、尺寸、页面索引）
    ├── models/
    │   └── data_models.h       # 数据模型（设备/场景/报警/传感器）
    └── views/                  # 页面视图（纯UI构建）
        ├── dashboard_page.h/cpp        # 驾驶舱页面
        ├── device_control_page.h/cpp   # 设备控制页面
        ├── scene_page.h/cpp            # 场景管理页面
        ├── alarm_page.h/cpp            # 报警看板页面
        ├── sensor_page.h/cpp           # 传感器页面
        └── settings_page.h/cpp         # 系统设置页面
```

## 系统要求

- Ubuntu 18.04 或更高版本
- Qt 5.12 或更高版本
- GCC 编译器

## 编译

### 安装依赖

```bash
# Ubuntu 20.04+
sudo apt-get install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools

# Ubuntu 18.04
sudo apt-get install qt5-default qtbase5-dev
```

### 编译步骤

```bash
cd qt_app
mkdir build && cd build
qmake ../qt_app.pro
make -j$(nproc)

# 运行
../bin/fanzhou_rpc_client
```

### 使用Qt Creator

1. 打开Qt Creator
2. 文件 → 打开文件或项目 → 选择 `qt_app.pro`
3. 配置项目（选择Qt 5.12或更高版本）
4. 点击运行按钮

## 界面说明

### 驾驶舱
- 室外气象站数据（温度/湿度/风速/光照/降雨量）
- 棚内环境监测仪表（空气温度/湿度/CO₂/光照）
- 紧急停止按钮
- 24小时环境趋势区域

### 设备控制
- 顶部分组Tab标签（可滚动）
- 添加分组/添加设备按钮
- DC设备卡片：名称、规格、状态、开度滑块
- AC设备卡片：名称、规格、状态、运行时间、启停按钮

### 场景管理
- 全部/自动/手动 筛选Tab
- 新建场景按钮
- 场景卡片：名称、类型、触发条件、执行次数

### 报警看板
- 全部/紧急/警告/提示 筛选Tab
- 一键确认按钮
- 报警项：级别、标题、设备、描述、时间、确认状态
- 底部统计：未确认紧急/警告数

### 传感器
- 4列传感器卡片网格
- 卡片：位置、类型、名称、数值、更新时间
- 类型颜色编码（温度=橙、湿度=蓝、光照=黄、CO₂=紫、土壤=绿）

### 系统设置
- 网络配置：IP获取方式、IP/掩码/网关/DNS
- MQTT配置：服务器/端口/认证/主题
- 关于系统：版本信息、版权声明

## 与服务器通信

客户端使用TCP连接到RPC服务器，使用JSON-RPC 2.0协议进行通信。

### 支持的RPC方法

- `rpc.ping` - 测试连接
- `sys.info` - 获取系统信息
- `relay.nodes` - 获取设备列表
- `relay.control` - 控制继电器
- `relay.status` - 查询通道状态
- `relay.statusAll` - 查询所有通道状态
- `group.list` - 获取分组列表
- `group.create` - 创建分组
- `group.delete` - 删除分组
- `group.addDevice` - 添加设备到分组
- `group.removeDevice` - 从分组移除设备
- `group.control` - 分组控制
- `config.save` - 保存配置

## 许可证

MIT License
