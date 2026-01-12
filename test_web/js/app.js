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
    const headerStatusEl = document.getElementById('headerConnectionStatus');
    const connectBtn = document.getElementById('connectBtn');
    
    const statusTexts = {
        'connected': '已连接',
        'disconnected': '未连接',
        'connecting': '连接中...'
    };
    
    const statusHtml = `<span class="status-dot"></span><span>${statusTexts[status]}</span>`;
    
    // 更新连接设置页面的状态
    if (statusEl) {
        statusEl.className = 'status-badge ' + status;
        statusEl.innerHTML = statusHtml;
    }
    
    // 更新头部的状态
    if (headerStatusEl) {
        headerStatusEl.className = 'status-badge ' + status;
        headerStatusEl.innerHTML = statusHtml;
    }
    
    // 更新按钮文字
    if (connectBtn) {
        if (status === 'connected') {
            connectBtn.textContent = '🔌 断开';
            connectBtn.classList.add('danger');
        } else {
            connectBtn.textContent = '🔌 连接';
            connectBtn.classList.remove('danger');
        }
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
 * 同时获取分组信息和通道信息
 */
function refreshGroupList() {
    callMethod('group.list', {}, function(response) {
        if (response.result) {
            groupListCache = response.result.groups || response.result || [];
            
            // 先渲染一次基本列表
            renderGroupList();
            
            // 批量获取所有分组的通道信息
            let pendingRequests = groupListCache.length;
            if (pendingRequests === 0) return;
            
            groupListCache.forEach(group => {
                const groupId = group.groupId || group.id;
                callMethod('group.getChannels', { groupId: groupId }, function(chResponse) {
                    if (chResponse.result && chResponse.result.channels) {
                        group.channels = chResponse.result.channels;
                    }
                    pendingRequests--;
                    // 只在所有请求完成后重新渲染一次
                    if (pendingRequests === 0) {
                        renderGroupList();
                    }
                });
            });
        }
    });
}

/**
 * 渲染分组列表为卡片视图
 */
function renderGroupList() {
    const contentEl = document.getElementById('groupCards');
    const emptyEl = document.getElementById('groupCardsEmpty');
    
    if (!groupListCache || groupListCache.length === 0) {
        if (contentEl) contentEl.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    let html = '';
    groupListCache.forEach(group => {
        const groupId = group.groupId || group.id;
        const name = group.name || `分组${groupId}`;
        const devices = group.devices || [];
        const channels = group.channels || [];
        const deviceCount = devices.length;
        const channelCount = channels.length;
        
        // 构建设备标签
        let devicesHtml = '';
        if (devices.length > 0) {
            devices.forEach(nodeId => {
                devicesHtml += `<span class="group-device-tag">🔌 节点 ${nodeId}</span>`;
            });
        }
        
        // 构建通道标签
        if (channels.length > 0) {
            channels.forEach(ch => {
                const node = ch.node;
                const channel = ch.channel;
                devicesHtml += `<span class="group-device-tag channel">📡 节点${node} 通道${channel}</span>`;
            });
        }
        
        if (!devicesHtml) {
            devicesHtml = '<span class="group-empty-hint">暂无绑定设备或通道</span>';
        }
        
        html += `
            <div class="group-card" onclick="openEditGroupModal(${groupId})">
                <div class="group-card-header">
                    <div class="group-card-title">
                        📂 ${escapeHtml(name)}
                        <span class="group-card-id">ID: ${groupId}</span>
                    </div>
                    <div class="group-card-count">
                        ${deviceCount} 设备 / ${channelCount} 通道
                    </div>
                </div>
                <div class="group-card-body">
                    <div class="group-devices-label">绑定的设备和通道：</div>
                    <div class="group-devices-list">
                        ${devicesHtml}
                    </div>
                </div>
                <div class="group-card-actions" onclick="event.stopPropagation()">
                    <button onclick="controlGroupById(${groupId}, 'stop')">⏹️ 停止</button>
                    <button class="success" onclick="controlGroupById(${groupId}, 'fwd')">▶️ 正转</button>
                    <button class="warning" onclick="controlGroupById(${groupId}, 'rev')">◀️ 反转</button>
                    <button class="danger" onclick="deleteGroupById(${groupId})">🗑️</button>
                </div>
            </div>
        `;
    });
    
    if (contentEl) contentEl.innerHTML = html;
}

/**
 * 创建分组
 * 如果名称为空，自动生成名称
 */
function createGroup() {
    const groupId = parseInt(document.getElementById('newGroupId').value);
    let name = document.getElementById('newGroupName').value.trim();
    
    // 如果名称为空，自动生成名称（触控屏没有键盘）
    if (!name) {
        name = `分组${groupId}`;
    }
    
    callMethod('group.create', {
        groupId: groupId,
        name: name
    }, function(response) {
        if (response.result) {
            log('info', `分组 "${name}" 创建成功`);
            refreshGroupList();
            // 关闭弹窗
            closeModal('groupModal');
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
 * 
 * 说明：
 * - 设备列表和卡片视图已整合为统一的卡片视图
 * - 每个设备卡片既显示状态信息，也提供控制按钮
 * - 支持单通道控制和全部通道控制
 * - 设备在线状态由服务端判断（30秒内有CAN响应认为在线）
 * ======================================================== */

/**
 * 刷新设备列表
 * 调用 relay.nodes RPC 获取所有设备节点信息，然后渲染设备卡片
 * 
 * 关于"无法控制设备"的问题：
 * 如果控制命令发送后设备没有响应，可能的原因：
 * 1. CAN总线未打开 - 检查 can.status 返回的 opened 字段
 * 2. 设备未正确连接 - 检查CAN线路和终端电阻
 * 3. 波特率不匹配 - 检查CAN配置
 * 可以点击"CAN诊断"按钮查看详细信息
 */
function refreshDeviceList() {
    // 同时获取CAN状态用于诊断
    callMethod('can.status', {}, function(response) {
        if (response.result) {
            // 如果CAN未打开，显示警告
            const canOpened = response.result.opened === true;
            const warningPanel = document.getElementById('canWarningPanel');
            if (warningPanel) {
                warningPanel.style.display = canOpened ? 'none' : 'block';
            }
        }
    });
    
    // 获取设备列表
    callMethod('relay.nodes', {}, function(response) {
        if (response.result) {
            deviceListCache = response.result.nodes || response.result || [];
            renderDeviceCards();
        }
    });
}

/**
 * 渲染设备卡片视图（整合了列表和卡片功能）
 * 
 * 功能说明：
 * - 显示设备基本信息（节点ID、名称、在线状态）
 * - 显示每个通道的状态
 * - 提供每个通道的控制按钮（停止/正转/反转）
 * - 提供全部通道的批量控制按钮
 * - 设备在线状态由服务端根据最后通信时间判断
 */
function renderDeviceCards() {
    const container = document.getElementById('deviceCards');
    const emptyEl = document.getElementById('deviceListEmpty');
    
    // 如果没有设备数据，显示空状态提示
    if (!deviceListCache || deviceListCache.length === 0) {
        container.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    // 隐藏空状态提示
    if (emptyEl) emptyEl.style.display = 'none';
    
    // 按节点ID排序设备列表
    const sortedDevices = [...deviceListCache].sort((a, b) => {
        const idA = a.nodeId || a.node || a;
        const idB = b.nodeId || b.node || b;
        return idA - idB;
    });
    
    let html = '';
    sortedDevices.forEach(device => {
        const nodeId = device.nodeId || device.node || device;
        const name = device.name || `节点 ${nodeId}`;
        const type = device.type || 'relay';
        // 在线状态必须由服务端明确返回true才认为在线
        const online = device.online === true;
        const channels = device.channels || DEFAULT_CHANNEL_COUNT;
        // 显示上次响应时间（如果有）
        const ageMs = device.ageMs;
        const ageText = (typeof ageMs === 'number') ? formatAge(ageMs) : '';
        
        // 构建通道控制HTML
        // 每个通道显示状态和三个控制按钮（停止/正转/反转）
        let channelHtml = '';
        for (let i = 0; i < channels; i++) {
            channelHtml += `
                <div class="channel-control-item">
                    <div class="ch-info">
                        <span class="ch-label">通道 ${i}</span>
                        <span class="ch-status stop" id="ch-status-${nodeId}-${i}">--</span>
                    </div>
                    <div class="ch-buttons">
                        <button class="ch-btn stop" onclick="controlSingleChannel(${nodeId}, ${i}, 'stop')" title="停止">⏹️</button>
                        <button class="ch-btn fwd" onclick="controlSingleChannel(${nodeId}, ${i}, 'fwd')" title="正转">▶️</button>
                        <button class="ch-btn rev" onclick="controlSingleChannel(${nodeId}, ${i}, 'rev')" title="反转">◀️</button>
                    </div>
                </div>
            `;
        }
        
        html += `
            <div class="device-card" data-node-id="${nodeId}">
                <div class="device-card-header">
                    <div class="device-card-title-group">
                        <span class="device-card-title">🔌 ${escapeHtml(name)}</span>
                        <span class="device-card-subtitle">节点ID: ${nodeId} | 类型: ${escapeHtml(type)}</span>
                    </div>
                    <span class="device-card-status ${online ? 'online' : 'offline'}">
                        ${online ? '🟢 在线' : '🔴 离线'}
                    </span>
                </div>
                
                <!-- 左右布局：设备状态+通道控制 -->
                <div class="device-card-content">
                    <!-- 左侧：设备状态 -->
                    <div class="device-status-area">
                        <div class="status-title">📊 设备状态</div>
                        <div class="device-status-item">
                            <span class="label">通道数</span>
                            <span class="value">${channels}</span>
                        </div>
                        <div class="device-status-item">
                            <span class="label">状态</span>
                            <span class="value">${online ? '🟢 在线' : '🔴 离线'}</span>
                        </div>
                        <div class="device-status-item">
                            <span class="label">响应</span>
                            <span class="value">${ageText || '--'}</span>
                        </div>
                    </div>
                    
                    <!-- 右侧：通道控制 -->
                    <div class="channel-control-area">
                        <div class="control-title">🎛️ 通道控制</div>
                        <div class="channel-control-grid">${channelHtml}</div>
                    </div>
                </div>
                
                <!-- 批量操作按钮 -->
                <div class="device-card-actions">
                    <button onclick="queryDeviceStatus(${nodeId})" title="查询所有通道状态">🔍 查询状态</button>
                    <button class="success" onclick="controlDeviceAll(${nodeId}, 'fwd')" title="所有通道正转">▶️ 全部正转</button>
                    <button class="warning" onclick="controlDeviceAll(${nodeId}, 'rev')" title="所有通道反转">◀️ 全部反转</button>
                    <button class="danger" onclick="controlDeviceAll(${nodeId}, 'stop')" title="所有通道停止">⏹️ 全部停止</button>
                </div>
            </div>
        `;
    });
    
    container.innerHTML = html;
}

/**
 * 控制单个通道
 * 向指定设备的指定通道发送控制命令
 * 
 * @param {number} nodeId - 设备节点ID
 * @param {number} channel - 通道号 (0-3)
 * @param {string} action - 动作 (stop/fwd/rev)
 * 
 * 说明：
 * 这个函数通过 relay.control RPC 方法发送控制命令
 * RPC调用会将命令入队，然后通过CAN总线发送给设备
 * 如果设备无响应，请检查CAN总线状态
 */
function controlSingleChannel(nodeId, channel, action) {
    log('info', `控制设备 ${nodeId} 通道 ${channel}: ${action}`);
    callMethod('relay.control', {
        node: nodeId,
        ch: channel,
        action: action
    }, function(response) {
        // 检查响应中是否有警告信息（如CAN队列拥堵）
        if (response.result && response.result.warning) {
            log('error', `⚠️ ${response.result.warning}`);
        }
        // 控制成功后刷新状态
        if (response.result && response.result.ok) {
            // 延迟200ms后查询状态，给设备响应时间
            setTimeout(() => queryDeviceStatus(nodeId), 200);
        }
    });
}

/**
 * 查询单个设备状态
 * 向设备发送状态查询命令，获取所有通道的当前状态
 * 
 * @param {number} nodeId - 设备节点ID
 */
function queryDeviceStatus(nodeId) {
    callMethod('relay.statusAll', {
        node: nodeId
    }, function(response) {
        if (response.result && response.result.ok) {
            // 更新通道状态显示
            const channels = response.result.channels || [];
            channels.forEach(ch => {
                updateChannelStatusDisplay(nodeId, ch.ch, ch.statusByte, ch.mode);
            });
            
            // 更新设备在线状态
            const online = response.result.online === true;
            updateDeviceOnlineStatus(nodeId, online);
            
            // 如果设备离线，显示诊断信息
            if (!online && response.result.diagnostic) {
                log('info', `设备 ${nodeId} 诊断: ${response.result.diagnostic}`);
            }
        }
    });
}

/**
 * 更新通道状态显示
 * 根据服务端返回的状态更新界面上的通道状态
 * 
 * @param {number} nodeId - 设备节点ID
 * @param {number} channel - 通道号
 * @param {number} statusByte - 状态字节
 * @param {number} mode - 模式 (0=停止, 1=正转, 2=反转)
 */
function updateChannelStatusDisplay(nodeId, channel, statusByte, mode) {
    const statusEl = document.getElementById(`ch-status-${nodeId}-${channel}`);
    if (!statusEl) return;
    
    // 根据模式设置状态文本和样式
    let statusText = '--';
    let statusClass = 'stop';
    
    switch (mode) {
        case 0:
            statusText = '停止';
            statusClass = 'stop';
            break;
        case 1:
            statusText = '正转';
            statusClass = 'fwd';
            break;
        case 2:
            statusText = '反转';
            statusClass = 'rev';
            break;
    }
    
    statusEl.textContent = statusText;
    statusEl.className = 'ch-status ' + statusClass;
}

/**
 * 更新设备在线状态显示
 * 
 * @param {number} nodeId - 设备节点ID
 * @param {boolean} online - 是否在线
 */
function updateDeviceOnlineStatus(nodeId, online) {
    const card = document.querySelector(`.device-card[data-node-id="${nodeId}"]`);
    if (!card) return;
    
    const statusEl = card.querySelector('.device-card-status');
    if (statusEl) {
        statusEl.className = 'device-card-status ' + (online ? 'online' : 'offline');
        statusEl.textContent = online ? '🟢 在线' : '🔴 离线';
    }
}

/**
 * 控制设备所有通道
 * 向指定设备的所有通道发送相同的控制命令
 * 
 * @param {number} nodeId - 设备节点ID
 * @param {string} action - 动作 (stop/fwd/rev)
 * 
 * 说明：
 * 这个函数会逐个向每个通道发送控制命令
 * 如果控制无效，请检查：
 * 1. CAN总线是否已打开（点击"CAN诊断"按钮）
 * 2. 设备是否正确连接
 * 3. 节点ID是否正确
 */
function controlDeviceAll(nodeId, action) {
    log('info', `控制设备 ${nodeId} 全部通道: ${action}`);
    
    // 从缓存中获取设备的通道数量
    // 如果缓存中没有，使用默认值 DEFAULT_CHANNEL_COUNT
    let channelCount = DEFAULT_CHANNEL_COUNT;
    const device = deviceListCache.find(d => (d.nodeId || d.node || d) === nodeId);
    if (device && device.channels) {
        channelCount = device.channels;
    }
    
    // 逐个通道发送控制命令
    for (let ch = 0; ch < channelCount; ch++) {
        callMethod('relay.control', {
            node: nodeId,
            ch: ch,
            action: action
        });
    }
    
    // 延迟后查询状态
    setTimeout(() => queryDeviceStatus(nodeId), 300);
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
 * 显示所有定时策略及其状态，使用卡片视图
 */
function renderStrategyList() {
    const contentEl = document.getElementById('strategyCards');
    const emptyEl = document.getElementById('strategyCardsEmpty');
    
    if (!strategyListCache || strategyListCache.length === 0) {
        if (contentEl) contentEl.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    // 动作名称映射
    const actionNames = {
        'stop': '⏹️ 停止',
        'fwd': '▶️ 正转',
        'rev': '◀️ 反转'
    };
    
    let html = '';
    strategyListCache.forEach(strategy => {
        const id = strategy.id;
        const name = strategy.name || `策略${id}`;
        const groupId = strategy.groupId;
        const channel = strategy.channel === -1 ? '全部通道' : `通道 ${strategy.channel}`;
        const action = actionNames[strategy.action] || strategy.action;
        const intervalSec = strategy.intervalSec;
        const dailyTime = strategy.dailyTime;
        const triggerType = strategy.triggerType || (dailyTime ? 'daily' : 'interval');
        const enabled = strategy.enabled !== false;
        const running = strategy.running === true;
        
        // 构建触发时间描述
        let triggerDesc = '';
        let triggerIcon = '⏱️';
        if (triggerType === 'daily' && dailyTime) {
            triggerDesc = `每天 ${dailyTime}`;
            triggerIcon = '📅';
        } else if (intervalSec) {
            if (intervalSec >= 3600) {
                const hours = Math.floor(intervalSec / 3600);
                const mins = Math.floor((intervalSec % 3600) / 60);
                triggerDesc = `每 ${hours}小时${mins > 0 ? mins + '分钟' : ''}`;
            } else if (intervalSec >= 60) {
                const mins = Math.floor(intervalSec / 60);
                const secs = intervalSec % 60;
                triggerDesc = `每 ${mins}分钟${secs > 0 ? secs + '秒' : ''}`;
            } else {
                triggerDesc = `每 ${intervalSec}秒`;
            }
        }
        
        // 状态文本
        const statusText = enabled ? (running ? '运行中' : '已启用') : '已禁用';
        const statusClass = enabled ? (running ? 'running' : 'enabled') : 'disabled';
        const cardClass = enabled ? '' : 'disabled';
        
        html += `
            <div class="strategy-card timer ${cardClass}" onclick="openEditStrategyModal(${id})">
                <div class="strategy-card-header">
                    <div class="strategy-card-title">
                        ⏱️ ${escapeHtml(name)}
                    </div>
                    <div class="strategy-card-status ${statusClass}">
                        ${statusText}
                    </div>
                </div>
                <div class="strategy-card-body">
                    <div class="strategy-card-info">
                        <div class="strategy-info-row">
                            <span class="label">策略ID</span>
                            <span class="value">${id}</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">目标分组</span>
                            <span class="value">分组 ${groupId}</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">控制通道</span>
                            <span class="value">${channel}</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">执行动作</span>
                            <span class="value">${action}</span>
                        </div>
                    </div>
                    <div class="strategy-card-trigger">
                        <div class="strategy-trigger-label">触发方式</div>
                        <div class="strategy-trigger-value">${triggerIcon} ${triggerDesc}</div>
                    </div>
                </div>
                <div class="strategy-card-actions" onclick="event.stopPropagation()">
                    <button onclick="toggleStrategyEnabled(${id}, ${!enabled})" 
                            class="${enabled ? 'warning' : 'success'}">
                        ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                    </button>
                    <button class="secondary" onclick="triggerStrategy(${id})">
                        🎯 触发
                    </button>
                    <button class="danger" onclick="deleteStrategy(${id})">
                        🗑️ 删除
                    </button>
                </div>
            </div>
        `;
    });
    
    if (contentEl) contentEl.innerHTML = html;
}

/**
 * 渲染传感器策略列表
 * 显示所有传感器触发策略，使用卡片视图
 */
function renderSensorStrategyList() {
    const contentEl = document.getElementById('sensorStrategyCards');
    const emptyEl = document.getElementById('sensorStrategyCardsEmpty');
    
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
    
    // 条件描述映射
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
        const conditionDesc = conditionDescriptions[strategy.condition] || strategy.condition;
        const threshold = strategy.threshold;
        const groupId = strategy.groupId;
        const channel = strategy.channel >= 0 ? `通道 ${strategy.channel}` : '全部通道';
        const action = actionNames[strategy.action] || strategy.action;
        const enabled = strategy.enabled !== false;
        const cooldown = strategy.cooldownSec || 0;
        
        const statusText = enabled ? '已启用' : '已禁用';
        const statusClass = enabled ? 'enabled' : 'disabled';
        const cardClass = enabled ? '' : 'disabled';
        
        html += `
            <div class="strategy-card sensor ${cardClass}" onclick="openEditSensorStrategyModal(${id})">
                <div class="strategy-card-header">
                    <div class="strategy-card-title">
                        📡 ${escapeHtml(name)}
                    </div>
                    <div class="strategy-card-status ${statusClass}">
                        ${statusText}
                    </div>
                </div>
                <div class="strategy-card-body">
                    <div class="strategy-card-info">
                        <div class="strategy-info-row">
                            <span class="label">策略ID</span>
                            <span class="value">${id}</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">传感器</span>
                            <span class="value">${sensorType} (节点 ${sensorNode})</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">目标分组</span>
                            <span class="value">分组 ${groupId}</span>
                        </div>
                        <div class="strategy-info-row">
                            <span class="label">执行动作</span>
                            <span class="value">${channel} → ${action}</span>
                        </div>
                    </div>
                    <div class="strategy-card-trigger">
                        <div class="strategy-trigger-label">触发条件</div>
                        <div class="strategy-trigger-value">当数值 ${conditionDesc} ${threshold} 时</div>
                        ${cooldown > 0 ? `<div style="font-size: 11px; color: #666; margin-top: 4px;">冷却时间: ${cooldown}秒</div>` : ''}
                    </div>
                </div>
                <div class="strategy-card-actions" onclick="event.stopPropagation()">
                    <button onclick="toggleSensorStrategyEnabled(${id}, ${!enabled})" 
                            class="${enabled ? 'warning' : 'success'}">
                        ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                    </button>
                    <button class="danger" onclick="deleteSensorStrategy(${id})">
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
 * 自动填充策略ID
 * 从服务器获取现有策略列表，找到下一个可用的ID
 * 解决"每个分组只能添加一个策略"的问题 - 实际上是因为策略ID冲突
 */
function autoFillStrategyId() {
    callMethod('auto.strategy.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        document.getElementById('newStrategyId').value = nextId;
        log('info', `下一个可用策略ID: ${nextId}`);
    });
}

/**
 * 自动填充传感器策略ID
 */
function autoFillSensorStrategyId() {
    callMethod('auto.sensor.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        document.getElementById('sensorStrategyId').value = nextId;
        log('info', `下一个可用传感器策略ID: ${nextId}`);
    });
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
            // 成功后关闭弹窗
            closeModal('strategyModal');
        } else if (response.error) {
            log('error', `创建失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 创建定时策略并关闭弹窗（用于弹窗按钮调用）
 */
function createTimerStrategyAndClose() {
    createTimerStrategy();
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
            // 成功后关闭弹窗
            closeModal('sensorStrategyModal');
        } else if (response.error) {
            log('error', `创建失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 创建传感器策略并关闭弹窗（用于弹窗按钮调用）
 */
function createSensorStrategyAndClose() {
    createSensorStrategy();
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

/* ========================================================
 * 配置保存功能
 * 
 * 解决"Web页面修改无法保存"的问题
 * 
 * 问题原因：
 * 之前通过Web页面（如创建分组、添加设备等）做的修改只保存在服务器内存中，
 * 服务重启后会丢失。
 * 
 * 解决方案：
 * 调用config.save RPC方法将配置持久化保存到服务器的配置文件中。
 * ======================================================== */

/**
 * 保存配置到服务器
 * 将当前所有修改（设备、分组、策略等）持久化保存到配置文件
 * 
 * 使用方法：
 * 1. 在Web页面进行修改（创建分组、添加设备等）
 * 2. 点击"💾 保存配置"按钮
 * 3. 服务重启后修改依然有效
 */
function saveConfig() {
    callMethod('config.save', {}, function(response) {
        if (response.result && response.result.ok === true) {
            log('info', '✅ 配置保存成功！修改已持久化到配置文件。');
            alert('配置保存成功！\n\n您的修改已保存到服务器配置文件，服务重启后仍然有效。');
        } else if (response.result && response.result.ok === false) {
            // 处理result存在但ok为false的情况
            const msg = response.result.message || '操作未成功';
            log('error', `❌ 配置保存失败: ${msg}`);
            alert('配置保存失败！\n\n' + msg);
        } else if (response.error) {
            log('error', `❌ 配置保存失败: ${response.error.message || '未知错误'}`);
            alert('配置保存失败！\n\n' + (response.error.message || '未知错误'));
        } else {
            log('error', '❌ 配置保存失败: 未知响应格式');
            alert('配置保存失败！\n\n未知响应格式');
        }
    });
}

/**
 * 重新加载配置
 * 从服务器配置文件重新加载配置，会覆盖当前未保存的修改
 */
function reloadConfig() {
    if (!confirm('确定要重新加载配置吗？\n\n这将会覆盖当前未保存的修改。')) {
        return;
    }
    
    callMethod('config.reload', {}, function(response) {
        if (response.result && response.result.ok === true) {
            log('info', '✅ 配置已重新加载');
            // 刷新各列表
            refreshDeviceList();
            refreshGroupList();
            refreshStrategyList();
        } else if (response.result && response.result.ok === false) {
            // 处理result存在但ok为false的情况
            const msg = response.result.message || '操作未成功';
            log('error', `❌ 重新加载失败: ${msg}`);
        } else if (response.error) {
            log('error', `❌ 重新加载失败: ${response.error.message || '未知错误'}`);
        } else {
            log('error', '❌ 重新加载失败: 未知响应格式');
        }
    });
}

/**
 * 获取完整配置信息
 * 显示当前运行时的完整配置
 */
function getConfig() {
    callMethod('config.get', {}, function(response) {
        if (response.result && response.result.ok !== undefined) {
            // 验证结果包含有效数据
            log('info', '当前配置信息:\n' + JSON.stringify(response.result, null, 2));
        } else if (response.result) {
            // 结果存在但格式不确定
            log('info', '当前配置信息:\n' + JSON.stringify(response.result, null, 2));
        } else if (response.error) {
            log('error', `获取配置失败: ${response.error.message || '未知错误'}`);
        } else {
            log('error', '获取配置失败: 未知响应格式');
        }
    });
}

/**
 * 检查CAN总线状态
 * 
 * 诊断"CAN无法发送"的问题
 * 
 * 常见原因：
 * 1. CAN总线未正确连接
 * 2. 波特率设置不匹配
 * 3. 缺少120Ω终端电阻
 * 4. CAN_H/CAN_L接线问题
 * 5. CAN接口未启动（需要执行 ip link set can0 up）
 */
function checkCanStatus() {
    callMethod('can.status', {}, function(response) {
        if (response.result) {
            const result = response.result;
            // 安全获取字段值，处理可能不存在的情况
            const canInterface = result.interface || 'can0';
            const bitrate = result.bitrate || '未知';
            const opened = result.opened === true;
            const txQueueSize = typeof result.txQueueSize === 'number' ? result.txQueueSize : 0;
            
            let message = '=== CAN总线状态 ===\n';
            message += `接口: ${canInterface}\n`;
            message += `波特率: ${bitrate}\n`;
            message += `已打开: ${opened ? '✅ 是' : '❌ 否'}\n`;
            message += `发送队列: ${txQueueSize}个待发送帧\n`;
            
            if (result.diagnostic) {
                message += '\n⚠️ 诊断信息:\n' + result.diagnostic;
            }
            
            if (!opened) {
                message += '\n\n❗ CAN总线未打开，无法发送控制命令！';
                message += '\n请检查：';
                message += '\n1. CAN接口是否存在：ip link show ' + canInterface;
                message += '\n2. CAN接口是否启动：ip link set ' + canInterface + ' up';
                message += '\n3. 波特率是否正确：canconfig ' + canInterface + ' bitrate ' + bitrate;
            } else if (txQueueSize > 10) {
                message += '\n\n⚠️ 发送队列拥堵，可能原因：';
                message += '\n1. CAN总线未连接设备（无ACK）';
                message += '\n2. 波特率不匹配';
                message += '\n3. 缺少终端电阻（120Ω）';
                message += '\n4. 接线问题（CAN_H/CAN_L）';
            }
            
            log('info', message);
        } else if (response.error) {
            log('error', `获取CAN状态失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 弹窗（Modal）功能
 * 用于分组管理和策略管理的弹窗式操作
 * ======================================================== */

/**
 * 打开弹窗
 * @param {string} modalId - 弹窗ID
 */
function openModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
        modal.style.display = 'flex';
        // 添加动画效果
        setTimeout(() => {
            modal.classList.add('show');
        }, 10);
    }
}

/**
 * 关闭弹窗
 * @param {string} modalId - 弹窗ID
 */
function closeModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
        modal.classList.remove('show');
        setTimeout(() => {
            modal.style.display = 'none';
        }, 300);
    }
}

/**
 * 点击弹窗背景关闭弹窗
 * @param {Event} event - 点击事件
 * @param {string} modalId - 弹窗ID
 */
function closeModalOnBackground(event, modalId) {
    if (event.target.classList.contains('modal-overlay')) {
        closeModal(modalId);
    }
}

/**
 * 打开创建分组弹窗
 */
function openCreateGroupModal() {
    // 设置默认值
    const groupIdInput = document.getElementById('newGroupId');
    const groupNameInput = document.getElementById('newGroupName');
    
    if (groupIdInput) groupIdInput.value = 1;
    if (groupNameInput) groupNameInput.value = '';
    
    // 尝试自动填充下一个可用的分组ID（如果已连接）
    if (ws && ws.readyState === WebSocket.OPEN) {
        callMethod('group.list', {}, function(response) {
            let maxId = 0;
            if (response.result) {
                const groups = response.result.groups || response.result || [];
                groups.forEach(g => {
                    const id = g.groupId || g.id || 0;
                    if (id > maxId) maxId = id;
                });
            }
            if (groupIdInput) groupIdInput.value = maxId + 1;
        });
    }
    
    // 无论是否连接都打开弹窗
    openModal('groupModal');
}

/**
 * 打开管理设备弹窗
 */
function openManageDeviceModal() {
    openModal('manageDeviceModal');
}

/**
 * 打开创建策略弹窗
 */
function openCreateStrategyModal() {
    // 自动填充下一个可用的策略ID
    autoFillStrategyId();
    document.getElementById('newStrategyName').value = '';
    openModal('strategyModal');
}

/**
 * 打开创建传感器策略弹窗
 */
function openCreateSensorStrategyModal() {
    autoFillSensorStrategyId();
    document.getElementById('sensorStrategyName').value = '';
    openModal('sensorStrategyModal');
}

/**
 * 批量控制所有分组
 * @param {string} action - 动作 (stop/fwd/rev)
 */
function batchControlGroups(action) {
    if (!groupListCache || groupListCache.length === 0) {
        log('info', '暂无分组数据，请先刷新列表');
        return;
    }
    
    const actionNames = {
        'stop': '停止',
        'fwd': '正转',
        'rev': '反转'
    };
    
    log('info', `批量控制所有分组: ${actionNames[action] || action}`);
    
    // 对每个分组的所有通道执行操作
    groupListCache.forEach(group => {
        const groupId = group.groupId || group.id;
        // 控制所有通道（使用通道-1表示全部通道，或逐个发送）
        for (let ch = 0; ch < DEFAULT_CHANNEL_COUNT; ch++) {
            callMethod('group.control', {
                groupId: groupId,
                ch: ch,
                action: action
            });
        }
    });
}

/* ========================================================
 * 急停功能
 * 
 * 紧急停止所有设备的所有通道
 * 使用 relay.emergencyStop RPC 方法
 * ======================================================== */

/**
 * 急停 - 立即停止所有设备
 * 不需要确认，直接执行
 */
function emergencyStop() {
    log('info', '🛑 执行急停命令...');
    
    callMethod('relay.emergencyStop', {}, function(response) {
        if (response.result && response.result.ok === true) {
            const stoppedChannels = response.result.stoppedChannels || 0;
            const deviceCount = response.result.deviceCount || 0;
            const failedChannels = response.result.failedChannels || 0;
            
            log('info', `✅ 急停执行完成！已停止 ${deviceCount} 个设备的 ${stoppedChannels} 个通道`);
            
            if (failedChannels > 0) {
                log('error', `⚠️ ${failedChannels} 个通道停止失败`);
            }
            
            // 刷新设备状态
            setTimeout(refreshDeviceList, 500);
        } else if (response.error) {
            log('error', `❌ 急停执行失败: ${response.error.message || '未知错误'}`);
        } else {
            log('error', '❌ 急停执行失败: 未知响应格式');
        }
    });
}

/* ========================================================
 * 传感器管理功能
 * 
 * 传感器接口支持串口(Serial)和CAN两种通讯方式
 * ======================================================== */

/**
 * 获取传感器列表
 * @param {string} commType - 可选过滤：'serial' 或 'can'
 */
function getSensorList(commType) {
    const params = {};
    if (commType) {
        params.commType = commType;
    }
    
    callMethod('sensor.list', params, function(response) {
        if (response.result && response.result.ok) {
            const sensors = response.result.sensors || [];
            log('info', `获取到 ${sensors.length} 个传感器设备`);
            
            if (sensors.length > 0) {
                let sensorInfo = '传感器列表:\n';
                sensors.forEach(sensor => {
                    sensorInfo += `  - 节点 ${sensor.nodeId}: ${sensor.name} (${sensor.typeName}, ${sensor.commTypeName})\n`;
                });
                log('info', sensorInfo);
            }
        } else if (response.error) {
            log('error', `获取传感器列表失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 读取传感器数据
 * @param {number} nodeId - 传感器节点ID
 */
function readSensor(nodeId) {
    callMethod('sensor.read', { nodeId: nodeId }, function(response) {
        if (response.result && response.result.ok) {
            const sensor = response.result;
            let sensorInfo = `=== 传感器 ${nodeId} 信息 ===\n`;
            sensorInfo += `名称: ${sensor.name || '--'}\n`;
            sensorInfo += `类型: ${sensor.typeName || '--'}\n`;
            sensorInfo += `通信方式: ${sensor.commTypeName || '--'}\n`;
            sensorInfo += `总线: ${sensor.bus || '--'}`;
            if (sensor.params) {
                sensorInfo += `\n参数: ${JSON.stringify(sensor.params)}`;
            }
            log('info', sensorInfo);
        } else if (response.error) {
            log('error', `读取传感器失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 快速策略创建功能
 * 
 * 提供一键创建常用策略模板，对新手更友好
 * ======================================================== */

/**
 * 策略模板定义
 * 每个模板包含预设的策略参数，用户只需选择即可创建
 */
const STRATEGY_TEMPLATES = {
    // 定时通风：每小时运行5分钟
    'ventilation': {
        type: 'timer',
        name: '定时通风',
        description: '每小时自动启动通风设备',
        groupId: 1,
        channel: -1,  // 所有通道
        action: 'fwd',
        intervalSec: 3600,  // 1小时
        enabled: true,
        autoStart: true
    },
    // 定时浇水：每天早上8点
    'irrigation': {
        type: 'timer',
        name: '定时浇水',
        description: '每天早上8点定时浇水',
        groupId: 1,
        channel: 0,
        action: 'fwd',
        intervalSec: 43200,  // 12小时（备用）
        dailyTime: '08:00',
        triggerType: 'daily',
        enabled: true,
        autoStart: true
    },
    // 高温自动降温：温度超过30度启动
    'hightemp': {
        type: 'sensor',
        name: '高温自动降温',
        description: '温度超过30°C时自动启动风扇',
        sensorType: 'temperature',
        sensorNode: 1,
        condition: 'gt',
        threshold: 30,
        groupId: 1,
        channel: -1,
        action: 'fwd',
        cooldownSec: 300,
        enabled: true
    },
    // 低温保护：温度低于10度启动
    'lowtemp': {
        type: 'sensor',
        name: '低温自动保暖',
        description: '温度低于10°C时自动启动加热',
        sensorType: 'temperature',
        sensorNode: 1,
        condition: 'lt',
        threshold: 10,
        groupId: 1,
        channel: 0,
        action: 'fwd',
        cooldownSec: 300,
        enabled: true
    }
};

/**
 * 一键创建快速策略
 * @param {string} templateId - 模板ID
 */
function createQuickStrategy(templateId) {
    const template = STRATEGY_TEMPLATES[templateId];
    if (!template) {
        log('error', `未知的策略模板: ${templateId}`);
        return;
    }
    
    // 检查连接状态
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        alert('请先连接到服务器！\n\n点击左侧"📡 连接"按钮连接服务器后再创建策略。');
        showPage('connection');
        return;
    }
    
    // 确认创建
    const confirmMsg = `确定要创建"${template.name}"策略吗？\n\n${template.description}`;
    if (!confirm(confirmMsg)) {
        return;
    }
    
    log('info', `正在创建快速策略: ${template.name}...`);
    
    if (template.type === 'timer') {
        createQuickTimerStrategy(template);
    } else if (template.type === 'sensor') {
        createQuickSensorStrategy(template);
    }
}

/**
 * 创建快速定时策略
 * @param {object} template - 策略模板
 */
function createQuickTimerStrategy(template) {
    // 获取下一个可用的策略ID
    callMethod('auto.strategy.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        
        const params = {
            id: nextId,
            name: template.name,
            groupId: template.groupId,
            channel: template.channel,
            action: template.action,
            intervalSec: template.intervalSec,
            enabled: template.enabled,
            autoStart: template.autoStart
        };
        
        // 如果有每日定时设置
        if (template.triggerType === 'daily' && template.dailyTime) {
            params.triggerType = 'daily';
            params.dailyTime = template.dailyTime;
        }
        
        callMethod('auto.strategy.create', params, function(response) {
            if (response.result && response.result.ok) {
                log('info', `✅ 策略"${template.name}"创建成功！(ID: ${nextId})`);
                alert(`策略"${template.name}"创建成功！\n\n策略ID: ${nextId}\n\n提示：记得点击"💾 保存配置"将修改保存到服务器。`);
                refreshStrategyList();
            } else if (response.error) {
                log('error', `创建策略失败: ${response.error.message || '未知错误'}`);
                alert(`创建策略失败：${response.error.message || '未知错误'}`);
            }
        });
    });
}

/**
 * 创建快速传感器策略
 * @param {object} template - 策略模板
 */
function createQuickSensorStrategy(template) {
    // 获取下一个可用的策略ID
    callMethod('auto.sensor.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        
        const params = {
            id: nextId,
            name: template.name,
            sensorType: template.sensorType,
            sensorNode: template.sensorNode,
            condition: template.condition,
            threshold: template.threshold,
            groupId: template.groupId,
            channel: template.channel,
            action: template.action,
            cooldownSec: template.cooldownSec,
            enabled: template.enabled
        };
        
        callMethod('auto.sensor.create', params, function(response) {
            if (response.result && response.result.ok) {
                log('info', `✅ 传感器策略"${template.name}"创建成功！(ID: ${nextId})`);
                alert(`传感器策略"${template.name}"创建成功！\n\n策略ID: ${nextId}\n\n提示：记得点击"💾 保存配置"将修改保存到服务器。`);
                refreshStrategyList();
            } else if (response.error) {
                log('error', `创建策略失败: ${response.error.message || '未知错误'}`);
                alert(`创建策略失败：${response.error.message || '未知错误'}`);
            }
        });
    });
}

/* ========================================================
 * 简化策略向导功能
 * 
 * 提供步骤化的策略创建流程，更适合新手
 * ======================================================== */

// 向导当前选择的类型
let wizardSelectedType = null;

/**
 * 打开简化策略向导
 */
function openSimpleStrategyWizard() {
    // 检查连接状态
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        alert('请先连接到服务器！\n\n点击左侧"📡 连接"按钮连接服务器后再创建策略。');
        showPage('connection');
        return;
    }
    
    // 重置向导状态
    wizardSelectedType = null;
    document.getElementById('wizardStep1').style.display = 'block';
    document.getElementById('wizardStep2Timer').style.display = 'none';
    document.getElementById('wizardStep2Sensor').style.display = 'none';
    document.getElementById('wizardSubmitBtn').style.display = 'none';
    
    // 清除选中状态
    document.getElementById('wizardTypeTimer').style.borderColor = '#e0e0e0';
    document.getElementById('wizardTypeTimer').style.background = 'white';
    document.getElementById('wizardTypeSensor').style.borderColor = '#e0e0e0';
    document.getElementById('wizardTypeSensor').style.background = 'white';
    
    // 清空表单 - 定时策略
    document.getElementById('wizardTimerName').value = '';
    document.getElementById('wizardTimerGroupId').value = '1';
    document.getElementById('wizardTimerChannel').value = '-1';
    document.getElementById('wizardTimerAction').value = 'fwd';
    document.getElementById('wizardTimerTriggerType').value = 'interval';
    document.getElementById('wizardTimerInterval').value = '3600';
    document.getElementById('wizardTimerDailyTime').value = '08:00';
    
    // 重置触发类型输入框显示状态
    document.getElementById('wizardIntervalInputGroup').style.display = 'block';
    document.getElementById('wizardDailyTimeInputGroup').style.display = 'none';
    
    // 清空表单 - 传感器策略
    document.getElementById('wizardSensorName').value = '';
    document.getElementById('wizardSensorType').value = 'temperature';
    document.getElementById('wizardSensorNode').value = '1';
    document.getElementById('wizardSensorCondition').value = 'gt';
    document.getElementById('wizardSensorThreshold').value = '30';
    document.getElementById('wizardSensorGroupId').value = '1';
    document.getElementById('wizardSensorAction').value = 'fwd';
    
    openModal('simpleStrategyWizard');
}

/**
 * 选择向导策略类型
 * @param {string} type - 'timer' 或 'sensor'
 */
function selectWizardType(type) {
    wizardSelectedType = type;
    
    // 更新UI状态
    const timerOption = document.getElementById('wizardTypeTimer');
    const sensorOption = document.getElementById('wizardTypeSensor');
    
    if (type === 'timer') {
        timerOption.style.borderColor = '#667eea';
        timerOption.style.background = 'linear-gradient(135deg, rgba(102, 126, 234, 0.1) 0%, rgba(118, 75, 162, 0.1) 100%)';
        sensorOption.style.borderColor = '#e0e0e0';
        sensorOption.style.background = 'white';
        
        document.getElementById('wizardStep2Timer').style.display = 'block';
        document.getElementById('wizardStep2Sensor').style.display = 'none';
    } else if (type === 'sensor') {
        sensorOption.style.borderColor = '#e67e22';
        sensorOption.style.background = 'linear-gradient(135deg, rgba(230, 126, 34, 0.1) 0%, rgba(243, 156, 18, 0.1) 100%)';
        timerOption.style.borderColor = '#e0e0e0';
        timerOption.style.background = 'white';
        
        document.getElementById('wizardStep2Timer').style.display = 'none';
        document.getElementById('wizardStep2Sensor').style.display = 'block';
    }
    
    // 显示提交按钮
    document.getElementById('wizardSubmitBtn').style.display = 'inline-flex';
}

/**
 * 提交向导策略
 */
function submitWizardStrategy() {
    if (!wizardSelectedType) {
        alert('请先选择策略类型！');
        return;
    }
    
    if (wizardSelectedType === 'timer') {
        submitWizardTimerStrategy();
    } else if (wizardSelectedType === 'sensor') {
        submitWizardSensorStrategy();
    }
}

/**
 * 切换简易向导定时策略触发类型的输入框显示
 */
function toggleWizardTimerTypeInputs() {
    const triggerType = document.getElementById('wizardTimerTriggerType').value;
    const intervalGroup = document.getElementById('wizardIntervalInputGroup');
    const dailyTimeGroup = document.getElementById('wizardDailyTimeInputGroup');
    
    if (triggerType === 'interval') {
        intervalGroup.style.display = 'block';
        dailyTimeGroup.style.display = 'none';
    } else if (triggerType === 'daily') {
        intervalGroup.style.display = 'none';
        dailyTimeGroup.style.display = 'block';
    }
}

/**
 * 将时间字符串转换为秒数（从00:00开始）
 * @param {string} timeStr - 时间字符串，格式为 HH:MM
 * @returns {number} 秒数
 */
function timeToSeconds(timeStr) {
    if (!timeStr || typeof timeStr !== 'string') return 0;
    const parts = timeStr.split(':');
    if (parts.length < 2) return 0;
    const hours = parseInt(parts[0]) || 0;
    const minutes = parseInt(parts[1]) || 0;
    return hours * 3600 + minutes * 60;
}

/**
 * 提交向导定时策略
 * 支持间隔执行和每日定时两种触发方式
 */
function submitWizardTimerStrategy() {
    const name = document.getElementById('wizardTimerName').value.trim();
    const groupId = parseInt(document.getElementById('wizardTimerGroupId').value);
    const channel = parseInt(document.getElementById('wizardTimerChannel').value);
    const action = document.getElementById('wizardTimerAction').value;
    const triggerType = document.getElementById('wizardTimerTriggerType').value;
    const intervalSec = parseInt(document.getElementById('wizardTimerInterval').value);
    const dailyTime = document.getElementById('wizardTimerDailyTime').value;
    
    // 根据触发类型验证参数
    if (triggerType === 'interval') {
        if (!intervalSec || intervalSec < 1) {
            alert('请输入有效的执行间隔！');
            return;
        }
    } else if (triggerType === 'daily') {
        if (!dailyTime) {
            alert('请选择每日执行时间！');
            return;
        }
    }
    
    log('info', '正在创建定时策略...');
    
    // 获取下一个可用的策略ID并创建
    callMethod('auto.strategy.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        const strategyName = name || `定时策略_${nextId}`;
        
        const params = {
            id: nextId,
            name: strategyName,
            groupId: groupId,
            channel: channel,
            action: action,
            enabled: true,
            autoStart: true
        };
        
        // 根据触发类型设置不同的参数
        if (triggerType === 'interval') {
            params.intervalSec = intervalSec;
            params.triggerType = 'interval';
        } else if (triggerType === 'daily') {
            params.dailyTime = dailyTime;
            params.dailyTimeSec = timeToSeconds(dailyTime);
            params.triggerType = 'daily';
            // 设置一个默认间隔以防RPC验证需要
            params.intervalSec = 86400;
        }
        
        callMethod('auto.strategy.create', params, function(response) {
            if (response.result && response.result.ok) {
                const triggerDesc = triggerType === 'daily' ? 
                    `每日 ${dailyTime} 执行` : 
                    `每 ${intervalSec} 秒执行`;
                log('info', `✅ 策略"${strategyName}"创建成功！(ID: ${nextId}, ${triggerDesc})`);
                alert(`策略创建成功！\n\n名称: ${strategyName}\n策略ID: ${nextId}\n触发方式: ${triggerDesc}\n\n提示：记得点击"💾 保存配置"将修改保存到服务器。`);
                closeModal('simpleStrategyWizard');
                refreshStrategyList();
            } else if (response.error) {
                log('error', `创建策略失败: ${response.error.message || '未知错误'}`);
                alert(`创建策略失败：${response.error.message || '未知错误'}`);
            }
        });
    });
}

/**
 * 提交向导传感器策略
 */
function submitWizardSensorStrategy() {
    const name = document.getElementById('wizardSensorName').value.trim();
    const sensorType = document.getElementById('wizardSensorType').value;
    const sensorNode = parseInt(document.getElementById('wizardSensorNode').value);
    const condition = document.getElementById('wizardSensorCondition').value;
    const threshold = parseFloat(document.getElementById('wizardSensorThreshold').value);
    const groupId = parseInt(document.getElementById('wizardSensorGroupId').value);
    const action = document.getElementById('wizardSensorAction').value;
    
    log('info', '正在创建传感器策略...');
    
    // 获取下一个可用的策略ID并创建
    callMethod('auto.sensor.list', {}, function(response) {
        let maxId = 0;
        if (response.result && response.result.strategies) {
            response.result.strategies.forEach(s => {
                if (s.id > maxId) maxId = s.id;
            });
        }
        const nextId = maxId + 1;
        const strategyName = name || `传感器策略_${nextId}`;
        
        const params = {
            id: nextId,
            name: strategyName,
            sensorType: sensorType,
            sensorNode: sensorNode,
            condition: condition,
            threshold: threshold,
            groupId: groupId,
            channel: -1,  // 所有通道
            action: action,
            cooldownSec: 60,
            enabled: true
        };
        
        callMethod('auto.sensor.create', params, function(response) {
            if (response.result && response.result.ok) {
                log('info', `✅ 传感器策略"${strategyName}"创建成功！(ID: ${nextId})`);
                alert(`传感器策略创建成功！\n\n名称: ${strategyName}\n策略ID: ${nextId}\n\n提示：记得点击"💾 保存配置"将修改保存到服务器。`);
                closeModal('simpleStrategyWizard');
                refreshStrategyList();
            } else if (response.error) {
                log('error', `创建策略失败: ${response.error.message || '未知错误'}`);
                alert(`创建策略失败：${response.error.message || '未知错误'}`);
            }
        });
    });
}

/* ========================================================
 * 设备通道管理功能 - 支持按通道绑定到分组
 * ======================================================== */

/**
 * 打开管理设备通道弹窗
 */
function openManageChannelModal() {
    openModal('manageChannelModal');
}

/**
 * 添加通道到分组
 */
function addChannelToGroup() {
    const groupId = parseInt(document.getElementById('channelGroupId').value);
    const nodeId = parseInt(document.getElementById('channelNodeId').value);
    const channel = parseInt(document.getElementById('channelNumber').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    if (!nodeId || nodeId <= 0 || nodeId > 255) {
        alert('请输入有效的设备节点ID (1-255)');
        return;
    }
    
    callMethod('group.addChannel', {
        groupId: groupId,
        node: nodeId,
        channel: channel
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `通道 节点${nodeId}:通道${channel} 已添加到分组 ${groupId}`);
            viewGroupChannels();
            refreshGroupList();
        } else if (response.error) {
            log('error', `添加通道失败: ${response.error.message || '未知错误'}`);
            alert(`添加通道失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 从分组移除通道
 */
function removeChannelFromGroup() {
    const groupId = parseInt(document.getElementById('channelGroupId').value);
    const nodeId = parseInt(document.getElementById('channelNodeId').value);
    const channel = parseInt(document.getElementById('channelNumber').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    if (!nodeId || nodeId <= 0 || nodeId > 255) {
        alert('请输入有效的设备节点ID (1-255)');
        return;
    }
    
    if (confirm(`确定要从分组 ${groupId} 移除 节点${nodeId}:通道${channel} 吗？`)) {
        callMethod('group.removeChannel', {
            groupId: groupId,
            node: nodeId,
            channel: channel
        }, function(response) {
            if (response.result && response.result.ok) {
                log('info', `通道 节点${nodeId}:通道${channel} 已从分组 ${groupId} 移除`);
                viewGroupChannels();
                refreshGroupList();
            } else if (response.error) {
                log('error', `移除通道失败: ${response.error.message || '未知错误'}`);
                alert(`移除通道失败: ${response.error.message || '未知错误'}`);
            }
        });
    }
}

/**
 * 查看分组的通道列表
 */
function viewGroupChannels() {
    const groupId = parseInt(document.getElementById('channelGroupId').value);
    
    if (!groupId || groupId <= 0) {
        alert('请输入有效的分组ID');
        return;
    }
    
    callMethod('group.getChannels', { groupId: groupId }, function(response) {
        const displayEl = document.getElementById('channelListDisplay');
        const contentEl = document.getElementById('channelListContent');
        
        if (response.result && response.result.channels) {
            const channels = response.result.channels;
            if (channels.length === 0) {
                contentEl.innerHTML = '<span style="color: #999;">暂无通道</span>';
            } else {
                let html = '';
                channels.forEach(ch => {
                    html += `<span class="group-device-tag channel">📡 节点${ch.node} 通道${ch.channel}</span>`;
                });
                contentEl.innerHTML = html;
            }
            displayEl.style.display = 'block';
        } else if (response.error) {
            contentEl.innerHTML = `<span style="color: #e74c3c;">获取失败: ${response.error.message || '未知错误'}</span>`;
            displayEl.style.display = 'block';
        }
    });
}

/**
 * 打开编辑分组弹窗（点击分组卡片时调用）
 */
function openEditGroupModal(groupId) {
    // 设置分组ID并打开管理设备通道弹窗
    document.getElementById('channelGroupId').value = groupId;
    openManageChannelModal();
    // 自动加载该分组的通道列表
    setTimeout(() => viewGroupChannels(), 100);
}

/**
 * 打开编辑策略弹窗（点击策略卡片时调用）
 */
function openEditStrategyModal(strategyId) {
    // 目前简单实现：显示策略信息
    const strategy = strategyListCache.find(s => s.id === strategyId);
    if (strategy) {
        log('info', `查看策略 ${strategyId}: ${JSON.stringify(strategy, null, 2)}`);
        alert(`策略详情:\n\nID: ${strategy.id}\n名称: ${strategy.name || '未命名'}\n分组: ${strategy.groupId}\n通道: ${strategy.channel === -1 ? '全部' : strategy.channel}\n动作: ${strategy.action}\n间隔: ${strategy.intervalSec}秒\n状态: ${strategy.enabled ? '启用' : '禁用'}`);
    }
}

/**
 * 打开编辑传感器策略弹窗（点击传感器策略卡片时调用）
 */
function openEditSensorStrategyModal(strategyId) {
    // 目前简单实现：显示策略信息
    const strategy = sensorStrategyListCache.find(s => s.id === strategyId);
    if (strategy) {
        log('info', `查看传感器策略 ${strategyId}: ${JSON.stringify(strategy, null, 2)}`);
        alert(`传感器策略详情:\n\nID: ${strategy.id}\n名称: ${strategy.name || '未命名'}\n传感器类型: ${strategy.sensorType}\n传感器节点: ${strategy.sensorNode}\n条件: ${strategy.condition} ${strategy.threshold}\n分组: ${strategy.groupId}\n动作: ${strategy.action}\n状态: ${strategy.enabled ? '启用' : '禁用'}`);
    }
}
