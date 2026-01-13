# 泛舟RPC调试工具 - Tauri桌面应用

本目录包含将Web调试工具打包为Tauri桌面应用的相关文件。

## 功能特点

- **内置WebSocket代理**: 使用Tauri Sidecar集成websocat，无需手动启动代理
- **跨平台支持**: 支持Windows、macOS、Linux
- **一键启动**: 双击即可运行，自动连接RPC服务器

## 目录结构

```
src-tauri/
├── bin/                    # sidecar可执行文件目录
│   ├── websocat           # Linux可执行文件
│   ├── websocat-x86_64-pc-windows-msvc.exe  # Windows可执行文件
│   └── websocat-x86_64-apple-darwin         # macOS可执行文件
├── src/
│   └── main.rs            # Rust后端代码
├── Cargo.toml             # Rust依赖配置
├── build.rs               # 构建脚本
└── tauri.conf.json        # Tauri配置
```

## 安装步骤

### 1. 安装Rust

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### 2. 安装Tauri CLI

```bash
cargo install tauri-cli
```

### 3. 下载websocat

从 https://github.com/vi/websocat/releases 下载对应平台的websocat可执行文件：

#### Windows
```bash
# 下载 websocat.x86_64-pc-windows-gnu.exe 或 websocat.x86_64-pc-windows-msvc.exe
# 重命名为与您的Rust工具链匹配的目标三元组
# 如果使用MSVC工具链（默认）: websocat-x86_64-pc-windows-msvc.exe
# 如果使用GNU工具链: websocat-x86_64-pc-windows-gnu.exe
# 放置到 src-tauri/bin/ 目录
```

#### Linux
```bash
# 下载适合您系统的版本
# 对于大多数Linux发行版: websocat.x86_64-unknown-linux-musl（静态链接，兼容性更好）
# 重命名为 websocat-x86_64-unknown-linux-gnu
# 放置到 src-tauri/bin/ 目录
# 注意: musl版本是静态链接的，可以在glibc系统上运行
chmod +x src-tauri/bin/websocat-x86_64-unknown-linux-gnu
```

#### macOS
```bash
# 下载 websocat.x86_64-apple-darwin（Intel Mac）
# 或 websocat.aarch64-apple-darwin（Apple Silicon Mac）
# 重命名为对应架构的文件名
# 放置到 src-tauri/bin/ 目录
chmod +x src-tauri/bin/websocat-x86_64-apple-darwin
# 或
chmod +x src-tauri/bin/websocat-aarch64-apple-darwin
```

> **注意**: Tauri会自动根据目标平台选择正确的可执行文件。文件名格式为 `{name}-{target-triple}[.exe]`

### 4. 构建应用

```bash
cd test_web
cargo tauri build
```

### 5. 开发模式

```bash
cd test_web
cargo tauri dev
```

## 前端JavaScript集成

在`js/app.js`中添加以下代码来使用Tauri API：

```javascript
// 检测是否运行在Tauri环境中
const isTauri = window.__TAURI__ !== undefined;

// 启动websocat代理
async function startWebsocatProxy() {
    if (!isTauri) {
        console.log('不是Tauri环境，请手动启动websocat');
        return;
    }
    
    const { invoke } = window.__TAURI__.tauri;
    
    try {
        const pid = await invoke('start_websocat', {
            wsPort: 12346,
            tcpHost: '127.0.0.1',
            tcpPort: 12345
        });
        console.log(`websocat已启动，PID: ${pid}`);
    } catch (error) {
        console.error('启动websocat失败:', error);
    }
}

// 停止websocat代理
async function stopWebsocatProxy() {
    if (!isTauri) return;
    
    const { invoke } = window.__TAURI__.tauri;
    
    try {
        await invoke('stop_websocat');
        console.log('websocat已停止');
    } catch (error) {
        console.error('停止websocat失败:', error);
    }
}

// 检查websocat状态
async function checkWebsocatStatus() {
    if (!isTauri) return false;
    
    const { invoke } = window.__TAURI__.tauri;
    
    try {
        return await invoke('is_websocat_running');
    } catch (error) {
        console.error('检查状态失败:', error);
        return false;
    }
}
```

## Tauri命令说明

| 命令 | 参数 | 返回值 | 描述 |
|------|------|--------|------|
| `start_websocat` | `wsPort`, `tcpHost`, `tcpPort` | `u32` (PID) | 启动websocat代理 |
| `stop_websocat` | 无 | `()` | 停止websocat代理 |
| `is_websocat_running` | 无 | `bool` | 检查代理是否运行 |
| `get_websocat_pid` | 无 | `Option<u32>` | 获取进程PID |

## 配置说明

### tauri.conf.json 关键配置

```json
{
  "tauri": {
    "allowlist": {
      "shell": {
        "execute": true,
        "sidecar": true,
        "scope": [
          {
            "name": "bin/websocat",
            "sidecar": true,
            "args": true
          }
        ]
      }
    },
    "bundle": {
      "externalBin": [
        "bin/websocat"
      ]
    }
  }
}
```

- `shell.sidecar`: 允许使用sidecar功能
- `shell.scope`: 限制只能执行websocat
- `externalBin`: 声明需要打包的外部可执行文件

## 故障排除

### websocat启动失败

1. 确认websocat可执行文件存在于正确位置
2. 确认文件有执行权限（Linux/macOS）
3. 检查端口是否被占用

### 连接失败

1. 确认RPC服务器正在运行（端口12345）
2. 确认websocat代理已启动（端口12346）
3. 检查防火墙设置

## 许可证

MIT License
