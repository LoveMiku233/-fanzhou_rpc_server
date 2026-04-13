# 泛舟RPC客户端

基于Qt5.12的GUI客户端，用于连接和控制泛舟RPC服务器。

## 功能特性

- 🔌 **设备管理**：查看和管理CAN继电器设备
- 📂 **分组管理**：创建、删除、管理设备分组
- 🎛️ **继电器控制**：直接控制单个继电器通道
- 📝 **通信日志**：实时查看RPC通信日志
- 💾 **配置管理**：保存和加载服务器配置

## 系统要求

- Ubuntu 18.04 或更高版本
- Qt 5.12 或更高版本
- GCC 编译器

## 编译

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install qt5-default qtbase5-dev

# 或者使用Qt官方安装器安装Qt 5.12
```

### 编译步骤

```bash
# 进入项目目录
cd qt_app

# 创建构建目录
mkdir build && cd build

# 运行qmake
qmake ../qt_app.pro

# 编译
make -j$(nproc)

# 运行
./bin/fanzhou_rpc_client
```

### 使用Qt Creator

1. 打开Qt Creator
2. 文件 -> 打开文件或项目 -> 选择 `qt_app.pro`
3. 配置项目（选择Qt 5.12或更高版本）
4. 点击运行按钮

## 使用说明

### 连接服务器

1. 在"服务器地址"输入框中输入RPC服务器的IP地址
2. 确认端口号（默认12345）
3. 点击"连接"按钮

### 设备管理

- 点击"刷新设备列表"获取所有已配置的设备
- 点击"查询全部状态"刷新所有设备的通道状态
- 点击设备行查看详细状态

### 分组管理

- **创建分组**：输入分组ID和名称，点击"创建"
- **添加设备**：输入分组ID和设备节点ID，点击"添加"
- **分组控制**：选择分组、通道和动作，点击"执行"

### 继电器控制

- 选择节点ID、通道和动作
- 点击"执行控制"发送控制命令
- 使用快捷控制按钮可以快速操作单个通道

### 保存配置

通过RPC调用创建的修改默认只保存在内存中，重启后会丢失。
点击工具栏的"保存配置"按钮将配置持久化到服务器配置文件。

## 目录结构

```
qt_app/
├── qt_app.pro              # Qt项目文件
├── README.md               # 本文档
├── src/
│   ├── main.cpp            # 程序入口
│   ├── mainwindow.h/cpp    # 主窗口
│   ├── rpc_client.h/cpp    # RPC客户端
│   ├── device_widget.h/cpp # 设备管理页面
│   ├── group_widget.h/cpp  # 分组管理页面
│   ├── relay_control_widget.h/cpp  # 继电器控制页面
│   └── connection_dialog.h/cpp     # 连接对话框
└── resources/
    ├── resources.qrc       # 资源文件
    └── styles/
        └── style.qss       # 样式表
```

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

更多RPC方法请参考服务器端的API文档。

## 许可证

MIT License
