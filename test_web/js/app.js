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

/**
 * 格式化时间间隔为人类可读格式
 * @param {number} ms - 毫秒数
 * @returns {string} 格式化的时间字符串
 */
function formatAge(ms) {
    if (ms < 1000) {
        return `${Math.round(ms)}毫秒前`;
    } else if (ms < 60000) {
        return `${(ms / 1000).toFixed(1)}秒前`;
    } else if (ms < 3600000) {
        return `${Math.floor(ms / 60000)}分钟前`;
    } else {
        return `超过1小时`;
    }
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

/**
 * 验证分组和设备ID的输入值
 * @returns {{valid: boolean, groupId: number, nodeId: number}} 验证结果
 */
function validateGroupDeviceInput() {
    const groupId = parseInt(document.getElementById('addDeviceGroupId').value);
    const nodeId = parseInt(document.getElementById('addDeviceNodeId').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return { valid: false, groupId: 0, nodeId: 0 };
    }
    if (!nodeId || nodeId <= 0 || nodeId > 255) {
        alert('请输入有效的设备节点ID (1-255)');
        return { valid: false, groupId: 0, nodeId: 0 };
    }
    
    return { valid: true, groupId: groupId, nodeId: nodeId };
}

/**
 * 添加设备到分组
 */
function addDeviceToGroup() {
    const input = validateGroupDeviceInput();
    if (!input.valid) return;
    
    callMethod('group.addDevice', {
        groupId: input.groupId,
        node: input.nodeId
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `设备 ${input.nodeId} 已添加到分组 ${input.groupId}`);
            refreshGroupList();
        } else if (response.error) {
            log('error', `添加失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 从分组移除设备
 */
function removeDeviceFromGroup() {
    const input = validateGroupDeviceInput();
    if (!input.valid) return;
    
    if (confirm(`确定要从分组 ${input.groupId} 移除设备 ${input.nodeId} 吗？`)) {
        callMethod('group.removeDevice', {
            groupId: input.groupId,
            node: input.nodeId
        }, function(response) {
            if (response.result && response.result.ok) {
                log('info', `设备 ${input.nodeId} 已从分组 ${input.groupId} 移除`);
                refreshGroupList();
            } else if (response.error) {
                log('error', `移除失败: ${response.error.message || '未知错误'}`);
            }
        });
    }
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
 * 设备在线状态由服务端根据最后通信时间判断（30秒内有响应认为在线）
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
        // 在线状态必须由服务端明确返回true才认为在线
        const online = device.online === true;
        // 显示上次响应时间（如果有）
        const ageMs = device.ageMs;
        const ageText = (typeof ageMs === 'number') ? formatAge(ageMs) : '';
        
        html += `
            <div class="data-list-item">
                <div class="item-info">
                    <span class="item-name">🔌 ${escapeHtml(name)}</span>
                    <span class="item-detail">
                        节点ID: ${nodeId} | 
                        类型: ${escapeHtml(type)} | 
                        状态: ${online ? '🟢 在线' : '🔴 离线'}${ageText ? ' | 响应: ' + ageText : ''}
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
 * 设备在线状态由服务端根据最后通信时间判断
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
        // 在线状态必须由服务端明确返回true才认为在线
        const online = device.online === true;
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
 * 策略管理功能
 * ======================================================== */

// 策略列表缓存
let strategyListCache = [];

// 传感器策略列表缓存
let sensorStrategyListCache = [];

/**
 * 刷新策略列表
 * 同时获取定时策略和传感器策略
 */
function refreshStrategyList() {
    // 获取定时策略列表
    callMethod('auto.strategy.list', {}, function(response) {
        if (response.result) {
            strategyListCache = response.result.strategies || [];
            renderStrategyList();
        } else if (response.error) {
            log('error', `获取策略列表失败: ${response.error.message || '未知错误'}`);
        }
    });
    
    // 获取传感器策略列表
    callMethod('auto.sensor.list', {}, function(response) {
        if (response.result) {
            sensorStrategyListCache = response.result.strategies || [];
            renderSensorStrategyList();
        }
    });
}

/**
 * 渲染策略列表
 * 显示所有定时策略及其状态，使用更清晰的布局
 */
function renderStrategyList() {
    const contentEl = document.getElementById('strategyListContent');
    const emptyEl = document.getElementById('strategyListEmpty');
    
    if (!strategyListCache || strategyListCache.length === 0) {
        contentEl.innerHTML = '';
        emptyEl.style.display = 'block';
        return;
    }
    
    emptyEl.style.display = 'none';
    
    // 动作名称映射
    const actionNames = {
        'stop': '⏹️ 停止',
        'fwd': '▶️ 正转',
        'rev': '◀️ 反转'
    };
    
    // 触发类型名称映射
    const triggerTypeNames = {
        'interval': '⏱️ 间隔执行',
        'daily': '📅 每日定时'
    };
    
    let html = '';
    strategyListCache.forEach(strategy => {
        const id = strategy.id;
        const name = strategy.name || `策略${id}`;
        const groupId = strategy.groupId;
        const channel = strategy.channel === -1 ? '全部' : strategy.channel;
        const action = actionNames[strategy.action] || strategy.action;
        const intervalSec = strategy.intervalSec;
        const dailyTime = strategy.dailyTime;
        const triggerType = strategy.triggerType || (dailyTime ? 'daily' : 'interval');
        const enabled = strategy.enabled !== false;
        const running = strategy.running === true;
        const attached = strategy.attached === true;
        
        // 构建触发时间描述
        let triggerDesc = '';
        if (triggerType === 'daily' && dailyTime) {
            triggerDesc = `📅 每天 ${dailyTime}`;
        } else if (intervalSec) {
            // 将秒数转换为更易读的格式
            if (intervalSec >= 3600) {
                const hours = Math.floor(intervalSec / 3600);
                const mins = Math.floor((intervalSec % 3600) / 60);
                triggerDesc = `⏱️ 每 ${hours}小时${mins > 0 ? mins + '分钟' : ''}`;
            } else if (intervalSec >= 60) {
                const mins = Math.floor(intervalSec / 60);
                const secs = intervalSec % 60;
                triggerDesc = `⏱️ 每 ${mins}分钟${secs > 0 ? secs + '秒' : ''}`;
            } else {
                triggerDesc = `⏱️ 每 ${intervalSec}秒`;
            }
        }
        
        // 状态图标
        const statusIcon = enabled ? (running ? '🟢' : '🟡') : '🔴';
        const statusText = enabled ? (running ? '运行中' : '已启用') : '已禁用';
        
        html += `
            <div class="data-list-item" style="flex-wrap: wrap; gap: 10px;">
                <div class="item-info" style="min-width: 200px;">
                    <span class="item-name">⏱️ ${escapeHtml(name)}</span>
                    <span class="item-detail">
                        <strong>ID:</strong> ${id} | 
                        <strong>分组:</strong> ${groupId} | 
                        <strong>通道:</strong> ${channel}
                    </span>
                    <span class="item-detail">
                        <strong>动作:</strong> ${action} | 
                        <strong>触发:</strong> ${triggerDesc}
                    </span>
                    <span class="item-detail">
                        ${statusIcon} ${statusText}
                        ${attached ? ' | 🔗 已挂载' : ''}
                    </span>
                </div>
                <div class="item-actions" style="display: flex; flex-wrap: wrap; gap: 6px;">
                    <button onclick="toggleStrategyEnabled(${id}, ${!enabled})" 
                            class="${enabled ? 'warning' : 'success'}" 
                            title="${enabled ? '点击禁用此策略' : '点击启用此策略'}">
                        ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                    </button>
                    <button class="secondary" onclick="triggerStrategy(${id})" title="立即执行一次此策略">
                        🎯 触发
                    </button>
                    <button class="danger" onclick="deleteStrategy(${id})" title="永久删除此策略">
                        🗑️ 删除
                    </button>
                </div>
            </div>
        `;
    });
    
    contentEl.innerHTML = html;
}

/**
 * 渲染传感器策略列表
 * 显示所有传感器触发策略，使用更清晰的布局
 */
function renderSensorStrategyList() {
    const contentEl = document.getElementById('sensorStrategyListContent');
    const emptyEl = document.getElementById('sensorStrategyListEmpty');
    
    if (!sensorStrategyListCache || sensorStrategyListCache.length === 0) {
        if (contentEl) contentEl.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    // 传感器类型名称映射
    const sensorTypeNames = {
        'temperature': '🌡️ 温度',
        'humidity': '💧 湿度',
        'light': '💡 光照',
        'pressure': '📊 压力',
        'soil_moisture': '🌱 土壤湿度',
        'co2': '🌫️ CO2'
    };
    
    // 条件名称映射
    const conditionNames = {
        'gt': '>',
        'lt': '<',
        'eq': '=',
        'gte': '>=',
        'lte': '<='
    };
    
    // 条件描述映射（更易理解）
    const conditionDescriptions = {
        'gt': '大于',
        'lt': '小于',
        'eq': '等于',
        'gte': '大于等于',
        'lte': '小于等于'
    };
    
    // 动作名称映射
    const actionNames = {
        'stop': '⏹️ 停止',
        'fwd': '▶️ 正转',
        'rev': '◀️ 反转'
    };
    
    let html = '';
    sensorStrategyListCache.forEach(strategy => {
        const id = strategy.id;
        const name = strategy.name || `传感器策略${id}`;
        const sensorType = sensorTypeNames[strategy.sensorType] || strategy.sensorType;
        const sensorNode = strategy.sensorNode;
        const condition = conditionNames[strategy.condition] || strategy.condition;
        const conditionDesc = conditionDescriptions[strategy.condition] || strategy.condition;
        const threshold = strategy.threshold;
        const groupId = strategy.groupId;
        const channel = strategy.channel >= 0 ? strategy.channel : '全部';
        const action = actionNames[strategy.action] || strategy.action;
        const enabled = strategy.enabled !== false;
        const cooldown = strategy.cooldownSec || 0;
        
        // 状态图标
        const statusIcon = enabled ? '🟢' : '🔴';
        const statusText = enabled ? '已启用' : '已禁用';
        
        html += `
            <div class="data-list-item" style="flex-wrap: wrap; gap: 10px;">
                <div class="item-info" style="min-width: 200px;">
                    <span class="item-name">📡 ${escapeHtml(name)}</span>
                    <span class="item-detail">
                        <strong>ID:</strong> ${id} | 
                        <strong>传感器:</strong> ${sensorType} (节点 ${sensorNode})
                    </span>
                    <span class="item-detail">
                        <strong>触发条件:</strong> 当数值 ${conditionDesc} ${threshold} 时
                    </span>
                    <span class="item-detail">
                        <strong>执行:</strong> 分组 ${groupId} 通道 ${channel} → ${action}
                        ${cooldown > 0 ? ` | <strong>冷却:</strong> ${cooldown}秒` : ''}
                    </span>
                    <span class="item-detail">
                        ${statusIcon} ${statusText}
                    </span>
                </div>
                <div class="item-actions" style="display: flex; flex-wrap: wrap; gap: 6px;">
                    <button onclick="toggleSensorStrategyEnabled(${id}, ${!enabled})" 
                            class="${enabled ? 'warning' : 'success'}"
                            title="${enabled ? '点击禁用此策略' : '点击启用此策略'}">
                        ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                    </button>
                    <button class="danger" onclick="deleteSensorStrategy(${id})" title="永久删除此策略">
                        🗑️ 删除
                    </button>
                </div>
            </div>
        `;
    });
    
    if (contentEl) contentEl.innerHTML = html;
}

/**
 * 切换策略启用状态
 * @param {number} id - 策略ID
 * @param {boolean} enabled - 是否启用
 */
function toggleStrategyEnabled(id, enabled) {
    callMethod('auto.strategy.enable', {
        id: id,
        enabled: enabled
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `策略 ${id} 已${enabled ? '启用' : '禁用'}`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `操作失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 手动触发策略
 * @param {number} id - 策略ID
 */
function triggerStrategy(id) {
    callMethod('auto.strategy.trigger', {
        id: id
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `策略 ${id} 已触发`);
        } else if (response.error) {
            log('error', `触发失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 删除定时策略
 * @param {number} id - 策略ID
 */
function deleteStrategy(id) {
    if (!confirm(`确定要删除策略 ${id} 吗？`)) return;
    
    callMethod('auto.strategy.delete', {
        id: id
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `策略 ${id} 已删除`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `删除失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 切换定时策略触发类型的输入框显示
 * 根据选择的触发方式（间隔/每日定时）显示相应的配置选项
 */
function toggleTimerTypeInputs() {
    const triggerType = document.getElementById('newStrategyTriggerType').value;
    const intervalGroup = document.getElementById('intervalInputGroup');
    const dailyTimeGroup = document.getElementById('dailyTimeInputGroup');
    
    if (triggerType === 'interval') {
        // 间隔执行模式
        intervalGroup.style.display = 'block';
        dailyTimeGroup.style.display = 'none';
    } else if (triggerType === 'daily') {
        // 每日定时模式
        intervalGroup.style.display = 'none';
        dailyTimeGroup.style.display = 'block';
    }
}

/**
 * 将时间字符串转换为当天的秒数
 * @param {string} timeStr - 格式为 "HH:MM" 的时间字符串
 * @returns {number} 从午夜开始的秒数，如果格式无效则返回0
 */
function timeToSeconds(timeStr) {
    // 验证输入
    if (!timeStr || typeof timeStr !== 'string') {
        console.warn('timeToSeconds: 无效的时间字符串', timeStr);
        return 0;
    }
    
    // 验证格式 HH:MM
    const timePattern = /^([0-1]?[0-9]|2[0-3]):([0-5][0-9])$/;
    if (!timePattern.test(timeStr)) {
        console.warn('timeToSeconds: 时间格式不正确，应为 HH:MM', timeStr);
        return 0;
    }
    
    const parts = timeStr.split(':');
    const hours = parseInt(parts[0]) || 0;
    const minutes = parseInt(parts[1]) || 0;
    return hours * 3600 + minutes * 60;
}

/**
 * 创建定时策略
 * 支持两种触发方式：间隔执行和每日定时执行
 * 从表单获取参数并调用RPC创建策略
 */
function createTimerStrategy() {
    const id = parseInt(document.getElementById('newStrategyId').value);
    const name = document.getElementById('newStrategyName').value.trim();
    const groupId = parseInt(document.getElementById('newStrategyGroupId').value);
    const channel = parseInt(document.getElementById('newStrategyChannel').value);
    const action = document.getElementById('newStrategyAction').value;
    const autoStart = document.getElementById('newStrategyAutoStart').value === 'true';
    
    // 获取触发方式
    const triggerType = document.getElementById('newStrategyTriggerType').value;
    
    if (!name) {
        alert('请输入策略名称');
        return;
    }
    
    // 构建策略参数
    const params = {
        id: id,
        name: name,
        groupId: groupId,
        channel: channel,
        action: action,
        enabled: true,
        autoStart: autoStart
    };
    
    // 根据触发方式设置不同的时间参数
    if (triggerType === 'interval') {
        // 间隔执行模式
        const intervalSec = parseInt(document.getElementById('newStrategyInterval').value);
        if (!intervalSec || intervalSec < 1) {
            alert('请输入有效的执行间隔（至少1秒）');
            return;
        }
        params.intervalSec = intervalSec;
        params.triggerType = 'interval';
    } else if (triggerType === 'daily') {
        // 每日定时模式
        const dailyTime = document.getElementById('newStrategyDailyTime').value;
        if (!dailyTime) {
            alert('请选择每日执行时间');
            return;
        }
        params.dailyTime = dailyTime;
        params.dailyTimeSec = timeToSeconds(dailyTime);
        params.triggerType = 'daily';
    }
    
    callMethod('auto.strategy.create', params, function(response) {
        if (response.result && response.result.ok) {
            const triggerDesc = triggerType === 'daily' ? 
                `每日 ${params.dailyTime} 执行` : 
                `每 ${params.intervalSec} 秒执行`;
            log('info', `定时策略 "${name}" 创建成功（${triggerDesc}）`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `创建失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 创建传感器触发策略
 * 当传感器数值满足条件时自动触发分组控制
 */
function createSensorStrategy() {
    const id = parseInt(document.getElementById('sensorStrategyId').value);
    const name = document.getElementById('sensorStrategyName').value.trim();
    const sensorType = document.getElementById('sensorType').value;
    const sensorNode = parseInt(document.getElementById('sensorNodeId').value);
    const condition = document.getElementById('sensorCondition').value;
    const threshold = parseFloat(document.getElementById('sensorThreshold').value);
    const groupId = parseInt(document.getElementById('sensorGroupId').value);
    const channel = parseInt(document.getElementById('sensorChannel').value);
    const action = document.getElementById('sensorAction').value;
    const cooldownSec = parseInt(document.getElementById('sensorCooldown').value);
    const enabled = document.getElementById('sensorEnabled').value === 'true';
    
    if (!name) {
        alert('请输入策略名称');
        return;
    }
    
    callMethod('auto.sensor.create', {
        id: id,
        name: name,
        sensorType: sensorType,
        sensorNode: sensorNode,
        condition: condition,
        threshold: threshold,
        groupId: groupId,
        channel: channel,
        action: action,
        cooldownSec: cooldownSec,
        enabled: enabled
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `传感器策略 "${name}" 创建成功`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `创建失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 切换传感器策略启用状态
 * @param {number} id - 策略ID
 * @param {boolean} enabled - 是否启用
 */
function toggleSensorStrategyEnabled(id, enabled) {
    callMethod('auto.sensor.enable', {
        id: id,
        enabled: enabled
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `传感器策略 ${id} 已${enabled ? '启用' : '禁用'}`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `操作失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 删除传感器策略
 * @param {number} id - 策略ID
 */
function deleteSensorStrategy(id) {
    if (!confirm(`确定要删除传感器策略 ${id} 吗？`)) return;
    
    callMethod('auto.sensor.delete', {
        id: id
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `传感器策略 ${id} 已删除`);
            refreshStrategyList();
        } else if (response.error) {
            log('error', `删除失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 分组通道管理功能
 * ======================================================== */

/**
 * 添加通道到分组
 * 将指定设备的指定通道添加到分组的控制列表中
 */
function addChannelToGroup() {
    const groupId = parseInt(document.getElementById('programGroupId').value);
    const node = parseInt(document.getElementById('programNodeId').value);
    const channel = parseInt(document.getElementById('programChannel').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    if (!node || node <= 0 || node > 255) {
        alert('请输入有效的设备节点ID (1-255)');
        return;
    }
    
    callMethod('group.addChannel', {
        groupId: groupId,
        node: node,
        channel: channel
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `通道 ${channel} (节点${node}) 已添加到分组 ${groupId}`);
        } else if (response.error) {
            log('error', `添加失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 从分组移除通道
 */
function removeChannelFromGroup() {
    const groupId = parseInt(document.getElementById('programGroupId').value);
    const node = parseInt(document.getElementById('programNodeId').value);
    const channel = parseInt(document.getElementById('programChannel').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    
    if (!confirm(`确定要从分组 ${groupId} 移除节点 ${node} 的通道 ${channel} 吗？`)) return;
    
    callMethod('group.removeChannel', {
        groupId: groupId,
        node: node,
        channel: channel
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `通道 ${channel} (节点${node}) 已从分组 ${groupId} 移除`);
        } else if (response.error) {
            log('error', `移除失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 查看分组通道列表
 */
function getGroupChannels() {
    const groupId = parseInt(document.getElementById('programGroupId').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    
    callMethod('group.getChannels', {
        groupId: groupId
    }, function(response) {
        if (response.result) {
            const channels = response.result.channels || [];
            if (channels.length === 0) {
                log('info', `分组 ${groupId} 暂无绑定的通道`);
            } else {
                let channelInfo = `分组 ${groupId} 的通道列表:\n`;
                channels.forEach(ch => {
                    channelInfo += `  - 节点 ${ch.node} 通道 ${ch.channel}\n`;
                });
                log('info', channelInfo);
            }
        } else if (response.error) {
            log('error', `查询失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 设备配置功能
 * ======================================================== */

/**
 * 读取设备配置
 * 获取服务器系统信息并显示
 */
function readDeviceConfig() {
    callMethod('sys.info', {}, function(response) {
        const panel = document.getElementById('deviceConfigPanel');
        const content = document.getElementById('deviceConfigContent');
        
        if (response.result) {
            const configInfo = {
                serverVersion: response.result.serverVersion,
                rpcPort: response.result.rpcPort,
                canInterface: response.result.canInterface,
                canBitrate: response.result.canBitrate,
                deviceCount: response.result.deviceCount,
                groupCount: response.result.groupCount
            };
            
            content.textContent = JSON.stringify(configInfo, null, 2);
            panel.style.display = 'block';
            log('info', '设备配置读取成功');
        } else if (response.error) {
            content.textContent = `读取失败: ${response.error.message || '未知错误'}`;
            panel.style.display = 'block';
            log('error', `设备配置读取失败: ${response.error.message || '未知错误'}`);
        } else {
            content.textContent = '无法读取设备配置';
            panel.style.display = 'block';
        }
    });
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
