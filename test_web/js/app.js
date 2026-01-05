/**
 * 泛舟RPC服务器调试工具 - JavaScript模块
 * 
 * 功能说明：
 * 1. WebSocket连接管理 - 与RPC服务器建立和维护连接
 * 2. RPC请求发送和响应处理 - JSON-RPC 2.0协议
 * 3. 页面导航功能 - 切换不同功能页面
 * 4. 继电器控制功能 - 单节点和分组控制
 * 5. 设备和分组管理功能
 * 6. 日志记录功能
 */

/* ========================================================
 * 全局变量定义
 * ======================================================== */

// WebSocket连接对象
let ws = null;

// RPC请求ID计数器
let requestId = 1;

// 待处理的RPC请求回调映射
const pendingRequests = new Map();

// 设备列表缓存
let deviceListCache = [];

// 分组列表缓存
let groupListCache = [];

// 日志条目数量限制
const MAX_LOG_ENTRIES = 100;

// 默认通道数量（GD427继电器默认4通道）
const DEFAULT_CHANNEL_COUNT = 4;

/* ========================================================
 * 页面导航功能
 * ======================================================== */

/**
 * 显示指定页面
 * @param {string} pageName - 页面名称
 */
function showPage(pageName) {
    // 隐藏所有页面内容
    document.querySelectorAll('.page-content').forEach(el => {
        el.classList.remove('active');
    });
    
    // 移除所有导航按钮的激活状态
    document.querySelectorAll('.nav-btn').forEach(el => {
        el.classList.remove('active');
    });
    
    // 显示选中的页面
    const targetPage = document.getElementById('page-' + pageName);
    if (targetPage) {
        targetPage.classList.add('active');
    }
    
    // 激活对应的导航按钮
    const targetBtn = document.querySelector(`.nav-btn[data-page="${pageName}"]`);
    if (targetBtn) {
        targetBtn.classList.add('active');
    }
}

/* ========================================================
 * WebSocket连接管理
 * ======================================================== */

/**
 * 切换WebSocket连接状态
 */
function toggleConnection() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        // 断开连接
        ws.close();
    } else {
        // 建立连接
        connect();
    }
}

/**
 * 建立WebSocket连接
 */
function connect() {
    const host = document.getElementById('serverHost').value.trim();
    const port = parseInt(document.getElementById('serverPort').value) || 12346;
    
    if (!host) {
        log('error', '请输入服务器地址');
        return;
    }
    
    // 更新连接状态为"连接中"
    updateConnectionStatus('connecting');
    
    // 构建WebSocket URL
    const wsUrl = `ws://${host}:${port}`;
    log('info', `正在连接到 ${wsUrl}...`);
    
    try {
        ws = new WebSocket(wsUrl);
        
        // 连接成功回调
        ws.onopen = function() {
            log('info', '✅ WebSocket连接成功');
            updateConnectionStatus('connected');
            
            // 连接成功后自动发送ping测试
            callMethod('rpc.ping', {});
        };
        
        // 接收消息回调
        ws.onmessage = function(event) {
            handleResponse(event.data);
        };
        
        // 连接关闭回调
        ws.onclose = function(event) {
            log('info', `❌ WebSocket连接已关闭 (code: ${event.code})`);
            updateConnectionStatus('disconnected');
            ws = null;
        };
        
        // 连接错误回调
        ws.onerror = function(error) {
            log('error', '⚠️ WebSocket连接错误，请检查：\n1. 服务器地址是否正确\n2. WebSocket代理是否运行\n3. 防火墙是否允许连接');
            updateConnectionStatus('disconnected');
        };
        
    } catch (e) {
        log('error', `连接失败: ${e.message}`);
        updateConnectionStatus('disconnected');
    }
}

/**
 * 更新连接状态显示
 * @param {string} status - 状态类型: 'connected' | 'disconnected' | 'connecting'
 */
function updateConnectionStatus(status) {
    const statusEl = document.getElementById('connectionStatus');
    const connectBtn = document.getElementById('connectBtn');
    
    statusEl.className = 'status-badge ' + status;
    
    const statusTexts = {
        'connected': '已连接',
        'disconnected': '未连接',
        'connecting': '连接中...'
    };
    
    statusEl.innerHTML = `<span class="status-dot"></span><span>${statusTexts[status]}</span>`;
    
    // 更新按钮文字
    if (status === 'connected') {
        connectBtn.textContent = '🔌 断开';
        connectBtn.classList.add('danger');
    } else {
        connectBtn.textContent = '🔌 连接';
        connectBtn.classList.remove('danger');
    }
}

/* ========================================================
 * RPC请求发送
 * ======================================================== */

/**
 * 构建JSON-RPC请求对象
 * @param {string} method - 方法名
 * @param {object} params - 参数对象
 * @returns {object} JSON-RPC请求对象
 */
function buildRequest(method, params) {
    return {
        jsonrpc: "2.0",
        id: requestId++,
        method: method,
        params: params || {}
    };
}

/**
 * 发送RPC方法调用
 * @param {string} method - 方法名
 * @param {object} params - 参数对象
 * @param {function} callback - 可选的回调函数
 */
function callMethod(method, params, callback) {
    const request = buildRequest(method, params);
    
    // 记录发送日志
    log('send', request);
    
    // 检查WebSocket连接状态
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        // 未连接时显示命令行方式
        const host = document.getElementById('serverHost').value.trim() || 'localhost';
        const jsonStr = JSON.stringify(request).replace(/'/g, "'\\''");
        
        log('info', {
            message: '未连接WebSocket，请使用命令行方式：',
            command: `echo '${jsonStr}' | nc ${host} 12345`
        });
        return;
    }
    
    // 保存回调函数
    if (callback) {
        pendingRequests.set(request.id, callback);
    }
    
    // 发送请求（需要添加换行符，因为服务器使用行分隔的JSON协议）
    ws.send(JSON.stringify(request) + '\n');
}

/**
 * 处理RPC响应
 * @param {string} data - 响应数据
 */
function handleResponse(data) {
    try {
        const response = JSON.parse(data);
        
        // 记录接收日志
        log('recv', response);
        
        // 检查是否有等待的回调
        if (response.id && pendingRequests.has(response.id)) {
            const callback = pendingRequests.get(response.id);
            pendingRequests.delete(response.id);
            callback(response);
        }
        
    } catch (e) {
        log('error', `解析响应失败: ${e.message}`);
    }
}

/* ========================================================
 * 日志功能
 * ======================================================== */

/**
 * 记录日志
 * @param {string} type - 日志类型: 'send' | 'recv' | 'error' | 'info'
 * @param {any} message - 日志内容
 */
function log(type, message) {
    const container = document.getElementById('logContainer');
    const time = new Date().toLocaleTimeString();
    
    // 移除占位符
    const placeholder = container.querySelector('.log-placeholder');
    if (placeholder) {
        placeholder.remove();
    }
    
    // 格式化消息内容
    let formattedMessage = message;
    if (typeof message === 'object') {
        formattedMessage = JSON.stringify(message, null, 2);
    }
    
    // 类型标签配置
    const typeLabels = {
        'send': '发送',
        'recv': '接收',
        'error': '错误',
        'info': '信息'
    };
    
    // 创建日志条目
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerHTML = `
        <div class="log-header">
            <span class="log-time">[${time}]</span>
            <span class="log-direction ${type}">${typeLabels[type] || type}</span>
        </div>
        <div class="log-content">${escapeHtml(formattedMessage)}</div>
    `;
    
    // 插入到容器顶部
    container.insertBefore(entry, container.firstChild);
    
    // 限制日志条目数量
    const entries = container.querySelectorAll('.log-entry');
    if (entries.length > MAX_LOG_ENTRIES) {
        entries[entries.length - 1].remove();
    }
}

/**
 * 清空日志
 */
function clearLog() {
    const container = document.getElementById('logContainer');
    container.innerHTML = '<div class="log-placeholder">日志已清空</div>';
}

/**
 * HTML转义 - 防止XSS攻击
 * @param {string} text - 原始文本
 * @returns {string} 转义后的文本
 */
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

/* ========================================================
 * 继电器控制功能
 * ======================================================== */

/**
 * 控制单个继电器
 * 注意：此方法只控制指定节点的指定通道，不会控制其他节点
 */
function controlRelay() {
    const node = parseInt(document.getElementById('relayNode').value);
    const ch = parseInt(document.getElementById('relayChannel').value);
    const action = document.getElementById('relayAction').value;
    
    callMethod('relay.control', {
        node: node,
        ch: ch,
        action: action
    });
}

/**
 * 查询继电器单通道状态
 */
function queryRelay() {
    const node = parseInt(document.getElementById('relayNode').value);
    const ch = parseInt(document.getElementById('relayChannel').value);
    
    callMethod('relay.status', {
        node: node,
        ch: ch
    });
}

/**
 * 查询继电器全部通道状态
 */
function queryRelayAll() {
    const node = parseInt(document.getElementById('relayNode').value);
    
    callMethod('relay.statusAll', {
        node: node
    });
}

/* ========================================================
 * 分组管理功能
 * ======================================================== */

/**
 * 刷新分组列表
 */
function refreshGroupList() {
    callMethod('group.list', {}, function(response) {
        if (response.result) {
            groupListCache = response.result.groups || response.result || [];
            renderGroupList();
        }
    });
}

/**
 * 渲染分组列表
 */
function renderGroupList() {
    const contentEl = document.getElementById('groupListContent');
    const emptyEl = document.getElementById('groupListEmpty');
    
    if (!groupListCache || groupListCache.length === 0) {
        contentEl.innerHTML = '';
        emptyEl.style.display = 'block';
        return;
    }
    
    emptyEl.style.display = 'none';
    
    let html = '';
    groupListCache.forEach(group => {
        const groupId = group.groupId || group.id;
        const name = group.name || `分组${groupId}`;
        const deviceCount = group.devices ? group.devices.length : 0;
        const enabled = group.enabled !== false;
        
        html += `
            <div class="data-list-item">
                <div class="item-info">
                    <span class="item-name">📂 ${escapeHtml(name)}</span>
                    <span class="item-detail">
                        ID: ${groupId} | 
                        设备数: ${deviceCount} | 
                        状态: ${enabled ? '✅ 启用' : '❌ 禁用'}
                    </span>
                </div>
                <div class="item-actions">
                    <button onclick="controlGroupById(${groupId}, 'stop')">⏹️ 停止</button>
                    <button class="success" onclick="controlGroupById(${groupId}, 'fwd')">▶️ 正转</button>
                    <button class="warning" onclick="controlGroupById(${groupId}, 'rev')">◀️ 反转</button>
                    <button class="danger" onclick="deleteGroupById(${groupId})">🗑️</button>
                </div>
            </div>
        `;
    });
    
    contentEl.innerHTML = html;
}

/**
 * 创建分组
 */
function createGroup() {
    const groupId = parseInt(document.getElementById('newGroupId').value);
    const name = document.getElementById('newGroupName').value.trim();
    
    if (!name) {
        alert('请输入分组名称');
        return;
    }
    
    callMethod('group.create', {
        groupId: groupId,
        name: name
    }, function(response) {
        if (response.result) {
            log('info', '分组创建成功');
            refreshGroupList();
        }
    });
}

/**
 * 控制指定分组
 * 分组控制会向分组中的所有设备发送控制命令
 * @param {number} groupId - 分组ID
 * @param {string} action - 动作 (stop/fwd/rev)
 */
function controlGroupById(groupId, action) {
    // 获取当前选择的通道
    const ch = parseInt(document.getElementById('groupControlChannel')?.value || 0);
    callMethod('group.control', {
        groupId: groupId,
        ch: ch,
        action: action
    });
}

/**
 * 删除指定分组
 * @param {number} groupId - 分组ID
 */
function deleteGroupById(groupId) {
    if (confirm(`确定要删除分组 ${groupId} 吗？`)) {
        callMethod('group.delete', {
            groupId: groupId
        }, function(response) {
            if (response.result) {
                log('info', '分组删除成功');
                refreshGroupList();
            }
        });
    }
}

/**
 * 控制分组（从控制面板）
 * 此方法向分组中的所有设备发送相同的控制命令
 */
function controlGroup() {
    const groupId = parseInt(document.getElementById('groupControlId').value);
    const ch = parseInt(document.getElementById('groupControlChannel').value);
    const action = document.getElementById('groupControlAction').value;
    
    callMethod('group.control', {
        groupId: groupId,
        ch: ch,
        action: action
    });
}

/* ========================================================
 * 设备管理功能
 * ======================================================== */

/**
 * 刷新设备列表
 */
function refreshDeviceList() {
    callMethod('relay.nodes', {}, function(response) {
        if (response.result) {
            deviceListCache = response.result.nodes || response.result || [];
            renderDeviceList();
            renderDeviceCards();
        }
    });
}

/**
 * 渲染设备列表
 */
function renderDeviceList() {
    const contentEl = document.getElementById('deviceListContent');
    const emptyEl = document.getElementById('deviceListEmpty');
    
    if (!deviceListCache || deviceListCache.length === 0) {
        contentEl.innerHTML = '';
        emptyEl.style.display = 'block';
        return;
    }
    
    emptyEl.style.display = 'none';
    
    let html = '';
    deviceListCache.forEach(device => {
        const nodeId = device.nodeId || device.node || device;
        const name = device.name || `节点 ${nodeId}`;
        const type = device.type || 'relay';
        const online = device.online !== false;
        
        html += `
            <div class="data-list-item">
                <div class="item-info">
                    <span class="item-name">🔌 ${escapeHtml(name)}</span>
                    <span class="item-detail">
                        节点ID: ${nodeId} | 
                        类型: ${escapeHtml(type)} | 
                        状态: ${online ? '🟢 在线' : '🔴 离线'}
                    </span>
                </div>
                <div class="item-actions">
                    <button onclick="queryDeviceStatus(${nodeId})">🔍 查询状态</button>
                    <button class="success" onclick="controlDeviceAll(${nodeId}, 'fwd')">▶️ 全部正转</button>
                    <button class="danger" onclick="controlDeviceAll(${nodeId}, 'stop')">⏹️ 全部停止</button>
                </div>
            </div>
        `;
    });
    
    contentEl.innerHTML = html;
}

/**
 * 渲染设备卡片视图
 */
function renderDeviceCards() {
    const container = document.getElementById('deviceCards');
    
    if (!deviceListCache || deviceListCache.length === 0) {
        container.innerHTML = '';
        return;
    }
    
    let html = '';
    deviceListCache.forEach(device => {
        const nodeId = device.nodeId || device.node || device;
        const name = device.name || `节点 ${nodeId}`;
        const online = device.online !== false;
        const channels = device.channels || 4;
        
        let channelHtml = '';
        for (let i = 0; i < channels; i++) {
            channelHtml += `
                <div class="channel-item">
                    <div class="ch-label">通道 ${i}</div>
                    <div class="ch-status stop" id="ch-status-${nodeId}-${i}">--</div>
                </div>
            `;
        }
        
        html += `
            <div class="device-card">
                <div class="device-card-header">
                    <span class="device-card-title">🔌 ${escapeHtml(name)} (ID: ${nodeId})</span>
                    <span class="device-card-status ${online ? 'online' : 'offline'}">
                        ${online ? '在线' : '离线'}
                    </span>
                </div>
                <div class="channel-grid">${channelHtml}</div>
            </div>
        `;
    });
    
    container.innerHTML = html;
}

/**
 * 查询单个设备状态
 * @param {number} nodeId - 节点ID
 */
function queryDeviceStatus(nodeId) {
    callMethod('relay.statusAll', {
        node: nodeId
    });
}

/**
 * 控制设备所有通道
 * 这会向指定节点的所有通道发送控制命令
 * @param {number} nodeId - 节点ID
 * @param {string} action - 动作 (stop/fwd/rev)
 */
function controlDeviceAll(nodeId, action) {
    // 控制所有通道（逐个发送控制命令）
    for (let ch = 0; ch < DEFAULT_CHANNEL_COUNT; ch++) {
        callMethod('relay.control', {
            node: nodeId,
            ch: ch,
            action: action
        });
    }
}

/* ========================================================
 * 自定义调用功能
 * ======================================================== */

/**
 * 发送自定义RPC调用
 */
function callCustomMethod() {
    const method = document.getElementById('methodName').value.trim();
    const paramsStr = document.getElementById('methodParams').value;
    
    if (!method) {
        alert('请输入方法名');
        return;
    }
    
    let params = {};
    try {
        params = JSON.parse(paramsStr);
    } catch (e) {
        alert('参数格式错误，请输入有效的JSON');
        return;
    }
    
    callMethod(method, params);
}

/* ========================================================
 * 页面初始化
 * ======================================================== */

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    log('info', '🚀 泛舟RPC调试工具已就绪');
    log('info', '请输入服务器地址并点击"连接"按钮');
    
    // 自动从URL参数获取服务器地址
    const urlParams = new URLSearchParams(window.location.search);
    const hostParam = urlParams.get('host');
    const portParam = urlParams.get('port');
    
    if (hostParam) {
        document.getElementById('serverHost').value = hostParam;
    }
    if (portParam) {
        document.getElementById('serverPort').value = portParam;
    }
    
    // 如果URL参数中有autoconnect=true，则自动连接
    if (urlParams.get('autoconnect') === 'true') {
        setTimeout(connect, 500);
    }
});
