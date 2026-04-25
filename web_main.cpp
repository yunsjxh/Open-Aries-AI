#include "web_server.hpp"
#include "provider_manager.hpp"
#include "mcp_client.hpp"
#include "file_manager.hpp"
#include "screenshot.hpp"
#include "action_parser.hpp"
#include "action_executor.hpp"
#include "ui_automation.hpp"
#include "prompt_templates.hpp"
#include "update_checker.hpp"
#include "status_window.hpp"
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <conio.h>
#include <cctype>

using namespace aries;

static const char* EMBEDDED_INDEX_HTML = R"htmlend(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Open-Aries-AI - Dashboard</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #0f0f14;
            --bg-secondary: #16161d;
            --bg-tertiary: #1c1c26;
            --bg-card: #1e1e28;
            --bg-hover: #252532;
            --border-color: #2a2a3a;
            --border-light: #3a3a4a;
            --text-primary: #f0f0f5;
            --text-secondary: #a0a0b0;
            --text-muted: #6a6a7a;
            --accent-primary: #6366f1;
            --accent-secondary: #818cf8;
            --accent-gradient: linear-gradient(135deg, #6366f1 0%, #8b5cf6 100%);
            --success: #22c55e;
            --warning: #f59e0b;
            --error: #ef4444;
            --info: #3b82f6;
            --sidebar-width: 260px;
            --header-height: 64px;
            --radius-sm: 6px;
            --radius-md: 10px;
            --radius-lg: 14px;
            --shadow-sm: 0 1px 2px rgba(0,0,0,0.3);
            --shadow-md: 0 4px 12px rgba(0,0,0,0.4);
            --shadow-lg: 0 8px 24px rgba(0,0,0,0.5);
            --transition-fast: 150ms ease;
            --transition-normal: 250ms ease;
        }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: var(--bg-primary); color: var(--text-primary); min-height: 100vh; line-height: 1.5; }
        .app-container { display: flex; min-height: 100vh; }
        .sidebar { width: var(--sidebar-width); background: var(--bg-secondary); border-right: 1px solid var(--border-color); display: flex; flex-direction: column; position: fixed; top: 0; left: 0; height: 100vh; z-index: 100; transition: transform var(--transition-normal); }
        .sidebar-header { padding: 20px; border-bottom: 1px solid var(--border-color); }
        .logo { display: flex; align-items: center; gap: 12px; }
        .logo-icon { width: 40px; height: 40px; background: var(--accent-gradient); border-radius: var(--radius-md); display: flex; align-items: center; justify-content: center; }
        .logo-icon svg { width: 24px; height: 24px; color: white; }
        .logo-text { font-size: 18px; font-weight: 700; background: var(--accent-gradient); -webkit-background-clip: text; -webkit-text-fill-color: transparent; background-clip: text; }
        .logo-version { font-size: 11px; color: var(--text-muted); font-weight: 500; }
        .sidebar-nav { flex: 1; padding: 16px 12px; overflow-y: auto; }
        .nav-section { margin-bottom: 24px; }
        .nav-section-title { font-size: 11px; font-weight: 600; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; padding: 0 12px; margin-bottom: 8px; }
        .nav-item { display: flex; align-items: center; gap: 12px; padding: 12px; border-radius: var(--radius-md); color: var(--text-secondary); cursor: pointer; transition: all var(--transition-fast); margin-bottom: 4px; }
        .nav-item:hover { background: var(--bg-hover); color: var(--text-primary); }
        .nav-item.active { background: rgba(99, 102, 241, 0.15); color: var(--accent-secondary); }
        .nav-item svg { width: 20px; height: 20px; flex-shrink: 0; }
        .nav-item span { font-size: 14px; font-weight: 500; }
        .sidebar-footer { padding: 16px; border-top: 1px solid var(--border-color); }
        .status-indicator { display: flex; align-items: center; gap: 10px; padding: 12px; background: var(--bg-tertiary); border-radius: var(--radius-md); }
        .status-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); animation: pulse 2s infinite; }
        .status-dot.error { background: var(--error); }
        .status-dot.warning { background: var(--warning); }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
        .status-text { font-size: 13px; color: var(--text-secondary); }
        .main-content { flex: 1; margin-left: var(--sidebar-width); display: flex; flex-direction: column; }
        .header { height: var(--header-height); background: var(--bg-secondary); border-bottom: 1px solid var(--border-color); display: flex; align-items: center; justify-content: space-between; padding: 0 24px; position: sticky; top: 0; z-index: 50; }
        .header-title { font-size: 18px; font-weight: 600; }
        .header-actions { display: flex; align-items: center; gap: 12px; }
        .header-stat { display: flex; align-items: center; gap: 8px; padding: 8px 14px; background: var(--bg-tertiary); border-radius: var(--radius-md); font-size: 13px; }
        .header-stat-label { color: var(--text-muted); }
        .header-stat-value { color: var(--text-primary); font-weight: 600; }
        .content-area { flex: 1; padding: 24px; overflow-y: auto; }
        .page { display: none; }
        .page.active { display: block; }
        .card { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: var(--radius-lg); padding: 24px; margin-bottom: 20px; }
        .card-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 20px; }
        .card-title { font-size: 16px; font-weight: 600; display: flex; align-items: center; gap: 10px; }
        .card-title svg { width: 20px; height: 20px; color: var(--accent-secondary); }
        .input-group { margin-bottom: 16px; }
        .checkbox-label { display: flex; align-items: center; gap: 8px; font-size: 14px; cursor: pointer; user-select: none; }
        .checkbox-label input[type="checkbox"] { width: 18px; height: 18px; cursor: pointer; }
        .input-label { display: block; font-size: 13px; font-weight: 500; color: var(--text-secondary); margin-bottom: 8px; }
        .input-field { width: 100%; padding: 12px 16px; background: var(--bg-tertiary); border: 1px solid var(--border-color); border-radius: var(--radius-md); color: var(--text-primary); font-size: 14px; font-family: inherit; transition: border-color var(--transition-fast); }
        .input-field:focus { outline: none; border-color: var(--accent-primary); }
        .input-field::placeholder { color: var(--text-muted); }
        textarea.input-field { min-height: 120px; resize: vertical; line-height: 1.6; }
        .input-row { display: grid; grid-template-columns: repeat(2, 1fr); gap: 16px; }
        .btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; padding: 10px 20px; border: none; border-radius: var(--radius-md); font-size: 14px; font-weight: 500; font-family: inherit; cursor: pointer; transition: all var(--transition-fast); }
        .btn svg { width: 18px; height: 18px; }
        .btn-primary { background: var(--accent-gradient); color: white; }
        .btn-primary:hover { opacity: 0.9; transform: translateY(-1px); }
        .btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border-color); }
        .btn-secondary:hover { background: var(--bg-hover); border-color: var(--border-light); }
        .btn-danger { background: rgba(239, 68, 68, 0.15); color: var(--error); border: 1px solid rgba(239, 68, 68, 0.3); }
        .btn-danger:hover { background: rgba(239, 68, 68, 0.25); }
        .btn-group { display: flex; gap: 10px; margin-top: 20px; }
        .log-container { background: var(--bg-primary); border: 1px solid var(--border-color); border-radius: var(--radius-md); padding: 16px; max-height: 400px; overflow-y: auto; font-family: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace; font-size: 13px; line-height: 1.7; }
        .log-entry { padding: 4px 0; border-bottom: 1px solid var(--border-color); display: flex; gap: 12px; }
        .log-entry:last-child { border-bottom: none; }
        .log-time { color: var(--text-muted); flex-shrink: 0; }
        .log-message { color: var(--text-secondary); }
        .log-entry.info .log-message { color: var(--info); }
        .log-entry.success .log-message { color: var(--success); }
        .log-entry.error .log-message { color: var(--error); }
        .log-entry.warning .log-message { color: var(--warning); }
        .server-grid { display: grid; gap: 16px; }
        .server-card { background: var(--bg-tertiary); border: 1px solid var(--border-color); border-radius: var(--radius-md); padding: 16px; display: flex; justify-content: space-between; align-items: flex-start; }
        .server-info h4 { font-size: 15px; font-weight: 600; margin-bottom: 4px; }
        .server-info p { font-size: 13px; color: var(--text-muted); }
        .server-badge { padding: 4px 10px; background: rgba(34, 197, 94, 0.15); color: var(--success); border-radius: var(--radius-sm); font-size: 12px; font-weight: 500; }
        .tools-section { margin-top: 12px; padding-top: 12px; border-top: 1px solid var(--border-color); }
        .tool-tag { display: inline-block; padding: 4px 10px; background: var(--bg-hover); border-radius: var(--radius-sm); font-size: 12px; margin-right: 8px; margin-bottom: 8px; }
        .tool-tag strong { color: var(--accent-secondary); }
        .provider-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 12px; }
        .provider-card { background: var(--bg-tertiary); border: 2px solid var(--border-color); border-radius: var(--radius-md); padding: 16px; text-align: center; cursor: pointer; transition: all var(--transition-fast); }
        .provider-card:hover { border-color: var(--border-light); background: var(--bg-hover); }
        .provider-card.active { border-color: var(--accent-primary); background: rgba(99, 102, 241, 0.1); }
        .provider-icon { width: 40px; height: 40px; margin: 0 auto 10px; background: var(--bg-hover); border-radius: var(--radius-md); display: flex; align-items: center; justify-content: center; }
        .provider-icon svg { width: 24px; height: 24px; color: var(--text-secondary); }
        .provider-card.active .provider-icon svg { color: var(--accent-secondary); }
        .provider-name { font-size: 14px; font-weight: 600; margin-bottom: 4px; }
        .provider-model { font-size: 12px; color: var(--text-muted); }
        .provider-actions { display: flex; gap: 4px; margin-top: 8px; justify-content: center; }
        .btn-icon { background: var(--bg-hover); border: 1px solid var(--border-color); border-radius: var(--radius-sm); padding: 4px; cursor: pointer; color: var(--text-muted); transition: all var(--transition-fast); }
        .btn-icon:hover { background: var(--bg-tertiary); color: var(--text-primary); border-color: var(--border-light); }
        .screenshot-preview { max-width: 100%; border-radius: var(--radius-md); border: 1px solid var(--border-color); margin-top: 16px; }
        .empty-state { text-align: center; padding: 40px 20px; color: var(--text-muted); }
        .empty-state svg { width: 48px; height: 48px; margin-bottom: 16px; opacity: 0.5; }
        .empty-state p { font-size: 14px; }
        .mobile-menu-btn { display: none; padding: 8px; background: none; border: none; color: var(--text-primary); cursor: pointer; }
        .mobile-menu-btn svg { width: 24px; height: 24px; }
        @media (max-width: 768px) {
            .sidebar { transform: translateX(-100%); }
            .sidebar.open { transform: translateX(0); }
            .main-content { margin-left: 0; }
            .mobile-menu-btn { display: block; }
            .input-row { grid-template-columns: 1fr; }
            .header-stats { display: none; }
        }
        .overlay { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0, 0, 0, 0.5); z-index: 99; }
        .overlay.active { display: block; }
        .checkbox-wrapper { display: flex; align-items: center; gap: 10px; cursor: pointer; }
        .checkbox-wrapper input[type="checkbox"] { width: 18px; height: 18px; accent-color: var(--accent-primary); cursor: pointer; }
        .select-field { width: 100%; padding: 12px 16px; background: var(--bg-tertiary); border: 1px solid var(--border-color); border-radius: var(--radius-md); color: var(--text-primary); font-size: 14px; font-family: inherit; cursor: pointer; appearance: none; background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='%23a0a0b0' stroke-width='2'%3E%3Cpolyline points='6 9 12 15 18 9'%3E%3C/polyline%3E%3C/svg%3E"); background-repeat: no-repeat; background-position: right 12px center; }
        .select-field:focus { outline: none; border-color: var(--accent-primary); }
    </style>
</head>
<body>
    <div class="app-container">
        <div class="overlay" id="overlay" onclick="toggleSidebar()"></div>
        <aside class="sidebar" id="sidebar">
            <div class="sidebar-header">
                <div class="logo">
                    <div class="logo-icon">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                            <path d="M12 2L2 7l10 5 10-5-10-5z"/>
                            <path d="M2 17l10 5 10-5"/>
                            <path d="M2 12l10 5 10-5"/>
                        </svg>
                    </div>
                    <div>
                        <div class="logo-text">Open-Aries-AI</div>
                        <div class="logo-version">v1.3.1</div>
                    </div>
                </div>
            </div>
            <nav class="sidebar-nav">
                <div class="nav-section">
                    <div class="nav-section-title">主要</div>
                    <div class="nav-item active" data-page="task" onclick="showPage('task')">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5 3 19 12 5 21 5 3"/></svg>
                        <span>任务执行</span>
                    </div>
                    <div class="nav-item" data-page="mcp" onclick="showPage('mcp')">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 4h16c1.1 0 2 .9 2 2v12c0 1.1-.9 2-2 2H4c-1.1 0-2-.9-2-2V6c0-1.1.9-2 2-2z"/><polyline points="22,6 12,13 2,6"/></svg>
                        <span>MCP 服务器</span>
                    </div>
                    <div class="nav-item" data-page="logs" onclick="showPage('logs')">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
                        <span>系统日志</span>
                    </div>
                </div>
                <div class="nav-section">
                    <div class="nav-section-title">配置</div>
                    <div class="nav-item" data-page="settings" onclick="showPage('settings')">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
                        <span>设置</span>
                    </div>
                </div>
            </nav>
            <div class="sidebar-footer">
                <div class="status-indicator">
                    <div class="status-dot" id="statusDot"></div>
                    <div class="status-text" id="statusText">就绪</div>
                </div>
            </div>
        </aside>
        <main class="main-content">
            <header class="header">
                <div style="display: flex; align-items: center; gap: 16px;">
                    <button class="mobile-menu-btn" onclick="toggleSidebar()">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="18" x2="21" y2="18"/></svg>
                    </button>
                    <h1 class="header-title" id="pageTitle">任务执行</h1>
                </div>
                <div class="header-actions">
                    <div class="header-stat"><span class="header-stat-label">提供商:</span><span class="header-stat-value" id="currentProvider">-</span></div>
                    <div class="header-stat"><span class="header-stat-label">模型:</span><span class="header-stat-value" id="currentModel">-</span></div>
                    <div class="header-stat"><span class="header-stat-label">MCP:</span><span class="header-stat-value" id="mcpCount">0</span></div>
                    <button class="btn btn-secondary" onclick="checkUpdate()" title="检查更新" style="padding: 8px 12px;"><svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 12a9 9 0 11-6.219-8.56"/><polyline points="21 3 21 9 15 9"/></svg></button>
                    <a href="https://github.com/yunsjxh/Open-Aries-AI" target="_blank" class="btn btn-secondary" title="GitHub" style="padding: 8px 12px; text-decoration: none;"><svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0024 12c0-6.63-5.37-12-12-12z"/></svg></a>
                </div>
            </header>
            <div class="content-area">
                <div class="page active" id="taskPage">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>任务目标</h3>
                        </div>
                        <div class="input-group">
                            <textarea class="input-field" id="taskInput" placeholder="输入任务目标，例如：打开记事本并写入 Hello World"></textarea>
                        </div>
                        <div class="input-row">
                            <div class="input-group">
                                <label class="input-label">模型名称 (可选，留空使用默认) <span id="modelTestStatus" style="font-weight: normal; font-size: 12px;"></span></label>
                                <div style="display: flex; gap: 8px;">
                                    <input type="text" class="input-field" id="modelNameInput" placeholder="例如: gpt-4o, deepseek-chat" style="flex: 1;" onchange="testModelAvailability()" />
                                    <button class="btn btn-secondary" onclick="testModelAvailability()" title="测试模型可用性" style="padding: 8px 12px; white-space: nowrap;"><svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg> 测试</button>
                                </div>
                            </div>
                            <div class="input-group">
                                <label class="input-label">最大迭代次数</label>
                                <input type="number" class="input-field" id="maxIterationsInput" value="10" min="1" max="50" />
                            </div>
                        </div>
                        <div class="input-row">
                            <div class="input-group">
                                <label class="checkbox-label">
                                    <input type="checkbox" id="useVisionModel" checked onchange="toggleVisionProvider()" />
                                    当前模型支持视觉（图像识别）
                                </label>
                            </div>
                        </div>
                        <div id="visionProviderSection" style="display:none;">
                            <div class="input-row">
                                <div class="input-group">
                                    <label class="checkbox-label">
                                        <input type="checkbox" id="useVisionTranscribe" checked />
                                        使用视觉模型转述屏幕内容
                                    </label>
                                </div>
                            </div>
                            <div class="input-row">
                                <div class="input-group">
                                    <label class="input-label">视觉模型提供商</label>
                                    <select class="input-field" id="visionProviderSelect"><option value="">-- 选择提供商 --</option></select>
                                </div>
                                <div class="input-group">
                                    <label class="input-label">视觉模型名称</label>
                                    <input type="text" class="input-field" id="visionModelName" placeholder="例如: gpt-4o, Qwen2-VL-7B" />
                                </div>
                            </div>
                        </div>
                        <div class="btn-group">
                            <button class="btn btn-primary" onclick="executeTask()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5 3 19 12 5 21 5 3"/></svg>执行</button>
                            <button class="btn btn-secondary" onclick="stopTask()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="6" y="6" width="12" height="12"/></svg>停止</button>
                            <button class="btn btn-secondary" onclick="takeScreenshot()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>截图</button>
                        </div>
                    </div>
                    <div id="screenshotContainer" style="display:none;">
                        <div class="card">
                            <div class="card-header">
                                <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>屏幕截图</h3>
                            </div>
                            <img id="screenshotPreview" class="screenshot-preview" />
                        </div>
                    </div>
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/></svg>执行日志</h3>
                        </div>
                        <div class="log-container" id="taskLog">
                            <div class="log-entry info"><span class="log-time">--:--:--</span><span class="log-message">等待任务输入...</span></div>
                        </div>
                    </div>
                </div>
                <div class="page" id="mcpPage">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>连接 MCP 服务器</h3>
                        </div>
                        <div class="input-row">
                            <div class="input-group">
                                <label class="input-label">服务器名称</label>
                                <input type="text" class="input-field" id="mcpName" placeholder="例如: fetch" />
                            </div>
                            <div class="input-group">
                                <label class="input-label">服务器类型</label>
                                <select class="select-field" id="mcpType">
                                    <option value="http">HTTP (streamable_http)</option>
                                    <option value="stdio">Stdio (本地进程)</option>
                                </select>
                            </div>
                        </div>
                        <div class="input-group" id="httpUrlGroup">
                            <label class="input-label">HTTP URL</label>
                            <input type="text" class="input-field" id="mcpUrl" placeholder="例如: https://mcp.example.com/mcp" />
                        </div>
                        <div class="input-group" id="stdioGroup" style="display:none;">
                            <label class="input-label">启动命令</label>
                            <input type="text" class="input-field" id="mcpCommand" placeholder="例如: npx -y @modelcontextprotocol/server-filesystem" />
                        </div>
                        <div class="btn-group">
                            <button class="btn btn-primary" onclick="connectMcp()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/></svg>连接</button>
                            <button class="btn btn-danger" onclick="disconnectAllMcp()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>断开所有</button>
                        </div>
                    </div>
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg>已连接的服务器</h3>
                        </div>
                        <div class="server-grid" id="mcpServersList">
                            <div class="empty-state">
                                <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg>
                                <p>暂无已连接的服务器</p>
                            </div>
                        </div>
                    </div>
                </div>
                <div class="page" id="settingsPage">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/></svg>AI 提供商</h3>
                        </div>
                        <div class="provider-grid" id="providerList"></div>
                        <div id="customProviderForm" style="display:none; margin-top: 20px; padding-top: 20px; border-top: 1px solid var(--border-color);">
                            <div class="input-row">
                                <div class="input-group">
                                    <label class="input-label">提供商名称</label>
                                    <input type="text" class="input-field" id="customProviderName" placeholder="例如: siliconflow" />
                                </div>
                                <div class="input-group">
                                    <label class="input-label">API Base URL</label>
                                    <input type="text" class="input-field" id="customProviderUrl" placeholder="例如: https://api.siliconflow.cn/v1" />
                                </div>
                            </div>
                            <div class="input-group">
                                <label class="input-label">API Key</label>
                                <input type="password" class="input-field" id="customProviderApiKey" placeholder="输入 API Key" />
                            </div>
                            <button class="btn btn-primary" onclick="saveProvider()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg><span id="providerBtnText">添加提供商</span></button>
                        </div>
                    </div>
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>高级设置</h3>
                        </div>
                        <div class="checkbox-wrapper">
                            <input type="checkbox" id="visionMode" />
                            <label class="checkbox-label" for="visionMode">启用视觉模式（模型支持图像识别）</label>
                        </div>
                    </div>
                </div>
                <div class="page" id="logsPage">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>系统日志</h3>
                            <div class="btn-group" style="margin: 0;">
                                <button class="btn btn-secondary" onclick="refreshLogs()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/></svg>刷新</button>
                                <button class="btn btn-secondary" onclick="clearLogs()"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>清空</button>
                            </div>
                        </div>
                        <div class="log-container" id="systemLog" style="max-height: 500px;">
                            <div class="log-entry info"><span class="log-time">--:--:--</span><span class="log-message">加载中...</span></div>
                        </div>
                    </div>
                </div>
            </div>
        </main>
    </div>
    <script>
        const API_BASE = '';
        let currentSelectedProvider = '';
        let isLoadingSettings = false;
        let isLoadingMcp = false;
        let isLoadingLogs = false;
        let currentPage = 'task';
        const pageTitles = { task: '任务执行', mcp: 'MCP 服务器', settings: '设置', logs: '系统日志' };
        function toggleVisionProvider() {
            const useVision = document.getElementById('useVisionModel').checked;
            const section = document.getElementById('visionProviderSection');
            section.style.display = useVision ? 'none' : 'block';
            if (!useVision) refreshVisionProviders();
        }
        async function refreshVisionProviders() {
            try {
                const response = await fetch(API_BASE + '/api/settings');
                const result = await response.json();
                const select = document.getElementById('visionProviderSelect');
                select.innerHTML = '<option value="">-- 选择提供商 --</option>';
                if (result.providers) {
                    result.providers.forEach(p => {
                        const option = document.createElement('option');
                        option.value = p.name;
                        option.textContent = p.name + (p.hasKey ? ' ✓' : '');
                        select.appendChild(option);
                    });
                }
            } catch (e) { console.error('Failed to refresh vision providers:', e); }
        }
        function toggleSidebar() {
            document.getElementById('sidebar').classList.toggle('open');
            document.getElementById('overlay').classList.toggle('active');
        }
        function showPage(name) {
            if (currentPage === name) return;
            currentPage = name;
            document.querySelectorAll('.nav-item').forEach(item => item.classList.remove('active'));
            document.querySelectorAll('.page').forEach(page => page.classList.remove('active'));
            document.querySelector('.nav-item[data-page="' + name + '"]').classList.add('active');
            document.getElementById(name + 'Page').classList.add('active');
            document.getElementById('pageTitle').textContent = pageTitles[name];
            if (window.innerWidth <= 768) toggleSidebar();
            if (name === 'mcp') refreshMcpServers();
            if (name === 'logs') refreshLogs();
            if (name === 'settings') refreshSettings();
        }
        function log(message, type = 'info') {
            const logContainer = document.getElementById('taskLog');
            const entry = document.createElement('div');
            entry.className = 'log-entry ' + type;
            const time = new Date().toLocaleTimeString();
            entry.innerHTML = '<span class="log-time">' + time + '</span><span class="log-message">' + message + '</span>';
            logContainer.appendChild(entry);
            logContainer.scrollTop = logContainer.scrollHeight;
        }
        let logRefreshInterval = null;
        let lastLogCount = 0;
        async function refreshTaskLogs() {
            try {
                const response = await fetch(API_BASE + '/api/logs');
                const result = await response.json();
                if (result.logs && result.logs.length > lastLogCount) {
                    const logContainer = document.getElementById('taskLog');
                    for (let i = lastLogCount; i < result.logs.length; i++) {
                        const l = result.logs[i];
                        const entry = document.createElement('div');
                        entry.className = 'log-entry ' + (l.type || 'info');
                        entry.innerHTML = '<span class="log-time">' + l.time + '</span><span class="log-message">' + l.message + '</span>';
                        logContainer.appendChild(entry);
                    }
                    logContainer.scrollTop = logContainer.scrollHeight;
                    lastLogCount = result.logs.length;
                }
            } catch (e) { console.error('Failed to refresh task logs:', e); }
        }
        async function executeTask() {
            const task = document.getElementById('taskInput').value.trim();
            if (!task) { alert('请输入任务目标'); return; }
            const model = document.getElementById('modelNameInput').value.trim();
            const maxIterations = parseInt(document.getElementById('maxIterationsInput').value) || 10;
            const useVision = document.getElementById('useVisionModel').checked;
            let visionConfig = null;
            if (!useVision) {
                const useTranscribe = document.getElementById('useVisionTranscribe').checked;
                if (useTranscribe) {
                    const visionProvider = document.getElementById('visionProviderSelect').value;
                    const visionModel = document.getElementById('visionModelName').value.trim();
                    if (visionProvider) visionConfig = { provider: visionProvider, model: visionModel };
                }
            }
            document.getElementById('taskLog').innerHTML = '';
            lastLogCount = 0;
            document.getElementById('statusText').textContent = '执行中...';
            document.getElementById('statusDot').classList.add('error');
            try {
                const response = await fetch(API_BASE + '/api/task', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ task, model, maxIterations, useVision, visionConfig })
                });
                const result = await response.json();
                if (result.success) {
                    logRefreshInterval = setInterval(() => { refreshTaskLogs(); checkTaskStatus(); }, 500);
                } else {
                    log('任务失败: ' + result.error, 'error');
                    document.getElementById('statusText').textContent = '就绪';
                    document.getElementById('statusDot').classList.remove('error');
                }
            } catch (e) {
                log('请求失败: ' + e.message, 'error');
                document.getElementById('statusText').textContent = '就绪';
                document.getElementById('statusDot').classList.remove('error');
            }
        }
        async function checkTaskStatus() {
            try {
                const response = await fetch(API_BASE + '/api/task/status');
                const result = await response.json();
                if (!result.running && logRefreshInterval) {
                    clearInterval(logRefreshInterval);
                    logRefreshInterval = null;
                    document.getElementById('statusText').textContent = '就绪';
                    document.getElementById('statusDot').classList.remove('error');
                    refreshTaskLogs();
                }
            } catch (e) { console.error('Failed to check task status:', e); }
        }
        function stopTask() { fetch(API_BASE + '/api/task/stop', { method: 'POST' }); }
        async function takeScreenshot() {
            log('正在截取屏幕...', 'info');
            try {
                const response = await fetch(API_BASE + '/api/screenshot');
                const result = await response.json();
                if (result.success) {
                    document.getElementById('screenshotPreview').src = 'data:image/png;base64,' + result.image;
                    document.getElementById('screenshotContainer').style.display = 'block';
                    log('截图完成', 'success');
                } else { log('截图失败: ' + result.error, 'error'); }
            } catch (e) { log('截图请求失败: ' + e.message, 'error'); }
        }
        async function connectMcp() {
            const name = document.getElementById('mcpName').value.trim();
            const type = document.getElementById('mcpType').value;
            if (!name) { alert('请输入服务器名称'); return; }
            let data = { name, type };
            if (type === 'http') {
                data.url = document.getElementById('mcpUrl').value.trim();
                if (!data.url) { alert('请输入 HTTP URL'); return; }
            } else {
                data.command = document.getElementById('mcpCommand').value.trim();
                if (!data.command) { alert('请输入启动命令'); return; }
            }
            try {
                const response = await fetch(API_BASE + '/api/mcp/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                const result = await response.json();
                if (result.success) {
                    let msg = '连接成功: ' + result.serverInfo;
                    if (result.available) {
                        msg += '\n✅ 服务器可用';
                        if (result.tools && result.tools.length > 0) {
                            msg += '\n可用工具: ' + result.tools.map(t => t.name).join(', ');
                        }
                    } else {
                        msg += '\n⚠️ 无法获取工具列表';
                    }
                    alert(msg);
                    refreshMcpServers();
                }
                else { alert('连接失败: ' + result.error); }
            } catch (e) { alert('请求失败: ' + e.message); }
        }
        async function disconnectAllMcp() {
            if (!confirm('确定要断开所有 MCP 服务器吗？')) return;
            try { await fetch(API_BASE + '/api/mcp/disconnect-all', { method: 'POST' }); refreshMcpServers(); }
            catch (e) { alert('请求失败: ' + e.message); }
        }
        async function refreshMcpServers() {
            if (isLoadingMcp) return;
            isLoadingMcp = true;
            try {
                const response = await fetch(API_BASE + '/api/mcp/list');
                const result = await response.json();
                const container = document.getElementById('mcpServersList');
                document.getElementById('mcpCount').textContent = result.servers ? result.servers.length : 0;
                if (result.servers && result.servers.length > 0) {
                    container.innerHTML = result.servers.map(server => '<div class="server-card"><div class="server-info"><h4>' + server.name + '</h4><p>' + (server.version || '未知版本') + '</p>' + (server.tools && server.tools.length > 0 ? '<div class="tools-section">' + server.tools.map(tool => '<span class="tool-tag"><strong>' + tool.name + '</strong>: ' + (tool.description || '') + '</span>').join('') + '</div>' : '') + '</div><span class="server-badge">已连接</span></div>').join('');
                } else {
                    container.innerHTML = '<div class="empty-state"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg><p>暂无已连接的服务器</p></div>';
                }
            } catch (e) { console.error('Failed to refresh MCP servers:', e); }
            finally { isLoadingMcp = false; }
        }
        function selectProvider(name) {
            document.querySelectorAll('.provider-card').forEach(card => card.classList.remove('active'));
            event.currentTarget.classList.add('active');
            currentSelectedProvider = name;
            document.getElementById('customProviderForm').style.display = name === 'custom' ? 'block' : 'none';
            try { fetch(API_BASE + '/api/settings/provider', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ provider: name }) }); }
            catch (e) { console.error('Failed to select provider:', e); }
        }
        async function saveProvider() {
            const name = document.getElementById('customProviderName').value.trim();
            const url = document.getElementById('customProviderUrl').value.trim();
            const apiKey = document.getElementById('customProviderApiKey').value.trim();
            const isEditMode = document.getElementById('customProviderName').dataset.editMode === 'true';
            
            if (!name || !url) { alert('请填写提供商名称和 API Base URL'); return; }
            
            // 如果是编辑模式且有 API Key，使用 update 端点
            // 如果是添加模式，使用 add 端点
            const endpoint = isEditMode ? '/api/settings/provider/update' : '/api/settings/provider/add';
            
            try {
                const response = await fetch(API_BASE + endpoint, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name, url, apiKey })
                });
                const result = await response.json();
                if (result.success) {
                    alert(isEditMode ? '提供商更新成功' : '提供商添加成功');
                    document.getElementById('customProviderName').value = '';
                    document.getElementById('customProviderUrl').value = '';
                    document.getElementById('customProviderApiKey').value = '';
                    document.getElementById('customProviderName').dataset.editMode = 'false';
                    document.getElementById('providerBtnText').textContent = '添加提供商';
                    document.getElementById('customProviderApiKey').placeholder = '输入 API Key';
                    document.getElementById('customProviderForm').style.display = 'none';
                    // 重置只读状态
                    document.getElementById('customProviderName').readOnly = false;
                    document.getElementById('customProviderUrl').readOnly = false;
                    document.getElementById('customProviderName').style.opacity = '1';
                    document.getElementById('customProviderUrl').style.opacity = '1';
                    
                    // 自动切换到新添加/更新的提供商
                    if (!isEditMode && name) {
                        try {
                            await fetch(API_BASE + '/api/settings/provider', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({ provider: name })
                            });
                            document.getElementById('currentProvider').textContent = name;
                        } catch (e) { console.error('Failed to switch provider:', e); }
                    }
                    
                    refreshSettings();
                } else { alert((isEditMode ? '更新' : '添加') + '提供商失败: ' + result.error); }
            } catch (e) { alert('请求失败: ' + e.message); }
        }
        function editProvider(name, baseUrl) {
            const builtInProviders = ['zhipu', 'deepseek', 'openai'];
            const isBuiltIn = builtInProviders.includes(name.toLowerCase());
            
            document.getElementById('customProviderName').value = name;
            document.getElementById('customProviderUrl').value = baseUrl;
            document.getElementById('customProviderApiKey').value = '';
            document.getElementById('customProviderApiKey').placeholder = '留空则保持原 API Key 不变';
            document.getElementById('customProviderForm').style.display = 'block';
            document.getElementById('providerBtnText').textContent = '更新提供商';
            document.querySelectorAll('.provider-card').forEach(card => card.classList.remove('active'));
            currentSelectedProvider = name;
            document.getElementById('customProviderName').dataset.editMode = 'true';
            
            // 内置提供商：名称和 URL 只读
            if (isBuiltIn) {
                document.getElementById('customProviderName').readOnly = true;
                document.getElementById('customProviderUrl').readOnly = true;
                document.getElementById('customProviderName').style.opacity = '0.6';
                document.getElementById('customProviderUrl').style.opacity = '0.6';
            } else {
                document.getElementById('customProviderName').readOnly = false;
                document.getElementById('customProviderUrl').readOnly = false;
                document.getElementById('customProviderName').style.opacity = '1';
                document.getElementById('customProviderUrl').style.opacity = '1';
            }
        }
        async function deleteProvider(name) {
            if (!confirm('确定要删除提供商 "' + name + '" 吗？')) return;
            try {
                const response = await fetch(API_BASE + '/api/settings/provider/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name })
                });
                const result = await response.json();
                if (result.success) {
                    alert('提供商已删除');
                    refreshSettings();
                } else { alert('删除失败: ' + result.error); }
            } catch (e) { alert('请求失败: ' + e.message); }
        }
        async function refreshSettings() {
            if (isLoadingSettings) return;
            isLoadingSettings = true;
            try {
                const response = await fetch(API_BASE + '/api/settings');
                const result = await response.json();
                if (result.provider) document.getElementById('currentProvider').textContent = result.provider;
                if (result.model) document.getElementById('currentModel').textContent = result.model;
                if (result.visionMode !== undefined) document.getElementById('visionMode').checked = result.visionMode;
                if (result.providers && result.providers.length > 0) {
                    const container = document.getElementById('providerList');
                    const builtInProviders = ['zhipu', 'deepseek', 'openai'];
                    container.innerHTML = result.providers.map(p => {
                        const isBuiltIn = builtInProviders.includes(p.name.toLowerCase());
                        return '<div class="provider-card ' + (p.name === result.provider ? 'active' : '') + '" onclick="selectProviderByName(\'' + p.name + '\')">' +
                            '<div class="provider-icon"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M8 14s1.5 2 4 2 4-2 4-2"/><line x1="9" y1="9" x2="9.01" y2="9"/><line x1="15" y1="9" x2="15.01" y2="9"/></svg></div>' +
                            '<div class="provider-name">' + p.name + '</div>' +
                            '<div class="provider-model">' + (p.hasKey ? '✓ 已配置' : '未配置') + '</div>' +
                            '<div class="provider-actions" onclick="event.stopPropagation()"><button class="btn-icon" onclick="editProvider(\'' + p.name + '\', \'' + (p.baseUrl || '') + '\')" title="编辑"><svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg></button>' +
                            (isBuiltIn ? '' : '<button class="btn-icon" onclick="deleteProvider(\'' + p.name + '\')" title="删除"><svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg></button>') +
                            '</div></div>';
                    }).join('') + '<div class="provider-card" onclick="showCustomProviderForm()"><div class="provider-icon"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg></div><div class="provider-name">添加新提供商</div><div class="provider-model">OpenAI 兼容</div></div>';
                }
            } catch (e) { console.error('Failed to refresh settings:', e); }
            finally { isLoadingSettings = false; }
        }
        function selectProviderByName(name) {
            document.querySelectorAll('.provider-card').forEach(card => card.classList.remove('active'));
            event.currentTarget.classList.add('active');
            currentSelectedProvider = name;
            document.getElementById('customProviderForm').style.display = 'none';
            try {
                fetch(API_BASE + '/api/settings/provider', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ provider: name })
                }).then(() => { document.getElementById('currentProvider').textContent = name; });
            } catch (e) { console.error('Failed to select provider:', e); }
        }
        function showCustomProviderForm() {
            document.querySelectorAll('.provider-card').forEach(card => card.classList.remove('active'));
            event.currentTarget.classList.add('active');
            currentSelectedProvider = '';
            document.getElementById('customProviderForm').style.display = 'block';
            document.getElementById('customProviderName').value = '';
            document.getElementById('customProviderUrl').value = '';
            document.getElementById('customProviderApiKey').value = '';
            document.getElementById('customProviderApiKey').placeholder = '输入 API Key';
            document.getElementById('providerBtnText').textContent = '添加提供商';
            document.getElementById('customProviderName').dataset.editMode = 'false';
            // 重置只读状态
            document.getElementById('customProviderName').readOnly = false;
            document.getElementById('customProviderUrl').readOnly = false;
            document.getElementById('customProviderName').style.opacity = '1';
            document.getElementById('customProviderUrl').style.opacity = '1';
        }
        async function refreshLogs() {
            if (isLoadingLogs) return;
            isLoadingLogs = true;
            try {
                const response = await fetch(API_BASE + '/api/logs');
                const result = await response.json();
                const container = document.getElementById('systemLog');
                if (result.logs && result.logs.length > 0) {
                    container.innerHTML = result.logs.map(l => '<div class="log-entry ' + l.type + '"><span class="log-time">' + l.time + '</span><span class="log-message">' + l.message + '</span></div>').join('');
                } else { container.innerHTML = '<div class="log-entry info"><span class="log-time">--:--:--</span><span class="log-message">暂无日志</span></div>'; }
            } catch (e) { console.error('Failed to refresh logs:', e); }
            finally { isLoadingLogs = false; }
        }
        function clearLogs() { document.getElementById('systemLog').innerHTML = '<div class="log-entry info"><span class="log-time">--:--:--</span><span class="log-message">日志已清空</span></div>'; }
        document.getElementById('mcpType').addEventListener('change', function() {
            document.getElementById('httpUrlGroup').style.display = this.value === 'http' ? 'block' : 'none';
            document.getElementById('stdioGroup').style.display = this.value === 'stdio' ? 'block' : 'none';
        });
        document.getElementById('visionMode').addEventListener('change', async function() {
            try {
                await fetch(API_BASE + '/api/settings/vision', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ enabled: this.checked })
                });
            } catch (e) { console.error('Failed to set vision mode:', e); }
        });
        async function checkUpdate() {
            try {
                const response = await fetch(API_BASE + '/api/update/check');
                const result = await response.json();
                if (!result.success) {
                    alert('检查更新失败: ' + (result.error || '无法连接到GitHub'));
                } else if (result.hasUpdate) {
                    alert('发现新版本: ' + result.latestVersion + '\n当前版本: ' + result.currentVersion + '\n\n请访问 GitHub 下载最新版本');
                } else if (result.currentVersion === result.latestVersion) {
                    alert('当前已是最新版本: ' + result.currentVersion);
                } else {
                    alert('当前版本比发布版本更新: ' + result.currentVersion);
                }
            } catch (e) { alert('检查更新失败: ' + e.message); }
        }
        async function testModelAvailability() {
            const model = document.getElementById('modelNameInput').value.trim();
            const statusSpan = document.getElementById('modelTestStatus');
            
            statusSpan.textContent = '测试中...';
            statusSpan.style.color = 'var(--text-muted)';
            
            try {
                const response = await fetch(API_BASE + '/api/model/test', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ model })
                });
                const result = await response.json();
                if (result.success) {
                    statusSpan.textContent = '✅ 可用 (' + result.model + ')';
                    statusSpan.style.color = 'var(--success)';
                } else {
                    statusSpan.textContent = '❌ 不可用';
                    statusSpan.style.color = 'var(--error)';
                    console.error('Model test failed:', result.error);
                }
            } catch (e) {
                statusSpan.textContent = '❌ 测试失败';
                statusSpan.style.color = 'var(--error)';
                console.error('Model test error:', e);
            }
        }
        refreshSettings();
        refreshMcpServers();
    </script>
</body>
</html>
)htmlend";

web::HttpResponse handleIndex(const web::HttpRequest& req) {
    web::HttpResponse res;
    res.setHtml(EMBEDDED_INDEX_HTML);
    return res;
}

std::mutex g_logMutex;
std::vector<std::pair<std::string, std::string>> g_logs;
std::atomic<bool> g_taskRunning(false);
std::atomic<bool> g_taskStop(false);
std::string g_currentTask;
bool g_visionMode = true;
std::string g_visionProvider;
std::string g_visionModel;
const std::string LOG_FILE = "aries_web.log";

void initLogFile() {
    // 启动时清空日志文件
    std::ofstream logFile(LOG_FILE, std::ios::trunc);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);
        logFile << "[" << timeStr << "] 日志开始记录" << std::endl;
        logFile.close();
    }
}

void addLog(const std::string& message, const std::string& type = "info") {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);
    g_logs.push_back({timeStr, message});
    if (g_logs.size() > 1000) {
        g_logs.erase(g_logs.begin());
    }
    std::cout << "[" << timeStr << "] " << message << std::endl;
    
    // 写入日志文件
    std::ofstream logFile(LOG_FILE, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "] " << message << std::endl;
        logFile.close();
    }
}

std::string getLogsJson() {
    std::ostringstream oss;
    oss << "{\"logs\":[";
    std::lock_guard<std::mutex> lock(g_logMutex);
    for (size_t i = 0; i < g_logs.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"time\":\"" << g_logs[i].first << "\",\"message\":\"" 
            << web::escapeJson(g_logs[i].second) << "\",\"type\":\"info\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string getControlListDescription() {
    UIAutomationTool uiaTool;
    if (!uiaTool.initialize()) {
        return "Error: UI Automation 初始化失败";
    }
    
    std::ostringstream oss;
    oss << "=== 当前屏幕控件列表 ===\n\n";
    
    std::string activeTree = uiaTool.getActiveWindowControlTree(2);
    if (activeTree.find("Error:") != std::string::npos) {
        oss << "【顶层窗口列表】\n";
        oss << uiaTool.listTopLevelWindows();
    } else {
        oss << "【活动窗口控件树】\n";
        oss << activeTree;
    }
    
    return oss.str();
}

web::HttpResponse handleGetSettings(const web::HttpRequest& req) {
    auto& providerMgr = ProviderManager::getInstance();
    std::string provider = providerMgr.getCurrentProviderName();
    std::string model = providerMgr.getCurrentModel(provider);
    
    auto providerNames = providerMgr.getProviderNames();
    
    std::ostringstream oss;
    oss << "{\"success\":true,";
    oss << "\"provider\":\"" << provider << "\",";
    oss << "\"model\":\"" << model << "\",";
    oss << "\"visionMode\":" << (g_visionMode ? "true" : "false") << ",";
    oss << "\"providers\":[";
    for (size_t i = 0; i < providerNames.size(); i++) {
        if (i > 0) oss << ",";
        std::string info = providerMgr.getProviderInfo(providerNames[i]);
        bool hasKey = providerMgr.hasSavedApiKey(providerNames[i]);
        std::string baseUrl = providerMgr.getBaseUrl(providerNames[i]);
        oss << "{\"name\":\"" << providerNames[i] << "\",\"info\":\"" << web::escapeJson(info) << "\",\"hasKey\":" << (hasKey ? "true" : "false") << ",\"baseUrl\":\"" << web::escapeJson(baseUrl) << "\"}";
    }
    oss << "]";
    oss << "}";
    
    web::HttpResponse res;
    res.setJson(oss.str());
    return res;
}

web::HttpResponse handleSetProvider(const web::HttpRequest& req) {
    std::string provider = web::extractJsonString(req.body, "provider");
    
    auto& providerMgr = ProviderManager::getInstance();
    auto* config = providerMgr.getConfig(provider);
    if (config) {
        providerMgr.setCurrentProvider(provider);
        addLog("切换提供商: " + provider);
        web::HttpResponse res;
        res.setJson("{\"success\":true}");
        return res;
    }
    
    web::HttpResponse res;
    res.setError(400, "Provider not found");
    return res;
}

web::HttpResponse handleAddProvider(const web::HttpRequest& req) {
    std::string name = web::extractJsonString(req.body, "name");
    std::string url = web::extractJsonString(req.body, "url");
    std::string apiKey = web::extractJsonString(req.body, "apiKey");
    
    if (name.empty() || url.empty()) {
        web::HttpResponse res;
        res.setError(400, "Name and URL are required");
        return res;
    }
    
    auto& providerMgr = ProviderManager::getInstance();
    
    // 使用 saveCustomProvider 保存提供商配置和 API Key 到 JSON
    providerMgr.saveCustomProvider(name, url, "", apiKey);
    
    addLog("添加自定义提供商: " + name);
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleDeleteProvider(const web::HttpRequest& req) {
    std::string name = web::extractJsonString(req.body, "name");
    
    if (name.empty()) {
        web::HttpResponse res;
        res.setError(400, "Name is required");
        return res;
    }
    
    auto& providerMgr = ProviderManager::getInstance();
    
    // 从 JSON 配置中删除
    auto providers = SecureStorage::loadProvidersFromJson();
    providers.erase(name);
    SecureStorage::saveProvidersToJson(providers);
    
    // 从内存中删除
    providerMgr.removeProviderConfig(name);
    
    addLog("删除提供商: " + name);
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleUpdateProvider(const web::HttpRequest& req) {
    std::string name = web::extractJsonString(req.body, "name");
    std::string url = web::extractJsonString(req.body, "url");
    std::string apiKey = web::extractJsonString(req.body, "apiKey");
    
    if (name.empty() || url.empty()) {
        web::HttpResponse res;
        res.setError(400, "Name and URL are required");
        return res;
    }
    
    auto& providerMgr = ProviderManager::getInstance();
    
    // 更新提供商配置（会覆盖同名配置）
    providerMgr.saveCustomProvider(name, url, "", apiKey);
    
    addLog("更新提供商: " + name);
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleSaveApiKey(const web::HttpRequest& req) {
    std::string provider = web::extractJsonString(req.body, "provider");
    std::string apiKey = web::extractJsonString(req.body, "apiKey");
    
    if (apiKey.empty()) {
        web::HttpResponse res;
        res.setError(400, "API Key is empty");
        return res;
    }
    
    auto& providerMgr = ProviderManager::getInstance();
    providerMgr.saveProviderApiKey(provider, apiKey);
    addLog("保存 API Key: " + provider);
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleSetVision(const web::HttpRequest& req) {
    bool enabled = web::extractJsonBool(req.body, "enabled");
    g_visionMode = enabled;
    addLog(std::string("设置视觉模式: ") + (enabled ? "开启" : "关闭"));
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleGetMcpList(const web::HttpRequest& req) {
    auto& mcpMgr = mcp::MCPClientManager::getInstance();
    auto names = mcpMgr.getClientNames();
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"servers\":[";
    
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) oss << ",";
        std::string info = mcpMgr.getServerInfo(names[i]);
        oss << "{\"name\":\"" << names[i] << "\",\"version\":\"" << web::escapeJson(info) << "\"}";
    }
    
    oss << "]}";
    
    web::HttpResponse res;
    res.setJson(oss.str());
    return res;
}

web::HttpResponse handleMcpConnect(const web::HttpRequest& req) {
    std::string name = web::extractJsonString(req.body, "name");
    std::string type = web::extractJsonString(req.body, "type");
    
    if (name.empty()) {
        web::HttpResponse res;
        res.setError(400, "Name is required");
        return res;
    }
    
    auto& mcpMgr = mcp::MCPClientManager::getInstance();
    bool success = false;
    std::string error;
    
    if (type == "http") {
        std::string url = web::extractJsonString(req.body, "url");
        if (url.empty()) {
            web::HttpResponse res;
            res.setError(400, "URL is required for HTTP type");
            return res;
        }
        success = mcpMgr.connectHttpServer(name, url);
        if (!success) error = mcpMgr.getLastError();
    } else {
        std::string command = web::extractJsonString(req.body, "command");
        std::string args = web::extractJsonString(req.body, "args");
        if (command.empty()) {
            web::HttpResponse res;
            res.setError(400, "Command is required for stdio type");
            return res;
        }
        success = mcpMgr.connectServer(name, command, args);
        if (!success) error = mcpMgr.getLastError();
    }
    
    if (success) {
        // 连接成功后自动检测可用性 - 获取工具列表
        auto tools = mcpMgr.getServerTools(name);
        bool available = !tools.empty();
        
        addLog("MCP 服务器连接成功: " + name + (available ? " (可用)" : " (无法获取工具列表)"));
        
        std::ostringstream oss;
        oss << "{\"success\":true,\"serverInfo\":\"" << web::escapeJson(mcpMgr.getServerInfo(name)) << "\",\"available\":" << (available ? "true" : "false") << ",\"tools\":[";
        for (size_t i = 0; i < tools.size(); i++) {
            if (i > 0) oss << ",";
            oss << "{\"name\":\"" << web::escapeJson(tools[i].name) << "\",\"description\":\"" << web::escapeJson(tools[i].description) << "\"}";
        }
        oss << "]}";
        web::HttpResponse res;
        res.setJson(oss.str());
        return res;
    } else {
        addLog("MCP 服务器连接失败: " + name + " - " + error);
        web::HttpResponse res;
        res.setError(500, error);
        return res;
    }
}

web::HttpResponse handleMcpDisconnectAll(const web::HttpRequest& req) {
    auto& mcpMgr = mcp::MCPClientManager::getInstance();
    mcpMgr.disconnectAll();
    addLog("断开所有 MCP 服务器");
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleScreenshot(const web::HttpRequest& req) {
    std::string screenshotPath = "temp/screenshot.png";
    
    if (!capture_screen(screenshotPath)) {
        web::HttpResponse res;
        res.setError(500, "Failed to capture screenshot");
        return res;
    }
    
    std::ifstream file(screenshotPath, std::ios::binary);
    if (!file.is_open()) {
        web::HttpResponse res;
        res.setError(500, "Failed to read screenshot");
        return res;
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string data = oss.str();
    
    static const char* base64Chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string base64;
    int val = 0, valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            base64.push_back(base64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        base64.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (base64.size() % 4) {
        base64.push_back('=');
    }
    
    web::HttpResponse res;
    res.setJson("{\"success\":true,\"image\":\"" + base64 + "\"}");
    return res;
}

web::HttpResponse handleGetLogs(const web::HttpRequest& req) {
    web::HttpResponse res;
    res.setJson(getLogsJson());
    return res;
}

web::HttpResponse handleTask(const web::HttpRequest& req) {
    std::string task = web::extractJsonString(req.body, "task");
    std::string model = web::extractJsonString(req.body, "model");
    int maxIterations = 10;
    bool useVision = web::extractJsonBool(req.body, "useVision");
    
    // 提取 maxIterations
    {
        std::string searchKey = "\"maxIterations\"";
        size_t pos = req.body.find(searchKey);
        if (pos != std::string::npos) {
            pos = req.body.find(":", pos);
            if (pos != std::string::npos) {
                while (pos < req.body.length() && (req.body[pos] == ':' || req.body[pos] == ' ' || req.body[pos] == '\t')) {
                    pos++;
                }
                if (pos < req.body.length() && (req.body[pos] >= '0' && req.body[pos] <= '9')) {
                    maxIterations = 0;
                    while (pos < req.body.length() && (req.body[pos] >= '0' && req.body[pos] <= '9')) {
                        maxIterations = maxIterations * 10 + (req.body[pos] - '0');
                        pos++;
                    }
                }
            }
        }
    }
    
    // 提取视觉模型配置
    std::string visionProvider;
    std::string visionModel;
    {
        size_t visionConfigPos = req.body.find("\"visionConfig\"");
        if (visionConfigPos != std::string::npos) {
            size_t objStart = req.body.find("{", visionConfigPos);
            size_t objEnd = req.body.find("}", objStart);
            if (objStart != std::string::npos && objEnd != std::string::npos) {
                std::string visionConfigStr = req.body.substr(objStart, objEnd - objStart + 1);
                visionProvider = web::extractJsonString(visionConfigStr, "provider");
                visionModel = web::extractJsonString(visionConfigStr, "model");
            }
        }
    }
    
    if (task.empty()) {
        web::HttpResponse res;
        res.setError(400, "Task is empty");
        return res;
    }
    
    if (g_taskRunning) {
        web::HttpResponse res;
        res.setError(400, "Task already running");
        return res;
    }
    
    addLog("开始执行任务: " + task);
    addLog(std::string("模式: ") + (useVision ? "视觉模式" : "纯文本模式"));
    
    auto& providerMgr = ProviderManager::getInstance();
    std::string providerName = providerMgr.getCurrentProviderName();
    
    // 如果指定了模型，设置模型
    if (!model.empty()) {
        providerMgr.setCurrentModel(providerName, model);
        addLog("使用模型: " + model);
    }
    
    // 重新创建 provider 以使用新模型
    providerMgr.setCurrentProvider(providerName);
    auto provider = providerMgr.getCurrentProvider();
    
    if (!provider) {
        web::HttpResponse res;
        res.setError(400, "No AI provider configured");
        return res;
    }
    
    g_taskRunning = true;
    g_taskStop = false;
    g_currentTask = task;
    g_visionMode = useVision;
    g_visionProvider = visionProvider;
    g_visionModel = visionModel;
    
    // 创建状态窗口（在主线程）
    auto& statusWindow = StatusWindow::getInstance();
    statusWindow.create();
    statusWindow.setStopCallback([]() {
        g_taskStop = true;
        addLog("用户请求停止任务");
    });
    
    // 最小化浏览器窗口和控制台窗口
    HWND browserWnd = GetForegroundWindow();
    if (browserWnd) {
        ShowWindow(browserWnd, SW_MINIMIZE);
    }
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        ShowWindow(consoleWnd, SW_MINIMIZE);
    }
    
    const int finalMaxIterations = maxIterations;
    
    std::thread([finalMaxIterations]() {
        auto& providerMgr = ProviderManager::getInstance();
        auto provider = providerMgr.getCurrentProvider();
        std::string mainProviderName = providerMgr.getCurrentProviderName(); // 保存主提供商名称
        
        // 如果配置了视觉模型转述，创建视觉模型 provider
        AIProviderPtr visionProvider = nullptr;
        if (!g_visionMode && !g_visionProvider.empty()) {
            addLog("创建视觉模型 provider: " + g_visionProvider);
            
            // 直接创建视觉模型 provider，不切换当前 provider
            auto* visionConfig = providerMgr.getConfig(g_visionProvider);
            if (visionConfig) {
                addLog("视觉模型配置: baseUrl=" + visionConfig->baseUrl + ", apiKey长度=" + std::to_string(visionConfig->apiKey.length()));
                // 使用 createProviderWithModel 确保使用正确的模型名称
                visionProvider = providerMgr.createProviderWithModel(g_visionProvider, g_visionModel);
            } else {
                addLog("错误: 找不到视觉模型配置: " + g_visionProvider);
            }
            
            if (visionProvider) {
                addLog("视觉模型转述: " + g_visionProvider + (g_visionModel.empty() ? "" : " / " + g_visionModel));
            } else {
                addLog("错误: 无法创建视觉模型 provider");
            }
        }
        
        ActionParser parser;
        ActionExecutor executor;
        std::vector<std::string> actionHistory;
        
        int currentIteration = 0;
        int maxIter = finalMaxIterations;
        
        while (currentIteration < maxIter && !g_taskStop) {
            addLog("迭代 " + std::to_string(currentIteration + 1) + "/" + std::to_string(maxIter));
            StatusWindow::getInstance().setIteration(currentIteration + 1, maxIter);
            StatusWindow::getInstance().addLog("迭代 " + std::to_string(currentIteration + 1));
            
            std::string systemPrompt;
            std::string userMessage;
            std::vector<ChatMessage> messages;
            
            if (g_visionMode) {
                addLog("使用视觉模式...");
                int screenWidth, screenHeight;
                get_screen_size(screenWidth, screenHeight);
                systemPrompt = PromptTemplates::buildSystemPrompt(screenWidth, screenHeight, false);
                
                auto& mcpMgr = mcp::MCPClientManager::getInstance();
                auto servers = mcpMgr.getClientNames();
                if (!servers.empty()) {
                    systemPrompt += "\n\n【已连接的 MCP 服务器】\n";
                    systemPrompt += mcpMgr.getAllToolsDescription();
                }
                
                std::string screenshotPath = "temp/screenshot.png";
                capture_screen(screenshotPath);
                addLog("截图完成");
                
                std::string userContent = "任务: " + g_currentTask;
                if (!actionHistory.empty()) {
                    userContent += "\n\n【动作历史】\n";
                    for (const auto& h : actionHistory) {
                        userContent += h + "\n";
                    }
                }
                
                if (g_taskStop) break;
                
                addLog("发送请求到 AI...");
                auto [success, response] = provider->sendMessageWithImages(userContent, {screenshotPath}, systemPrompt);
                
                if (g_taskStop) break;
                
                if (!success) {
                    addLog("AI 请求失败: " + provider->getLastError());
                    break;
                }
                if (response.empty()) {
                    addLog("AI 响应为空");
                    break;
                }
                
                addLog("AI 响应: " + response.substr(0, 500) + (response.length() > 500 ? "..." : ""));
                
                ParsedAgentAction action = parser.parse(response);
                
                if (!action.isValid()) {
                    addLog("未解析到有效动作，原始响应: " + response.substr(0, 300));
                    break;
                }
                
                if (g_taskStop) break;
                
                addLog("执行动作: " + action.action);
                StatusWindow::getInstance().setCurrentAction(action.action);
                StatusWindow::getInstance().addLog(action.action);
                
                // 设置动作详细信息
                std::string detail;
                auto& fields = action.fields;
                if (action.action == "Click" || action.action == "DoubleClick") {
                    if (fields.count("x") && fields.count("y")) {
                        detail = "位置: (" + fields["x"] + ", " + fields["y"] + ")";
                    }
                    if (action.target.has_value()) {
                        detail += (detail.empty() ? std::string() : " ") + "目标: " + action.target.value();
                    }
                } else if (action.action == "Type") {
                    if (fields.count("text")) {
                        std::string text = fields["text"];
                        if (text.length() > 30) text = text.substr(0, 30) + "...";
                        detail = "文本: " + text;
                    }
                } else if (action.action == "Scroll") {
                    if (fields.count("direction")) {
                        detail = "方向: " + fields["direction"];
                        if (fields.count("distance")) detail += ", 距离: " + fields["distance"];
                    }
                } else if (action.action == "Wait") {
                    if (fields.count("duration")) {
                        detail = "时长: " + fields["duration"] + "ms";
                    }
                } else if (action.action == "Launch") {
                    if (fields.count("app")) {
                        detail = "应用: " + fields["app"];
                    }
                } else if (action.action == "MCP_Tool") {
                    if (fields.count("server") && fields.count("tool")) {
                        detail = "工具: " + fields["server"] + "/" + fields["tool"];
                    }
                } else if (action.target.has_value()) {
                    detail = "目标: " + action.target.value();
                }
                StatusWindow::getInstance().setActionDetail(detail);
                
                auto result = executor.execute(action);
                
                std::string historyEntry;
                if (result.success) {
                    historyEntry = action.action + " -> 成功";
                    if (!result.message.empty() && result.message != "成功") {
                        historyEntry += "\n结果: " + result.message;
                    }
                } else {
                    historyEntry = action.action + " -> 失败: " + result.message;
                }
                actionHistory.push_back(historyEntry);
                addLog(action.action + " -> " + (result.success ? "成功" : "失败"));
                StatusWindow::getInstance().addLog(result.success ? "✓ 成功" : "✗ 失败");
                
                // 不区分大小写检查 Stop/Finish
                std::string actionLower = action.action;
                for (char& c : actionLower) c = tolower(c);
                if (actionLower == "stop" || actionLower == "finish") {
                    addLog("任务完成");
                    break;
                }
            } else {
                addLog("使用纯文本模式...");
                systemPrompt = PromptTemplates::buildTextModeSystemPrompt();
                
                auto& mcpMgr = mcp::MCPClientManager::getInstance();
                auto servers = mcpMgr.getClientNames();
                if (!servers.empty()) {
                    systemPrompt += "\n\n【已连接的 MCP 服务器】\n";
                    systemPrompt += mcpMgr.getAllToolsDescription();
                }
                
                std::string userContent;
                
                // 如果配置了视觉模型转述，使用视觉模型
                if (visionProvider) {
                    addLog("正在截取屏幕并由视觉模型转述...");
                    addLog("视觉模型提供商: " + g_visionProvider);
                    addLog("视觉模型名称: " + (g_visionModel.empty() ? "(默认)" : g_visionModel));
                    
                    std::string screenshotPath = "temp/screenshot.png";
                    capture_screen_compressed(screenshotPath, 1280);  // 使用压缩截图，最大宽度1280
                    
                    // 检查截图文件是否存在
                    std::ifstream checkFile(screenshotPath);
                    if (!checkFile.good()) {
                        addLog("截图文件不存在，回退到控件列表模式");
                        std::string controlList = getControlListDescription();
                        userContent = "\n\n【当前屏幕控件信息】\n" + controlList;
                    } else {
                        checkFile.close();
                        
                        std::string visionSystemPrompt = "你是屏幕描述助手。简洁描述屏幕内容，格式：\n【窗口】标题\n【内容】主要内容\n【控件】按钮/输入框名称和位置\n【状态】界面状态\n每项不超过50字。";
                        
                        addLog("正在发送请求到视觉模型...");
                        auto [visionSuccess, visionResponse] = visionProvider->sendMessageWithImages(
                            "简洁描述截图内容：", {screenshotPath}, visionSystemPrompt);
                        
                        if (visionSuccess && !visionResponse.empty()) {
                            addLog("视觉模型转述完成，响应长度: " + std::to_string(visionResponse.length()));
                            userContent = "\n\n【屏幕内容描述（由视觉模型生成）】\n" + visionResponse;
                        } else {
                            std::string errorMsg = visionProvider->getLastError();
                            addLog("视觉模型转述失败: " + errorMsg);
                            std::string controlList = getControlListDescription();
                            userContent = "\n\n【当前屏幕控件信息】\n" + controlList;
                        }
                    }
                } else {
                    std::string controlList = getControlListDescription();
                    addLog("获取控件列表完成");
                    userContent = "\n\n【当前屏幕控件信息】\n" + controlList;
                }
                
                userContent += "\n\n任务: " + g_currentTask;
                
                if (!actionHistory.empty()) {
                    userContent += "\n\n【动作历史】\n";
                    for (const auto& h : actionHistory) {
                        userContent += h + "\n";
                    }
                }
                
                std::vector<ChatMessage> messages;
                messages.push_back(ChatMessage("user", userContent));
                
                if (g_taskStop) break;
                
                addLog("发送请求到 AI...");
                auto [success, response] = provider->sendMessage(messages, systemPrompt);
                
                if (g_taskStop) break;
                
                if (!success) {
                    addLog("AI 请求失败");
                    break;
                }
                if (response.empty()) {
                    addLog("AI 响应为空");
                    break;
                }
                
                addLog("AI 响应: " + response.substr(0, 500) + (response.length() > 500 ? "..." : ""));
                
                ParsedAgentAction action = parser.parse(response);
                
                if (!action.isValid()) {
                    addLog("未解析到有效动作，原始响应: " + response.substr(0, 300));
                    break;
                }
                
                if (g_taskStop) break;
                
                addLog("执行动作: " + action.action);
                StatusWindow::getInstance().setCurrentAction(action.action);
                StatusWindow::getInstance().addLog(action.action);
                
                // 设置动作详细信息
                std::string detail2;
                auto& fields2 = action.fields;
                if (action.action == "Click" || action.action == "DoubleClick") {
                    if (fields2.count("x") && fields2.count("y")) {
                        detail2 = "位置: (" + fields2["x"] + ", " + fields2["y"] + ")";
                    }
                    if (action.target.has_value()) {
                        detail2 += (detail2.empty() ? std::string() : " ") + "目标: " + action.target.value();
                    }
                } else if (action.action == "Type") {
                    if (fields2.count("text")) {
                        std::string text = fields2["text"];
                        if (text.length() > 30) text = text.substr(0, 30) + "...";
                        detail2 = "文本: " + text;
                    }
                } else if (action.action == "Scroll") {
                    if (fields2.count("direction")) {
                        detail2 = "方向: " + fields2["direction"];
                        if (fields2.count("distance")) detail2 += ", 距离: " + fields2["distance"];
                    }
                } else if (action.action == "Wait") {
                    if (fields2.count("duration")) {
                        detail2 = "时长: " + fields2["duration"] + "ms";
                    }
                } else if (action.action == "Launch") {
                    if (fields2.count("app")) {
                        detail2 = "应用: " + fields2["app"];
                    }
                } else if (action.action == "MCP_Tool") {
                    if (fields2.count("server") && fields2.count("tool")) {
                        detail2 = "工具: " + fields2["server"] + "/" + fields2["tool"];
                    }
                } else if (action.target.has_value()) {
                    detail2 = "目标: " + action.target.value();
                }
                StatusWindow::getInstance().setActionDetail(detail2);
                
                auto result = executor.execute(action);
                
                std::string historyEntry;
                if (result.success) {
                    historyEntry = action.action + " -> 成功";
                    if (!result.message.empty() && result.message != "成功") {
                        historyEntry += "\n结果: " + result.message;
                    }
                } else {
                    historyEntry = action.action + " -> 失败: " + result.message;
                }
                actionHistory.push_back(historyEntry);
                addLog(action.action + " -> " + (result.success ? "成功" : "失败"));
                StatusWindow::getInstance().addLog(result.success ? "✓ 成功" : "✗ 失败");
                
                // 不区分大小写检查 Stop/Finish
                std::string actionLower = action.action;
                for (char& c : actionLower) c = tolower(c);
                if (actionLower == "stop" || actionLower == "finish") {
                    addLog("任务完成");
                    break;
                }
            }
            
            currentIteration++;
            
            // 达到最大迭代次数时询问是否继续
            if (!g_taskStop && currentIteration >= maxIter) {
                HWND consoleWnd = GetConsoleWindow();
                if (consoleWnd) {
                    ShowWindow(consoleWnd, SW_RESTORE);
                    SetForegroundWindow(consoleWnd);
                }
                
                int msgResult = MessageBoxW(
                    nullptr,
                    L"已达到最大迭代次数，任务可能未完成。\n\n是否继续执行？",
                    L"Open-Aries-AI - 任务执行",
                    MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND
                );
                
                if (msgResult == IDYES) {
                    addLog("用户选择继续执行任务");
                    maxIter += 10;
                    if (consoleWnd) {
                        ShowWindow(consoleWnd, SW_MINIMIZE);
                    }
                } else {
                    addLog("用户选择结束任务");
                    break;
                }
            }
        }
        
        g_taskRunning = false;
        addLog("任务执行结束");
        StatusWindow::getInstance().destroy();
    }).detach();
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleTaskStop(const web::HttpRequest& req) {
    g_taskStop = true;
    addLog("正在停止任务...");
    
    web::HttpResponse res;
    res.setJson("{\"success\":true}");
    return res;
}

web::HttpResponse handleTaskStatus(const web::HttpRequest& req) {
    std::ostringstream oss;
    oss << "{\"running\":" << (g_taskRunning ? "true" : "false") << ",";
    oss << "\"task\":\"" << web::escapeJson(g_currentTask) << "\"}";
    
    web::HttpResponse res;
    res.setJson(oss.str());
    return res;
}

web::HttpResponse handleCheckUpdate(const web::HttpRequest& req) {
    std::string currentVersion = "v1.3.1";
    std::string latestVersion = "";
    bool hasUpdate = false;
    
    addLog("正在检查更新...");
    ReleaseInfo info = UpdateChecker::checkForUpdate("https://github.com/yunsjxh/Open-Aries-AI");
    
    addLog("更新检查结果: success=" + std::string(info.success ? "true" : "false"));
    addLog("获取到的版本号: [" + info.version + "]");
    addLog("当前版本号: [" + currentVersion + "]");
    addLog("错误信息: " + info.errorMessage);
    
    if (info.success) {
        latestVersion = info.version;
        int cmpResult = UpdateChecker::compareVersions(info.version, currentVersion);
        addLog("版本比较结果: " + std::to_string(cmpResult));
        if (cmpResult == 0) {
            // 版本相同，统一使用当前版本号格式
            latestVersion = currentVersion;
        } else if (cmpResult > 0) {
            hasUpdate = true;
        }
    }
    
    std::ostringstream oss;
    oss << "{\"success\":" << (info.success ? "true" : "false") << ",";
    oss << "\"currentVersion\":\"" << currentVersion << "\",";
    oss << "\"latestVersion\":\"" << web::escapeJson(latestVersion) << "\",";
    oss << "\"hasUpdate\":" << (hasUpdate ? "true" : "false") << ",";
    oss << "\"error\":\"" << web::escapeJson(info.errorMessage) << "\"}";
    
    web::HttpResponse res;
    res.setJson(oss.str());
    return res;
}

web::HttpResponse handleTestModel(const web::HttpRequest& req) {
    std::string model = web::extractJsonString(req.body, "model");
    
    auto& providerMgr = ProviderManager::getInstance();
    std::string providerName = providerMgr.getCurrentProviderName();
    
    if (providerName.empty()) {
        web::HttpResponse res;
        res.setError(400, "未选择提供商");
        return res;
    }
    
    auto provider = providerMgr.createProviderWithModel(providerName, model);
    if (!provider) {
        web::HttpResponse res;
        res.setError(400, "无法创建提供商实例，请检查 API Key 配置");
        return res;
    }
    
    // 发送简单测试请求
    std::vector<ChatMessage> messages;
    messages.emplace_back("user", "回复OK两个字母即可");
    
    auto [success, response] = provider->sendMessage(messages, "你是一个助手，请简洁回复。");
    
    if (success) {
        addLog("模型测试成功: " + providerName + " / " + provider->getModelName());
        web::HttpResponse res;
        res.setJson("{\"success\":true,\"model\":\"" + web::escapeJson(provider->getModelName()) + "\"}");
        return res;
    } else {
        addLog("模型测试失败: " + provider->getLastError());
        web::HttpResponse res;
        res.setError(500, provider->getLastError());
        return res;
    }
}

int main() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 初始化日志文件（清空旧日志）
    initLogFile();
    
    std::cout << R"(
      ___           ___                       ___           ___                    ___                 
      /\  \         /\  \          ___        /\  \         /\  \                  /\  \          ___   
     /::\  \       /::\  \        /\  \      /::\  \       /::\  \                /::\  \        /\  \  
    /:/\:\  \     /:/\:\  \       \:\  \    /:/\:\  \     /:/\ \  \              /:/\:\  \       \:\  \ 
   /::\~\:\  \   /::\~\:\  \      /::\__\  /::\~\:\  \   _\:\~\ \  \            /::\~\:\  \      /::\__\ 
  /:/\:\ \:\__\ /:/\:\ \:\__\  __/:/\/__/ /:/\:\ \:\__\ /\ \:\ \ \__\          /:/\:\ \:\__\  __/:/\/__/ 
  \/__\:\/:/  / \/_|::\/:/  / /\/:/  /    \:\~\:\ \/__/ \:\ \:\ \/__/          \/__\:\/:/  / /\/:/  /   
       \::/  /     |:|::/  /  \::/__/      \:\ \:\__\    \:\ \:\__\                 \::/  /  \::/__/    
       /:/  /      |:|\/__/    \:\__\       \:\ \/__/     \:\/:/  /                 /:/  /    \:\__\    
      /:/  /       |:|  |       \/__/        \:\__\        \::/  /                 /:/  /      \/__/    
      \/__/         \|__|                     \/__/         \/__/                  \/__/
)" << std::endl;
    std::cout << "  Open-Aries-AI - Web 服务器  版本 v1.3.1" << std::endl;
    std::cout << std::endl;
    
    CreateDirectoryA("temp", NULL);
    
    auto& providerMgr = ProviderManager::getInstance();
    providerMgr.registerBuiltInProviders();
    providerMgr.loadSavedProviders();  // 加载保存的提供商配置
    
    web::WebServer server(8080);
    
    server.get("/", handleIndex);
    server.get("/index.html", handleIndex);
    
    server.get("/api/settings", handleGetSettings);
    server.post("/api/settings/provider", handleSetProvider);
    server.post("/api/settings/provider/add", handleAddProvider);
    server.post("/api/settings/provider/update", handleUpdateProvider);
    server.post("/api/settings/provider/delete", handleDeleteProvider);
    server.post("/api/settings/apikey", handleSaveApiKey);
    server.post("/api/settings/vision", handleSetVision);
    
    server.get("/api/mcp/list", handleGetMcpList);
    server.post("/api/mcp/connect", handleMcpConnect);
    server.post("/api/mcp/disconnect-all", handleMcpDisconnectAll);
    
    server.get("/api/screenshot", handleScreenshot);
    server.get("/api/logs", handleGetLogs);
    
    server.post("/api/task", handleTask);
    server.post("/api/task/stop", handleTaskStop);
    server.get("/api/task/status", handleTaskStatus);
    server.get("/api/update/check", handleCheckUpdate);
    server.post("/api/model/test", handleTestModel);
    
    addLog("Web 服务器启动");
    
    if (!server.start()) {
        std::cerr << "服务器启动失败" << std::endl;
        return 1;
    }
    
    std::cout << "服务器运行中: http://localhost:" << server.getPort() << std::endl;
    std::cout << "按 'q' 键退出" << std::endl;
    
    while (server.isRunning()) {
        if (_kbhit() && _getch() == 'q') {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    server.stop();
    std::cout << "服务器已停止" << std::endl;
    
    return 0;
}
