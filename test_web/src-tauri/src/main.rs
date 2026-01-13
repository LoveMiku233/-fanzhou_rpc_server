// 泛舟RPC调试工具 - Tauri后端
// 
// 主要功能：
// 1. 启动websocat作为WebSocket到TCP的代理
// 2. 管理代理进程的生命周期
// 3. 提供前端调用接口

#![cfg_attr(
    all(not(debug_assertions), target_os = "windows"),
    windows_subsystem = "windows"
)]

use std::sync::{Arc, Mutex};
use tauri::api::process::{Command, CommandChild, CommandEvent};
use tauri::{Manager, State};

/// 存储websocat进程的状态
struct WebsocatState {
    child: Arc<Mutex<Option<CommandChild>>>,
}

/// 启动websocat代理
/// 
/// # 参数
/// - `ws_port`: WebSocket监听端口（默认12346）
/// - `tcp_host`: TCP目标地址（默认127.0.0.1）
/// - `tcp_port`: TCP目标端口（默认12345）
/// 
/// # 返回
/// - 成功返回进程PID
/// - 失败返回错误信息
#[tauri::command]
async fn start_websocat(
    state: State<'_, WebsocatState>,
    ws_port: Option<u16>,
    tcp_host: Option<String>,
    tcp_port: Option<u16>,
) -> Result<u32, String> {
    let ws_port = ws_port.unwrap_or(12346);
    let tcp_host = tcp_host.unwrap_or_else(|| "127.0.0.1".to_string());
    let tcp_port = tcp_port.unwrap_or(12345);

    // 检查是否已有进程在运行
    {
        let child_guard = state.child.lock().map_err(|e| e.to_string())?;
        if child_guard.is_some() {
            return Err("websocat已经在运行中".to_string());
        }
    }

    // 构建websocat参数
    // websocat --text ws-l:0.0.0.0:{ws_port} tcp:{tcp_host}:{tcp_port}
    let ws_listen = format!("ws-l:0.0.0.0:{}", ws_port);
    let tcp_target = format!("tcp:{}:{}", tcp_host, tcp_port);

    let (mut rx, child) = Command::new_sidecar("websocat")
        .map_err(|e| format!("创建sidecar失败: {}", e))?
        .args(["--text", &ws_listen, &tcp_target])
        .spawn()
        .map_err(|e| format!("启动websocat失败: {}", e))?;

    let pid = child.pid();

    // 保存子进程引用
    {
        let mut child_guard = state.child.lock().map_err(|e| e.to_string())?;
        *child_guard = Some(child);
    }

    // 在后台线程中处理输出
    tauri::async_runtime::spawn(async move {
        while let Some(event) = rx.recv().await {
            match event {
                CommandEvent::Stdout(line) => {
                    println!("[websocat stdout] {}", line);
                }
                CommandEvent::Stderr(line) => {
                    eprintln!("[websocat stderr] {}", line);
                }
                CommandEvent::Terminated(payload) => {
                    println!(
                        "[websocat] 进程退出: code={:?}, signal={:?}",
                        payload.code, payload.signal
                    );
                    break;
                }
                _ => {}
            }
        }
    });

    Ok(pid)
}

/// 停止websocat代理
#[tauri::command]
async fn stop_websocat(state: State<'_, WebsocatState>) -> Result<(), String> {
    let mut child_guard = state.child.lock().map_err(|e| e.to_string())?;
    
    if let Some(child) = child_guard.take() {
        child.kill().map_err(|e| format!("停止websocat失败: {}", e))?;
        Ok(())
    } else {
        Err("websocat未在运行".to_string())
    }
}

/// 检查websocat是否在运行
#[tauri::command]
async fn is_websocat_running(state: State<'_, WebsocatState>) -> Result<bool, String> {
    let child_guard = state.child.lock().map_err(|e| e.to_string())?;
    Ok(child_guard.is_some())
}

/// 获取websocat进程的PID
#[tauri::command]
async fn get_websocat_pid(state: State<'_, WebsocatState>) -> Result<Option<u32>, String> {
    let child_guard = state.child.lock().map_err(|e| e.to_string())?;
    Ok(child_guard.as_ref().map(|c| c.pid()))
}

fn main() {
    tauri::Builder::default()
        .manage(WebsocatState {
            child: Arc::new(Mutex::new(None)),
        })
        .invoke_handler(tauri::generate_handler![
            start_websocat,
            stop_websocat,
            is_websocat_running,
            get_websocat_pid,
        ])
        .run(tauri::generate_context!())
        .expect("运行Tauri应用失败");
}
