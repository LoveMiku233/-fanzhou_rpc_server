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
 * 7. Tauri Sidecar集成 - 自动启动websocat代理
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

// 传感器映射缓存
let sensorMappingCache = [];

// 日志条目数量限制
const MAX_LOG_ENTRIES = 100;

// 默认通道数量（GD427继电器默认4通道）
const DEFAULT_CHANNEL_COUNT = 4;

// 状态查询延迟时间（毫秒）- 给设备响应时间
const STATUS_QUERY_DELAY_MS = 200;

// 使用分组绑定的通道（ch=-1表示控制分组通过addChannel添加的特定通道）
const BOUND_CHANNELS = -1;

// 检测是否运行在Tauri环境中
// 需要检查 window.__TAURI__ 对象是否存在且具有预期的 API 结构
// 简单检查 !== undefined 不够可靠，需要验证是否有实际的 Tauri API
function detectTauriEnvironment() {
    if (!window.__TAURI__) {
        return false;
    }
    // 检查是否有核心 API（Tauri v2.x 使用 core.invoke，v1.x 使用 tauri.invoke 或直接 invoke）
    const hasV2Api = window.__TAURI__.core && typeof window.__TAURI__.core.invoke === 'function';
    const hasV1Api = (window.__TAURI__.tauri && typeof window.__TAURI__.tauri.invoke === 'function') ||
                     (typeof window.__TAURI__.invoke === 'function');
    return hasV2Api || hasV1Api;
}

// 使用函数检测 Tauri 环境，而不是简单检查对象存在
// 注意：必须使用 var 而不是 const，因为 const 重复声明会抛出 SyntaxError
window.isTauri = detectTauriEnvironment();
var isTauri = window.isTauri;

// websocat代理是否正在运行
let websocatRunning = false;

/* ========================================================
 * 认证和启动页集成
 * ======================================================== */

/**
 * 检查认证状态
 * 如果未通过启动页认证，重定向到启动页
 */
function checkAuthentication() {
    // 检查sessionStorage中的认证状态
    const authenticated = sessionStorage.getItem('rpc_authenticated');
    
    // 在Tauri环境中，如果未认证，则重定向到启动页
    if (!authenticated && isTauri) {
        // 未认证，重定向到启动页
        window.location.href = 'launch.html';
        return;
    }
    
    // 在浏览器环境中（非Tauri），允许直接访问以便于开发测试
}

/**
 * 检查是否应该自动连接
 * 检查sessionStorage和URL参数
 * @returns {boolean} 是否应该自动连接
 */
function shouldAutoConnect() {
    const autoConnect = sessionStorage.getItem('rpc_authenticated') === 'true';
    const urlParams = new URLSearchParams(window.location.search);
    const urlAutoConnect = urlParams.get('autoconnect') === 'true';
    return autoConnect || urlAutoConnect;
}

/**
 * 从启动页加载保存的设置
 * 自动填充服务器地址和端口，并自动连接
 */
function loadLaunchSettings() {
    const savedHost = sessionStorage.getItem('rpc_host');
    const savedRpcPort = sessionStorage.getItem('rpc_port');
    const savedWsPort = sessionStorage.getItem('ws_port');
    
    // 也检查URL参数（用于通过Python脚本启动的场景）
    const urlParams = new URLSearchParams(window.location.search);
    const urlHost = urlParams.get('host');
    const urlRpcPort = urlParams.get('rpcPort') || urlParams.get('port');
    const urlWsPort = urlParams.get('wsPort');
    
    // 优先使用URL参数，其次使用sessionStorage的值
    const finalHost = urlHost || savedHost;
    const finalRpcPort = urlRpcPort || savedRpcPort;
    const finalWsPort = urlWsPort || savedWsPort;
    
    if (finalHost) {
        const hostInput = document.getElementById('serverHost');
        if (hostInput) {
            hostInput.value = finalHost;
        }
    }
    
    if (finalRpcPort) {
        const rpcPortInput = document.getElementById('rpcPort');
        if (rpcPortInput) {
            rpcPortInput.value = finalRpcPort;
        }
    }
    
    if (finalWsPort) {
        const wsPortInput = document.getElementById('serverPort');
        if (wsPortInput) {
            wsPortInput.value = finalWsPort;
        }
    }
    
    // 如果有保存的设置或URL参数指定自动连接，则自动连接
    if (finalHost && finalWsPort && shouldAutoConnect()) {
        // 延迟一点执行自动连接，让页面完全加载
        setTimeout(function() {
            log('info', '检测到已保存的连接设置，正在自动连接...');
            log('info', `目标RPC服务器: ${finalHost}:${finalRpcPort || 12345}`);
            connect();
        }, 500);
    }
}

/* ========================================================
 * Tauri Sidecar 集成 - websocat代理管理
 * ======================================================== */

/**
 * 获取Tauri invoke函数
 * 兼容Tauri v1.x和v2.x的不同API结构
 * @param {boolean} verbose - 是否输出详细调试信息，默认false
 * @returns {function|null} invoke函数，如果不可用返回null
 */
function getTauriInvoke(verbose = false) {
    // verbose参数用于调试，设为true时输出详细日志（任何环境均可使用）
    if (verbose) {
        console.log('[DEBUG] 检查 Tauri API...');
        console.log('[DEBUG] window.__TAURI__ 存在:', !!window.__TAURI__);
    }
    
    if (!window.__TAURI__) {
        // 非Tauri环境不输出错误日志，这是正常情况
        return null;
    }
    
    if (verbose) {
        // 输出 __TAURI__ 对象的所有键
        console.log('[DEBUG] __TAURI__ 对象键:', Object.keys(window.__TAURI__));
    }
    
    // Tauri v2.x: invoke is in window.__TAURI__.core
    if (window.__TAURI__.core && typeof window.__TAURI__.core.invoke === 'function') {
        if (verbose) {
            console.log('[DEBUG] 找到 Tauri v2.x invoke API (core.invoke)');
        }
        return window.__TAURI__.core.invoke;
    }
    
    // Tauri v1.x: invoke is in window.__TAURI__.tauri
    if (window.__TAURI__.tauri && typeof window.__TAURI__.tauri.invoke === 'function') {
        if (verbose) {
            console.log('[DEBUG] 找到 Tauri v1.x invoke API (tauri.invoke)');
        }
        return window.__TAURI__.tauri.invoke;
    }
    
    // Tauri v1.x alternative: invoke might be directly on window.__TAURI__
    if (typeof window.__TAURI__.invoke === 'function') {
        if (verbose) {
            console.log('[DEBUG] 找到 Tauri v1.x invoke API (直接在 __TAURI__ 上)');
        }
        return window.__TAURI__.invoke;
    }
    
    // 只在Tauri环境中但找不到invoke时输出错误
    if (isTauri) {
        console.error('[DEBUG] 未找到 invoke 函数！__TAURI__ 结构:', JSON.stringify(Object.keys(window.__TAURI__)));
        if (window.__TAURI__.core) {
            console.error('[DEBUG] __TAURI__.core 键:', Object.keys(window.__TAURI__.core));
        }
    }
    
    return null;
}

/**
 * 启动websocat代理（仅在Tauri环境中可用）
 * 
 * 工作原理：
 * websocat在本地监听WebSocket连接，并将数据转发到远程TCP服务器
 * 浏览器 → WebSocket(localhost:wsPort) → websocat → TCP(tcpHost:tcpPort)
 * 
 * @param {number} wsPort - WebSocket监听端口（默认12346）
 * @param {string} tcpHost - TCP目标地址（远程RPC服务器IP）
 * @param {number} tcpPort - TCP目标端口（默认12345）
 * @returns {Promise<number|null>} 成功返回PID，失败返回null
 */
async function startWebsocatProxy(wsPort = 12346, tcpHost = '127.0.0.1', tcpPort = 12345) {
    // 非Tauri环境下提示用户手动启动websocat
    if (!isTauri) {
        log('info', `当前为浏览器环境，请手动启动websocat代理：\nwebsocat --text ws-l:0.0.0.0:${wsPort} tcp:${tcpHost}:${tcpPort}`);
        return null;
    }
    
    // Tauri环境下尝试调用后端命令
    try {
        const invoke = getTauriInvoke();
        if (!invoke) {
            log('error', '启动websocat失败: Tauri invoke API不可用，请检查Tauri版本兼容性');
            log('error', '可能的原因：\n1. Tauri shell插件未正确配置\n2. capabilities/shell.json 缺失\n3. Tauri版本不兼容');
            return null;
        }
        
        log('info', `正在启动代理：本机:${wsPort} → ${tcpHost}:${tcpPort}`);
        
        const pid = await invoke('start_websocat', {
            wsPort: wsPort,
            tcpHost: tcpHost,
            tcpPort: tcpPort
        });
        websocatRunning = true;
        log('info', `✅ websocat代理已启动，PID: ${pid}`);
        log('info', `数据流向：浏览器 → WebSocket(localhost:${wsPort}) → websocat → TCP(${tcpHost}:${tcpPort})`);
        updateWebsocatStatus(true, tcpHost, tcpPort);
        return pid;
    } catch (error) {
        console.error('startWebsocatProxy 出错:', error);
        log('error', `启动websocat失败: ${error}`);
        log('error', '请检查：\n1. websocat可执行文件是否存在于bin目录\n2. 目标RPC服务器是否可达\n3. 端口是否被占用');
        return null;
    }
}

/**
 * 停止websocat代理
 * @returns {Promise<boolean>} 成功返回true
 */
async function stopWebsocatProxy() {
    if (!isTauri) {
        log('info', '不是Tauri环境，请手动停止websocat进程');
        return false;
    }
    
    try {
        const invoke = getTauriInvoke();
        if (!invoke) {
            log('error', '停止websocat失败: Tauri invoke API不可用');
            return false;
        }
        await invoke('stop_websocat');
        websocatRunning = false;
        log('info', '✅ websocat代理已停止');
        updateWebsocatStatus(false);
        return true;
    } catch (error) {
        log('error', `停止websocat失败: ${error}`);
        return false;
    }
}

/**
 * 检查websocat是否在运行
 * @returns {Promise<boolean>}
 */
async function checkWebsocatStatus() {
    if (!isTauri) return false;
    
    try {
        const invoke = getTauriInvoke();
        if (!invoke) {
            console.error('检查websocat状态失败: Tauri invoke API不可用');
            return false;
        }
        const running = await invoke('is_websocat_running');
        websocatRunning = running;
        updateWebsocatStatus(running);
        return running;
    } catch (error) {
        console.error('检查websocat状态失败:', error);
        return false;
    }
}

/**
 * 获取websocat进程PID
 * @returns {Promise<number|null>}
 */
async function getWebsocatPid() {
    if (!isTauri) return null;
    
    try {
        const invoke = getTauriInvoke();
        if (!invoke) {
            console.error('获取websocat PID失败: Tauri invoke API不可用');
            return null;
        }
        return await invoke('get_websocat_pid');
    } catch (error) {
        console.error('获取websocat PID失败:', error);
        return null;
    }
}

/**
 * 切换websocat代理状态
 * 从UI中读取目标RPC服务器地址和端口
 */
async function toggleWebsocatProxy() {
    if (websocatRunning) {
        await stopWebsocatProxy();
    } else {
        // 获取目标RPC服务器地址和端口
        const tcpHost = document.getElementById('serverHost').value.trim();
        if (!tcpHost) {
            log('error', '请先输入RPC服务器地址');
            return;
        }
        const tcpPort = parseInt(document.getElementById('rpcPort').value) || 12345;
        const wsPort = parseInt(document.getElementById('serverPort').value) || 12346;
        
        await startWebsocatProxy(wsPort, tcpHost, tcpPort);
    }
}

/**
 * 更新websocat状态显示
 * @param {boolean} running - 是否正在运行
 * @param {string} tcpHost - 目标TCP主机（可选）
 * @param {number} tcpPort - 目标TCP端口（可选）
 */
function updateWebsocatStatus(running, tcpHost, tcpPort) {
    const btn = document.getElementById('websocatToggleBtn');
    if (btn) {
        if (running) {
            const hostInfo = tcpHost ? ` (→${tcpHost}:${tcpPort})` : '';
            btn.textContent = '🛑 停止代理' + hostInfo;
            btn.classList.add('danger');
            btn.classList.remove('success');
        } else {
            btn.textContent = '🚀 启动代理';
            btn.classList.add('success');
            btn.classList.remove('danger');
        }
    }
}

/**
 * 初始化Tauri功能
 */
async function initTauri() {
    if (!isTauri) {
        // 非Tauri环境，静默跳过初始化
        return;
    }
    
    console.log('[Tauri] 检测到Tauri环境，开始初始化...');
    
    try {
        // 显示Tauri相关的UI元素
        const websocatBtn = document.getElementById('websocatToggleBtn');
        if (websocatBtn) {
            websocatBtn.style.display = 'inline-block';
        }
        
        const tauriHint = document.getElementById('tauriHint');
        if (tauriHint) {
            tauriHint.style.display = 'block';
        }
        
        // 隐藏手动代理说明（Tauri环境下不需要）
        const manualHelp = document.getElementById('manualProxyHelp');
        if (manualHelp) {
            manualHelp.style.display = 'none';
        }
        
        // 检查websocat状态
        await checkWebsocatStatus();
        console.log('[Tauri] 初始化完成');
    } catch (error) {
        console.error('[Tauri] 初始化失败:', error);
    }
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    try {
        // 首先检查认证状态
        checkAuthentication();
        
        // 初始化 Tauri 功能（异步，但不阻塞后续执行）
        initTauri().catch(function(error) {
            console.error('Tauri初始化异常:', error);
        });
        
        // 初始化按钮点击处理
        // - 为导航按钮、连接按钮等添加 addEventListener（所有环境）
        // - 在 Tauri 环境中启用事件委托，解决 inline onclick 不触发的问题
        initButtonClickHandlers();
        
        // 从启动页获取保存的设置并自动填充
        loadLaunchSettings();
        
        // 显示初始化完成信息
        log('info', '🚀 泛舟RPC调试工具已就绪');
        
        // 如果没有自动连接，提示用户手动连接
        if (!shouldAutoConnect()) {
            log('info', '请输入服务器地址并点击"连接"按钮');
        }
    } catch (error) {
        console.error('页面初始化失败:', error);
    }
});

/**
 * 初始化按钮点击处理
 * 使用事件委托处理所有按钮点击事件
 * 
 * 问题背景：
 * 在 Tauri WebView 中，HTML 元素上的 inline onclick 属性
 * 可能不会正确触发点击事件。用户看到鼠标悬停有动画效果，但点击没有反应。
 * 
 * 解决方案：
 * 使用事件委托在 document.body 上监听所有点击事件，
 * 当检测到被点击的元素有 onclick 属性时，手动执行该属性中的代码。
 * 
 * 安全说明：
 * 此应用的 HTML 内容完全由开发者控制，不接受用户输入的 HTML。
 * onclick 属性中的代码都是开发者自己编写的可信代码。
 * 
 * @description 解决 Tauri 中 inline onclick 事件不触发的问题
 */
function initButtonClickHandlers() {
    /**
     * 为按钮绑定点击事件的辅助函数
     * @param {HTMLElement} element - 按钮元素
     * @param {Function} handler - 事件处理函数
     */
    function bindClickHandler(element, handler) {
        if (element) {
            element.addEventListener('click', function(e) {
                e.preventDefault();
                e.stopPropagation();
                handler();
            });
        }
    }
    
    // 为导航按钮添加事件监听
    document.querySelectorAll('.nav-btn').forEach(function(btn) {
        const page = btn.getAttribute('data-page');
        if (page) {
            bindClickHandler(btn, function() { showPage(page); });
        }
    });
    
    // 为连接按钮添加事件监听
    bindClickHandler(document.getElementById('connectBtn'), toggleConnection);
    
    // 为 websocat 代理按钮添加事件监听
    bindClickHandler(document.getElementById('websocatToggleBtn'), toggleWebsocatProxy);
    
    // 只在 Tauri 环境中启用事件委托，避免普通浏览器的性能影响
    if (!isTauri) {
        console.log('非 Tauri 环境，跳过事件委托初始化');
        return;
    }
    
    // 定义常量：向上查找 onclick 属性的最大层级数
    var MAX_ONCLICK_SEARCH_DEPTH = 10;
    
    /**
     * 使用事件委托处理所有按钮和可点击元素的点击事件
     * 这解决了 Tauri WebView 中 inline onclick 属性不触发的问题
     * 
     * 工作原理：
     * 1. 在 document.body 上监听 click 事件（捕获阶段）
     * 2. 当点击发生时，检查目标元素是否有 onclick 属性
     * 3. 如果有，使用 Function 构造器执行 onclick 代码
     * 4. 停止事件传播，防止重复触发
     * 
     * 使用捕获阶段的原因：
     * 在 Tauri 中 inline onclick 可能根本不触发，我们需要在事件传播的最早阶段拦截
     */
    document.body.addEventListener('click', function(e) {
        // 获取被点击的元素（向上查找到有 onclick 属性的元素）
        var target = e.target;
        var depth = 0;
        
        while (target && target !== document.body && depth < MAX_ONCLICK_SEARCH_DEPTH) {
            // 检查元素是否有 onclick 属性
            var onclickAttr = target.getAttribute('onclick');
            
            if (onclickAttr) {
                // 阻止事件继续传播，避免重复触发
                // 这可以防止 native onclick 再次触发同一事件
                e.stopPropagation();
                e.stopImmediatePropagation();
                
                try {
                    // 使用 Function 构造器执行 onclick 代码
                    // 安全说明：onclick 属性中的代码都是开发者自己编写的可信代码
                    // 使用 with(window) 确保可以访问全局函数如 showPage、refreshDeviceList 等
                    var clickHandler = new Function('event', 'with(window){return (' + onclickAttr + ');}');
                    var result = clickHandler.call(target, e);
                    
                    // 只有当 onclick 处理程序明确返回 false 时才阻止默认行为
                    // 这遵循了传统 onclick 的行为规范
                    if (result === false) {
                        e.preventDefault();
                    }
                } catch (error) {
                    console.error('Tauri onclick 执行失败:', error, onclickAttr);
                }
                
                return; // 已处理，退出
            }
            
            target = target.parentElement;
            depth++;
        }
    }, true); // 使用捕获阶段，在 Tauri 中 inline onclick 不触发时仍能拦截事件
    
    console.log('Tauri 按钮点击处理已初始化（事件委托模式）');
}

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

    if (pageName === 'sensors') {
        refreshSensorMapping();
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
 * 
 * 连接原理：
 * 1. 浏览器通过WebSocket连接到本地的websocat代理（localhost:wsPort）
 * 2. websocat代理将数据转发到远程RPC服务器（tcpHost:tcpPort）
 * 
 * 注意：serverHost字段现在表示目标RPC服务器地址，不是WebSocket连接地址
 * WebSocket始终连接到localhost，因为websocat代理运行在本机
 */
function connect() {
    const tcpHost = document.getElementById('serverHost').value.trim();
    const wsPort = parseInt(document.getElementById('serverPort').value) || 12346;
    const rpcPortEl = document.getElementById('rpcPort');
    const rpcPort = rpcPortEl ? (parseInt(rpcPortEl.value) || 12345) : 12345;
    
    if (!tcpHost) {
        log('error', '请输入RPC服务器地址');
        return;
    }
    
    // 更新连接状态为"连接中"
    updateConnectionStatus('connecting');
    
    // WebSocket连接到本地代理（localhost），而不是远程服务器
    // 远程连接由websocat代理处理
    const wsHost = 'localhost';
    const wsUrl = `ws://${wsHost}:${wsPort}`;
    
    log('info', `正在通过本地代理连接到 ${wsUrl}...`);
    log('info', `目标RPC服务器: ${tcpHost}:${rpcPort}`);
    
    try {
        ws = new WebSocket(wsUrl);
        
        // 连接成功回调
        ws.onopen = function() {
            log('info', '✅ WebSocket连接成功');
            log('info', `已通过代理连接到目标: ${tcpHost}`);
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
            log('error', '⚠️ WebSocket连接错误，请检查：');
            log('error', '1. websocat代理是否已启动（先点击"启动代理"按钮）');
            log('error', '2. 本地代理端口是否正确（默认12346）');
            log('error', '3. 目标RPC服务器是否可达');
            log('error', '4. 防火墙是否允许连接');
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
 * group.list 现在直接返回通道信息，不需要额外调用 group.getChannels
 */
function refreshGroupList() {
    callMethod('group.list', {}, function(response) {
        if (response.result) {
            groupListCache = response.result.groups || response.result || [];
            // group.list 已经包含 channels 信息，直接渲染
            renderGroupList();
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
    // 使用BOUND_CHANNELS（-1）表示控制分组绑定的通道
    // 这样会调用后端的queueGroupBoundChannelsControl()方法
    // 只控制通过group.addChannel添加的特定通道
    callMethod('group.control', {
        groupId: groupId,
        ch: BOUND_CHANNELS,
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
            // 延迟后查询状态，给设备响应时间
            setTimeout(() => queryDeviceStatus(nodeId), STATUS_QUERY_DELAY_MS);
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
 * 使用 relay.controlMulti API 一次性控制所有通道，节省 CAN 总线带宽
 * 相比逐个通道发送（4帧），这种方式只需要1帧CAN消息
 * 如果控制无效，请检查：
 * 1. CAN总线是否已打开（点击"CAN诊断"按钮）
 * 2. 设备是否正确连接
 * 3. 节点ID是否正确
 */
function controlDeviceAll(nodeId, action) {
    log('info', `控制设备 ${nodeId} 全部通道: ${action} (使用 controlMulti API)`);
    
    // 使用 relay.controlMulti API 一次性控制所有通道
    // 所有通道执行相同的动作，使用 DEFAULT_CHANNEL_COUNT 保持一致性
    const actions = new Array(DEFAULT_CHANNEL_COUNT).fill(action);
    
    callMethod('relay.controlMulti', {
        node: nodeId,
        actions: actions
    }, function(response) {
        // 检查响应中是否有警告信息
        if (response.result && response.result.warning) {
            log('error', `⚠️ ${response.result.warning}`);
        }
        // 控制成功后刷新状态
        if (response.result && response.result.ok) {
            // 延迟后查询状态，给设备响应时间
            setTimeout(() => queryDeviceStatus(nodeId), STATUS_QUERY_DELAY_MS);
        } else if (response.error) {
            log('error', `控制失败: ${response.error.message || '未知错误'}`);
        }
    });
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
        const channel = strategy.channel === -1 ? '📂 分组绑定通道' : `通道 ${strategy.channel}`;
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
        const channel = strategy.channel >= 0 ? `通道 ${strategy.channel}` : '📂 分组绑定通道';
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
 * 同步所有策略到云端
 */
function syncStrategiesToCloud() {
    log('info', '正在同步策略到云端...');
    
    callMethod('auto.strategy.syncToCloud', {
        method: 'set'
    }, function(response) {
        if (response.result && response.result.ok) {
            const count = response.result.syncedCount || 0;
            const ids = response.result.syncedIds || [];
            log('info', `☁️ 已同步 ${count} 个策略到云端: [${ids.join(', ')}]`);
        } else if (response.error) {
            log('error', `同步失败: ${response.error.message || '未知错误'}`);
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

/**
 * 刷新传感器映射表
 */
function refreshSensorMapping() {
    callMethod('sensor.list', { source: 'all' }, function(response) {
        if (response.result && response.result.ok) {
            sensorMappingCache = response.result.sensors || [];
            renderSensorMappingTable();
        } else if (response.error) {
            log('error', `获取传感器映射失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 渲染传感器映射表格
 */
function renderSensorMappingTable() {
    const tableBody = document.getElementById('sensorMappingTableBody');
    const emptyEl = document.getElementById('sensorMappingEmpty');
    const cardEl = document.getElementById('sensorMappingCard');

    if (!tableBody) {
        return;
    }

    if (!sensorMappingCache || sensorMappingCache.length === 0) {
        if (emptyEl) emptyEl.style.display = 'block';
        if (cardEl) cardEl.style.display = 'none';
        tableBody.innerHTML = '<tr><td colspan="7" class="sensor-mapping-placeholder">暂无传感器数据</td></tr>';
        return;
    }

    if (emptyEl) emptyEl.style.display = 'none';
    if (cardEl) cardEl.style.display = 'block';

    let html = '';
    sensorMappingCache.forEach(sensor => {
        const name = escapeHtml(sensor.name || sensor.sensorId || '--');
        const sensorId = escapeHtml(sensor.sensorId || '--');
        const source = String(sensor.source || 'local').toLowerCase();
        const sourceLabel = source === 'mqtt' ? '☁️ MQTT' : '🧩 本地';
        const sourceClass = source === 'mqtt' ? 'mqtt' : '';
        const nodeLabel = sensor.nodeId !== undefined ? `节点 ${sensor.nodeId}` : '--';
        const channelLabel = sensor.channel !== undefined ? `通道 ${sensor.channel}` : '';
        const nodeInfo = channelLabel ? `${nodeLabel} / ${channelLabel}` : nodeLabel;
        const dataPath = source === 'mqtt'
            ? [sensor.topic, sensor.jsonPath].filter(Boolean).join(' / ')
            : (sensor.params || '--');
        const mappingPath = typeof dataPath === 'object' ? JSON.stringify(dataPath) : dataPath;
        let valueText = sensor.hasValue ? sensor.value : '--';
        if (valueText && typeof valueText === 'object') {
            valueText = JSON.stringify(valueText);
        }
        const unit = sensor.unit || '';
        const displayValue = sensor.hasValue ? `${valueText}${unit ? ' ' + unit : ''}` : '暂无数据';
        const updateTime = sensor.updateTime || '--';
        const statusClass = sensor.enabled === false ? 'offline' : '';
        const statusText = sensor.enabled === false ? '已停用' : (sensor.hasValue ? '在线' : '待采集');

        html += `
            <tr>
                <td>
                    <strong>${name}</strong>
                    <div style="font-size: 11px; color: #888;">${sensorId}</div>
                </td>
                <td><span class="sensor-mapping-tag ${sourceClass}">${sourceLabel}</span></td>
                <td>${escapeHtml(nodeInfo)}</td>
                <td style="color: #666;">${escapeHtml(String(mappingPath || '--'))}</td>
                <td><strong>${escapeHtml(String(displayValue))}</strong></td>
                <td>${escapeHtml(updateTime)}</td>
                <td><span class="sensor-mapping-status ${statusClass}">${statusText}</span></td>
            </tr>
        `;
    });

    tableBody.innerHTML = html;
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

/* ========================================================
 * MQTT多通道管理功能
 * 
 * 支持多个MQTT通道同时连接不同的Broker
 * 当设备状态变化时，自动向所有已连接通道推送消息
 * ======================================================== */

// MQTT通道列表缓存
let mqttChannelsCache = [];

/**
 * 刷新MQTT通道列表
 */
function refreshMqttChannels() {
    callMethod('mqtt.channels.list', {}, function(response) {
        if (response.result && response.result.ok) {
            mqttChannelsCache = response.result.channels || [];
            renderMqttChannels();
        } else if (response.error) {
            log('error', `获取MQTT通道失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 渲染MQTT通道列表
 */
function renderMqttChannels() {
    const container = document.getElementById('mqttChannelsContainer');
    const emptyEl = document.getElementById('mqttChannelsEmpty');
    
    if (!mqttChannelsCache || mqttChannelsCache.length === 0) {
        if (container) container.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    let html = '<div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(350px, 1fr)); gap: 15px;">';
    
    mqttChannelsCache.forEach(channel => {
        const connected = channel.connected === true;
        const enabled = channel.enabled !== false;
        const statusClass = connected ? 'connected' : (enabled ? 'disconnected' : 'disabled');
        const statusText = connected ? '🟢 已连接' : (enabled ? '🔴 未连接' : '⚪ 已禁用');
        
        html += `
            <div class="mqtt-channel-card" style="background: #f8f9fa; border-radius: 12px; padding: 20px; border: 2px solid ${connected ? '#4caf50' : '#e0e0e0'};">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                    <div style="font-size: 16px; font-weight: 600; color: #333;">
                        ☁️ ${escapeHtml(channel.name || 'MQTT通道 ' + channel.channelId)}
                    </div>
                    <span style="font-size: 12px; padding: 4px 10px; border-radius: 10px; background: ${connected ? '#c8e6c9' : '#f5f5f5'}; color: ${connected ? '#2e7d32' : '#666'};">
                        ${statusText}
                    </span>
                </div>
                <div style="font-size: 13px; color: #666; margin-bottom: 15px;">
                    <div style="margin-bottom: 5px;">🌐 <strong>Broker:</strong> ${escapeHtml(channel.broker)}:${channel.port}</div>
                    <div style="margin-bottom: 5px;">📨 <strong>已发送:</strong> ${channel.messagesSent || 0} 条</div>
                    <div>📥 <strong>已接收:</strong> ${channel.messagesReceived || 0} 条</div>
                </div>
                <div style="display: flex; gap: 8px;">
                    ${connected 
                        ? `<button onclick="disconnectMqttChannel(${channel.channelId})" class="danger" style="flex: 1;">断开</button>`
                        : `<button onclick="connectMqttChannel(${channel.channelId})" class="success" style="flex: 1;">连接</button>`
                    }
                    <button onclick="removeMqttChannel(${channel.channelId})" class="secondary" style="flex: 1;">删除</button>
                </div>
            </div>
        `;
    });
    
    html += '</div>';
    if (container) container.innerHTML = html;
}

/**
 * 打开添加MQTT通道弹窗
 */
function openAddMqttChannelModal() {
    // 设置默认值
    let nextId = 1;
    if (mqttChannelsCache.length > 0) {
        const maxId = Math.max(...mqttChannelsCache.map(c => c.channelId || 0));
        nextId = maxId + 1;
    }
    
    document.getElementById('mqttChannelId').value = nextId;
    document.getElementById('mqttChannelName').value = '';
    document.getElementById('mqttBroker').value = '';
    document.getElementById('mqttPort').value = '1883';
    document.getElementById('mqttClientId').value = '';
    document.getElementById('mqttTopicPrefix').value = 'fanzhou/device/';
    document.getElementById('mqttUsername').value = '';
    document.getElementById('mqttPassword').value = '';
    document.getElementById('mqttKeepAlive').value = '60';
    document.getElementById('mqttQos').value = '0';
    document.getElementById('mqttAutoReconnect').value = 'true';
    
    openModal('mqttChannelModal');
}

/**
 * 添加MQTT通道
 */
function addMqttChannel() {
    const channelId = parseInt(document.getElementById('mqttChannelId').value);
    const name = document.getElementById('mqttChannelName').value.trim();
    const broker = document.getElementById('mqttBroker').value.trim();
    const port = parseInt(document.getElementById('mqttPort').value) || 1883;
    const clientId = document.getElementById('mqttClientId').value.trim();
    const topicPrefix = document.getElementById('mqttTopicPrefix').value.trim();
    const username = document.getElementById('mqttUsername').value.trim();
    const password = document.getElementById('mqttPassword').value;
    const keepAliveSec = parseInt(document.getElementById('mqttKeepAlive').value) || 60;
    const qos = parseInt(document.getElementById('mqttQos').value) || 0;
    const autoReconnect = document.getElementById('mqttAutoReconnect').value === 'true';
    
    if (!broker) {
        alert('请输入Broker地址');
        return;
    }
    
    callMethod('mqtt.channels.add', {
        channelId: channelId,
        name: name || `MQTT通道${channelId}`,
        broker: broker,
        port: port,
        clientId: clientId,
        topicPrefix: topicPrefix,
        username: username,
        password: password,
        keepAliveSec: keepAliveSec,
        qos: qos,
        autoReconnect: autoReconnect,
        enabled: true
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ MQTT通道 ${channelId} 添加成功`);
            closeModal('mqttChannelModal');
            refreshMqttChannels();
        } else if (response.error) {
            log('error', `添加失败: ${response.error.message || '未知错误'}`);
            alert(`添加失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 连接MQTT通道
 */
function connectMqttChannel(channelId) {
    callMethod('mqtt.channels.connect', { channelId: channelId }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `正在连接MQTT通道 ${channelId}...`);
            // 延迟刷新状态
            setTimeout(refreshMqttChannels, 1000);
        } else if (response.error) {
            log('error', `连接失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 断开MQTT通道
 */
function disconnectMqttChannel(channelId) {
    callMethod('mqtt.channels.disconnect', { channelId: channelId }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `已断开MQTT通道 ${channelId}`);
            refreshMqttChannels();
        } else if (response.error) {
            log('error', `断开失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 删除MQTT通道
 */
function removeMqttChannel(channelId) {
    if (!confirm(`确定要删除MQTT通道 ${channelId} 吗？`)) return;
    
    callMethod('mqtt.channels.remove', { channelId: channelId }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `MQTT通道 ${channelId} 已删除`);
            refreshMqttChannels();
        } else if (response.error) {
            log('error', `删除失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 系统资源监控功能
 * 
 * 显示CPU、内存、存储、网络等系统资源使用情况
 * 简化版本 - 无图表，更轻量级
 * ======================================================== */

// 自动刷新定时器
let monitorAutoRefreshTimer = null;
let monitorAutoRefreshEnabled = false;

/**
 * 刷新系统监控数据
 */
function refreshSystemMonitor() {
    callMethod('sys.monitor.current', {}, function(response) {
        if (response.result && response.result.ok) {
            updateMonitorDisplay(response.result);
        } else if (response.error) {
            log('error', `获取系统监控数据失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 更新监控显示
 */
function updateMonitorDisplay(data) {
    // CPU使用率
    const cpuUsage = data.cpu ? data.cpu.total : 0;
    document.getElementById('cpuUsageValue').textContent = cpuUsage.toFixed(1) + '%';
    document.getElementById('cpuCoreInfo').textContent = (data.cpu ? data.cpu.coreCount : '--') + ' 核心';
    
    // 内存使用
    const memUsage = data.memory ? data.memory.usagePercent : 0;
    const memUsed = data.memory ? data.memory.usedMB : 0;
    const memTotal = data.memory ? data.memory.totalMB : 0;
    document.getElementById('memUsageValue').textContent = memUsage.toFixed(1) + '%';
    document.getElementById('memDetailInfo').textContent = `${memUsed.toFixed(0)} / ${memTotal.toFixed(0)} MB`;
    
    // 存储使用（取第一个分区）
    if (data.storages && data.storages.length > 0) {
        const storage = data.storages.find(s => s.mount === '/') || data.storages[0];
        document.getElementById('storageUsageValue').textContent = storage.usagePercent.toFixed(1) + '%';
        document.getElementById('storageDetailInfo').textContent = 
            `${storage.usedGB.toFixed(1)} / ${storage.totalGB.toFixed(1)} GB`;
        
        // 渲染存储详情列表
        let storageHtml = '';
        data.storages.forEach(st => {
            const usageColor = st.usagePercent > 80 ? '#e53935' : (st.usagePercent > 60 ? '#fb8c00' : '#43a047');
            storageHtml += `
                <div style="background: white; padding: 12px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #e0e0e0;">
                    <div style="font-weight: 600; margin-bottom: 8px;">💾 ${escapeHtml(st.mount)}</div>
                    <div style="font-size: 12px; color: #666; margin-bottom: 8px;">文件系统: ${escapeHtml(st.fs)}</div>
                    <div style="background: #e0e0e0; border-radius: 4px; height: 8px; overflow: hidden;">
                        <div style="background: ${usageColor}; height: 100%; width: ${st.usagePercent}%;"></div>
                    </div>
                    <div style="font-size: 12px; color: #666; margin-top: 5px;">
                        ${st.usedGB.toFixed(1)} / ${st.totalGB.toFixed(1)} GB (${st.usagePercent.toFixed(1)}%)
                    </div>
                </div>
            `;
        });
        document.getElementById('storageDetailsList').innerHTML = storageHtml;
    }
    
    // 系统负载
    if (data.load) {
        document.getElementById('loadAvgValue').textContent = data.load.avg1.toFixed(2);
    }
    
    // 运行时间
    if (data.uptimeSec) {
        const hours = Math.floor(data.uptimeSec / 3600);
        const days = Math.floor(hours / 24);
        const remainingHours = hours % 24;
        document.getElementById('uptimeInfo').textContent = 
            days > 0 ? `运行 ${days} 天 ${remainingHours} 小时` : `运行 ${hours} 小时`;
    }
    
    // 网络接口列表
    if (data.networks && data.networks.length > 0) {
        let netHtml = '';
        data.networks.forEach(net => {
            netHtml += `
                <div style="background: white; padding: 12px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #e0e0e0;">
                    <div style="font-weight: 600; margin-bottom: 8px;">🌐 ${escapeHtml(net.interface)}</div>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 12px;">
                        <div>
                            <span style="color: #4caf50;">⬇️</span> 下载: ${net.rxKBps ? net.rxKBps.toFixed(1) : '0.0'} KB/s
                            <div style="color: #999;">总计: ${net.rxMB ? net.rxMB.toFixed(1) : '0.0'} MB</div>
                        </div>
                        <div>
                            <span style="color: #2196f3;">⬆️</span> 上传: ${net.txKBps ? net.txKBps.toFixed(1) : '0.0'} KB/s
                            <div style="color: #999;">总计: ${net.txMB ? net.txMB.toFixed(1) : '0.0'} MB</div>
                        </div>
                    </div>
                </div>
            `;
        });
        document.getElementById('networkInterfacesList').innerHTML = netHtml;
    }
}

/**
 * 切换自动刷新
 */
function toggleAutoRefresh() {
    monitorAutoRefreshEnabled = !monitorAutoRefreshEnabled;
    const btn = document.getElementById('autoRefreshBtn');
    
    if (monitorAutoRefreshEnabled) {
        btn.textContent = '⏸️ 停止刷新';
        btn.classList.add('success');
        monitorAutoRefreshTimer = setInterval(refreshSystemMonitor, 3000);
        log('info', '自动刷新已开启（每3秒）');
    } else {
        btn.textContent = '⏯️ 自动刷新';
        btn.classList.remove('success');
        if (monitorAutoRefreshTimer) {
            clearInterval(monitorAutoRefreshTimer);
            monitorAutoRefreshTimer = null;
        }
        log('info', '自动刷新已停止');
    }
}

/* ========================================================
 * MQTT调试功能
 * 
 * 支持发布消息、订阅管理、消息接收和回调
 * ======================================================== */

// MQTT消息列表（最多保存100条）
let mqttMessages = [];
const MAX_MQTT_MESSAGES = 100;

// MQTT订阅列表缓存
let mqttSubscriptionsCache = [];

// MQTT消息回调函数映射（主题 -> 回调函数）
const mqttMessageCallbacks = new Map();

/**
 * 发布MQTT消息
 */
function mqttPublish() {
    const channelId = parseInt(document.getElementById('mqttDebugChannelId').value);
    const topic = document.getElementById('mqttDebugTopic').value.trim();
    const qos = parseInt(document.getElementById('mqttDebugQos').value);
    const payload = document.getElementById('mqttDebugPayload').value.trim();
    
    if (!topic) {
        alert('请输入主题');
        return;
    }
    
    callMethod('mqtt.publish', {
        channelId: channelId,
        topic: topic,
        payload: payload,
        qos: qos
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 消息已发布到 ${topic}`);
        } else if (response.error) {
            log('error', `发布失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 添加MQTT订阅
 */
function mqttSubscribe() {
    const channelId = parseInt(document.getElementById('mqttSubChannelId').value);
    const topic = document.getElementById('mqttSubTopic').value.trim();
    const qos = parseInt(document.getElementById('mqttSubQos').value);
    
    if (!topic) {
        alert('请输入订阅主题');
        return;
    }
    
    callMethod('mqtt.subscribe', {
        channelId: channelId,
        topic: topic,
        qos: qos
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 已订阅主题: ${topic}`);
            
            // 注册默认回调函数（显示消息）
            registerMqttCallback(topic, function(message) {
                addMqttMessage(message);
            });
            
            // 刷新订阅列表
            mqttListSubscriptions();
        } else if (response.error) {
            log('error', `订阅失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 取消MQTT订阅
 */
function mqttUnsubscribe() {
    const channelId = parseInt(document.getElementById('mqttSubChannelId').value);
    const topic = document.getElementById('mqttSubTopic').value.trim();
    
    if (!topic) {
        alert('请输入要取消的订阅主题');
        return;
    }
    
    callMethod('mqtt.unsubscribe', {
        channelId: channelId,
        topic: topic
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 已取消订阅: ${topic}`);
            
            // 移除回调函数
            unregisterMqttCallback(topic);
            
            // 刷新订阅列表
            mqttListSubscriptions();
        } else if (response.error) {
            log('error', `取消订阅失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 获取MQTT订阅列表
 */
function mqttListSubscriptions() {
    const channelId = parseInt(document.getElementById('mqttSubChannelId').value);
    
    callMethod('mqtt.subscriptions', {
        channelId: channelId
    }, function(response) {
        if (response.result && response.result.ok) {
            mqttSubscriptionsCache = response.result.subscriptions || [];
            renderMqttSubscriptions();
        } else if (response.error) {
            log('error', `获取订阅列表失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 渲染MQTT订阅列表
 */
function renderMqttSubscriptions() {
    const displayEl = document.getElementById('mqttSubscriptionsList');
    const contentEl = document.getElementById('mqttSubscriptionsContent');
    
    if (mqttSubscriptionsCache.length === 0) {
        contentEl.innerHTML = '<span style="color: #999;">暂无订阅</span>';
    } else {
        // 清空内容
        contentEl.innerHTML = '';
        
        mqttSubscriptionsCache.forEach((sub, index) => {
            const span = document.createElement('span');
            span.className = 'mqtt-sub-tag';
            span.style.cssText = 'background: linear-gradient(135deg, #e3f2fd 0%, #bbdefb 100%); color: #1565c0; padding: 6px 12px; border-radius: 20px; font-size: 12px; display: inline-flex; align-items: center; gap: 6px;';
            span.textContent = `📥 ${sub.topic} (QoS ${sub.qos}) `;
            
            const btn = document.createElement('button');
            btn.style.cssText = 'background: none; border: none; color: #e53935; cursor: pointer; padding: 0; font-size: 14px; min-width: auto;';
            btn.textContent = '✕';
            btn.dataset.topic = sub.topic;
            btn.addEventListener('click', function() {
                quickUnsubscribe(this.dataset.topic);
            });
            
            span.appendChild(btn);
            contentEl.appendChild(span);
        });
    }
    displayEl.style.display = 'block';
}

/**
 * 快速取消订阅
 */
function quickUnsubscribe(topic) {
    const channelId = parseInt(document.getElementById('mqttSubChannelId').value);
    
    callMethod('mqtt.unsubscribe', {
        channelId: channelId,
        topic: topic
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 已取消订阅: ${topic}`);
            unregisterMqttCallback(topic);
            mqttListSubscriptions();
        }
    });
}

/**
 * 注册MQTT消息回调函数
 * @param {string} topic - 订阅主题（支持通配符匹配）
 * @param {function} callback - 回调函数，参数为消息对象
 */
function registerMqttCallback(topic, callback) {
    mqttMessageCallbacks.set(topic, callback);
    log('info', `已注册回调: ${topic}`);
}

/**
 * 取消注册MQTT消息回调
 * @param {string} topic - 订阅主题
 */
function unregisterMqttCallback(topic) {
    mqttMessageCallbacks.delete(topic);
}

/**
 * 触发MQTT消息回调
 * @param {object} message - MQTT消息对象
 */
function triggerMqttCallback(message) {
    const topic = message.topic;
    
    // 精确匹配
    if (mqttMessageCallbacks.has(topic)) {
        mqttMessageCallbacks.get(topic)(message);
        return;
    }
    
    // 通配符匹配
    for (const [pattern, callback] of mqttMessageCallbacks) {
        if (matchMqttTopic(pattern, topic)) {
            callback(message);
            return;
        }
    }
}

/**
 * MQTT主题通配符匹配
 * 根据MQTT规范实现通配符匹配：
 * - '#' 必须是最后一个字符，匹配剩余所有层级
 * - '+' 匹配单个层级
 * @param {string} pattern - 订阅主题模式
 * @param {string} topic - 实际主题
 * @returns {boolean} 是否匹配
 */
function matchMqttTopic(pattern, topic) {
    const patternParts = pattern.split('/');
    const topicParts = topic.split('/');
    
    for (let i = 0; i < patternParts.length; i++) {
        // '#' 必须是最后一个部分，匹配剩余所有层级
        if (patternParts[i] === '#') {
            // 根据MQTT规范，'#'必须是最后一个字符
            if (i === patternParts.length - 1) {
                return true;
            }
            // '#'不在最后位置是无效的模式
            return false;
        }
        
        // 检查是否还有topic层级可以匹配
        if (i >= topicParts.length) {
            return false;
        }
        
        // '+' 匹配单个层级
        if (patternParts[i] === '+') {
            continue;
        }
        
        // 精确匹配
        if (patternParts[i] !== topicParts[i]) {
            return false;
        }
    }
    
    // 模式和主题必须有相同的层级数（除非使用了'#'）
    return patternParts.length === topicParts.length;
}

/**
 * 添加MQTT消息到显示列表
 * @param {object} message - MQTT消息对象
 */
function addMqttMessage(message) {
    const now = new Date();
    const time = now.toLocaleTimeString();
    
    mqttMessages.unshift({
        time: time,
        topic: message.topic,
        payload: message.payload,
        qos: message.qos || 0
    });
    
    // 限制消息数量
    if (mqttMessages.length > MAX_MQTT_MESSAGES) {
        mqttMessages.pop();
    }
    
    renderMqttMessages();
}

/**
 * 渲染MQTT消息列表
 */
function renderMqttMessages() {
    const container = document.getElementById('mqttMessagesContainer');
    
    if (mqttMessages.length === 0) {
        container.innerHTML = '<div style="color: #666; text-align: center; padding: 20px;">等待消息...</div>';
        return;
    }
    
    let html = '';
    mqttMessages.forEach(msg => {
        html += `
            <div style="padding: 10px 0; border-bottom: 1px solid #333;">
                <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 8px;">
                    <span style="color: #888; font-size: 11px;">[${msg.time}]</span>
                    <span style="background: #264f78; color: #6cb6ff; padding: 2px 8px; border-radius: 4px; font-size: 11px;">QoS ${msg.qos}</span>
                    <span style="color: #f0c674;">${escapeHtml(msg.topic)}</span>
                </div>
                <div style="color: #d4d4d4; white-space: pre-wrap; word-break: break-all; padding-left: 10px; border-left: 2px solid #444;">${escapeHtml(typeof msg.payload === 'object' ? JSON.stringify(msg.payload, null, 2) : msg.payload)}</div>
            </div>
        `;
    });
    
    container.innerHTML = html;
}

/**
 * 清空MQTT消息
 */
function clearMqttMessages() {
    mqttMessages = [];
    renderMqttMessages();
}

/* ========================================================
 * Token管理功能
 * 
 * 支持创建、查看、删除访问Token
 * Token用于API认证和设备授权
 * ======================================================== */

// Token列表缓存
let tokenListCache = [];

/**
 * 刷新Token列表
 */
function refreshTokenList() {
    // 先获取认证状态
    callMethod('auth.status', {}, function(response) {
        if (response.result) {
            updateAuthStatusDisplay(response.result);
        }
    });
    
    // 获取Token列表
    callMethod('auth.tokens.list', {}, function(response) {
        if (response.result && response.result.ok) {
            tokenListCache = response.result.tokens || [];
            renderTokenList();
        } else if (response.error) {
            log('error', `获取Token列表失败: ${response.error.message || '未知错误'}`);
            // 如果获取失败，可能是认证功能未启用
            document.getElementById('tokenListEmpty').style.display = 'block';
            document.getElementById('tokenListContainer').innerHTML = '';
        }
    });
}

/**
 * 更新认证状态显示
 */
function updateAuthStatusDisplay(authStatus) {
    const enabledEl = document.getElementById('authEnabledStatus');
    const currentTokenEl = document.getElementById('currentTokenStatus');
    const expireEl = document.getElementById('tokenExpireStatus');
    
    if (enabledEl) {
        if (authStatus.enabled) {
            enabledEl.textContent = '✅ 已启用';
            enabledEl.style.color = '#2e7d32';
        } else {
            enabledEl.textContent = '❌ 未启用';
            enabledEl.style.color = '#e53935';
        }
    }
    
    if (currentTokenEl) {
        if (authStatus.currentToken) {
            // 显示Token的前几位
            const tokenPreview = authStatus.currentToken.substring(0, 8) + '...';
            currentTokenEl.textContent = tokenPreview;
            currentTokenEl.style.color = '#2e7d32';
        } else {
            currentTokenEl.textContent = '未登录';
            currentTokenEl.style.color = '#666';
        }
    }
    
    if (expireEl) {
        if (authStatus.tokenExpireSec) {
            const hours = Math.floor(authStatus.tokenExpireSec / 3600);
            if (hours > 24) {
                expireEl.textContent = `${Math.floor(hours / 24)} 天`;
            } else {
                expireEl.textContent = `${hours} 小时`;
            }
        } else {
            expireEl.textContent = '--';
        }
    }
}

/**
 * 渲染Token列表
 * 使用数据索引而非直接嵌入token值以提高安全性
 */
function renderTokenList() {
    const container = document.getElementById('tokenListContainer');
    const emptyEl = document.getElementById('tokenListEmpty');
    
    if (!tokenListCache || tokenListCache.length === 0) {
        if (container) container.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    let html = '';
    tokenListCache.forEach((token, index) => {
        const createdAt = token.createdAt ? new Date(token.createdAt).toLocaleString() : '--';
        const expiresAt = token.expiresAt ? new Date(token.expiresAt).toLocaleString() : '永不过期';
        const isExpired = token.expiresAt && new Date(token.expiresAt) < new Date();
        const tokenId = token.id || index;
        
        html += `
            <div style="background: ${isExpired ? '#ffebee' : '#f8f9fa'}; border-radius: 12px; padding: 20px; border: 2px solid ${isExpired ? '#e53935' : '#e0e0e0'};" data-token-index="${index}">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                    <div style="font-size: 14px; font-weight: 600; color: #333;">
                        🔑 ${escapeHtml(token.name || 'Token ' + (index + 1))}
                    </div>
                    <span style="font-size: 12px; padding: 4px 10px; border-radius: 10px; background: ${isExpired ? '#ffcdd2' : '#c8e6c9'}; color: ${isExpired ? '#c62828' : '#2e7d32'};">
                        ${isExpired ? '已过期' : '有效'}
                    </span>
                </div>
                <div style="font-size: 13px; color: #666; margin-bottom: 15px;">
                    <div style="margin-bottom: 5px; font-family: monospace; background: #e0e0e0; padding: 8px; border-radius: 4px;">
                        ID: ${escapeHtml(String(tokenId))}
                    </div>
                    <div style="margin-bottom: 3px;">📅 创建时间: ${createdAt}</div>
                    <div>⏰ 过期时间: ${expiresAt}</div>
                </div>
                <div style="display: flex; gap: 8px;">
                    <button onclick="copyTokenByIndex(${index})" class="secondary" style="flex: 1;">📋 复制</button>
                    <button onclick="revokeTokenByIndex(${index})" class="danger" style="flex: 1;">🗑️ 撤销</button>
                </div>
            </div>
        `;
    });
    
    if (container) container.innerHTML = html;
}

/**
 * 通过索引复制Token
 */
function copyTokenByIndex(index) {
    if (tokenListCache && tokenListCache[index]) {
        const token = tokenListCache[index].token || tokenListCache[index].id;
        copyToken(token);
    }
}

/**
 * 通过索引撤销Token
 */
function revokeTokenByIndex(index) {
    if (tokenListCache && tokenListCache[index]) {
        const token = tokenListCache[index].token || tokenListCache[index].id;
        revokeToken(token);
    }
}

/**
 * 打开创建Token弹窗
 * 使用模态框代替prompt()以提供更好的用户体验
 */
function openCreateTokenModal() {
    // 重置表单
    document.getElementById('newTokenName').value = 'API Token';
    document.getElementById('newTokenExpireHours').value = '24';
    
    // 打开弹窗
    openModal('createTokenModal');
}

/**
 * 从弹窗创建Token
 */
function createTokenFromModal() {
    const tokenName = document.getElementById('newTokenName').value.trim() || 'API Token';
    const expireHours = parseInt(document.getElementById('newTokenExpireHours').value) || 24;
    
    const params = {
        name: tokenName,
        expireSec: expireHours * 3600
    };
    
    callMethod('auth.tokens.create', params, function(response) {
        if (response.result && response.result.ok) {
            const newToken = response.result.token;
            log('info', `✅ Token创建成功！`);
            
            // 关闭创建弹窗
            closeModal('createTokenModal');
            
            // 显示Token在安全的弹窗中
            showGeneratedToken(newToken);
            
            refreshTokenList();
        } else if (response.error) {
            log('error', `创建Token失败: ${response.error.message || '未知错误'}`);
            alert(`创建Token失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 在安全的弹窗中显示生成的Token
 */
function showGeneratedToken(token) {
    const displayEl = document.getElementById('generatedTokenDisplay');
    const statusEl = document.getElementById('tokenCopyStatus');
    
    if (displayEl) {
        displayEl.value = token;
    }
    if (statusEl) {
        statusEl.style.display = 'none';
    }
    
    openModal('showTokenModal');
}

/**
 * 复制生成的Token
 */
function copyGeneratedToken() {
    const displayEl = document.getElementById('generatedTokenDisplay');
    const statusEl = document.getElementById('tokenCopyStatus');
    
    if (displayEl && displayEl.value) {
        copyToken(displayEl.value);
        
        // 显示复制成功状态
        if (statusEl) {
            statusEl.style.display = 'block';
            setTimeout(function() {
                statusEl.style.display = 'none';
            }, 3000);
        }
    }
}

/**
 * 切换Token显示/隐藏
 */
function toggleTokenVisibility() {
    const displayEl = document.getElementById('generatedTokenDisplay');
    const toggleBtn = document.getElementById('toggleTokenBtn');
    
    if (displayEl && toggleBtn) {
        if (displayEl.type === 'password') {
            displayEl.type = 'text';
            toggleBtn.textContent = '🙈 隐藏';
        } else {
            displayEl.type = 'password';
            toggleBtn.textContent = '👁️ 显示';
        }
    }
}

/**
 * 复制Token到剪贴板
 */
function copyToken(token) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(token).then(function() {
            log('info', '✅ Token已复制到剪贴板');
        }).catch(function(err) {
            log('error', '复制失败: ' + err);
            fallbackCopy(token);
        });
    } else {
        fallbackCopy(token);
    }
}

/**
 * 回退复制方法
 */
function fallbackCopy(text) {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    try {
        document.execCommand('copy');
        log('info', '✅ Token已复制到剪贴板');
    } catch (err) {
        log('error', '复制失败，请手动复制');
    }
    document.body.removeChild(textarea);
}

/**
 * 撤销Token
 */
function revokeToken(token) {
    if (!confirm('确定要撤销此Token吗？撤销后将无法恢复。')) return;
    
    callMethod('auth.tokens.revoke', { token: token }, function(response) {
        if (response.result && response.result.ok) {
            log('info', '✅ Token已撤销');
            refreshTokenList();
        } else if (response.error) {
            log('error', `撤销失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 使用密码登录获取Token
 */
function performLogin() {
    const password = document.getElementById('loginPassword').value;
    
    if (!password) {
        alert('请输入密码');
        return;
    }
    
    callMethod('auth.login', { secret: password }, function(response) {
        if (response.result && response.result.ok) {
            const token = response.result.token;
            log('info', '✅ 登录成功！');
            
            // 保存Token到sessionStorage
            sessionStorage.setItem('auth_token', token);
            
            // 显示Token在安全的弹窗中，不在alert中显示敏感信息
            showGeneratedToken(token);
            
            refreshTokenList();
        } else if (response.error) {
            log('error', `登录失败: ${response.error.message || '未知错误'}`);
            alert(`登录失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 连接设备管理功能
 * 
 * 显示和管理当前连接到RPC服务器的所有客户端
 * ======================================================== */

// 连接列表缓存
let connectionListCache = [];

/**
 * 刷新连接列表
 */
function refreshConnectionList() {
    callMethod('rpc.connections', {}, function(response) {
        if (response.result && response.result.ok) {
            connectionListCache = response.result.connections || [];
            updateConnectionStats(response.result);
            renderConnectionList();
        } else if (response.error) {
            log('error', `获取连接列表失败: ${response.error.message || '未知错误'}`);
            // 显示空状态
            document.getElementById('connectionListEmpty').style.display = 'block';
            document.getElementById('connectionListContainer').innerHTML = '';
        }
    });
}

/**
 * 更新连接统计信息
 */
function updateConnectionStats(data) {
    const totalEl = document.getElementById('totalConnectionsCount');
    const authedEl = document.getElementById('authedConnectionsCount');
    const wsEl = document.getElementById('wsConnectionsCount');
    const tcpEl = document.getElementById('tcpConnectionsCount');
    
    const connections = data.connections || [];
    const total = connections.length;
    const authed = connections.filter(c => c.authenticated === true).length;
    const wsCount = connections.filter(c => c.type === 'websocket' || c.type === 'ws').length;
    const tcpCount = connections.filter(c => c.type === 'tcp').length;
    
    if (totalEl) totalEl.textContent = total;
    if (authedEl) authedEl.textContent = authed;
    if (wsEl) wsEl.textContent = wsCount;
    if (tcpEl) tcpEl.textContent = tcpCount;
}

/**
 * 渲染连接列表
 */
function renderConnectionList() {
    const container = document.getElementById('connectionListContainer');
    const emptyEl = document.getElementById('connectionListEmpty');
    
    if (!connectionListCache || connectionListCache.length === 0) {
        if (container) container.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    let html = '<div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(350px, 1fr)); gap: 15px;">';
    
    connectionListCache.forEach((conn, index) => {
        const isAuthed = conn.authenticated === true;
        const connType = conn.type || 'unknown';
        const connTime = conn.connectedAt ? new Date(conn.connectedAt).toLocaleString() : '--';
        const lastActivity = conn.lastActivityAt ? new Date(conn.lastActivityAt).toLocaleString() : '--';
        const remoteAddr = conn.remoteAddress || conn.ip || 'unknown';
        const connId = conn.id || conn.connectionId || index;
        
        // 根据连接类型选择图标
        const typeIcon = connType === 'websocket' || connType === 'ws' ? '🌐' : '🔌';
        const typeLabel = connType === 'websocket' || connType === 'ws' ? 'WebSocket' : 'TCP';
        
        html += `
            <div style="background: #f8f9fa; border-radius: 12px; padding: 20px; border: 2px solid ${isAuthed ? '#4caf50' : '#ff9800'};">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                    <div style="font-size: 14px; font-weight: 600; color: #333;">
                        ${typeIcon} ${escapeHtml(remoteAddr)}
                    </div>
                    <div style="display: flex; gap: 8px; align-items: center;">
                        <span style="font-size: 11px; padding: 3px 8px; border-radius: 8px; background: #e3f2fd; color: #1565c0;">
                            ${typeLabel}
                        </span>
                        <span style="font-size: 11px; padding: 3px 8px; border-radius: 8px; background: ${isAuthed ? '#c8e6c9' : '#fff3e0'}; color: ${isAuthed ? '#2e7d32' : '#e65100'};">
                            ${isAuthed ? '🔓 已认证' : '🔒 未认证'}
                        </span>
                    </div>
                </div>
                <div style="font-size: 13px; color: #666; margin-bottom: 15px;">
                    <div style="margin-bottom: 3px;">🆔 连接ID: ${escapeHtml(String(connId))}</div>
                    <div style="margin-bottom: 3px;">📅 连接时间: ${connTime}</div>
                    <div style="margin-bottom: 3px;">⏱️ 最后活动: ${lastActivity}</div>
                    ${conn.requestCount ? `<div>📊 请求次数: ${conn.requestCount}</div>` : ''}
                </div>
                <div style="display: flex; gap: 8px;">
                    <button onclick="disconnectClient('${escapeHtml(String(connId))}')" class="danger" style="flex: 1;">断开连接</button>
                </div>
            </div>
        `;
    });
    
    html += '</div>';
    if (container) container.innerHTML = html;
}

/**
 * 断开指定客户端连接
 */
function disconnectClient(connectionId) {
    if (!confirm(`确定要断开连接 ${connectionId} 吗？`)) return;
    
    callMethod('rpc.disconnect', { connectionId: connectionId }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 已断开连接 ${connectionId}`);
            refreshConnectionList();
        } else if (response.error) {
            log('error', `断开失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 断开所有客户端连接
 */
function disconnectAllClients() {
    if (!confirm('确定要断开所有客户端连接吗？这将中断所有当前会话。')) return;
    
    callMethod('rpc.disconnectAll', {}, function(response) {
        if (response.result && response.result.ok) {
            const count = response.result.disconnectedCount || 0;
            log('info', `✅ 已断开 ${count} 个连接`);
            refreshConnectionList();
        } else if (response.error) {
            log('error', `断开失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 开发者调试功能
 * ======================================================== */

/**
 * 设置调试传感器值
 */
function setDebugSensorValue() {
    const sensorId = document.getElementById('debugSensorId').value.trim();
    const identifier = document.getElementById('debugSensorIdentifier').value.trim();
    const value = parseFloat(document.getElementById('debugSensorValue').value);
    
    if (!identifier) {
        log('error', '请填写标识符 (identifier)');
        return;
    }
    
    if (isNaN(value)) {
        log('error', '请输入有效的数值');
        return;
    }
    
    // 策略条件使用 identifier 作为传感器值的键
    // 所以我们直接使用 identifier 作为 sensorId 来设置值
    callMethod('sensor.setValue', { 
        sensorId: identifier, 
        value: value 
    }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 已设置传感器 ${identifier} = ${value}`);
            // 也显示设备ID信息
            if (sensorId) {
                log('info', `   (设备: ${sensorId})`);
            }
        } else if (response.error) {
            log('error', `设置失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 获取调试传感器当前值
 */
function getDebugSensorValue() {
    const identifier = document.getElementById('debugSensorIdentifier').value.trim();
    
    if (!identifier) {
        log('error', '请填写标识符 (identifier)');
        return;
    }
    
    // 使用 sensor.value 获取传感器值
    callMethod('sensor.value', { sensorId: identifier }, function(response) {
        if (response.result) {
            const result = response.result;
            if (result.hasValue) {
                log('info', `📊 传感器 ${identifier} 当前值: ${result.value} (更新时间: ${result.updateTime || 'N/A'})`);
            } else {
                log('warning', `⚠️ 传感器 ${identifier} 暂无数据`);
            }
        } else if (response.error) {
            log('error', `读取失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 快速设置温度值
 */
function setDebugSensorQuick(temperature) {
    document.getElementById('debugSensorId').value = 'test_ab';
    document.getElementById('debugSensorIdentifier').value = 'temperature';
    document.getElementById('debugSensorValue').value = temperature;
    setDebugSensorValue();
}

/**
 * 设置屏幕亮度
 */
function setScreenBrightness() {
    const brightness = parseInt(document.getElementById('debugBrightness').value);
    
    callMethod('sys.brightness.set', { brightness: brightness }, function(response) {
        if (response.result && response.result.ok) {
            log('info', `✅ 亮度已设置为 ${brightness} (路径: ${response.result.path || 'unknown'})`);
        } else if (response.error) {
            log('error', `设置亮度失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 获取当前屏幕亮度
 */
function getScreenBrightness() {
    callMethod('sys.brightness.get', {}, function(response) {
        if (response.result && response.result.ok) {
            const brightness = response.result.brightness;
            document.getElementById('debugBrightness').value = brightness;
            document.getElementById('debugBrightnessValue').textContent = brightness;
            log('info', `📊 当前亮度: ${brightness}`);
        } else if (response.error) {
            log('error', `读取亮度失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 查询4G模块状态
 */
function get4GStatus() {
    const resultEl = document.getElementById('debug4GResult');
    resultEl.style.display = 'block';
    resultEl.textContent = '正在查询4G状态...';
    
    callMethod('sys.4g.status', {}, function(response) {
        if (response.result) {
            const result = response.result;
            let statusText = '';
            
            if (result.parsed) {
                statusText += `=== 4G模块信息 ===\n`;
                statusText += `制造商: ${result.parsed.manufacturer || 'N/A'}\n`;
                statusText += `型号: ${result.parsed.model || 'N/A'}\n`;
                statusText += `状态: ${result.parsed.state || 'N/A'}\n`;
                if (result.parsed.failedReason) {
                    statusText += `失败原因: ${result.parsed.failedReason}\n`;
                }
                statusText += `信号质量: ${result.parsed.signalQuality !== undefined ? result.parsed.signalQuality + '%' : 'N/A'}\n`;
                statusText += `主端口: ${result.parsed.primaryPort || 'N/A'}\n`;
                statusText += `设备ID: ${result.parsed.equipmentId || 'N/A'}\n`;
            }
            
            statusText += `\n=== 网络接口 usb0 ===\n`;
            if (result.usb0Ip) {
                statusText += `IP地址: ${result.usb0Ip}\n`;
            }
            statusText += `状态: ${result.usb0Up ? '运行中' : '未运行'}\n`;
            
            if (result.usb0Info) {
                statusText += `\n详细信息:\n${result.usb0Info}\n`;
            }
            
            resultEl.textContent = statusText;
            log('info', '✅ 4G状态查询完成');
        } else if (response.error) {
            resultEl.textContent = `查询失败: ${response.error.message || '未知错误'}`;
            log('error', `查询4G状态失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/**
 * 4G拨号连接
 */
function connect4G() {
    const resultEl = document.getElementById('debug4GResult');
    resultEl.style.display = 'block';
    resultEl.textContent = '正在执行4G拨号连接...';
    
    callMethod('sys.4g.connect', {}, function(response) {
        if (response.result) {
            const result = response.result;
            let statusText = `=== 4G拨号结果 ===\n`;
            statusText += `总体状态: ${result.ok ? '✅ 成功' : '❌ 失败'}\n\n`;
            
            if (result.steps) {
                result.steps.forEach((step, index) => {
                    statusText += `步骤 ${index + 1}: ${step.step}\n`;
                    statusText += `  命令: ${step.command}\n`;
                    statusText += `  状态: ${step.success ? '✅ 成功' : '❌ 失败'}\n`;
                    if (step.output) statusText += `  输出: ${step.output}\n`;
                    if (step.error) statusText += `  错误: ${step.error}\n`;
                    statusText += '\n';
                });
            }
            
            resultEl.textContent = statusText;
            
            if (result.ok) {
                log('info', '✅ 4G拨号连接成功');
            } else {
                log('warning', '⚠️ 4G拨号连接失败，请检查详细信息');
            }
        } else if (response.error) {
            resultEl.textContent = `连接失败: ${response.error.message || '未知错误'}`;
            log('error', `4G拨号失败: ${response.error.message || '未知错误'}`);
        }
    });
}

/* ========================================================
 * 设置页面功能函数
 * ======================================================== */

/**
 * 从设置页面连接/断开
 */
function toggleConnectionFromSettings() {
    // 同步设置页面的值到主页面
    const settingsHost = document.getElementById('settingsHost');
    const settingsRpcPort = document.getElementById('settingsRpcPort');
    const settingsWsPort = document.getElementById('settingsWsPort');
    const mainHost = document.getElementById('serverHost');
    const mainRpcPort = document.getElementById('rpcPort');
    const mainWsPort = document.getElementById('serverPort');
    
    if (settingsHost && settingsHost.value) {
        if (mainHost) mainHost.value = settingsHost.value;
    }
    if (settingsRpcPort && settingsRpcPort.value) {
        if (mainRpcPort) mainRpcPort.value = settingsRpcPort.value;
    }
    if (settingsWsPort && settingsWsPort.value) {
        if (mainWsPort) mainWsPort.value = settingsWsPort.value;
    }
    
    toggleConnection();
}

/**
 * 从设置页面启动/停止代理
 */
function toggleWebsocatProxyFromSettings() {
    // 同步设置页面的值到主页面
    const settingsHost = document.getElementById('settingsHost');
    const settingsRpcPort = document.getElementById('settingsRpcPort');
    const settingsWsPort = document.getElementById('settingsWsPort');
    const mainHost = document.getElementById('serverHost');
    const mainRpcPort = document.getElementById('rpcPort');
    const mainWsPort = document.getElementById('serverPort');
    
    if (settingsHost && settingsHost.value) {
        if (mainHost) mainHost.value = settingsHost.value;
    }
    if (settingsRpcPort && settingsRpcPort.value) {
        if (mainRpcPort) mainRpcPort.value = settingsRpcPort.value;
    }
    if (settingsWsPort && settingsWsPort.value) {
        if (mainWsPort) mainWsPort.value = settingsWsPort.value;
    }
    
    toggleWebsocatProxy();
}

/**
 * 高级控制 - 继电器控制
 */
function controlRelayAdvanced() {
    const node = parseInt(document.getElementById('advRelayNode').value) || 1;
    const channel = parseInt(document.getElementById('advRelayChannel').value) || 0;
    const action = document.getElementById('advRelayAction').value || 'stop';
    
    callMethod('relay.control', {
        node: node,
        ch: channel,
        action: action
    });
}

/**
 * 高级控制 - 继电器查询
 */
function queryRelayAdvanced() {
    const node = parseInt(document.getElementById('advRelayNode').value) || 1;
    callMethod('relay.statusAll', { node: node });
}

/**
 * 高级控制 - 自定义RPC调用
 */
function callCustomMethodAdvanced() {
    const method = document.getElementById('advMethodName').value.trim();
    if (!method) {
        log('error', '请输入方法名');
        return;
    }
    
    let params = {};
    try {
        const paramsText = document.getElementById('advMethodParams').value.trim();
        if (paramsText) {
            params = JSON.parse(paramsText);
        }
    } catch (e) {
        log('error', '参数格式错误，请使用有效的JSON格式');
        return;
    }
    
    callMethod(method, params);
}

/**
 * 同步设置页面的连接信息
 */
function syncSettingsFields() {
    const mainHost = document.getElementById('serverHost');
    const mainRpcPort = document.getElementById('rpcPort');
    const mainWsPort = document.getElementById('serverPort');
    const settingsHost = document.getElementById('settingsHost');
    const settingsRpcPort = document.getElementById('settingsRpcPort');
    const settingsWsPort = document.getElementById('settingsWsPort');
    
    if (mainHost && settingsHost) {
        settingsHost.value = mainHost.value;
    }
    if (mainRpcPort && settingsRpcPort) {
        settingsRpcPort.value = mainRpcPort.value;
    }
    if (mainWsPort && settingsWsPort) {
        settingsWsPort.value = mainWsPort.value;
    }
}

// 在页面切换时同步设置
const originalShowPage = typeof showPage === 'function' ? showPage : null;
if (originalShowPage) {
    window.showPage = function(pageName) {
        originalShowPage(pageName);
        if (pageName === 'settings') {
            syncSettingsFields();
            // 显示/隐藏代理按钮
            const websocatBtn = document.getElementById('settingsWebsocatBtn');
            if (websocatBtn) {
                websocatBtn.style.display = isTauri ? 'inline-flex' : 'none';
            }
        }
        // 刷新场景列表页面
        if (pageName === 'strategy') {
            refreshSceneList();
        }
        // 刷新MQTT页面
        if (pageName === 'mqtt') {
            refreshMqttChannels();
        }
    };
}

/* ========================================================
 * 泛舟云协议 - 场景管理功能
 * 符合 fanzhoucloud 协议规范
 * ======================================================== */

// 场景数据缓存
let sceneListCache = [];
let timerListCache = [];

/**
 * 刷新场景列表
 * 优先使用 cloud.scene.list，如果不支持则使用 auto.strategy.list 作为降级方案
 */
function refreshSceneList() {
    // 从服务器获取场景列表
    callMethod('cloud.scene.list', {}, function(response) {
        if (response.result) {
            sceneListCache = response.result.scenes || [];
            renderSceneList();
        } else {
            // cloud.scene.list 不支持时，使用 auto.strategy.list 作为降级方案
            // auto.strategy.list 返回的策略包含 type 字段 (auto/manual)，可以作为场景显示
            log('info', 'cloud.scene.list 不可用，使用 auto.strategy.list 获取场景数据');
            callMethod('auto.strategy.list', {}, function(strategyResponse) {
                if (strategyResponse.result) {
                    // 过滤出 auto 和 manual 类型的策略作为场景
                    const strategies = strategyResponse.result.strategies || [];
                    sceneListCache = strategies.filter(s => s.type === 'auto' || s.type === 'manual');
                    renderSceneList();
                } else {
                    log('error', `获取场景列表失败: ${strategyResponse.error?.message || '未知错误'}`);
                    sceneListCache = [];
                    renderSceneList();
                }
            });
        }
    });
    
    // 刷新定时器列表
    callMethod('auto.strategy.list', {}, function(response) {
        if (response.result) {
            timerListCache = response.result.strategies || [];
            renderTimerList();
        }
    });
}

/**
 * 从云端同步场景
 */
function syncSceneFromCloud() {
    log('info', '正在从云端同步场景配置...');
    callMethod('cloud.scene.sync', { id: 0 }, function(response) {
        if (response.result) {
            log('info', '✅ 场景同步成功');
            refreshSceneList();
        } else {
            log('error', `场景同步失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 渲染场景列表
 */
function renderSceneList() {
    const autoContainer = document.getElementById('autoSceneCards');
    const autoEmptyEl = document.getElementById('autoSceneCardsEmpty');
    const autoCountEl = document.getElementById('autoSceneCount');
    
    const manualContainer = document.getElementById('manualSceneCards');
    const manualEmptyEl = document.getElementById('manualSceneCardsEmpty');
    const manualCountEl = document.getElementById('manualSceneCount');
    
    // 分类场景 (兼容服务器字段名: type -> sceneType)
    const autoScenes = sceneListCache.filter(s => s.sceneType === 'auto' || s.strategyType === 'auto' || s.type === 'auto');
    const manualScenes = sceneListCache.filter(s => s.sceneType === 'manual' || s.strategyType === 'manual' || s.type === 'manual');
    
    // 更新计数
    if (autoCountEl) autoCountEl.textContent = autoScenes.length;
    if (manualCountEl) manualCountEl.textContent = manualScenes.length;
    
    // 渲染自动场景
    if (autoScenes.length === 0) {
        if (autoContainer) autoContainer.innerHTML = '';
        if (autoEmptyEl) autoEmptyEl.style.display = 'block';
    } else {
        if (autoEmptyEl) autoEmptyEl.style.display = 'none';
        if (autoContainer) autoContainer.innerHTML = autoScenes.map(scene => renderSceneCard(scene)).join('');
    }
    
    // 渲染手动场景
    if (manualScenes.length === 0) {
        if (manualContainer) manualContainer.innerHTML = '';
        if (manualEmptyEl) manualEmptyEl.style.display = 'block';
    } else {
        if (manualEmptyEl) manualEmptyEl.style.display = 'none';
        if (manualContainer) manualContainer.innerHTML = manualScenes.map(scene => renderSceneCard(scene)).join('');
    }
}

/**
 * 渲染单个场景卡片
 * 兼容服务器返回的字段名 (id -> sceneId, name -> sceneName, type -> sceneType)
 */
function renderSceneCard(scene) {
    // 兼容服务器字段名
    const id = scene.sceneId || scene.strategyId || scene.id || 0;
    const name = scene.sceneName || scene.strategyName || scene.name || `场景${id}`;
    const type = scene.sceneType || scene.strategyType || scene.type || 'auto';
    const enabled = scene.status === 0 || scene.enabled !== false;
    const version = scene.version || 1;
    const matchType = scene.matchType === 1 ? 'OR' : 'AND';
    const conditions = scene.conditions || [];
    const actions = scene.actions || [];
    const effectiveTime = (scene.effectiveBeginTime && scene.effectiveEndTime) 
        ? `${scene.effectiveBeginTime} - ${scene.effectiveEndTime}` 
        : '全天';
    
    const statusClass = enabled ? 'enabled' : 'disabled';
    const statusText = enabled ? '🟢 启用' : '🔴 禁用';
    const typeIcon = type === 'auto' ? '🤖' : '👆';
    const borderColor = enabled ? (type === 'auto' ? '#667eea' : '#e67e22') : '#9e9e9e';
    
    // 生成条件描述 (兼容服务器字段名: value -> identifierValue, le/ge -> elt/egt)
    let conditionDesc = '无条件';
    if (conditions.length > 0) {
        const condTexts = conditions.slice(0, 2).map(c => {
            const opText = { gt: '>', lt: '<', ge: '≥', le: '≤', egt: '≥', elt: '≤', eq: '=', ne: '≠' }[c.op] || c.op;
            // 兼容 value 和 identifierValue 两种字段名
            const condValue = c.identifierValue !== undefined ? c.identifierValue : c.value;
            return `${c.identifier} ${opText} ${condValue}`;
        });
        conditionDesc = condTexts.join(matchType === 'OR' ? ' 或 ' : ' 且 ');
        if (conditions.length > 2) conditionDesc += ` +${conditions.length - 2}条`;
    }
    
    // 生成动作描述 (兼容服务器字段名)
    let actionDesc = '无动作';
    if (actions.length > 0) {
        const actTexts = actions.slice(0, 2).map(a => {
            // 兼容 identifierValue 和 value 两种字段名
            const actValue = a.identifierValue !== undefined ? a.identifierValue : a.value;
            const valText = { 0: '停止', 1: '正转', 2: '反转' }[actValue] || actValue;
            // 兼容 identifier 和 node/channel 两种格式
            const actId = a.identifier || (a.node !== undefined ? `node_${a.node}_ch${a.channel}` : '');
            return `${actId}→${valText}`;
        });
        actionDesc = actTexts.join(', ');
        if (actions.length > 2) actionDesc += ` +${actions.length - 2}个`;
    }
    
    return `
        <div class="scene-card" style="background: white; border-radius: 12px; padding: 16px; border: 2px solid ${borderColor}; box-shadow: 0 2px 8px rgba(0,0,0,0.08);">
            <div style="display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 10px;">
                <div>
                    <div style="font-size: 15px; font-weight: 600; color: #333;">
                        ${typeIcon} ${escapeHtml(name)}
                    </div>
                    <div style="font-size: 11px; color: #999; margin-top: 2px;">ID: ${id} | v${version}</div>
                </div>
                <span style="font-size: 11px; padding: 3px 8px; border-radius: 8px; background: ${enabled ? '#e8f5e9' : '#f5f5f5'}; color: ${enabled ? '#2e7d32' : '#666'};">
                    ${statusText}
                </span>
            </div>
            
            <div style="background: #f8f9fa; padding: 10px; border-radius: 8px; margin-bottom: 10px;">
                <div style="font-size: 11px; color: #666; margin-bottom: 4px;">📊 触发条件 (${matchType})</div>
                <div style="font-size: 12px; color: #333;">${escapeHtml(conditionDesc)}</div>
            </div>
            
            <div style="background: #e8f5e9; padding: 10px; border-radius: 8px; margin-bottom: 10px;">
                <div style="font-size: 11px; color: #666; margin-bottom: 4px;">⚡ 执行动作</div>
                <div style="font-size: 12px; color: #333;">${escapeHtml(actionDesc)}</div>
            </div>
            
            <div style="font-size: 11px; color: #888; margin-bottom: 10px;">
                ⏰ 生效时间: ${effectiveTime}
            </div>
            
            <div style="display: flex; gap: 8px;">
                <button onclick="editScene(${id})" class="secondary" style="flex: 1; padding: 6px; font-size: 11px;">✏️ 编辑</button>
                <button onclick="toggleSceneStatus(${id}, ${enabled ? 1 : 0})" style="flex: 1; padding: 6px; font-size: 11px; background: ${enabled ? '#ff9800' : '#4caf50'}; color: white;">
                    ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                </button>
                ${type === 'manual' ? `<button onclick="triggerScene(${id})" class="success" style="flex: 1; padding: 6px; font-size: 11px;">🚀 执行</button>` : ''}
                <button onclick="deleteScene(${id})" class="danger" style="padding: 6px 10px; font-size: 11px;">🗑️</button>
            </div>
        </div>
    `;
}

/**
 * 渲染定时器列表
 */
function renderTimerList() {
    const container = document.getElementById('timerCards');
    const emptyEl = document.getElementById('timerCardsEmpty');
    const countEl = document.getElementById('timerCount');
    
    // 过滤定时器类型的策略
    const timers = timerListCache.filter(t => t.triggerType || t.dailyTime || t.intervalSec);
    
    if (countEl) countEl.textContent = timers.length;
    
    if (timers.length === 0) {
        if (container) container.innerHTML = '';
        if (emptyEl) emptyEl.style.display = 'block';
        return;
    }
    
    if (emptyEl) emptyEl.style.display = 'none';
    
    let html = '';
    timers.forEach(timer => {
        const id = timer.id || timer.strategyId || 0;
        const name = timer.name || timer.strategyName || `定时器${id}`;
        const enabled = timer.enabled !== false;
        const running = timer.running === true;
        const triggerType = timer.triggerType || (timer.dailyTime ? 'daily' : 'interval');
        const triggerDesc = triggerType === 'daily' 
            ? `每日 ${timer.dailyTime}` 
            : `每 ${formatInterval(timer.intervalSec)} 执行`;
        
        const statusText = running ? '🏃 运行中' : (enabled ? '⏸️ 就绪' : '⚪ 禁用');
        const borderColor = running ? '#4caf50' : (enabled ? '#9c27b0' : '#9e9e9e');
        
        html += `
            <div class="timer-card" style="background: white; border-radius: 12px; padding: 16px; border: 2px solid ${borderColor}; box-shadow: 0 2px 8px rgba(0,0,0,0.08);">
                <div style="display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 10px;">
                    <div>
                        <div style="font-size: 15px; font-weight: 600; color: #333;">
                            ⏰ ${escapeHtml(name)}
                        </div>
                        <div style="font-size: 11px; color: #999; margin-top: 2px;">ID: ${id}</div>
                    </div>
                    <span style="font-size: 11px; padding: 3px 8px; border-radius: 8px; background: ${running ? '#e8f5e9' : '#f5f5f5'}; color: ${running ? '#2e7d32' : '#666'};">
                        ${statusText}
                    </span>
                </div>
                
                <div style="background: #f3e5f5; padding: 10px; border-radius: 8px; margin-bottom: 10px;">
                    <div style="font-size: 12px; color: #333;">🔄 ${triggerDesc}</div>
                </div>
                
                <div style="display: flex; gap: 8px;">
                    <button onclick="toggleTimerEnabled(${id}, ${!enabled})" style="flex: 1; padding: 6px; font-size: 11px; background: ${enabled ? '#ff9800' : '#4caf50'}; color: white;">
                        ${enabled ? '⏸️ 禁用' : '▶️ 启用'}
                    </button>
                    <button onclick="triggerTimer(${id})" class="success" style="flex: 1; padding: 6px; font-size: 11px;">🚀 执行</button>
                    <button onclick="deleteTimer(${id})" class="danger" style="padding: 6px 10px; font-size: 11px;">🗑️</button>
                </div>
            </div>
        `;
    });
    
    if (container) container.innerHTML = html;
}

/**
 * 格式化时间间隔
 */
function formatInterval(seconds) {
    if (!seconds) return '--';
    if (seconds < 60) return `${seconds}秒`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}分钟`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}小时`;
    return `${Math.floor(seconds / 86400)}天`;
}

/**
 * 打开场景编辑器
 * 兼容服务器返回的字段名 (id -> sceneId, name -> sceneName, type -> sceneType)
 */
function openSceneEditorModal(scene = null) {
    const title = document.getElementById('sceneEditorTitle');
    if (title) title.textContent = scene ? '✏️ 编辑场景' : '✨ 创建新场景';
    
    // 清空表单 - 兼容服务器字段名
    document.getElementById('sceneId').value = scene?.sceneId || scene?.strategyId || scene?.id || '';
    document.getElementById('sceneName').value = scene?.sceneName || scene?.strategyName || scene?.name || '';
    document.getElementById('sceneType').value = scene?.sceneType || scene?.strategyType || scene?.type || 'auto';
    document.getElementById('sceneMatchType').value = scene?.matchType || 0;
    // 兼容 status 和 enabled 字段
    const statusValue = scene?.status !== undefined ? scene.status : (scene?.enabled === false ? 1 : 0);
    document.getElementById('sceneStatus').value = statusValue;
    document.getElementById('sceneEffectiveBeginTime').value = scene?.effectiveBeginTime || '00:00';
    document.getElementById('sceneEffectiveEndTime').value = scene?.effectiveEndTime || '23:59';
    
    // 渲染条件和动作
    renderSceneConditions(scene?.conditions || []);
    renderSceneActions(scene?.actions || []);
    
    // 显示/隐藏条件区域
    toggleSceneConditions();
    
    openModal('sceneEditorModal');
}

/**
 * 切换条件区域显示
 */
function toggleSceneConditions() {
    const sceneType = document.getElementById('sceneType').value;
    const conditionsSection = document.getElementById('sceneConditionsSection');
    if (conditionsSection) {
        conditionsSection.style.display = sceneType === 'manual' ? 'none' : 'block';
    }
}

/**
 * 渲染条件列表
 */
function renderSceneConditions(conditions) {
    const container = document.getElementById('sceneConditionsList');
    if (!container) return;
    
    if (conditions.length === 0) {
        // 添加一个空条件模板
        container.innerHTML = getConditionItemHtml();
        return;
    }
    
    container.innerHTML = conditions.map(cond => getConditionItemHtml(cond)).join('');
}

/**
 * 获取条件项HTML
 * 支持自定义属性标识字符串，同时兼容服务器返回的数据格式
 * 服务器字段映射: device -> deviceCode, value -> identifierValue
 */
function getConditionItemHtml(cond = {}) {
    // 兼容服务器返回的字段名 (device -> deviceCode, value -> identifierValue)
    const deviceCode = cond.deviceCode || cond.device || cond.sensor_dev || '';
    const identifier = cond.identifier || '';
    const condValue = cond.identifierValue !== undefined ? cond.identifierValue : (cond.value !== undefined ? cond.value : 30);
    
    return `
        <div class="scene-condition-item" style="background: white; padding: 12px; border-radius: 8px; margin-bottom: 8px;">
            <div class="form-row" style="margin-bottom: 0;">
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">设备编码 <span style="color: #999;">(空则查本地)</span></label>
                    <input type="text" class="cond-device-code" value="${escapeHtml(deviceCode)}" placeholder="空则查本地传感器" style="font-size: 12px;">
                </div>
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">属性标识 <span style="color: #999;">(可自定义)</span></label>
                    <input type="text" class="cond-identifier" value="${escapeHtml(identifier)}" placeholder="temperature" style="font-size: 12px;" list="identifierSuggestions">
                    <datalist id="identifierSuggestions">
                        <option value="airTemp">空气温度</option>
                        <option value="airHum">空气湿度</option>
                        <option value="light">光照强度</option>
                        <option value="co2">CO₂浓度</option>
                        <option value="soilTemp">土壤温度</option>
                        <option value="soilHum">土壤湿度</option>
                        <option value="soilEC">土壤EC</option>
                        <option value="ph">PH值</option>
                        <option value="do">溶解氧</option>
                        <option value="windSpeed">风速</option>
                        <option value="pressure">气压</option>
                        <option value="temperature">温度</option>
                        <option value="humidity">湿度</option>
                    </datalist>
                </div>
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">操作符</label>
                    <select class="cond-op" style="font-size: 12px;">
                        <option value="gt" ${cond.op === 'gt' ? 'selected' : ''}>大于 (&gt;)</option>
                        <option value="lt" ${cond.op === 'lt' ? 'selected' : ''}>小于 (&lt;)</option>
                        <option value="ge" ${cond.op === 'ge' || cond.op === 'egt' ? 'selected' : ''}>大于等于 (≥)</option>
                        <option value="le" ${cond.op === 'le' || cond.op === 'elt' ? 'selected' : ''}>小于等于 (≤)</option>
                        <option value="eq" ${cond.op === 'eq' ? 'selected' : ''}>等于 (=)</option>
                        <option value="ne" ${cond.op === 'ne' ? 'selected' : ''}>不等于 (≠)</option>
                    </select>
                </div>
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">阈值</label>
                    <input type="number" class="cond-value" value="${condValue}" step="0.1" style="font-size: 12px;">
                </div>
                <button onclick="removeSceneCondition(this)" class="danger" style="padding: 6px 10px; font-size: 11px; align-self: flex-end;">✕</button>
            </div>
        </div>
    `;
}

/**
 * 添加条件
 */
function addSceneCondition() {
    const container = document.getElementById('sceneConditionsList');
    if (!container) return;
    const div = document.createElement('div');
    div.innerHTML = getConditionItemHtml();
    container.appendChild(div.firstElementChild);
}

/**
 * 移除条件
 */
function removeSceneCondition(btn) {
    const item = btn.closest('.scene-condition-item');
    if (item) item.remove();
}

/**
 * 渲染动作列表
 */
function renderSceneActions(actions) {
    const container = document.getElementById('sceneActionsList');
    if (!container) return;
    
    if (actions.length === 0) {
        container.innerHTML = getActionItemHtml();
        return;
    }
    
    container.innerHTML = actions.map(act => getActionItemHtml(act)).join('');
}

/**
 * 获取动作项HTML
 * 兼容服务器返回的数据格式 (value -> identifierValue, node/channel -> identifier)
 */
function getActionItemHtml(act = {}) {
    // 兼容服务器返回的字段名 (value -> identifierValue, node/channel -> identifier)
    const actionValue = act.identifierValue !== undefined ? act.identifierValue : (act.value !== undefined ? act.value : 0);
    // 兼容 node/channel 格式
    const actionId = act.identifier || (act.node !== undefined ? `node_${act.node}_ch${act.channel}` : '');
    
    return `
        <div class="scene-action-item" style="background: white; padding: 12px; border-radius: 8px; margin-bottom: 8px;">
            <div class="form-row" style="margin-bottom: 0;">
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">属性标识 <span style="color: #999;">(node_X_swY)</span></label>
                    <input type="text" class="action-identifier" value="${escapeHtml(actionId)}" placeholder="node_1_sw1" style="font-size: 12px;">
                </div>
                <div class="form-group" style="margin-bottom: 0;">
                    <label style="font-size: 11px;">动作值</label>
                    <select class="action-value" style="font-size: 12px;">
                        <option value="0" ${actionValue === 0 ? 'selected' : ''}>⏹️ 停止 (0)</option>
                        <option value="1" ${actionValue === 1 ? 'selected' : ''}>▶️ 正转 (1)</option>
                        <option value="2" ${actionValue === 2 ? 'selected' : ''}>◀️ 反转 (2)</option>
                    </select>
                </div>
                <button onclick="removeSceneAction(this)" class="danger" style="padding: 6px 10px; font-size: 11px; align-self: flex-end;">✕</button>
            </div>
        </div>
    `;
}

/**
 * 添加动作
 */
function addSceneAction() {
    const container = document.getElementById('sceneActionsList');
    if (!container) return;
    const div = document.createElement('div');
    div.innerHTML = getActionItemHtml();
    container.appendChild(div.firstElementChild);
}

/**
 * 移除动作
 */
function removeSceneAction(btn) {
    const item = btn.closest('.scene-action-item');
    if (item) item.remove();
}

/**
 * 收集场景数据
 * 当设备编码为空时，使用 'local' 标识符表示查询本地传感器
 */
function collectSceneData() {
    const sceneId = document.getElementById('sceneId').value;
    const sceneName = document.getElementById('sceneName').value.trim();
    const sceneType = document.getElementById('sceneType').value;
    const matchType = parseInt(document.getElementById('sceneMatchType').value);
    const status = parseInt(document.getElementById('sceneStatus').value);
    const effectiveBeginTime = document.getElementById('sceneEffectiveBeginTime').value;
    const effectiveEndTime = document.getElementById('sceneEffectiveEndTime').value;
    
    // 收集条件
    const conditions = [];
    document.querySelectorAll('#sceneConditionsList .scene-condition-item').forEach(item => {
        const deviceCode = item.querySelector('.cond-device-code').value.trim();
        const identifier = item.querySelector('.cond-identifier').value.trim();
        const op = item.querySelector('.cond-op').value;
        const value = parseFloat(item.querySelector('.cond-value').value);
        
        if (identifier) {
            const cond = {
                identifier: identifier,
                op: op,
                identifierValue: value  // 使用服务器期望的字段名 'identifierValue'
            };
            // 当设备编码为空时，使用 'local' 表示查询本地传感器
            // 服务器会根据此标识使用本地传感器数据
            if (deviceCode) {
                cond.deviceCode = deviceCode;  // 使用服务器期望的字段名 'deviceCode'
            } else {
                cond.deviceCode = 'local';  // 空设备编码时查询本地传感器
            }
            conditions.push(cond);
        }
    });
    
    // 收集动作
    const actions = [];
    document.querySelectorAll('#sceneActionsList .scene-action-item').forEach(item => {
        const identifier = item.querySelector('.action-identifier').value.trim();
        const value = parseInt(item.querySelector('.action-value').value);
        
        if (identifier) {
            actions.push({
                identifier: identifier,
                identifierValue: value  // 使用服务器期望的字段名 'identifierValue'
            });
        }
    });
    
    return {
        sceneId: sceneId ? parseInt(sceneId) : null,
        sceneName: sceneName,
        sceneType: sceneType,
        matchType: matchType,
        status: status,
        effectiveBeginTime: effectiveBeginTime,
        effectiveEndTime: effectiveEndTime,
        conditions: conditions,
        actions: actions,
        version: 1
    };
}

/**
 * 预览场景JSON
 */
function previewSceneJson() {
    const data = collectSceneData();
    const json = JSON.stringify(data, null, 2);
    
    const content = document.getElementById('jsonPreviewContent');
    if (content) {
        content.textContent = json;
    }
    
    openModal('jsonPreviewModal');
}

/**
 * 复制JSON到剪贴板
 */
function copyJsonToClipboard() {
    const content = document.getElementById('jsonPreviewContent');
    if (content) {
        navigator.clipboard.writeText(content.textContent).then(() => {
            log('info', '✅ JSON已复制到剪贴板');
        }).catch(err => {
            log('error', '复制失败: ' + err);
        });
    }
}

/**
 * 保存场景
 */
function saveScene() {
    const data = collectSceneData();
    
    if (!data.sceneName) {
        alert('请输入场景名称');
        return;
    }
    
    if (data.actions.length === 0) {
        alert('请至少添加一个执行动作');
        return;
    }
    
    // 构建请求报文
    const method = data.sceneId ? 'set' : 'set';  // 新建和编辑都用set
    const requestId = `req_${Date.now()}`;
    
    const payload = {
        method: method,
        type: 'scene',
        data: data,
        requestId: requestId,
        timestamp: Date.now()
    };
    
    log('info', `正在${data.sceneId ? '更新' : '创建'}场景...`);
    
    callMethod('cloud.scene.set', payload, function(response) {
        if (response.result && response.result.code === 0) {
            log('info', `✅ 场景${data.sceneId ? '更新' : '创建'}成功`);
            closeModal('sceneEditorModal');
            refreshSceneList();
        } else {
            const errMsg = response.result?.message || response.error?.message || '未知错误';
            log('error', `场景保存失败: ${errMsg}`);
            alert(`保存失败: ${errMsg}`);
        }
    });
}

/**
 * 编辑场景
 * 兼容服务器返回的字段名 (id -> sceneId)
 */
function editScene(id) {
    const scene = sceneListCache.find(s => (s.sceneId || s.strategyId || s.id) === id);
    if (scene) {
        openSceneEditorModal(scene);
    } else {
        log('error', `未找到场景 ID: ${id}`);
    }
}

/**
 * 切换场景状态
 */
function toggleSceneStatus(id, newStatus) {
    callMethod('cloud.scene.status', { sceneId: id, status: newStatus }, function(response) {
        if (response.result) {
            log('info', `✅ 场景状态已${newStatus === 0 ? '启用' : '禁用'}`);
            refreshSceneList();
        } else {
            log('error', `状态切换失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 删除场景
 */
function deleteScene(id) {
    if (!confirm(`确定要删除场景 ID: ${id} 吗？`)) return;
    
    const requestId = `del_${Date.now()}`;
    const payload = {
        method: 'delete',
        type: 'scene',
        data: id,
        requestId: requestId,
        timestamp: Date.now()
    };
    
    callMethod('cloud.scene.delete', payload, function(response) {
        if (response.result && response.result.code === 0) {
            log('info', `✅ 场景已删除`);
            refreshSceneList();
        } else {
            log('error', `删除失败: ${response.result?.message || response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 触发手动场景
 */
function triggerScene(id) {
    callMethod('cloud.scene.trigger', { sceneId: id }, function(response) {
        if (response.result) {
            log('info', `✅ 场景 ${id} 已触发执行`);
        } else {
            log('error', `触发失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 快速创建场景模板
 * 使用服务器期望的字段名 (device, value)
 */
function createQuickScene(template) {
    const templates = {
        'hightemp_vent': {
            sceneName: '高温通风',
            sceneType: 'auto',
            matchType: 0,
            conditions: [{ device: 'local', identifier: 'airTemp', op: 'gt', value: 30 }],
            actions: [{ identifier: 'node_1_sw1', value: 1 }]
        },
        'lowtemp_heat': {
            sceneName: '低温保暖',
            sceneType: 'auto',
            matchType: 0,
            conditions: [{ device: 'local', identifier: 'airTemp', op: 'lt', value: 10 }],
            actions: [{ identifier: 'node_1_sw2', value: 1 }]
        },
        'dry_irrigation': {
            sceneName: '干旱灌溉',
            sceneType: 'auto',
            matchType: 0,
            conditions: [{ device: 'local', identifier: 'soilHum', op: 'lt', value: 30 }],
            actions: [{ identifier: 'node_1_sw3', value: 1 }]
        },
        'light_shade': {
            sceneName: '强光遮阳',
            sceneType: 'auto',
            matchType: 0,
            conditions: [{ device: 'local', identifier: 'light', op: 'gt', value: 50000 }],
            actions: [{ identifier: 'node_1_sw4', value: 1 }]
        }
    };
    
    const sceneData = templates[template];
    if (sceneData) {
        openSceneEditorModal(sceneData);
    }
}

/**
 * 打开定时器编辑器
 */
function openTimerEditorModal(timer = null) {
    const title = document.getElementById('timerEditorTitle');
    if (title) title.textContent = timer ? '✏️ 编辑定时器' : '⏰ 创建定时器';
    
    document.getElementById('timerId').value = timer?.id || timer?.strategyId || '';
    document.getElementById('timerName').value = timer?.name || timer?.strategyName || '';
    document.getElementById('timerTriggerType').value = timer?.triggerType || 'interval';
    document.getElementById('timerInterval').value = timer?.intervalSec || 3600;
    document.getElementById('timerDailyTime').value = timer?.dailyTime || '08:00';
    document.getElementById('timerActionIdentifier').value = timer?.actionIdentifier || 'node_1_sw1';
    document.getElementById('timerActionValue').value = timer?.actionValue || 1;
    document.getElementById('timerStatus').value = timer?.status || 0;
    
    toggleTimerInputs();
    openModal('timerEditorModal');
}

/**
 * 切换定时器输入框
 */
function toggleTimerInputs() {
    const triggerType = document.getElementById('timerTriggerType').value;
    const intervalGroup = document.getElementById('timerIntervalGroup');
    const dailyGroup = document.getElementById('timerDailyGroup');
    
    if (intervalGroup) intervalGroup.style.display = triggerType === 'interval' ? 'block' : 'none';
    if (dailyGroup) dailyGroup.style.display = triggerType === 'daily' ? 'block' : 'none';
}

/**
 * 保存定时器
 */
function saveTimer() {
    const id = document.getElementById('timerId').value;
    const name = document.getElementById('timerName').value.trim();
    const triggerType = document.getElementById('timerTriggerType').value;
    const intervalSec = parseInt(document.getElementById('timerInterval').value);
    const dailyTime = document.getElementById('timerDailyTime').value;
    const actionIdentifier = document.getElementById('timerActionIdentifier').value.trim();
    const actionValue = parseInt(document.getElementById('timerActionValue').value);
    const status = parseInt(document.getElementById('timerStatus').value);
    
    if (!name) {
        alert('请输入定时器名称');
        return;
    }
    
    if (!actionIdentifier) {
        alert('请输入执行动作标识');
        return;
    }
    
    const params = {
        id: id ? parseInt(id) : undefined,
        name: name,
        groupId: 1,
        channel: -1,
        action: actionValue === 1 ? 'fwd' : (actionValue === 2 ? 'rev' : 'stop'),
        triggerType: triggerType,
        intervalSec: triggerType === 'interval' ? intervalSec : undefined,
        dailyTime: triggerType === 'daily' ? dailyTime : undefined,
        enabled: status === 0,
        autoStart: true
    };
    
    callMethod('auto.strategy.create', params, function(response) {
        if (response.result) {
            log('info', `✅ 定时器${id ? '更新' : '创建'}成功`);
            closeModal('timerEditorModal');
            refreshSceneList();
        } else {
            log('error', `定时器保存失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 切换定时器启用状态
 */
function toggleTimerEnabled(id, enabled) {
    callMethod('auto.strategy.enable', { id: id, enabled: enabled }, function(response) {
        if (response.result) {
            log('info', `✅ 定时器已${enabled ? '启用' : '禁用'}`);
            refreshSceneList();
        } else {
            log('error', `状态切换失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 触发定时器
 */
function triggerTimer(id) {
    callMethod('auto.strategy.trigger', { id: id }, function(response) {
        if (response.result) {
            log('info', `✅ 定时器 ${id} 已触发执行`);
        } else {
            log('error', `触发失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 删除定时器
 */
function deleteTimer(id) {
    if (!confirm(`确定要删除定时器 ID: ${id} 吗？`)) return;
    
    callMethod('auto.strategy.delete', { id: id }, function(response) {
        if (response.result) {
            log('info', `✅ 定时器已删除`);
            refreshSceneList();
        } else {
            log('error', `删除失败: ${response.error?.message || '未知错误'}`);
        }
    });
}

/**
 * 测试场景触发（调试用）
 */
function testSceneTrigger() {
    const deviceCode = document.getElementById('debugDeviceCode')?.value || '';
    const identifier = document.getElementById('debugIdentifier')?.value || 'airTemp';
    const value = parseFloat(document.getElementById('debugIdentifierValue')?.value || 25);
    
    log('info', `模拟上报数据: ${identifier} = ${value}`);
    
    // 构造模拟数据
    const data = {};
    data[identifier] = value;
    
    callMethod('sensor.report', {
        deviceCode: deviceCode,
        data: data
    }, function(response) {
        if (response.result) {
            log('info', '✅ 模拟数据上报成功');
        } else {
            log('error', `上报失败: ${response.error?.message || '未知错误'}`);
        }
    });
}
