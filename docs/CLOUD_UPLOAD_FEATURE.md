# 云数据上传功能说明

## 功能概述

本次更新为泛舟RPC服务器和QT客户端添加了全面的云数据上传功能，包括：
1. RPC连接和Token管理（已验证）
2. QT APP自动连接功能
3. 云/MQTT连接状态显示
4. 可配置的云数据上传功能

## 1. RPC连接和Token管理

### IP白名单
- 本机连接（127.0.0.1, ::1, localhost）不需要token认证
- 在配置文件中可以添加白名单IP地址
- 白名单配置位置：`config.json` -> `main.auth.whitelist`

### Token认证
- 支持预设token列表
- 支持动态生成token (通过 `auth.login` RPC方法)
- Token可设置过期时间
- 公共方法（如 `rpc.ping`, `auth.login` 等）不需要认证

配置示例：
```json
{
  "main": {
    "auth": {
      "enabled": true,
      "secret": "your-secret-key",
      "tokenExpireSec": 3600,
      "allowedTokens": ["token1", "token2"],
      "whitelist": ["127.0.0.1", "192.168.1.100"],
      "publicMethods": ["rpc.ping", "rpc.list", "auth.login"]
    }
  }
}
```

## 2. QT APP自动连接

### 功能说明
- QT应用启动时自动连接到RPC服务器
- 可在设置页面启用/禁用自动连接
- 自动连接延迟500ms，确保UI完全初始化

### 使用方法
1. 打开QT应用
2. 进入"设置"页面
3. 在"RPC服务器"部分勾选"启动时自动连接"
4. 保存设置后，下次启动应用会自动连接

### 相关代码
- 设置存储：`QSettings` -> `settings/autoConnect`
- 连接逻辑：`MainWindow::attemptAutoConnect()`

## 3. 云/MQTT连接状态显示

### 状态栏显示
主窗口状态栏新增云连接状态指示器：
- **[云] 未配置**: 灰色，表示没有配置MQTT通道
- **[云] 未连接**: 灰色，表示RPC服务器未连接
- **[云] 断开 (0/N)**: 橙色，表示所有MQTT通道都断开
- **[云] 已连接 (N)**: 绿色加粗，表示所有MQTT通道都连接成功
- **[云] 部分连接 (M/N)**: 黄色，表示部分MQTT通道连接

### 更新频率
- 每5秒自动更新一次
- 使用 `mqtt.channels.list` RPC方法获取状态

## 4. 云数据上传配置

### 配置结构

#### 上传模式
- **变化上传** (`change`): 当数据变化时才上传
- **定时上传** (`interval`): 按固定时间间隔上传

#### 可上传的数据属性
1. **通道状态**: 继电器开/关/停止状态
2. **缺相状态**: 三相电源缺相检测
3. **电流值**: 继电器通道电流
4. **设备在线状态**: 设备是否在线

#### 变化检测配置
- **电流阈值**: 电流变化超过此值时上传（单位：安培）
- **仅状态改变上传**: 只有状态变化时才上传
- **最小上传间隔**: 防止频繁上传的最小时间间隔（秒）

### 配置文件格式

```json
{
  "cloudUpload": {
    "enabled": true,
    "uploadMode": "change",
    "intervalSec": 60,
    "uploadChannelStatus": true,
    "uploadPhaseLoss": true,
    "uploadCurrent": true,
    "uploadOnlineStatus": true,
    "currentThreshold": 0.1,
    "statusChangeOnly": true,
    "minUploadIntervalSec": 5
  }
}
```

### RPC API

#### 获取配置
```
方法: cloud.upload.get
参数: 无
返回: {
  "ok": true,
  "enabled": true,
  "uploadMode": "change",
  "intervalSec": 60,
  "uploadChannelStatus": true,
  "uploadPhaseLoss": true,
  "uploadCurrent": true,
  "uploadOnlineStatus": true,
  "currentThreshold": 0.1,
  "statusChangeOnly": true,
  "minUploadIntervalSec": 5
}
```

#### 设置配置
```
方法: cloud.upload.set
参数: {
  "enabled": true,
  "uploadMode": "change",
  "intervalSec": 60,
  "uploadChannelStatus": true,
  "uploadPhaseLoss": true,
  "uploadCurrent": true,
  "uploadOnlineStatus": true,
  "currentThreshold": 0.1,
  "statusChangeOnly": true,
  "minUploadIntervalSec": 5
}
返回: {"ok": true}
```

**注意**: 设置后需要调用 `config.save` 方法将配置持久化到文件。

### UI界面

在QT应用的"设置"页面新增"数据上传"标签页：

#### 云数据上传配置组
- 启用云数据上传（复选框）
- 上传模式选择（下拉框：变化上传/定时上传）
- 上传间隔设置（秒）

#### 上传数据属性组
- 通道状态（复选框）
- 缺相状态（复选框）
- 电流值（复选框）
- 设备在线状态（复选框）

#### 变化检测阈值组
- 电流阈值（0.0-100.0A）
- 仅状态改变时上传（复选框）
- 最小上传间隔（秒）

#### 操作按钮
- **获取配置**: 从服务器读取当前配置
- **保存配置**: 保存配置到服务器（需要再调用 config.save 持久化）

## 使用流程

### 1. 配置MQTT通道
首先需要配置至少一个MQTT通道：
```bash
# 使用RPC客户端或QT应用
mqtt.channels.add {
  "channelId": 1,
  "name": "主云平台",
  "broker": "mqtt.example.com",
  "port": 1883,
  "clientId": "fanzhou_001",
  "username": "user",
  "password": "pass",
  "topicPrefix": "fanzhou/devices"
}
```

### 2. 配置数据上传
在QT应用的"设置"->"数据上传"中：
1. 勾选"启用云数据上传"
2. 选择上传模式（推荐使用"变化上传"）
3. 选择要上传的数据属性
4. 根据需要调整阈值参数
5. 点击"保存配置"

### 3. 持久化配置
在"设置"->"连接"页面点击"保存配置到文件"按钮，或调用RPC：
```
config.save
```

### 4. 验证运行
1. 在主窗口状态栏查看云连接状态
2. 在日志页面查看数据上传记录
3. 在MQTT broker查看收到的消息

## 数据上传格式

上传到MQTT的数据格式：
```json
{
  "type": "device_value_change",
  "timestamp": 1642157890123,
  "deviceNode": 1,
  "channel": 0,
  "value": {
    "status": "forward",
    "current": 1.25,
    "phaseLoss": false,
    "online": true
  },
  "oldValue": {
    "status": "stop",
    "current": 0.0,
    "phaseLoss": false,
    "online": true
  }
}
```

Topic格式：`{topicPrefix}/device/{nodeId}/ch{channel}/change`

## 注意事项

1. **性能考虑**
   - 变化上传模式比定时上传更节省带宽
   - 合理设置最小上传间隔，避免频繁上传
   - 电流阈值不宜设置太小，避免微小波动触发上传

2. **网络可靠性**
   - MQTT支持自动重连
   - 建议配置QoS=1确保消息可靠传输
   - 可配置多个MQTT通道实现冗余

3. **安全性**
   - 本地连接（127.0.0.1）自动豁免token认证
   - 远程连接需要配置token或加入白名单
   - MQTT密码建议使用强密码

4. **调试**
   - 可在日志页面查看详细运行日志
   - 使用 `mqtt.channels.list` 查看通道状态
   - 使用 `cloud.upload.get` 查看当前配置

## 故障排除

### 自动连接失败
- 检查服务器是否运行
- 检查主机地址和端口配置
- 查看日志获取详细错误信息

### 云状态显示"未知"
- 确认RPC服务器已连接
- 检查 `mqtt.channels.list` 方法是否可用
- 查看服务器日志

### 数据没有上传
1. 检查云数据上传是否启用
2. 检查MQTT通道是否连接
3. 检查上传属性是否选中
4. 检查变化阈值设置是否合理
5. 查看服务器日志获取详细信息

## 开发说明

### 相关文件
- 后端配置: `src/core/core_config.h`, `src/core/core_config.cpp`
- 后端上下文: `src/core/core_context.h`, `src/core/core_context.cpp`
- RPC方法: `src/core/rpc_registry.cpp`
- QT主窗口: `qt_app/src/mainwindow.h`, `qt_app/src/mainwindow.cpp`
- QT设置页面: `qt_app/src/settings_widget.h`, `qt_app/src/settings_widget.cpp`

### 扩展建议
1. 添加数据上传统计信息（成功/失败次数）
2. 支持更多传感器数据类型
3. 添加数据压缩选项
4. 支持批量上传
5. 添加数据缓存机制（断网续传）
