# 泛舟RPC服务器 开发指南

## 文档信息
- **版本**: 1.0.0
- **更新日期**: 2026-01
- **适用对象**: 开发人员、代码接手者

---

## 1. 开发环境搭建

### 1.1 依赖项

**必需**:
- Qt 5.12+ 或 Qt 6.x
- C++17 编译器（GCC 7+ / Clang 6+）
- Linux 操作系统（用于SocketCAN）

**可选**:
- candump/cansend（CAN总线调试工具）
- WebSocket代理（用于Web调试界面）

### 1.2 编译步骤

```bash
# 克隆仓库
git clone https://github.com/username/fanzhou_rpc_server.git
cd fanzhou_rpc_server

# 编译服务器
mkdir build && cd build
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)

# 编译Qt应用
cd ../qt_app
mkdir build && cd build
qmake ../qt_app.pro
make -j$(nproc)
```

### 1.3 开发工具推荐

- **IDE**: Qt Creator / VS Code + CMake Tools
- **调试**: Qt Creator Debugger / GDB
- **CAN调试**: candump, cansend, socketcan-utils
- **API测试**: Web调试界面 / Postman (需WebSocket代理)

---

## 2. 代码规范

### 2.1 命名规范

```cpp
// 类名: 大驼峰
class CoreContext;
class JsonRpcDispatcher;

// 方法名: 小驼峰
void refreshDeviceList();
bool createGroup(int groupId);

// 成员变量: 小驼峰 + 下划线后缀
RpcClient *rpcClient_;
QLabel *statusLabel_;

// 常量: k前缀 + 大驼峰
const QString kKeyOk = QStringLiteral("ok");
constexpr int kMaxChannelId = 3;

// 枚举: 大驼峰
enum class DeviceTypeId {
    RelayGd427 = 1,
    SensorModbusTemp = 22
};
```

### 2.2 Qt字符串

优先使用 `QStringLiteral` 避免运行时构造：

```cpp
// 推荐
return QStringLiteral("missing parameter");

// 避免
return QString("missing parameter");
```

### 2.3 注释规范

```cpp
/**
 * @brief 创建设备分组
 * @param groupId 分组ID
 * @param name 分组名称
 * @param error 错误信息输出（可选）
 * @return 成功返回true
 */
bool createGroup(int groupId, const QString &name, QString *error = nullptr);
```

---

## 3. 核心开发任务

### 3.1 添加新RPC方法

**步骤**:

1. **在 `rpc_registry.cpp` 中注册方法**:

```cpp
void RpcRegistry::registerMyMethods()
{
    dispatcher_->registerMethod(QStringLiteral("myModule.myMethod"),
        [this](const QJsonObject &params) {
            // 1. 解析参数
            qint32 myParam = 0;
            if (!rpc::RpcHelpers::getI32(params, "myParam", myParam))
                return rpc::RpcHelpers::err(
                    rpc::RpcError::MissingParameter, 
                    QStringLiteral("missing myParam"));
            
            // 2. 调用业务逻辑
            QString error;
            if (!context_->myBusinessMethod(myParam, &error)) {
                return rpc::RpcHelpers::err(
                    rpc::RpcError::BadParameterValue, error);
            }
            
            // 3. 返回结果
            return QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("result"), myParam * 2}
            };
        });
}
```

2. **在 `registerAll()` 中调用注册**:

```cpp
void RpcRegistry::registerAll()
{
    // ... 现有注册
    registerMyMethods();  // 添加
}
```

3. **更新API文档**: `docs/API_REFERENCE.zh.md`

### 3.2 添加新设备驱动

**步骤**:

1. **定义设备类型** (`device_types.h`):

```cpp
enum class DeviceTypeId {
    // ... 现有类型
    MyNewDevice = 40,
};
```

2. **创建设备驱动类**:

```cpp
// src/device/my_device.h
class MyDevice : public DeviceAdapter
{
    Q_OBJECT
public:
    explicit MyDevice(quint8 nodeId, CanComm *can, QObject *parent = nullptr);
    
    bool init() override;
    void poll() override;
    QString name() const override;
    
    // 设备特有方法
    bool myControl(int value);
    int myStatus() const;

signals:
    void statusUpdated(int status);

private:
    quint8 nodeId_;
    CanComm *can_;
    int lastStatus_;
};
```

3. **在 `CoreContext` 中集成**:

```cpp
// 添加设备实例管理
QHash<quint8, MyDevice*> myDevices_;

// 在初始化时创建设备实例
MyDevice *dev = new MyDevice(nodeId, canBus, this);
myDevices_.insert(nodeId, dev);
```

### 3.3 添加Qt界面组件

**步骤**:

1. **创建Widget类**:

```cpp
// my_widget.h
class MyWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MyWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void logMessage(const QString &message, const QString &level);

private slots:
    void onRefreshClicked();
    void onActionClicked();

private:
    void setupUi();
    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    // ... 其他UI组件
};
```

2. **实现UI布局**:

```cpp
void MyWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 标题
    QLabel *titleLabel = new QLabel(QStringLiteral("我的页面"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold;"));
    mainLayout->addWidget(titleLabel);
    
    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setMinimumHeight(40);
    connect(refreshBtn, &QPushButton::clicked, 
            this, &MyWidget::onRefreshClicked);
    toolbarLayout->addWidget(refreshBtn);
    mainLayout->addLayout(toolbarLayout);
    
    // ... 其他组件
}
```

3. **在 `MainWindow` 中添加页面**:

```cpp
// mainwindow.cpp
myWidget_ = new MyWidget(rpcClient_, this);
stackedWidget_->addWidget(myWidget_);

// 添加导航按钮
QPushButton *myBtn = createNavButton(QStringLiteral("我的功能"));
connect(myBtn, &QPushButton::clicked, [this]() {
    stackedWidget_->setCurrentWidget(myWidget_);
});
```

---

## 4. 分组与通道绑定机制

### 4.1 设计原则

**新设计（2026-01）**:
- 分组通过绑定特定设备的特定通道来工作
- 策略执行时**仅控制分组中已绑定的通道**
- 策略创建时**不再指定控制通道**

**数据结构**:
```cpp
// 分组通道映射: groupId -> [channelKey, ...]
// channelKey = nodeId * 256 + channel
QHash<int, QList<int>> groupChannels_;
```

### 4.2 相关API

```cpp
// 添加通道到分组
bool addChannelToGroup(int groupId, quint8 nodeId, int channel, QString *error);

// 从分组移除通道
bool removeChannelFromGroup(int groupId, quint8 nodeId, int channel, QString *error);

// 获取分组通道列表
QList<int> getGroupChannels(int groupId);

// 执行分组控制（遍历分组中的所有通道）
void executeGroupControl(int groupId, const QString &action);
```

### 4.3 策略执行流程

```cpp
void executeStrategy(const AutoStrategyConfig &strategy)
{
    // 获取分组绑定的通道列表
    QList<int> channelKeys = getGroupChannels(strategy.groupId);
    
    // 对每个通道执行控制
    for (int key : channelKeys) {
        int nodeId = key / 256;
        int channel = key % 256;
        
        enqueueControl(nodeId, channel, parseAction(strategy.action), 
                      QStringLiteral("strategy:%1").arg(strategy.name));
    }
}
```

---

## 5. 常见开发任务

### 5.1 调试RPC调用

**使用Web调试界面**:
1. 启动服务器
2. 启动WebSocket代理: `websockify 12346 127.0.0.1:12345`
3. 打开 `test_web/index.html`
4. 连接并发送RPC请求

**使用命令行**:
```bash
# 发送RPC请求
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345

# 控制继电器
echo '{"jsonrpc":"2.0","id":2,"method":"relay.control","params":{"node":1,"ch":0,"action":"fwd"}}' | nc localhost 12345
```

### 5.2 调试CAN通信

```bash
# 监控CAN总线
candump can0

# 发送测试帧
cansend can0 101#0100000000000000

# 检查CAN接口状态
ip -details -statistics link show can0
```

### 5.3 配置文件调试

```bash
# 查看当前配置
echo '{"jsonrpc":"2.0","id":1,"method":"config.get","params":{}}' | nc localhost 12345 | jq

# 保存配置
echo '{"jsonrpc":"2.0","id":2,"method":"config.save","params":{}}' | nc localhost 12345
```

---

## 6. 测试指南

### 6.1 单元测试

```cpp
// tests/test_rpc_helpers.cpp
#include <QtTest>
#include "rpc/rpc_helpers.h"

class TestRpcHelpers : public QObject
{
    Q_OBJECT
    
private slots:
    void testGetI32()
    {
        QJsonObject params;
        params["value"] = 42;
        
        qint32 result = 0;
        QVERIFY(rpc::RpcHelpers::getI32(params, "value", result));
        QCOMPARE(result, 42);
    }
    
    void testGetI32FromString()
    {
        QJsonObject params;
        params["value"] = "123";
        
        qint32 result = 0;
        QVERIFY(rpc::RpcHelpers::getI32(params, "value", result));
        QCOMPARE(result, 123);
    }
};

QTEST_MAIN(TestRpcHelpers)
```

### 6.2 集成测试

```bash
#!/bin/bash
# tests/integration_test.sh

# 启动服务器
./fanzhou_rpc_server -c test_config.json &
SERVER_PID=$!
sleep 2

# 测试ping
RESULT=$(echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping"}' | nc localhost 12345)
if echo "$RESULT" | grep -q '"ok":true'; then
    echo "PASS: ping test"
else
    echo "FAIL: ping test"
    kill $SERVER_PID
    exit 1
fi

# 清理
kill $SERVER_PID
echo "All tests passed!"
```

---

## 7. 发布流程

### 7.1 版本号规范

使用语义版本号: `MAJOR.MINOR.PATCH`
- MAJOR: 不兼容的API变更
- MINOR: 向后兼容的功能新增
- PATCH: 向后兼容的问题修复

### 7.2 发布检查清单

- [ ] 更新版本号
- [ ] 更新CHANGELOG
- [ ] 运行所有测试
- [ ] 更新API文档
- [ ] 编译Release版本
- [ ] 创建Git标签
- [ ] 部署到测试环境验证

---

## 8. 故障排查

### 8.1 编译错误

**问题**: `undefined reference to vtable`

**解决**: 确保运行了 `qmake` 生成MOC文件

```bash
qmake
make clean
make
```

### 8.2 运行时错误

**问题**: CAN接口打开失败

**解决**:
```bash
# 检查CAN接口
ip link show can0

# 如果不存在，加载驱动
modprobe can
modprobe can_raw
modprobe vcan  # 虚拟CAN（用于测试）

# 创建虚拟CAN接口（用于测试）
ip link add dev vcan0 type vcan
ip link set up vcan0
```

### 8.3 性能问题

**问题**: RPC响应慢

**排查**:
1. 检查CAN发送队列: `can.status`
2. 检查控制队列: `control.queue.status`
3. 使用日志分析耗时

---

## 9. 代码接手指南

### 9.1 快速上手

1. **阅读文档**:
   - `README.zh.md` - 项目概述
   - `ARCHITECTURE.zh.md` - 系统架构
   - `API_REFERENCE.zh.md` - API详情

2. **运行项目**:
   ```bash
   # 编译
   qmake && make
   
   # 使用虚拟CAN测试
   ip link add dev vcan0 type vcan
   ip link set up vcan0
   ./fanzhou_rpc_server -c config/config_example.json
   ```

3. **熟悉代码结构**:
   - 从 `main.cpp` 开始
   - 追踪 `CoreContext::init()`
   - 查看 `RpcRegistry::registerAll()`

### 9.2 关键文件

| 文件 | 说明 |
|------|------|
| `src/core/core_context.*` | 系统核心，所有业务逻辑入口 |
| `src/core/rpc_registry.cpp` | 所有RPC方法定义 |
| `src/core/core_config.*` | 配置结构和加载/保存 |
| `src/device/can/relay_gd427.*` | 继电器驱动（主要设备） |
| `test_web/js/app.js` | Web界面功能实现 |
| `qt_app/src/mainwindow.cpp` | Qt应用主窗口 |

### 9.3 重要设计决策

1. **分组按通道绑定**: 策略执行时只控制分组中已绑定的通道
2. **配置持久化**: 必须调用 `config.save` 才能保存修改
3. **设备在线判断**: 30秒内有CAN响应认为在线
4. **控制队列**: 控制命令先入队再执行，避免总线拥堵

---

## 10. 联系与支持

- **问题反馈**: GitHub Issues
- **代码贡献**: Pull Request
- **技术讨论**: GitHub Discussions

---

*本文档由泛舟RPC服务器开发团队维护。*
