/**
 * 泛舟RPC服务器 - 蓝图编辑器模块
 * Blueprint Editor Module for FanZhou RPC Server
 * 
 * 功能说明：
 * 1. 可视化节点编辑器 - 拖放创建策略流程
 * 2. 节点连线功能 - 连接触发器和执行器
 * 3. 系统框图展示 - 显示系统运行状态
 * 4. 分组结构可视化 - 显示设备分组关系
 * 5. 一键生成策略 - 从蓝图生成RPC策略配置
 * 
 * 节点类型：
 * - trigger-time: 定时触发器（定时策略）
 * - trigger-sensor: 传感器触发器（温度、湿度等）
 * - action-relay: 继电器控制动作
 * - action-group: 分组控制动作
 * - device: 设备节点
 * - group: 分组节点
 * 
 * 策略类型说明：
 * - timer: 定时策略，绑定到分组，按间隔时间触发分组控制
 * - timer-relay: 定时策略，直接控制单个继电器（使用独立的RPC接口）
 * - sensor: 传感器触发策略，根据传感器数值条件触发分组控制
 * - sensor-relay: 传感器触发策略，直接控制单个继电器（使用独立的RPC接口）
 * 
 * 依赖说明：
 * 此模块依赖于 app.js 中定义的以下函数：
 * - callMethod(method, params, callback): 发送RPC调用
 * - log(type, message): 记录日志到通信日志面板
 * 请确保 app.js 在此模块之前加载
 */

/* ========================================================
 * 全局变量定义 - 蓝图编辑器状态
 * ======================================================== */

// 蓝图画布实例
let blueprintCanvas = null;

// 所有节点列表
let blueprintNodes = [];

// 所有连线列表
let blueprintConnections = [];

// 当前选中的节点
let selectedNode = null;

// 当前拖动状态
let dragState = {
    isDragging: false,      // 是否正在拖动
    node: null,             // 当前拖动的节点
    offsetX: 0,             // 鼠标相对节点的X偏移
    offsetY: 0,             // 鼠标相对节点的Y偏移
    isConnecting: false,    // 是否正在连线
    sourceNode: null,       // 连线起始节点
    sourcePort: null        // 连线起始端口
};

// 节点ID计数器
let nodeIdCounter = 1;

// 画布偏移（用于拖拽画布）
let canvasOffset = { x: 0, y: 0 };

// 缩放比例
let canvasScale = 1;

/* ========================================================
 * 节点类型定义 - 定义所有可用的节点类型
 * ======================================================== */

/**
 * 节点类型配置
 * 每种节点类型包含：名称、图标、颜色、端口配置等
 */
const NODE_TYPES = {
    // ===== 触发器类型 =====
    'trigger-time': {
        name: '定时触发器',
        icon: '⏰',
        color: '#3498db',
        category: 'trigger',
        description: '按照设定的时间间隔或每日固定时间触发',
        inputs: [],  // 触发器没有输入
        outputs: ['trigger'],  // 输出触发信号
        config: {
            triggerMode: {
                type: 'select',
                label: '触发模式',
                default: 'interval',
                options: [
                    { value: 'interval', label: '⏱️ 间隔执行' },
                    { value: 'daily', label: '📅 每日定时' }
                ]
            },
            intervalSec: { type: 'number', label: '间隔(秒)', default: 60, min: 1 },
            dailyTime: { type: 'time', label: '每日时间', default: '08:00' },
            autoStart: { type: 'boolean', label: '自动启动', default: true }
        }
    },
    'trigger-sensor': {
        name: '传感器触发器',
        icon: '📡',
        color: '#e67e22',
        category: 'trigger',
        description: '当传感器数值满足条件时触发',
        inputs: [],
        outputs: ['trigger'],
        config: {
            sensorType: { 
                type: 'select', 
                label: '传感器类型', 
                default: 'temperature',
                options: [
                    { value: 'temperature', label: '🌡️ 温度' },
                    { value: 'humidity', label: '💧 湿度' },
                    { value: 'light', label: '💡 光照' },
                    { value: 'pressure', label: '📊 压力' },
                    { value: 'soil_moisture', label: '🌱 土壤湿度' },
                    { value: 'co2', label: '🌫️ CO2' }
                ]
            },
            sensorNode: { type: 'number', label: '传感器节点ID', default: 1, min: 1, max: 255 },
            condition: { 
                type: 'select', 
                label: '条件', 
                default: 'gt',
                options: [
                    { value: 'gt', label: '> 大于' },
                    { value: 'lt', label: '< 小于' },
                    { value: 'eq', label: '= 等于' },
                    { value: 'gte', label: '>= 大于等于' },
                    { value: 'lte', label: '<= 小于等于' }
                ]
            },
            threshold: { type: 'number', label: '阈值', default: 25, step: 0.1 },
            cooldownSec: { type: 'number', label: '冷却时间(秒)', default: 60, min: 0 }
        }
    },
    
    // ===== 动作类型 =====
    'action-relay': {
        name: '继电器控制',
        icon: '🎛️',
        color: '#27ae60',
        category: 'action',
        description: '控制单个继电器通道',
        inputs: ['trigger'],  // 接收触发信号
        outputs: [],
        config: {
            nodeId: { type: 'number', label: '设备节点ID', default: 1, min: 1, max: 255 },
            channel: { 
                type: 'select', 
                label: '通道', 
                default: 0,
                options: [
                    { value: 0, label: '通道 0' },
                    { value: 1, label: '通道 1' },
                    { value: 2, label: '通道 2' },
                    { value: 3, label: '通道 3' }
                ]
            },
            action: { 
                type: 'select', 
                label: '动作', 
                default: 'stop',
                options: [
                    { value: 'stop', label: '⏹️ 停止' },
                    { value: 'fwd', label: '▶️ 正转' },
                    { value: 'rev', label: '◀️ 反转' }
                ]
            }
        }
    },
    'action-group': {
        name: '分组控制',
        icon: '📦',
        color: '#9b59b6',
        category: 'action',
        description: '控制整个分组的设备',
        inputs: ['trigger'],
        outputs: [],
        config: {
            groupId: { type: 'number', label: '分组ID', default: 1, min: 1 },
            channel: { 
                type: 'select', 
                label: '通道', 
                default: 0,
                options: [
                    { value: -1, label: '所有通道' },
                    { value: 0, label: '通道 0' },
                    { value: 1, label: '通道 1' },
                    { value: 2, label: '通道 2' },
                    { value: 3, label: '通道 3' }
                ]
            },
            action: { 
                type: 'select', 
                label: '动作', 
                default: 'stop',
                options: [
                    { value: 'stop', label: '⏹️ 停止' },
                    { value: 'fwd', label: '▶️ 正转' },
                    { value: 'rev', label: '◀️ 反转' }
                ]
            }
        }
    },
    
    // ===== 设备节点（用于框图展示）=====
    'device': {
        name: '设备节点',
        icon: '🔌',
        color: '#34495e',
        category: 'display',
        description: '表示一个物理设备',
        inputs: [],
        outputs: [],
        config: {
            nodeId: { type: 'number', label: '节点ID', default: 1 },
            name: { type: 'text', label: '设备名称', default: '设备' }
        }
    },
    'group': {
        name: '分组节点',
        icon: '📂',
        color: '#16a085',
        category: 'display',
        description: '表示一个设备分组',
        inputs: [],
        outputs: [],
        config: {
            groupId: { type: 'number', label: '分组ID', default: 1 },
            name: { type: 'text', label: '分组名称', default: '分组' }
        }
    }
};

/* ========================================================
 * 蓝图编辑器初始化
 * ======================================================== */

/**
 * 初始化蓝图编辑器
 * 设置画布、事件监听器等
 */
function initBlueprintEditor() {
    blueprintCanvas = document.getElementById('blueprintCanvas');
    if (!blueprintCanvas) {
        console.error('蓝图画布元素未找到');
        return;
    }
    
    // 设置画布事件监听
    setupCanvasEvents();
    
    // 设置节点面板拖放
    setupNodePalette();
    
    // 初始化SVG连线层
    initConnectionLayer();
    
    // 设置键盘快捷键
    setupKeyboardShortcuts();
    
    console.log('蓝图编辑器初始化完成');
}

/**
 * 设置键盘快捷键
 * Delete: 删除选中的节点
 * Ctrl+D: 复制选中的节点
 * Escape: 取消选择
 */
function setupKeyboardShortcuts() {
    document.addEventListener('keydown', function(e) {
        // 如果正在编辑表单，忽略快捷键
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') {
            return;
        }
        
        // Delete 键删除选中的节点
        if (e.key === 'Delete' && selectedNode) {
            if (confirm('确定要删除选中的节点吗？')) {
                deleteNode(selectedNode.id);
            }
        }
        
        // Ctrl+D 复制选中的节点
        if ((e.ctrlKey || e.metaKey) && e.key === 'd' && selectedNode) {
            e.preventDefault();
            duplicateSelectedNode();
        }
        
        // Escape 取消选择
        if (e.key === 'Escape') {
            deselectAllNodes();
        }
    });
}

/**
 * 复制选中的节点
 */
function duplicateSelectedNode() {
    if (!selectedNode) return;
    
    // 创建新节点，位置略微偏移
    const newNode = createNode(
        selectedNode.type,
        selectedNode.x + 30,
        selectedNode.y + 30
    );
    
    // 复制配置
    if (newNode && selectedNode.config) {
        newNode.config = JSON.parse(JSON.stringify(selectedNode.config));
        updateNodeContent(newNode);
    }
    
    // 选中新节点
    selectNode(newNode);
    
    if (typeof log === 'function') {
        log('info', `节点已复制: ${newNode.id}`);
    }
}

/**
 * 初始化连线层（SVG）
 * 用于绘制节点之间的连线
 */
function initConnectionLayer() {
    // 检查是否已存在SVG层
    let svg = blueprintCanvas.querySelector('.connection-layer');
    if (!svg) {
        svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('class', 'connection-layer');
        svg.style.position = 'absolute';
        svg.style.top = '0';
        svg.style.left = '0';
        svg.style.width = '100%';
        svg.style.height = '100%';
        svg.style.pointerEvents = 'none';
        svg.style.zIndex = '1';
        blueprintCanvas.insertBefore(svg, blueprintCanvas.firstChild);
    }
}

/**
 * 设置画布事件监听
 */
function setupCanvasEvents() {
    // 鼠标移动事件 - 处理拖动
    blueprintCanvas.addEventListener('mousemove', handleCanvasMouseMove);
    
    // 鼠标释放事件 - 结束拖动
    blueprintCanvas.addEventListener('mouseup', handleCanvasMouseUp);
    
    // 鼠标离开画布
    blueprintCanvas.addEventListener('mouseleave', handleCanvasMouseUp);
    
    // 点击画布空白处 - 取消选择
    blueprintCanvas.addEventListener('click', function(e) {
        if (e.target === blueprintCanvas || e.target.classList.contains('connection-layer')) {
            deselectAllNodes();
        }
    });
    
    // 支持拖放从节点面板
    blueprintCanvas.addEventListener('dragover', function(e) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'copy';
    });
    
    blueprintCanvas.addEventListener('drop', handleCanvasDrop);
}

/**
 * 设置节点面板拖放
 */
function setupNodePalette() {
    const paletteItems = document.querySelectorAll('.palette-item');
    paletteItems.forEach(item => {
        item.addEventListener('dragstart', function(e) {
            e.dataTransfer.setData('nodeType', this.dataset.nodeType);
            e.dataTransfer.effectAllowed = 'copy';
        });
    });
}

/* ========================================================
 * 节点创建和管理
 * ======================================================== */

/**
 * 创建新节点
 * @param {string} type - 节点类型
 * @param {number} x - X坐标
 * @param {number} y - Y坐标
 * @returns {object} 创建的节点对象
 */
function createNode(type, x, y) {
    const nodeType = NODE_TYPES[type];
    if (!nodeType) {
        console.error('未知节点类型:', type);
        return null;
    }
    
    // 创建节点数据对象
    const node = {
        id: 'node_' + (nodeIdCounter++),
        type: type,
        x: x,
        y: y,
        config: {}  // 节点配置参数
    };
    
    // 初始化默认配置值
    if (nodeType.config) {
        for (const [key, cfg] of Object.entries(nodeType.config)) {
            node.config[key] = cfg.default;
        }
    }
    
    // 添加到节点列表
    blueprintNodes.push(node);
    
    // 渲染节点DOM元素
    renderNode(node);
    
    // 选中新创建的节点
    selectNode(node);
    
    // 隐藏空状态提示
    updateEmptyHint();
    
    return node;
}

/**
 * 渲染节点DOM元素
 * @param {object} node - 节点数据对象
 */
function renderNode(node) {
    const nodeType = NODE_TYPES[node.type];
    
    // 创建节点容器
    const nodeEl = document.createElement('div');
    nodeEl.className = 'blueprint-node';
    nodeEl.id = node.id;
    nodeEl.style.left = node.x + 'px';
    nodeEl.style.top = node.y + 'px';
    nodeEl.style.borderColor = nodeType.color;
    nodeEl.dataset.nodeType = node.type;
    
    // 节点头部（图标和标题）
    const header = document.createElement('div');
    header.className = 'node-header';
    header.style.background = nodeType.color;
    header.innerHTML = `
        <span class="node-icon">${nodeType.icon}</span>
        <span class="node-title">${nodeType.name}</span>
        <button class="node-delete" onclick="deleteNode('${node.id}')" title="删除节点">×</button>
    `;
    nodeEl.appendChild(header);
    
    // 节点内容（显示配置摘要）
    const content = document.createElement('div');
    content.className = 'node-content';
    content.id = node.id + '_content';
    updateNodeContent(node, content);
    nodeEl.appendChild(content);
    
    // 输入端口
    if (nodeType.inputs && nodeType.inputs.length > 0) {
        const inputPorts = document.createElement('div');
        inputPorts.className = 'node-ports node-inputs';
        nodeType.inputs.forEach((portName, index) => {
            const port = document.createElement('div');
            port.className = 'node-port input-port';
            port.dataset.portName = portName;
            port.dataset.nodeId = node.id;
            port.dataset.portType = 'input';
            port.title = '输入: ' + portName;
            inputPorts.appendChild(port);
        });
        nodeEl.appendChild(inputPorts);
    }
    
    // 输出端口
    if (nodeType.outputs && nodeType.outputs.length > 0) {
        const outputPorts = document.createElement('div');
        outputPorts.className = 'node-ports node-outputs';
        nodeType.outputs.forEach((portName, index) => {
            const port = document.createElement('div');
            port.className = 'node-port output-port';
            port.dataset.portName = portName;
            port.dataset.nodeId = node.id;
            port.dataset.portType = 'output';
            port.title = '输出: ' + portName;
            
            // 添加连线事件
            port.addEventListener('mousedown', startConnection);
            outputPorts.appendChild(port);
        });
        nodeEl.appendChild(outputPorts);
    }
    
    // 节点拖动事件
    header.addEventListener('mousedown', function(e) {
        if (e.target.classList.contains('node-delete')) return;
        startDragNode(e, node);
    });
    
    // 点击选中节点
    nodeEl.addEventListener('click', function(e) {
        e.stopPropagation();
        selectNode(node);
    });
    
    // 双击编辑节点
    nodeEl.addEventListener('dblclick', function(e) {
        e.stopPropagation();
        editNodeConfig(node);
    });
    
    // 添加到画布
    blueprintCanvas.appendChild(nodeEl);
}

/**
 * 更新节点内容显示
 * @param {object} node - 节点对象
 * @param {HTMLElement} contentEl - 内容元素（可选）
 */
function updateNodeContent(node, contentEl) {
    const nodeType = NODE_TYPES[node.type];
    contentEl = contentEl || document.getElementById(node.id + '_content');
    if (!contentEl) return;
    
    let html = '';
    
    // 根据节点类型显示不同的摘要信息
    switch (node.type) {
        case 'trigger-time':
            const triggerMode = node.config.triggerMode || 'interval';
            if (triggerMode === 'daily') {
                html = `<div class="config-summary">📅 每天 <strong>${node.config.dailyTime || '08:00'}</strong> 执行</div>`;
            } else {
                html = `<div class="config-summary">⏱️ 每 <strong>${node.config.intervalSec || 60}</strong> 秒触发</div>`;
            }
            break;
        case 'trigger-sensor':
            const conditionLabels = { 'gt': '>', 'lt': '<', 'eq': '=', 'gte': '>=', 'lte': '<=' };
            html = `<div class="config-summary">${node.config.sensorType || 'temperature'} ${conditionLabels[node.config.condition] || '>'} ${node.config.threshold || 0}</div>`;
            break;
        case 'action-relay':
            const actionLabels = { 'stop': '停止', 'fwd': '正转', 'rev': '反转' };
            html = `<div class="config-summary">节点${node.config.nodeId || 1} 通道${node.config.channel || 0} → ${actionLabels[node.config.action] || '停止'}</div>`;
            break;
        case 'action-group':
            const actionLabels2 = { 'stop': '停止', 'fwd': '正转', 'rev': '反转' };
            const chText = node.config.channel === -1 ? '全部' : node.config.channel;
            html = `<div class="config-summary">分组${node.config.groupId || 1} 通道${chText} → ${actionLabels2[node.config.action] || '停止'}</div>`;
            break;
        case 'device':
            html = `<div class="config-summary">ID: ${node.config.nodeId || 1}<br>${node.config.name || '设备'}</div>`;
            break;
        case 'group':
            html = `<div class="config-summary">ID: ${node.config.groupId || 1}<br>${node.config.name || '分组'}</div>`;
            break;
        default:
            html = `<div class="config-summary">${nodeType.description || ''}</div>`;
    }
    
    contentEl.innerHTML = html;
}

/**
 * 删除节点
 * @param {string} nodeId - 节点ID
 */
function deleteNode(nodeId) {
    // 获取节点信息用于确认提示
    const node = blueprintNodes.find(n => n.id === nodeId);
    const nodeType = node ? NODE_TYPES[node.type] : null;
    const nodeName = nodeType ? nodeType.name : '节点';
    
    // 检查是否有连线
    const hasConnections = blueprintConnections.some(conn => 
        conn.sourceId === nodeId || conn.targetId === nodeId
    );
    
    // 构建确认消息
    let confirmMsg = `确定要删除 "${nodeName}" 吗？`;
    if (hasConnections) {
        confirmMsg += '\n\n⚠️ 此节点有连接的连线，删除后连线也会被移除。';
    }
    
    if (!confirm(confirmMsg)) {
        return;
    }
    
    // 从数组中移除
    const index = blueprintNodes.findIndex(n => n.id === nodeId);
    if (index !== -1) {
        blueprintNodes.splice(index, 1);
    }
    
    // 移除相关连线
    blueprintConnections = blueprintConnections.filter(conn => {
        if (conn.sourceId === nodeId || conn.targetId === nodeId) {
            // 移除连线SVG元素
            const lineEl = document.getElementById(conn.id);
            if (lineEl) lineEl.remove();
            return false;
        }
        return true;
    });
    
    // 移除DOM元素
    const nodeEl = document.getElementById(nodeId);
    if (nodeEl) {
        nodeEl.remove();
    }
    
    // 如果是选中的节点，清除选择
    if (selectedNode && selectedNode.id === nodeId) {
        selectedNode = null;
        hideNodeProperties();
    }
    
    logBlueprint('info', `节点 "${nodeName}" 已删除`);
    
    // 更新空状态提示
    updateEmptyHint();
}

/**
 * 更新空状态提示的显示
 * 当画布上没有节点时显示提示
 */
function updateEmptyHint() {
    const hint = document.getElementById('canvasEmptyHint');
    if (!hint) {
        // 蓝图画布可能尚未初始化，这是正常情况
        if (blueprintCanvas) {
            console.warn('updateEmptyHint: 未找到空状态提示元素 canvasEmptyHint');
        }
        return;
    }
    hint.style.display = blueprintNodes.length === 0 ? 'block' : 'none';
}

/**
 * 选中节点
 * @param {object} node - 节点对象
 */
function selectNode(node) {
    deselectAllNodes();
    selectedNode = node;
    
    const nodeEl = document.getElementById(node.id);
    if (nodeEl) {
        nodeEl.classList.add('selected');
    }
    
    // 显示节点属性面板
    showNodeProperties(node);
}

/**
 * 取消所有节点选择
 */
function deselectAllNodes() {
    document.querySelectorAll('.blueprint-node.selected').forEach(el => {
        el.classList.remove('selected');
    });
    selectedNode = null;
    hideNodeProperties();
}

/* ========================================================
 * 节点拖动功能
 * ======================================================== */

// 网格对齐大小（像素）
const GRID_SIZE = 20;

/**
 * 将坐标对齐到网格
 * @param {number} value - 原始坐标值
 * @param {boolean} snapToGrid - 是否对齐到网格
 * @returns {number} 对齐后的坐标
 */
function snapToGrid(value, enableSnap = true) {
    if (!enableSnap) return value;
    return Math.round(value / GRID_SIZE) * GRID_SIZE;
}

/**
 * 开始拖动节点
 * @param {MouseEvent} e - 鼠标事件
 * @param {object} node - 节点对象
 */
function startDragNode(e, node) {
    e.preventDefault();
    
    const nodeEl = document.getElementById(node.id);
    const rect = nodeEl.getBoundingClientRect();
    
    dragState.isDragging = true;
    dragState.node = node;
    dragState.offsetX = e.clientX - rect.left;
    dragState.offsetY = e.clientY - rect.top;
    
    nodeEl.classList.add('dragging');
    
    // 将节点提升到最上层
    nodeEl.style.zIndex = 100;
}

/**
 * 处理画布鼠标移动
 * @param {MouseEvent} e - 鼠标事件
 */
function handleCanvasMouseMove(e) {
    if (dragState.isDragging && dragState.node) {
        const canvasRect = blueprintCanvas.getBoundingClientRect();
        let x = e.clientX - canvasRect.left - dragState.offsetX;
        let y = e.clientY - canvasRect.top - dragState.offsetY;
        
        // 保持在画布边界内
        x = Math.max(0, Math.min(x, canvasRect.width - 100));
        y = Math.max(0, Math.min(y, canvasRect.height - 50));
        
        // 对齐到网格（按住Shift键禁用对齐）
        const enableSnap = !e.shiftKey;
        dragState.node.x = snapToGrid(x, enableSnap);
        dragState.node.y = snapToGrid(y, enableSnap);
        
        const nodeEl = document.getElementById(dragState.node.id);
        if (nodeEl) {
            nodeEl.style.left = dragState.node.x + 'px';
            nodeEl.style.top = dragState.node.y + 'px';
        }
        
        // 更新相关连线
        updateConnectionsForNode(dragState.node.id);
    }
    
    // 处理连线拖动
    if (dragState.isConnecting) {
        updateTempConnection(e);
    }
}

/**
 * 处理画布鼠标释放
 * @param {MouseEvent} e - 鼠标事件
 */
function handleCanvasMouseUp(e) {
    if (dragState.isDragging && dragState.node) {
        const nodeEl = document.getElementById(dragState.node.id);
        if (nodeEl) {
            nodeEl.classList.remove('dragging');
            // 恢复正常 z-index
            nodeEl.style.zIndex = '';
        }
    }
    
    // 结束连线
    if (dragState.isConnecting) {
        finishConnection(e);
    }
    
    dragState.isDragging = false;
    dragState.node = null;
    dragState.isConnecting = false;
    dragState.sourceNode = null;
    dragState.sourcePort = null;
    
    // 移除临时连线
    const tempLine = document.getElementById('temp-connection');
    if (tempLine) tempLine.remove();
}

/**
 * 处理画布拖放
 * @param {DragEvent} e - 拖放事件
 */
function handleCanvasDrop(e) {
    e.preventDefault();
    
    const nodeType = e.dataTransfer.getData('nodeType');
    if (!nodeType) return;
    
    const canvasRect = blueprintCanvas.getBoundingClientRect();
    let x = e.clientX - canvasRect.left - 75;  // 居中调整
    let y = e.clientY - canvasRect.top - 30;
    
    // 对齐到网格
    x = snapToGrid(Math.max(0, x));
    y = snapToGrid(Math.max(0, y));
    
    createNode(nodeType, x, y);
}

/* ========================================================
 * 连线功能
 * ======================================================== */

/**
 * 开始连线
 * @param {MouseEvent} e - 鼠标事件
 */
function startConnection(e) {
    e.stopPropagation();
    e.preventDefault();
    
    const port = e.target;
    const nodeId = port.dataset.nodeId;
    const portName = port.dataset.portName;
    
    dragState.isConnecting = true;
    dragState.sourceNode = nodeId;
    dragState.sourcePort = portName;
    
    // 创建临时连线
    createTempConnection(port);
}

/**
 * 创建临时连线（用于拖动时显示）
 * @param {HTMLElement} port - 起始端口元素
 */
function createTempConnection(port) {
    const svg = blueprintCanvas.querySelector('.connection-layer');
    if (!svg) return;
    
    const line = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    line.setAttribute('id', 'temp-connection');
    line.setAttribute('stroke', '#667eea');
    line.setAttribute('stroke-width', '3');
    line.setAttribute('fill', 'none');
    line.setAttribute('stroke-dasharray', '5,5');
    line.style.pointerEvents = 'none';
    
    svg.appendChild(line);
}

/**
 * 更新临时连线位置
 * @param {MouseEvent} e - 鼠标事件
 */
function updateTempConnection(e) {
    const line = document.getElementById('temp-connection');
    if (!line) return;
    
    const sourcePort = document.querySelector(`.output-port[data-node-id="${dragState.sourceNode}"]`);
    if (!sourcePort) return;
    
    const canvasRect = blueprintCanvas.getBoundingClientRect();
    const portRect = sourcePort.getBoundingClientRect();
    
    const x1 = portRect.left + portRect.width / 2 - canvasRect.left;
    const y1 = portRect.top + portRect.height / 2 - canvasRect.top;
    const x2 = e.clientX - canvasRect.left;
    const y2 = e.clientY - canvasRect.top;
    
    // 使用贝塞尔曲线
    const midX = (x1 + x2) / 2;
    const path = `M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`;
    line.setAttribute('d', path);
}

/**
 * 完成连线
 * @param {MouseEvent} e - 鼠标事件
 */
function finishConnection(e) {
    const targetPort = e.target;
    
    // 检查是否放在有效的输入端口上
    if (!targetPort.classList.contains('input-port')) {
        return;
    }
    
    const targetNodeId = targetPort.dataset.nodeId;
    const targetPortName = targetPort.dataset.portName;
    
    // 不能连接到自己
    if (targetNodeId === dragState.sourceNode) {
        return;
    }
    
    // 检查是否已存在相同连线
    const exists = blueprintConnections.some(conn => 
        conn.sourceId === dragState.sourceNode && 
        conn.targetId === targetNodeId
    );
    
    if (exists) {
        logBlueprint('warning', '连线已存在');
        return;
    }
    
    // 创建连线
    const connection = {
        id: 'conn_' + Date.now(),
        sourceId: dragState.sourceNode,
        sourcePort: dragState.sourcePort,
        targetId: targetNodeId,
        targetPort: targetPortName
    };
    
    blueprintConnections.push(connection);
    renderConnection(connection);
    
    logBlueprint('info', `已连接: ${dragState.sourceNode} → ${targetNodeId}`);
}

/**
 * 渲染连线
 * @param {object} connection - 连线对象
 */
function renderConnection(connection) {
    const svg = blueprintCanvas.querySelector('.connection-layer');
    if (!svg) return;
    
    const sourcePort = document.querySelector(`.output-port[data-node-id="${connection.sourceId}"]`);
    const targetPort = document.querySelector(`.input-port[data-node-id="${connection.targetId}"]`);
    
    if (!sourcePort || !targetPort) return;
    
    const canvasRect = blueprintCanvas.getBoundingClientRect();
    const sourceRect = sourcePort.getBoundingClientRect();
    const targetRect = targetPort.getBoundingClientRect();
    
    const x1 = sourceRect.left + sourceRect.width / 2 - canvasRect.left;
    const y1 = sourceRect.top + sourceRect.height / 2 - canvasRect.top;
    const x2 = targetRect.left + targetRect.width / 2 - canvasRect.left;
    const y2 = targetRect.top + targetRect.height / 2 - canvasRect.top;
    
    // 贝塞尔曲线路径
    const midX = (x1 + x2) / 2;
    const path = `M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`;
    
    const line = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    line.setAttribute('id', connection.id);
    line.setAttribute('class', 'connection-line');
    line.setAttribute('d', path);
    line.setAttribute('stroke', '#667eea');
    line.setAttribute('stroke-width', '3');
    line.setAttribute('fill', 'none');
    line.style.pointerEvents = 'stroke';
    line.style.cursor = 'pointer';
    
    // 点击删除连线
    line.addEventListener('click', function(e) {
        e.stopPropagation();
        if (confirm('删除此连线？')) {
            deleteConnection(connection.id);
        }
    });
    
    svg.appendChild(line);
}

/**
 * 更新与指定节点相关的所有连线
 * @param {string} nodeId - 节点ID
 */
function updateConnectionsForNode(nodeId) {
    blueprintConnections.forEach(conn => {
        if (conn.sourceId === nodeId || conn.targetId === nodeId) {
            const lineEl = document.getElementById(conn.id);
            if (lineEl) {
                lineEl.remove();
            }
            renderConnection(conn);
        }
    });
}

/**
 * 删除连线
 * @param {string} connectionId - 连线ID
 */
function deleteConnection(connectionId) {
    const index = blueprintConnections.findIndex(c => c.id === connectionId);
    if (index !== -1) {
        blueprintConnections.splice(index, 1);
    }
    
    const lineEl = document.getElementById(connectionId);
    if (lineEl) {
        lineEl.remove();
    }
    
    logBlueprint('info', '连线已删除');
}

/* ========================================================
 * 节点属性面板
 * ======================================================== */

/**
 * 显示节点属性面板
 * @param {object} node - 节点对象
 */
function showNodeProperties(node) {
    const panel = document.getElementById('nodePropertiesPanel');
    if (!panel) return;
    
    const nodeType = NODE_TYPES[node.type];
    
    let html = `
        <div class="properties-header">
            <span class="prop-icon">${nodeType.icon}</span>
            <span class="prop-title">${nodeType.name}</span>
        </div>
        <div class="properties-content">
            <p class="prop-description">${nodeType.description}</p>
            <div class="prop-node-id" style="font-size: 11px; color: #999; margin-bottom: 10px;">
                节点ID: ${node.id}
            </div>
    `;
    
    // 生成配置表单
    if (nodeType.config) {
        html += '<div class="prop-form">';
        for (const [key, cfg] of Object.entries(nodeType.config)) {
            html += generateConfigField(node.id, key, cfg, node.config[key]);
        }
        html += '</div>';
    }
    
    // 如果是从服务器加载的策略节点，添加策略管理按钮
    if (node.strategyId) {
        html += getStrategyManagementButtons(node);
    }
    
    html += `
        </div>
        <div class="properties-actions" style="flex-direction: column; gap: 8px;">
            <div style="display: flex; gap: 8px;">
                <button class="success" onclick="applyNodeConfig('${node.id}')" title="保存修改到节点">✓ 应用</button>
                <button class="secondary" onclick="copyNode('${node.id}')" title="复制此节点">📋 复制</button>
            </div>
            <button class="danger" onclick="deleteNode('${node.id}')" title="删除此节点" style="width: 100%;">🗑️ 删除节点</button>
        </div>
    `;
    
    panel.innerHTML = html;
    panel.style.display = 'block';
}

/**
 * 复制节点
 * 在原节点附近创建一个相同配置的新节点
 * @param {string} nodeId - 要复制的节点ID
 */
function copyNode(nodeId) {
    const sourceNode = blueprintNodes.find(n => n.id === nodeId);
    if (!sourceNode) {
        logBlueprint('error', '未找到要复制的节点');
        return;
    }
    
    // 创建新节点，位置偏移一些
    const newNode = createNode(sourceNode.type, sourceNode.x + 30, sourceNode.y + 30);
    if (newNode) {
        // 复制配置
        newNode.config = JSON.parse(JSON.stringify(sourceNode.config));
        updateNodeContent(newNode);
        
        logBlueprint('info', `节点已复制: ${NODE_TYPES[sourceNode.type].name}`);
    }
}

/**
 * 生成配置字段HTML
 * @param {string} nodeId - 节点ID
 * @param {string} key - 配置键名
 * @param {object} cfg - 配置定义
 * @param {any} value - 当前值
 * @returns {string} HTML字符串
 */
function generateConfigField(nodeId, key, cfg, value) {
    const fieldId = `config_${nodeId}_${key}`;
    let inputHtml = '';
    
    switch (cfg.type) {
        case 'number':
            inputHtml = `<input type="number" id="${fieldId}" value="${value}" 
                         ${cfg.min !== undefined ? 'min="' + cfg.min + '"' : ''} 
                         ${cfg.max !== undefined ? 'max="' + cfg.max + '"' : ''}
                         ${cfg.step !== undefined ? 'step="' + cfg.step + '"' : ''}>`;
            break;
            
        case 'text':
            inputHtml = `<input type="text" id="${fieldId}" value="${value || ''}">`;
            break;
            
        case 'time':
            // 时间类型输入框，使用HTML5 time input
            inputHtml = `<input type="time" id="${fieldId}" value="${value || '08:00'}">`;
            break;
            
        case 'boolean':
            inputHtml = `<select id="${fieldId}">
                <option value="true" ${value ? 'selected' : ''}>是</option>
                <option value="false" ${!value ? 'selected' : ''}>否</option>
            </select>`;
            break;
            
        case 'select':
            inputHtml = `<select id="${fieldId}">`;
            cfg.options.forEach(opt => {
                const selected = String(value) === String(opt.value) ? 'selected' : '';
                inputHtml += `<option value="${opt.value}" ${selected}>${opt.label}</option>`;
            });
            inputHtml += '</select>';
            break;
            
        default:
            inputHtml = `<input type="text" id="${fieldId}" value="${value || ''}">`;
    }
    
    return `
        <div class="prop-field">
            <label for="${fieldId}">${cfg.label}</label>
            ${inputHtml}
        </div>
    `;
}

/**
 * 应用节点配置
 * @param {string} nodeId - 节点ID
 */
function applyNodeConfig(nodeId) {
    const node = blueprintNodes.find(n => n.id === nodeId);
    if (!node) return;
    
    const nodeType = NODE_TYPES[node.type];
    if (!nodeType.config) return;
    
    // 读取所有配置值
    for (const [key, cfg] of Object.entries(nodeType.config)) {
        const fieldId = `config_${nodeId}_${key}`;
        const field = document.getElementById(fieldId);
        if (!field) continue;
        
        let value = field.value;
        
        // 类型转换
        switch (cfg.type) {
            case 'number':
                value = parseFloat(value) || cfg.default;
                break;
            case 'boolean':
                value = value === 'true';
                break;
        }
        
        node.config[key] = value;
    }
    
    // 更新节点显示
    updateNodeContent(node);
    
    logBlueprint('info', `节点 ${nodeId} 配置已更新`);
}

/**
 * 隐藏节点属性面板
 */
function hideNodeProperties() {
    const panel = document.getElementById('nodePropertiesPanel');
    if (panel) {
        panel.style.display = 'none';
    }
}

/**
 * 编辑节点配置（双击触发）
 * @param {object} node - 节点对象
 */
function editNodeConfig(node) {
    selectNode(node);
    // 面板已经显示，聚焦到第一个输入框
    const firstInput = document.querySelector('#nodePropertiesPanel input, #nodePropertiesPanel select');
    if (firstInput) {
        firstInput.focus();
    }
}

/* ========================================================
 * 蓝图策略生成
 * ======================================================== */

/**
 * 从蓝图生成策略
 * 遍历所有触发器节点及其连接，生成对应的RPC策略
 * 修改：使用getNextStrategyId自动生成唯一ID，解决"每个分组只能添加一个策略"的问题
 */
function generateStrategiesFromBlueprint() {
    const strategies = [];
    
    // 找出所有触发器节点
    const triggerNodes = blueprintNodes.filter(n => 
        n.type === 'trigger-time' || n.type === 'trigger-sensor'
    );
    
    triggerNodes.forEach(trigger => {
        // 找出该触发器连接的所有动作节点
        const connectedActions = blueprintConnections
            .filter(conn => conn.sourceId === trigger.id)
            .map(conn => blueprintNodes.find(n => n.id === conn.targetId))
            .filter(n => n && (n.type === 'action-relay' || n.type === 'action-group'));
        
        connectedActions.forEach(action => {
            // 根据触发器类型获取下一个可用的策略ID
            const strategyType = trigger.type === 'trigger-sensor' ? 'sensor' : 'timer';
            const strategyId = getNextStrategyId(strategyType);
            
            const strategy = buildStrategyFromNodes(strategyId, trigger, action);
            if (strategy) {
                strategies.push(strategy);
                
                // 更新已使用的策略ID到缓存，防止重复
                if (strategyType === 'timer') {
                    loadedTimerStrategies.push({ id: strategyId });
                } else {
                    loadedSensorStrategies.push({ id: strategyId });
                }
            }
        });
    });
    
    return strategies;
}

/**
 * 从节点构建策略配置
 * 支持间隔执行和每日定时两种触发模式
 * @param {number} id - 策略ID
 * @param {object} trigger - 触发器节点
 * @param {object} action - 动作节点
 * @returns {object} 策略配置对象
 */
function buildStrategyFromNodes(id, trigger, action) {
    if (trigger.type === 'trigger-time') {
        // 定时策略
        const triggerMode = trigger.config.triggerMode || 'interval';
        
        if (action.type === 'action-group') {
            const strategy = {
                type: 'timer',
                id: id,
                name: `蓝图策略_${id}`,
                groupId: action.config.groupId,
                channel: action.config.channel,
                action: action.config.action,
                enabled: true,
                autoStart: trigger.config.autoStart,
                triggerType: triggerMode
            };
            
            // 根据触发模式设置时间参数
            if (triggerMode === 'daily') {
                strategy.dailyTime = trigger.config.dailyTime || '08:00';
            } else {
                strategy.intervalSec = trigger.config.intervalSec;
            }
            
            return strategy;
        } else if (action.type === 'action-relay') {
            // 继电器控制需要先找到或创建分组
            const strategy = {
                type: 'timer-relay',
                id: id,
                name: `蓝图策略_${id}`,
                nodeId: action.config.nodeId,
                channel: action.config.channel,
                action: action.config.action,
                enabled: true,
                autoStart: trigger.config.autoStart,
                triggerType: triggerMode
            };
            
            // 根据触发模式设置时间参数
            if (triggerMode === 'daily') {
                strategy.dailyTime = trigger.config.dailyTime || '08:00';
            } else {
                strategy.intervalSec = trigger.config.intervalSec;
            }
            
            return strategy;
        }
    } else if (trigger.type === 'trigger-sensor') {
        // 传感器策略
        if (action.type === 'action-group') {
            return {
                type: 'sensor',
                id: id,
                name: `蓝图传感器策略_${id}`,
                sensorType: trigger.config.sensorType,
                sensorNode: trigger.config.sensorNode,
                condition: trigger.config.condition,
                threshold: trigger.config.threshold,
                groupId: action.config.groupId,
                channel: action.config.channel,
                action: action.config.action,
                cooldownSec: trigger.config.cooldownSec,
                enabled: true
            };
        } else if (action.type === 'action-relay') {
            // 传感器触发直接控制单个继电器
            return {
                type: 'sensor-relay',
                id: id,
                name: `蓝图传感器策略_${id}`,
                sensorType: trigger.config.sensorType,
                sensorNode: trigger.config.sensorNode,
                condition: trigger.config.condition,
                threshold: trigger.config.threshold,
                nodeId: action.config.nodeId,
                channel: action.config.channel,
                action: action.config.action,
                cooldownSec: trigger.config.cooldownSec,
                enabled: true
            };
        }
    }
    
    return null;
}

/**
 * 部署蓝图策略到服务器
 * 将生成的策略通过RPC调用发送到服务器
 * 支持间隔执行和每日定时两种触发模式
 * 修改：先获取服务器现有策略，确保ID不冲突
 */
function deployBlueprintStrategies() {
    logBlueprint('info', '正在检查服务器现有策略...');
    
    // 先获取服务器现有的策略，确保ID不冲突
    let loadedCount = 0;
    const totalCalls = 2;
    
    function onLoaded() {
        loadedCount++;
        if (loadedCount >= totalCalls) {
            // 所有策略都已加载，现在生成和部署
            doDeployStrategies();
        }
    }
    
    // 获取定时策略
    callMethod('auto.strategy.list', {}, function(response) {
        if (response.result && response.result.strategies) {
            loadedTimerStrategies = response.result.strategies;
        }
        onLoaded();
    });
    
    // 获取传感器策略
    callMethod('auto.sensor.list', {}, function(response) {
        if (response.result && response.result.strategies) {
            loadedSensorStrategies = response.result.strategies;
        }
        onLoaded();
    });
}

/**
 * 实际执行策略部署
 */
function doDeployStrategies() {
    const strategies = generateStrategiesFromBlueprint();
    
    if (strategies.length === 0) {
        logBlueprint('warning', '没有可部署的策略。请确保触发器节点已连接到动作节点。');
        alert('没有可部署的策略！\n\n请确保：\n1. 添加了触发器节点（定时或传感器）\n2. 添加了动作节点（继电器或分组控制）\n3. 触发器输出端口连接到动作输入端口');
        return;
    }
    
    logBlueprint('info', `准备部署 ${strategies.length} 个策略（同一分组可添加多个策略）...`);
    
    strategies.forEach(strategy => {
        if (strategy.type === 'timer') {
            // 部署定时策略（分组控制）
            const params = {
                id: strategy.id,
                name: strategy.name,
                groupId: strategy.groupId,
                channel: strategy.channel,
                action: strategy.action,
                enabled: strategy.enabled,
                autoStart: strategy.autoStart,
                triggerType: strategy.triggerType || 'interval'
            };
            
            // 根据触发类型设置时间参数
            if (strategy.triggerType === 'daily' && strategy.dailyTime) {
                params.dailyTime = strategy.dailyTime;
            } else if (strategy.intervalSec) {
                params.intervalSec = strategy.intervalSec;
            }
            
            callMethod('auto.strategy.create', params, function(response) {
                if (response.result && response.result.ok) {
                    const triggerDesc = strategy.triggerType === 'daily' ? 
                        `每日 ${strategy.dailyTime}` : 
                        `每 ${strategy.intervalSec}秒`;
                    logBlueprint('info', `✓ 定时策略 "${strategy.name}" (ID:${strategy.id}) 部署成功（${triggerDesc}）`);
                } else if (response.error) {
                    logBlueprint('error', `✗ 策略 "${strategy.name}" 部署失败: ${response.error.message}`);
                }
            });
        } else if (strategy.type === 'timer-relay') {
            // 部署定时继电器策略（直接控制单个继电器）
            const params = {
                id: strategy.id,
                name: strategy.name,
                nodeId: strategy.nodeId,
                channel: strategy.channel,
                action: strategy.action,
                enabled: strategy.enabled,
                autoStart: strategy.autoStart,
                triggerType: strategy.triggerType || 'interval'
            };
            
            // 根据触发类型设置时间参数
            if (strategy.triggerType === 'daily' && strategy.dailyTime) {
                params.dailyTime = strategy.dailyTime;
            } else if (strategy.intervalSec) {
                params.intervalSec = strategy.intervalSec;
            }
            
            callMethod('auto.relay.create', params, function(response) {
                if (response.result && response.result.ok) {
                    const triggerDesc = strategy.triggerType === 'daily' ? 
                        `每日 ${strategy.dailyTime}` : 
                        `每 ${strategy.intervalSec}秒`;
                    logBlueprint('info', `✓ 定时继电器策略 "${strategy.name}" 部署成功（${triggerDesc}）`);
                } else if (response.error) {
                    logBlueprint('error', `✗ 策略 "${strategy.name}" 部署失败: ${response.error.message}`);
                }
            });
        } else if (strategy.type === 'sensor') {
            // 部署传感器策略
            callMethod('auto.sensor.create', {
                id: strategy.id,
                name: strategy.name,
                sensorType: strategy.sensorType,
                sensorNode: strategy.sensorNode,
                condition: strategy.condition,
                threshold: strategy.threshold,
                groupId: strategy.groupId,
                channel: strategy.channel,
                action: strategy.action,
                cooldownSec: strategy.cooldownSec,
                enabled: strategy.enabled
            }, function(response) {
                if (response.result && response.result.ok) {
                    logBlueprint('info', `✓ 传感器策略 "${strategy.name}" 部署成功`);
                } else if (response.error) {
                    logBlueprint('error', `✗ 策略 "${strategy.name}" 部署失败: ${response.error.message}`);
                }
            });
        } else if (strategy.type === 'sensor-relay') {
            // 部署传感器继电器策略（传感器触发直接控制单个继电器）
            callMethod('auto.sensorRelay.create', {
                id: strategy.id,
                name: strategy.name,
                sensorType: strategy.sensorType,
                sensorNode: strategy.sensorNode,
                condition: strategy.condition,
                threshold: strategy.threshold,
                nodeId: strategy.nodeId,
                channel: strategy.channel,
                action: strategy.action,
                cooldownSec: strategy.cooldownSec,
                enabled: strategy.enabled
            }, function(response) {
                if (response.result && response.result.ok) {
                    logBlueprint('info', `✓ 传感器继电器策略 "${strategy.name}" 部署成功`);
                } else if (response.error) {
                    logBlueprint('error', `✗ 策略 "${strategy.name}" 部署失败: ${response.error.message}`);
                }
            });
        }
    });
}

/* ========================================================
 * 系统框图功能
 * ======================================================== */

/**
 * 生成系统运行框图
 * 从服务器获取设备和分组信息，自动布局显示
 */
function generateSystemDiagram() {
    // 清空当前蓝图
    clearBlueprint();
    
    // 获取设备列表
    callMethod('relay.nodes', {}, function(response) {
        if (response.result && response.result.nodes) {
            const nodes = response.result.nodes;
            let x = 50;
            let y = 50;
            
            // 创建设备节点
            nodes.forEach((device, index) => {
                const node = createNode('device', x, y);
                if (node) {
                    node.config.nodeId = device.node || device.nodeId;
                    node.config.name = `设备${device.node || device.nodeId}`;
                    updateNodeContent(node);
                }
                
                x += 200;
                if ((index + 1) % 4 === 0) {
                    x = 50;
                    y += 120;
                }
            });
        }
    });
    
    // 获取分组列表
    callMethod('group.list', {}, function(response) {
        if (response.result && response.result.groups) {
            const groups = response.result.groups;
            let x = 500;
            let y = 50;
            
            // 创建分组节点
            groups.forEach((group, index) => {
                const node = createNode('group', x, y);
                if (node) {
                    node.config.groupId = group.groupId;
                    node.config.name = group.name || `分组${group.groupId}`;
                    updateNodeContent(node);
                }
                
                y += 120;
            });
        }
    });
    
    logBlueprint('info', '系统框图生成完成');
}

/**
 * 清空蓝图
 * 删除画布上的所有节点和连线
 */
function clearBlueprint() {
    // 如果有节点，先确认
    if (blueprintNodes.length > 0) {
        if (!confirm(`确定要清空蓝图吗？\n\n将删除 ${blueprintNodes.length} 个节点和 ${blueprintConnections.length} 条连线。`)) {
            return;
        }
    }
    
    // 移除所有节点DOM
    document.querySelectorAll('.blueprint-node').forEach(el => el.remove());
    
    // 移除所有连线
    const svg = blueprintCanvas ? blueprintCanvas.querySelector('.connection-layer') : null;
    if (svg) {
        svg.innerHTML = '';
    }
    
    // 清空数据
    blueprintNodes = [];
    blueprintConnections = [];
    selectedNode = null;
    nodeIdCounter = 1;
    
    hideNodeProperties();
    updateEmptyHint();
    
    logBlueprint('info', '蓝图已清空');
}

/* ========================================================
 * 蓝图导入导出
 * ======================================================== */

/**
 * 导出蓝图为JSON
 * @returns {string} JSON字符串
 */
function exportBlueprint() {
    const data = {
        version: '1.0',
        nodes: blueprintNodes,
        connections: blueprintConnections
    };
    
    return JSON.stringify(data, null, 2);
}

/**
 * 导入蓝图
 * @param {string} jsonStr - JSON字符串
 */
function importBlueprint(jsonStr) {
    try {
        const data = JSON.parse(jsonStr);
        
        // 清空当前蓝图
        clearBlueprint();
        
        // 恢复节点
        if (data.nodes && Array.isArray(data.nodes)) {
            data.nodes.forEach(nodeData => {
                blueprintNodes.push(nodeData);
                renderNode(nodeData);
                
                // 更新ID计数器
                const idNum = parseInt(nodeData.id.replace('node_', ''));
                if (idNum >= nodeIdCounter) {
                    nodeIdCounter = idNum + 1;
                }
            });
        }
        
        // 恢复连线
        if (data.connections && Array.isArray(data.connections)) {
            data.connections.forEach(conn => {
                blueprintConnections.push(conn);
                renderConnection(conn);
            });
        }
        
        logBlueprint('info', `蓝图导入成功：${blueprintNodes.length} 个节点，${blueprintConnections.length} 条连线`);
        
    } catch (e) {
        logBlueprint('error', '蓝图导入失败：' + e.message);
        alert('导入失败：无效的JSON格式');
    }
}

/**
 * 下载蓝图文件
 */
function downloadBlueprint() {
    const json = exportBlueprint();
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `blueprint_${Date.now()}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    logBlueprint('info', '蓝图已导出');
}

/**
 * 上传并导入蓝图文件
 */
function uploadBlueprint() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    
    input.onchange = function(e) {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = function(e) {
            importBlueprint(e.target.result);
        };
        reader.readAsText(file);
    };
    
    input.click();
}

/* ========================================================
 * 策略管理功能 - 读取和管理服务器上的策略
 * ======================================================== */

// 已加载的策略缓存
let loadedTimerStrategies = [];
let loadedSensorStrategies = [];

/**
 * 从服务器读取现有策略并在画布上显示
 * 这解决了"策略管理太复杂"的问题，允许用户在蓝图编辑器中统一管理策略
 */
function loadExistingStrategies() {
    // 清空当前画布（需要用户确认）
    if (blueprintNodes.length > 0) {
        if (!confirm('读取策略将清空当前画布。继续？')) {
            return;
        }
    }
    
    // 强制清空（不再询问）
    clearBlueprintSilent();
    
    logBlueprint('info', '正在从服务器读取策略...');
    
    let loadedCount = 0;
    let totalCalls = 2;  // 定时策略和传感器策略
    
    function checkAllLoaded() {
        loadedCount++;
        if (loadedCount >= totalCalls) {
            logBlueprint('info', `策略读取完成：${loadedTimerStrategies.length} 个定时策略，${loadedSensorStrategies.length} 个传感器策略`);
            layoutStrategyNodes();
        }
    }
    
    // 读取定时策略
    callMethod('auto.strategy.list', {}, function(response) {
        if (response.result && response.result.strategies) {
            loadedTimerStrategies = response.result.strategies;
            loadedTimerStrategies.forEach((strategy, index) => {
                createStrategyNode('timer', strategy, index);
            });
        }
        checkAllLoaded();
    });
    
    // 读取传感器策略
    callMethod('auto.sensor.list', {}, function(response) {
        if (response.result && response.result.strategies) {
            loadedSensorStrategies = response.result.strategies;
            loadedSensorStrategies.forEach((strategy, index) => {
                createStrategyNode('sensor', strategy, index);
            });
        }
        checkAllLoaded();
    });
}

/**
 * 静默清空蓝图（不询问用户）
 */
function clearBlueprintSilent() {
    // 移除所有节点DOM
    document.querySelectorAll('.blueprint-node').forEach(el => el.remove());
    
    // 移除所有连线
    const svg = blueprintCanvas ? blueprintCanvas.querySelector('.connection-layer') : null;
    if (svg) {
        svg.innerHTML = '';
    }
    
    // 清空数据
    blueprintNodes = [];
    blueprintConnections = [];
    selectedNode = null;
    nodeIdCounter = 1;
    loadedTimerStrategies = [];
    loadedSensorStrategies = [];
    
    hideNodeProperties();
    updateEmptyHint();
}

/**
 * 根据服务器策略数据创建策略节点
 * @param {string} strategyType - 策略类型: 'timer' 或 'sensor'
 * @param {object} strategyData - 服务器返回的策略数据
 * @param {number} index - 策略在列表中的索引（用于布局）
 */
function createStrategyNode(strategyType, strategyData, index) {
    const row = Math.floor(index / 2);
    const col = index % 2;
    
    if (strategyType === 'timer') {
        // 创建定时触发器节点
        const triggerX = 50 + col * 450;
        const triggerY = 50 + row * 200;
        
        const triggerNode = createNodeSilent('trigger-time', triggerX, triggerY);
        if (triggerNode) {
            triggerNode.config.intervalSec = strategyData.intervalSec || 60;
            triggerNode.config.autoStart = strategyData.autoStart !== false;
            triggerNode.config.triggerMode = strategyData.dailyTime ? 'daily' : 'interval';
            if (strategyData.dailyTime) {
                triggerNode.config.dailyTime = strategyData.dailyTime;
            }
            // 存储策略ID用于后续操作
            triggerNode.strategyId = strategyData.id;
            triggerNode.strategyType = 'timer';
            triggerNode.strategyEnabled = strategyData.enabled !== false;
            triggerNode.strategyName = strategyData.name;
            updateNodeContent(triggerNode);
        }
        
        // 创建分组控制动作节点
        const actionX = triggerX + 200;
        const actionY = triggerY;
        
        const actionNode = createNodeSilent('action-group', actionX, actionY);
        if (actionNode) {
            actionNode.config.groupId = strategyData.groupId;
            actionNode.config.channel = strategyData.channel;
            actionNode.config.action = strategyData.action || 'stop';
            updateNodeContent(actionNode);
        }
        
        // 创建连线
        if (triggerNode && actionNode) {
            createConnectionSilent(triggerNode.id, actionNode.id);
        }
    } else if (strategyType === 'sensor') {
        // 创建传感器触发器节点
        const triggerX = 50 + col * 450;
        const triggerY = 300 + row * 200;  // 传感器策略在定时策略下方
        
        const triggerNode = createNodeSilent('trigger-sensor', triggerX, triggerY);
        if (triggerNode) {
            triggerNode.config.sensorType = strategyData.sensorType || 'temperature';
            triggerNode.config.sensorNode = strategyData.sensorNode || 1;
            triggerNode.config.condition = strategyData.condition || 'gt';
            triggerNode.config.threshold = strategyData.threshold || 25;
            triggerNode.config.cooldownSec = strategyData.cooldownSec || 60;
            // 存储策略ID用于后续操作
            triggerNode.strategyId = strategyData.id;
            triggerNode.strategyType = 'sensor';
            triggerNode.strategyEnabled = strategyData.enabled !== false;
            triggerNode.strategyName = strategyData.name;
            updateNodeContent(triggerNode);
        }
        
        // 创建分组控制动作节点
        const actionX = triggerX + 200;
        const actionY = triggerY;
        
        const actionNode = createNodeSilent('action-group', actionX, actionY);
        if (actionNode) {
            actionNode.config.groupId = strategyData.groupId;
            actionNode.config.channel = strategyData.channel;
            actionNode.config.action = strategyData.action || 'stop';
            updateNodeContent(actionNode);
        }
        
        // 创建连线
        if (triggerNode && actionNode) {
            createConnectionSilent(triggerNode.id, actionNode.id);
        }
    }
}

/**
 * 静默创建节点（不选中）
 */
function createNodeSilent(type, x, y) {
    const nodeType = NODE_TYPES[type];
    if (!nodeType) {
        console.error('未知节点类型:', type);
        return null;
    }
    
    // 创建节点数据对象
    const node = {
        id: 'node_' + (nodeIdCounter++),
        type: type,
        x: x,
        y: y,
        config: {}
    };
    
    // 初始化默认配置值
    if (nodeType.config) {
        for (const [key, cfg] of Object.entries(nodeType.config)) {
            node.config[key] = cfg.default;
        }
    }
    
    // 添加到节点列表
    blueprintNodes.push(node);
    
    // 渲染节点DOM元素
    renderNode(node);
    
    // 隐藏空状态提示
    updateEmptyHint();
    
    return node;
}

/**
 * 静默创建连线（不提示）
 */
function createConnectionSilent(sourceNodeId, targetNodeId) {
    const connection = {
        id: 'conn_' + Date.now() + '_' + Math.random().toString(36).slice(2, 7),
        sourceId: sourceNodeId,
        sourcePort: 'trigger',
        targetId: targetNodeId,
        targetPort: 'trigger'
    };
    
    blueprintConnections.push(connection);
    
    // 延迟渲染连线，确保节点已渲染完成
    setTimeout(() => {
        renderConnection(connection);
    }, 50);
}

/**
 * 布局策略节点
 * 读取策略后重新整理节点布局
 */
function layoutStrategyNodes() {
    // 布局已在 createStrategyNode 中完成
    updateEmptyHint();
}

/**
 * 获取下一个可用的策略ID
 * 解决"每个分组只能添加一个策略"的问题 - 实际上是因为策略ID冲突
 * @param {string} strategyType - 策略类型: 'timer' 或 'sensor'
 * @returns {number} 下一个可用的策略ID
 */
function getNextStrategyId(strategyType) {
    let maxId = 0;
    
    if (strategyType === 'timer') {
        // 检查已加载的定时策略
        loadedTimerStrategies.forEach(s => {
            if (s.id > maxId) maxId = s.id;
        });
        // 也检查画布上的节点
        blueprintNodes.forEach(n => {
            if (n.strategyType === 'timer' && n.strategyId > maxId) {
                maxId = n.strategyId;
            }
        });
    } else if (strategyType === 'sensor') {
        // 检查已加载的传感器策略
        loadedSensorStrategies.forEach(s => {
            if (s.id > maxId) maxId = s.id;
        });
        // 也检查画布上的节点
        blueprintNodes.forEach(n => {
            if (n.strategyType === 'sensor' && n.strategyId > maxId) {
                maxId = n.strategyId;
            }
        });
    }
    
    return maxId + 1;
}

/**
 * 扩展节点属性面板，显示策略管理选项
 * 当节点是从服务器加载的策略时，显示启用/禁用/删除按钮
 */
function getStrategyManagementButtons(node) {
    if (!node.strategyId) return '';
    
    const enableBtn = node.strategyEnabled ?
        `<button class="warning" onclick="toggleStrategyFromBlueprint('${node.id}')" title="禁用此策略">⏸️ 禁用</button>` :
        `<button class="success" onclick="toggleStrategyFromBlueprint('${node.id}')" title="启用此策略">▶️ 启用</button>`;
    
    // 转义策略名称以防止XSS攻击
    const safeStrategyName = node.strategyName ? escapeHtmlBlueprint(node.strategyName) : '';
    
    return `
        <div class="strategy-management" style="margin-top: 15px; padding-top: 15px; border-top: 1px solid #eee;">
            <h5 style="margin-bottom: 10px; color: #666;">📋 服务器策略管理</h5>
            <p style="font-size: 12px; color: #999; margin-bottom: 10px;">
                策略ID: ${node.strategyId} | ${node.strategyEnabled ? '✅ 已启用' : '❌ 已禁用'}
                ${safeStrategyName ? '<br>名称: ' + safeStrategyName : ''}
            </p>
            <div style="display: flex; gap: 8px; flex-wrap: wrap;">
                ${enableBtn}
                <button class="secondary" onclick="triggerStrategyFromBlueprint('${node.id}')" title="立即执行一次">🎯 触发</button>
                <button class="danger" onclick="deleteStrategyFromBlueprint('${node.id}')" title="从服务器删除">🗑️ 删除</button>
            </div>
        </div>
    `;
}

/**
 * 切换策略启用状态
 */
function toggleStrategyFromBlueprint(nodeId) {
    const node = blueprintNodes.find(n => n.id === nodeId);
    if (!node || !node.strategyId) return;
    
    const newEnabled = !node.strategyEnabled;
    const method = node.strategyType === 'sensor' ? 'auto.sensor.enable' : 'auto.strategy.enable';
    
    callMethod(method, {
        id: node.strategyId,
        enabled: newEnabled
    }, function(response) {
        if (response.result && response.result.ok) {
            node.strategyEnabled = newEnabled;
            logBlueprint('info', `策略 ${node.strategyId} 已${newEnabled ? '启用' : '禁用'}`);
            // 刷新属性面板
            if (selectedNode && selectedNode.id === nodeId) {
                showNodeProperties(node);
            }
        } else if (response.error) {
            logBlueprint('error', `操作失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 触发策略
 */
function triggerStrategyFromBlueprint(nodeId) {
    const node = blueprintNodes.find(n => n.id === nodeId);
    if (!node || !node.strategyId) return;
    
    // 只有定时策略支持手动触发
    if (node.strategyType !== 'timer') {
        logBlueprint('warning', '传感器策略不支持手动触发');
        return;
    }
    
    callMethod('auto.strategy.trigger', {
        id: node.strategyId
    }, function(response) {
        if (response.result && response.result.ok) {
            logBlueprint('info', `策略 ${node.strategyId} 已触发`);
        } else if (response.error) {
            logBlueprint('error', `触发失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 删除策略
 */
function deleteStrategyFromBlueprint(nodeId) {
    const node = blueprintNodes.find(n => n.id === nodeId);
    if (!node || !node.strategyId) return;
    
    const strategyName = node.strategyName || `策略${node.strategyId}`;
    if (!confirm(`确定要从服务器删除 "${strategyName}" 吗？\n\n此操作不可撤销！`)) {
        return;
    }
    
    const method = node.strategyType === 'sensor' ? 'auto.sensor.delete' : 'auto.strategy.delete';
    
    callMethod(method, {
        id: node.strategyId
    }, function(response) {
        if (response.result && response.result.ok) {
            logBlueprint('info', `策略 ${node.strategyId} 已从服务器删除`);
            // 从画布删除节点（静默，不再确认）
            deleteNodeSilent(nodeId);
        } else if (response.error) {
            logBlueprint('error', `删除失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 静默删除节点（不询问用户）
 */
function deleteNodeSilent(nodeId) {
    // 从数组中移除
    const index = blueprintNodes.findIndex(n => n.id === nodeId);
    if (index !== -1) {
        blueprintNodes.splice(index, 1);
    }
    
    // 移除相关连线
    blueprintConnections = blueprintConnections.filter(conn => {
        if (conn.sourceId === nodeId || conn.targetId === nodeId) {
            const lineEl = document.getElementById(conn.id);
            if (lineEl) lineEl.remove();
            return false;
        }
        return true;
    });
    
    // 移除DOM元素
    const nodeEl = document.getElementById(nodeId);
    if (nodeEl) {
        nodeEl.remove();
    }
    
    // 如果是选中的节点，清除选择
    if (selectedNode && selectedNode.id === nodeId) {
        selectedNode = null;
        hideNodeProperties();
    }
    
    updateEmptyHint();
}

/* ========================================================
 * 日志和工具函数
 * ======================================================== */

/**
 * HTML转义 - 防止XSS攻击
 * @param {string} text - 原始文本
 * @returns {string} 转义后的文本
 */
function escapeHtmlBlueprint(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

/**
 * 蓝图编辑器日志
 * @param {string} type - 日志类型
 * @param {string} message - 日志消息
 */
function logBlueprint(type, message) {
    // 调用主应用的日志功能
    if (typeof log === 'function') {
        log(type, `[蓝图] ${message}`);
    } else {
        console.log(`[Blueprint][${type}] ${message}`);
    }
}

/* ========================================================
 * 页面初始化
 * ======================================================== */

// 当蓝图页面显示时初始化
document.addEventListener('DOMContentLoaded', function() {
    // 延迟初始化，等待页面完全加载
    setTimeout(function() {
        if (document.getElementById('blueprintCanvas')) {
            initBlueprintEditor();
        }
    }, 100);
});
