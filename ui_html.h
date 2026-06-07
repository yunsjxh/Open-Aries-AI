#pragma once

static const char g_htmlUI[] = R"HTML(<!DOCTYPE html>
<html lang="zh-CN" class="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Open Aries AI</title>
    <style>
        /* ── Light (original) ── */
        :root {
            --bg:           #f8f8f7;
            --sidebar-bg:   #f0efed;
            --card:         #ffffff;
            --border:       #e6e4e1;
            --text:         #1b1b1b;
            --text-secondary: #6b6b6b;
            --text-muted:   #9d9d9b;
            --accent:       #2c2c2c;
            --accent-hover: #1a1a1a;
            --hover:        rgba(0,0,0,.04);
            --bubble-user:  #ececea;
            --bubble-user-text: #1b1b1b;
            --bubble-agent: #ffffff;
            --radius:       10px;
            --radius-sm:    6px;
            --shadow-sm:    0 1px 2px rgba(0,0,0,.04);
            --shadow-md:    0 2px 8px rgba(0,0,0,.06);
            --font:         -apple-system, "Microsoft YaHei", "PingFang SC", "Noto Sans SC", "Helvetica Neue", sans-serif;
            --modal-bg:     #ffffff;
            --titlebar-bg:  rgba(255,255,255,.6);
            --input-bar-bg: rgba(255,255,255,.6);
            --code-bg:      #1e1e1e;
            --code-text:    #d4d4d4;
            --sidebar-active-bg: #e3e1dd;
            --sidebar-avatar-bg: #d4d2ce;
            --btn-primary-text: #fff;
            --btn-cancel-bg: #fff;
            --danger-bg:    #fee2e2;
            --danger-text:  #dc2626;
            --danger-border:#fecaca;
            --danger-hover: #fecaca;
            --scrollbar-thumb: #d1cfcb;
            --scrollbar-thumb-hover: #b8b5b0;
            --skeleton-from: #ececea;
            --skeleton-to: #e4e2df;
            --link-color:   #2563eb;
            --agent-toggle-color: #2563eb;
            --overlay-bg:   rgba(0,0,0,.35);
            --modal-shadow: 0 8px 30px rgba(0,0,0,.15);
        }

        /* ── Aries Dark (from Android M3T night tokens) ── */
        :root.dark {
            --bg:           #0F141B;
            --sidebar-bg:   #152231;
            --card:         #121A24;
            --border:       #0A0D12;
            --text:         #E1E7F1;
            --text-secondary: #C1C7D3;
            --text-muted:   #9AA4B5;
            --accent:       #A9C7FF;
            --accent-hover: #7FB4FF;
            --hover:        rgba(255,255,255,.06);
            --bubble-user:  #234067;
            --bubble-user-text: #E1E7F1;
            --bubble-agent: #223247;
            --shadow-sm:    0 1px 2px rgba(0,0,0,.3);
            --shadow-md:    0 2px 8px rgba(0,0,0,.4);
            --modal-bg:     #121A24;
            --titlebar-bg:  rgba(15,20,27,.9);
            --input-bar-bg: rgba(15,20,27,.9);
            --code-bg:      #0F172A;
            --code-text:    #A9C7FF;
            --sidebar-active-bg: #1A2A3C;
            --sidebar-avatar-bg: #A9C7FF;
            --btn-primary-text: #003061;
            --btn-cancel-bg: #18222E;
            --danger-bg:    #3F1D1D;
            --danger-text:  #FFB4AB;
            --danger-border:#5F3030;
            --danger-hover: #4F2828;
            --scrollbar-thumb: #434A56;
            --scrollbar-thumb-hover: #8B92A0;
            --skeleton-from: #1E293B;
            --skeleton-to: #121A24;
            --link-color:   #A9C7FF;
            --agent-toggle-color: #A9C7FF;
            --overlay-bg:   rgba(0,0,0,.6);
            --modal-shadow: 0 8px 30px rgba(0,0,0,.5);
        }

        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: var(--font);
            overflow: hidden;
            background: var(--bg) !important;
            color: var(--text) !important;
            -webkit-font-smoothing: antialiased;
        }

        ::-webkit-scrollbar { width: 5px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: var(--scrollbar-thumb); border-radius: 10px; }
        ::-webkit-scrollbar-thumb:hover { background: var(--scrollbar-thumb-hover); }

        /* ---- Sidebar ---- */
        .sidebar {
            width: 240px; height: 100vh; flex-shrink: 0;
            background: var(--sidebar-bg);
            border-right: 1px solid var(--border);
            display: flex; flex-direction: column;
        }
        .sidebar-brand {
            height: 48px; display: flex; align-items: center; padding: 0 20px;
            font-size: 16px; font-weight: 600; letter-spacing: -.01em;
        }
        .sidebar-nav {
            flex: 1; overflow-y: auto; padding: 4px 10px;
        }
        .sidebar-section-title {
            font-size: 11px; font-weight: 500; color: var(--text-muted);
            padding: 16px 10px 6px; letter-spacing: .04em;
        }
        .sidebar-item {
            display: flex; align-items: center; gap: 8px;
            padding: 7px 10px; border-radius: var(--radius-sm);
            font-size: 13px; color: var(--text-secondary);
            cursor: pointer; transition: background .12s;
            white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
        }
        .sidebar-item:hover { background: var(--hover); color: var(--text); }
        .sidebar-item.active { background: var(--sidebar-active-bg); color: var(--text); font-weight: 500; }
        .sidebar-item svg { flex-shrink: 0; opacity: .55; }
        .sidebar-item.active svg { opacity: .8; }

        .sidebar-footer {
            padding: 10px; border-top: 1px solid var(--border);
            display: flex; align-items: center; gap: 8px; font-size: 13px;
        }
        .sidebar-footer .avatar {
            width: 26px; height: 26px; border-radius: 50%;
            background: var(--sidebar-avatar-bg); display: flex; align-items: center; justify-content: center;
            font-size: 11px; color: var(--btn-primary-text); flex-shrink: 0;
        }

        /* ---- Main ---- */
        .main {
            flex: 1; height: 100vh; display: flex; flex-direction: column;
            min-width: 0; background: var(--bg);
        }

        /* Titlebar */
        .titlebar {
            height: 48px; display: flex; align-items: center; justify-content: center;
            padding: 0 16px; border-bottom: 1px solid var(--border);
            background: var(--titlebar-bg); backdrop-filter: blur(8px);
            -webkit-app-region: drag; user-select: none;
        }
        .titlebar-title {
            font-size: 13px; font-weight: 500; color: var(--text-secondary);
        }
        .titlebar-actions {
            position: absolute; right: 8px; display: flex; align-items: center; gap: 4px;
            -webkit-app-region: no-drag;
        }
        .agent-toggle {
            border: none; background: none; cursor: pointer; font-size: 14px;
            padding: 4px 6px; border-radius: 4px; color: var(--text-muted);
            transition: all .15s; -webkit-app-region: no-drag; opacity: .45;
        }
        .agent-toggle:hover { opacity: .8; background: var(--hover); }
        .agent-toggle.active { opacity: 1; color: var(--agent-toggle-color); }
        .titlebar-btn {
            width: 32px; height: 32px; border: none; border-radius: 6px;
            background: transparent; cursor: pointer;
            display: flex; align-items: center; justify-content: center;
            color: var(--text-secondary); transition: all .12s; font-size: 12px;
            font-family: inherit;
        }
        .titlebar-btn:hover { background: var(--hover); color: var(--text); }
        .titlebar-btn.close:hover { background: #e81123; color: var(--btn-primary-text); }

        /* Chat area */
        .chat-area {
            flex: 1; overflow-y: auto; padding: 24px 20px;
            display: flex; flex-direction: column; gap: 20px;
        }

        .msg-system {
            text-align: center; font-size: 12px; color: var(--text-muted);
        }

        .msg-user {
            display: flex; justify-content: flex-end;
        }
        .msg-user .bubble {
            max-width: 65%; background: var(--bubble-user);
            padding: 10px 16px; border-radius: 14px 14px 4px 14px;
            font-size: 14px; line-height: 1.55; color: var(--bubble-user-text) !important;
        }

        .msg-agent {
            display: flex; gap: 10px;
        }
        .msg-agent .avatar {
            width: 30px; height: 30px; border-radius: 50%;
            background: var(--code-bg); flex-shrink: 0;
            display: flex; align-items: center; justify-content: center;
            color: var(--code-text); font-size: 12px; font-weight: 600;
        }
        .msg-agent .body { max-width: 70%; }
        .msg-agent .name {
            font-size: 12px; font-weight: 500; color: var(--accent); margin-bottom: 4px;
        }
        .msg-agent .bubble {
            background: var(--bubble-agent);
            border: 1px solid var(--border);
            padding: 10px 16px; border-radius: 14px 14px 14px 4px;
            font-size: 14px; line-height: 1.6; color: var(--text) !important;
            box-shadow: var(--shadow-sm);
        }
        .msg-agent .actions {
            display: flex; gap: 12px; margin-top: 6px; padding-left: 2px;
        }
        .msg-agent .actions button {
            border: none; background: none; color: var(--text-muted); cursor: pointer;
            font-size: 11px; padding: 2px 4px; transition: color .12s;
        }
        .msg-agent .actions button:hover { color: var(--text); }

        /* Markdown */
        .bubble p { margin: 0 0 8px; }
        .bubble p:last-child { margin-bottom: 0; }
        .bubble h2, .bubble h3, .bubble h4 { margin: 12px 0 6px; font-weight: 600; line-height: 1.3; }
        .bubble h2 { font-size: 16px; }
        .bubble h3 { font-size: 14px; }
        .bubble h4 { font-size: 13px; }
        .bubble ul, .bubble ol { margin: 4px 0; padding-left: 20px; }
        .bubble li { margin: 2px 0; }
        .bubble code {
            background: var(--hover); padding: 2px 6px; border-radius: 4px;
            font-size: 12px; font-family: "Cascadia Code", "Fira Code", "JetBrains Mono", Consolas, monospace;
        }
        .bubble pre {
            background: var(--code-bg); color: var(--code-text); padding: 12px 14px; border-radius: 8px;
            overflow-x: auto; margin: 8px 0; font-size: 12px; line-height: 1.5;
        }
        .bubble pre code {
            background: none; padding: 0; border-radius: 0; color: inherit; font-size: inherit;
        }
        .bubble strong { font-weight: 600; }
        .bubble em { font-style: italic; }
        .bubble h1 { font-size: 17px; font-weight: 700; margin: 12px 0 6px; }
        .bubble blockquote {
            border-left: 3px solid var(--border); margin: 8px 0; padding: 4px 12px;
            color: var(--text-secondary); background: var(--hover); border-radius: 0 4px 4px 0;
        }
        .bubble hr { border: none; border-top: 1px solid var(--border); margin: 12px 0; }
        .bubble a { color: var(--link-color); text-decoration: none; }
        .bubble a:hover { text-decoration: underline; }
        .bubble img { max-width: 100%; border-radius: 8px; margin: 4px 0; }
        .bubble table {
            border-collapse: collapse; width: 100%; margin: 8px 0; font-size: 12px;
        }
        .bubble th, .bubble td {
            border: 1px solid var(--border); padding: 6px 10px; text-align: left;
        }
        .bubble th { background: var(--hover); font-weight: 600; }
        .bubble del { text-decoration: line-through; color: var(--text-muted); }
        .bubble .task-done { color: var(--text-muted); text-decoration: line-through; list-style: none; }
        .bubble .task-pending { list-style: none; }

        .thinking-block {
            margin-bottom: 8px; font-size: 12px;
            border: 1px solid var(--border); border-radius: 6px;
            background: var(--hover); overflow: hidden;
        }
        .thinking-block summary {
            padding: 6px 10px; cursor: pointer; color: var(--text-muted);
            user-select: none; font-weight: 500;
        }
        .thinking-block summary:hover { color: var(--text); }
        .thinking-content {
            padding: 6px 10px 10px; color: var(--text-muted);
            white-space: pre-wrap; line-height: 1.5;
            border-top: 1px solid var(--border);
        }

        .tool-block {
            margin-bottom: 8px; font-size: 12px;
            border: 1px solid var(--border); border-radius: 6px;
            background: var(--hover); overflow: hidden;
        }
        .tool-block summary {
            padding: 6px 10px; cursor: pointer; color: var(--text-muted);
            user-select: none; font-weight: 500;
        }
        .tool-block summary:hover { color: var(--text); }
        .tool-content {
            padding: 6px 10px 10px; color: var(--text-muted);
            white-space: pre-wrap; line-height: 1.5;
            border-top: 1px solid var(--border);
            max-height: 300px; overflow-y: auto;
        }

        /* File chips */
        .file-chips {
            display: flex; flex-wrap: wrap; gap: 6px;
            padding: 6px 16px 0;
            max-width: 720px; margin: 0 auto; width: 100%;
        }
        .file-chip {
            display: inline-flex; align-items: center; gap: 4px;
            padding: 3px 8px; font-size: 12px;
            background: var(--card); border: 1px solid var(--border);
            border-radius: 6px; max-width: 200px;
        }
        .file-chip-icon { font-size: 12px; flex-shrink: 0; }
        .file-chip-name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--text-secondary); }
        .file-chip-remove {
            border: none; background: none; cursor: pointer;
            color: var(--text-muted); font-size: 10px; padding: 0 2px;
            flex-shrink: 0; line-height: 1;
        }
        .file-chip-remove:hover { color: var(--danger-text); }

        .toast {
            position: fixed; bottom: 80px; left: 50%; transform: translateX(-50%);
            background: var(--accent); color: var(--btn-primary-text); padding: 8px 20px; border-radius: 8px;
            font-size: 13px; z-index: 999; transition: opacity .3s;
            pointer-events: none;
        }

        /* Input */
        .input-area {
            padding: 12px 16px 16px; border-top: 1px solid var(--border);
            background: var(--input-bar-bg); backdrop-filter: blur(8px);
        }
        .input-row {
            max-width: 720px; margin: 0 auto;
            display: flex; align-items: flex-end; gap: 8px;
            background: var(--card); border: 1px solid var(--border);
            border-radius: var(--radius); padding: 8px 12px;
            box-shadow: var(--shadow-sm);
            transition: border-color .15s, box-shadow .15s;
        }
        .input-row:focus-within {
            border-color: var(--accent);
            box-shadow: var(--shadow-md);
        }
        .input-row textarea {
            flex: 1; border: none; outline: none; resize: none;
            font-family: var(--font); font-size: 14px; line-height: 1.5;
            color: var(--text); background: transparent;
        }
        .input-row textarea::placeholder { color: var(--text-muted); }
        .input-row .btn-attach {
            border: none; background: none; color: var(--text-muted); cursor: pointer;
            font-size: 12px; white-space: nowrap; padding: 4px 6px; border-radius: 4px;
            transition: color .12s;
        }
        .input-row .btn-attach:hover { color: var(--text); }
        .input-row .btn-send {
            width: 30px; height: 30px; border: none; border-radius: 50%;
            background: var(--accent); color: var(--btn-primary-text); cursor: pointer;
            display: flex; align-items: center; justify-content: center;
            transition: background .12s; flex-shrink: 0;
        }
        .input-row .btn-send:hover { background: var(--accent-hover); }
        .input-row .btn-send:disabled { background: var(--sidebar-avatar-bg); cursor: default; }
        .input-row .btn-stop { background: var(--agent-toggle-color); color: var(--btn-primary-text); }
        .input-row .btn-stop:hover { opacity: .85; }
        .agent-btn {
            display: inline-flex; align-items: center; gap: 5px;
            padding: 4px 12px; border-radius: 14px; border: 1px solid var(--border);
            background: var(--card); color: var(--text-secondary);
            font-size: 11px; cursor: pointer; font-family: var(--font);
            transition: all .15s linear;
        }
        .agent-btn:hover { background: var(--hover); }
        .agent-btn.active { background: var(--agent-toggle-color); color: var(--btn-primary-text); border-color: var(--agent-toggle-color); }
        /* Resize handles */
        .resize-handle { position: fixed; z-index: 9999; user-select: none; -webkit-user-select: none; pointer-events: auto; }
        .resize-handle.n { top: 0; left: 6px; right: 6px; height: 6px; cursor: n-resize; }
        .resize-handle.s { bottom: 0; left: 6px; right: 6px; height: 6px; cursor: s-resize; }
        .resize-handle.w { left: 0; top: 6px; bottom: 6px; width: 6px; cursor: w-resize; }
        .resize-handle.e { right: 0; top: 6px; bottom: 6px; width: 6px; cursor: e-resize; }
        .resize-handle.nw { top: 0; left: 0; width: 12px; height: 12px; cursor: nw-resize; }
        .resize-handle.ne { top: 0; right: 0; width: 12px; height: 12px; cursor: ne-resize; }
        .resize-handle.sw { bottom: 0; left: 0; width: 12px; height: 12px; cursor: sw-resize; }
        .resize-handle.se { bottom: 0; right: 0; width: 12px; height: 12px; cursor: se-resize; }

        .disclaimer {
            text-align: center; font-size: 11px; color: var(--text-muted);
            margin-top: 8px;
        }

        /* Animations */
        .fade-in { animation: fadeIn .2s ease-out; }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(4px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .skeleton {
            background: linear-gradient(90deg, var(--skeleton-from) 25%, var(--skeleton-to) 50%, var(--skeleton-from) 75%);
            background-size: 200% 100%;
            animation: shimmer 1.4s infinite;
            border-radius: 4px;
        }
        @keyframes shimmer {
            0% { background-position: 200% 0; }
            100% { background-position: -200% 0; }
        }

        .sidebar-footer .user-info {
            display: flex; align-items: center; gap: 8px;
            flex: 1; min-width: 0;
        }
        .sidebar-footer .user-name {
            overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
        }
        .sidebar-footer .btn-settings {
            width: 28px; height: 28px; border: none; background: transparent;
            border-radius: 6px; cursor: pointer; color: var(--text-muted);
            display: flex; align-items: center; justify-content: center;
            font-size: 16px; flex-shrink: 0; margin-left: auto;
            transition: background .12s, color .12s;
        }
        .sidebar-footer .btn-settings:hover { background: var(--hover); color: var(--text); }

        .modal-overlay {
            visibility: hidden; opacity: 0; pointer-events: none;
            position: fixed; inset: 0; background: var(--overlay-bg);
            display: flex; align-items: center; justify-content: center;
            z-index: 100;
            transition: opacity 0.2s linear, visibility 0.2s linear;
        }
        .modal-overlay.active { visibility: visible; opacity: 1; pointer-events: auto; }
        .modal-box {
            background: var(--modal-bg); border-radius: 12px; padding: 28px;
            width: 520px; max-height: 85vh; overflow-y: auto;
            box-shadow: var(--modal-shadow);
            transform: translateY(12px) scale(0.97);
            transition: transform 0.25s linear;
            position: relative;
        }
        .modal-overlay.active .modal-box {
            transform: translateY(0) scale(1);
        }
        .modal-box h3 { font-size: 16px; font-weight: 600; margin-bottom: 16px; }
        .modal-box label {
            display: block; font-size: 12px; color: var(--text-muted);
            margin-bottom: 4px; margin-top: 12px;
        }
        .modal-box input {
            width: 100%; padding: 8px 10px; border: 1px solid var(--border);
            border-radius: 6px; font-size: 13px; font-family: var(--font);
            outline: none; transition: border-color .12s; box-sizing: border-box;
        }
        .modal-box input:focus { border-color: var(--accent); }
        .modal-actions {
            display: flex; gap: 8px; justify-content: flex-end; margin-top: 20px;
        }
        .btn-primary {
            padding: 7px 18px; border: none; border-radius: 6px;
            background: var(--accent); color: var(--btn-primary-text); font-size: 13px;
            cursor: pointer; font-family: var(--font); transition: background .12s;
        }
        .btn-primary:hover { background: var(--accent-hover); }
        .btn-cancel {
            padding: 7px 18px; border: 1px solid var(--border); border-radius: 6px;
            background: var(--btn-cancel-bg); color: var(--text); font-size: 13px;
            cursor: pointer; font-family: var(--font); transition: background .12s;
        }
        .btn-cancel:hover { background: var(--hover); }
        .btn-danger { background: var(--danger-bg); color: var(--danger-text); border: 1px solid var(--danger-border); border-radius: var(--radius-sm); padding: 6px 12px; font-size: 12px; cursor: pointer; }
        .btn-danger:hover { background: var(--danger-hover); }
        .btn-sm { background: var(--hover); border: 1px solid var(--border); border-radius: var(--radius-sm); padding: 5px 10px; font-size: 12px; cursor: pointer; font-family: var(--font); color: var(--text); }
        .btn-sm:hover { background: var(--sidebar-active-bg); }

        /* Vertical tabs (right side) */
        .vtabs { position: relative; }
        .vtabs-indicator {
            position: absolute; right: 0; width: 2px; height: 32px;
            background: var(--accent); border-radius: 1px;
            top: 4px; pointer-events: none;
            transition: top 0.2s linear;
        }
        .vtab {
            padding: 8px 10px; font-size: 12px; cursor: pointer; color: var(--text-secondary);
            border-radius: var(--radius-sm); transition: all 0.15s linear;
            text-align: center; user-select: none; position: relative; z-index: 1;
        }
        .vtab:hover { background: var(--hover); color: var(--text); }
        .vtab.active { color: var(--text); font-weight: 500; }
        /* Theme chips */
        .theme-chip {
            width: 28px; height: 28px; border-radius: 50%; cursor: pointer;
            transition: transform 0.15s linear, box-shadow 0.15s linear;
            flex-shrink: 0;
        }
        .theme-chip:hover { transform: scale(1.15); }
        .theme-chip.active { box-shadow: 0 0 0 2px var(--card), 0 0 0 4px var(--accent); }
        /* Horizontal tabs (unused, kept for ref) */
        .tabs { display: flex; gap: 0; margin-bottom: 16px; border-bottom: 2px solid var(--border); }
        .tab { padding: 8px 16px; font-size: 13px; cursor: pointer; color: var(--text-secondary); border-bottom: 2px solid transparent; margin-bottom: -2px; transition: all .15s linear; }
        .tab:hover { color: var(--text); }
        .tab.active { color: var(--text); font-weight: 600; border-bottom-color: var(--accent); }

        /* MCP server row */
        .mcp-server-row { display: flex; gap: 6px; align-items: center; margin-bottom: 6px; padding: 6px; background: var(--sidebar-bg); border-radius: var(--radius-sm); }
        .mcp-server-row input { flex: 1; min-width: 0; padding: 5px 8px; font-size: 12px; border: 1px solid var(--border); border-radius: 4px; outline: none; font-family: var(--font); }
        .mcp-server-row input:focus { border-color: var(--accent); }
        .mcp-server-row .mcp-label { flex: 0 0 90px; }
        .mcp-server-row .mcp-cmd { flex: 2; }
        .mcp-server-row .mcp-args { flex: 1; }
        .mcp-header { display: flex; gap: 6px; font-size: 11px; color: var(--text-muted); margin-bottom: 4px; padding: 0 6px; }
        .mcp-header span:nth-child(1) { flex: 0 0 90px; }
        .mcp-header span:nth-child(2) { flex: 2; }
        .mcp-header span:nth-child(3) { flex: 1; }

        .ps-script {
            background: var(--code-bg); color: var(--code-text); padding: 12px;
            border-radius: 8px; font-family: Consolas, monospace;
            font-size: 12px; white-space: pre-wrap; max-height: 200px;
            overflow-y: auto; margin: 8px 0; line-height: 1.5;
        }

    </style>
</head>
<body style="display:flex;">
    <div class="resize-handle n" id="rh-n"></div><div class="resize-handle s" id="rh-s"></div>
    <div class="resize-handle w" id="rh-w"></div><div class="resize-handle e" id="rh-e"></div>
    <div class="resize-handle nw" id="rh-nw"></div><div class="resize-handle ne" id="rh-ne"></div>
    <div class="resize-handle sw" id="rh-sw"></div><div class="resize-handle se" id="rh-se"></div>

    <!-- Sidebar -->
    <aside class="sidebar">
        <div class="sidebar-brand">Open Aries AI</div>
        <nav class="sidebar-nav" id="sideNav"></nav>
        <div class="sidebar-footer" id="sideUser"></div>
    </aside>

    <!-- Main -->
    <main class="main">
        <header class="titlebar" id="titlebar">
            <span class="titlebar-title" id="chatTitle">与 Open Aries AI 的对话</span>
            <div class="titlebar-actions">
                <button class="titlebar-btn" id="btnMin" title="最小化">─</button>
                <button class="titlebar-btn" id="btnMax" title="最大化">□</button>
                <button class="titlebar-btn close" id="btnClose" title="关闭">✕</button>
            </div>
        </header>

        <div class="chat-area" id="chatContainer"></div>

        <div class="file-chips" id="fileChips" style="display:none;"></div>

        <div class="input-area">
            <div class="input-row">
                <button class="btn-attach" id="attachBtn">+ 文件</button>
                <textarea id="msgInput" placeholder="输入任务，交给我来帮你完成" rows="1"></textarea>
                <button class="btn-send" id="sendBtn" disabled>
                    <svg id="sendIcon" width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/></svg>
                    <svg id="stopIcon" width="13" height="13" viewBox="0 0 24 24" fill="currentColor" stroke="none" style="display:none;"><rect x="4" y="4" width="16" height="16" rx="2"/></svg>
                </button>
            </div>
            <div style="display:flex;align-items:center;justify-content:space-between;margin-top:6px;">
                <button class="agent-btn" id="btnAgent" onclick="toggleAgent()">Agent 模式</button>
                <p class="disclaimer">以上内容由 AI 生成</p>
            </div>
        </div>
    </main>

    <script>
        // === Data ===
        const App = {
            user: null,
            sessions: [],
            activeSessionId: 'default',
            messages: [],
            currentSessionId: null,
            pendingFiles: []
        };

        function saveCurrentMessages() {
            const s = App.sessions.find(x => x.id === App.activeSessionId);
            if (s) s.messages = [...App.messages];
        }
        function switchSession(id) {
            // Save and check if old session is empty → auto-delete
            if (App.activeSessionId !== id) {
                saveCurrentMessages();
                const old = App.sessions.find(x => x.id === App.activeSessionId);
                if (old && old.id !== 'default' && !hasUserMessages(old)) {
                    App.sessions = App.sessions.filter(x => x.id !== old.id);
                }
            }
            App.activeSessionId = id;
            let s = App.sessions.find(x => x.id === id);
            if (!s) { s = { id, title: '新对话', messages: [] }; App.sessions.unshift(s); }
            App.messages = [...s.messages];
            renderNav();
            renderMessages();
            document.getElementById('chatTitle').textContent = s.title || '与 Open Aries AI 的对话';
            post('switchSession', { sessionId: id });
        }
        function hasUserMessages(s) {
            return s.messages && s.messages.some(m => m.type === 'user');
        }

        // === Icons (inline SVGs, no FontAwesome) ===
        const icons = {
            chat:     '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15a2 2 0 01-2 2H7l-4 4V5a2 2 0 012-2h14a2 2 0 012 2z"/></svg>',
            compass:  '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><polygon points="16.24 7.76 14.12 14.12 7.76 16.24 9.88 9.88 16.24 7.76"/></svg>',
            book:     '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5A2.5 2.5 0 016.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 014 19.5v-15A2.5 2.5 0 016.5 2z"/></svg>',
            folder:   '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z"/></svg>',
            user:     '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2"/><circle cx="12" cy="7" r="4"/></svg>',
        };

        // === Render ===
        function renderNav() {
            const nav = document.getElementById('sideNav');
            nav.innerHTML = `
                <div class="sidebar-section-title">对话</div>
                <div class="sidebar-item" onclick="newSession()" style="color:var(--accent);font-weight:500;">
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
                    <span>新对话</span>
                </div>
                ${App.sessions.map(s => `
                    <div class="sidebar-item ${s.id === App.activeSessionId ? 'active' : ''}" onclick="switchSession('${s.id}')">
                        ${icons.chat}
                        <span>${esc(s.title || '新对话')}</span>
                    </div>
                `).join('')}
            `;
        }
        function newSession() {
            // Delete current session if empty (and not default)
            saveCurrentMessages();
            const old = App.sessions.find(x => x.id === App.activeSessionId);
            if (old && old.id !== 'default' && !hasUserMessages(old)) {
                App.sessions = App.sessions.filter(x => x.id !== old.id);
            }
            post('newSession', {});
        }
        window.addSession = function(id, title) {
            App.sessions.unshift({ id, title, messages: [] });
            switchSession(id);
        };
        function toggleAgent() {
            post('toggleAgent', {});
        }
        window.setAgentMode = function(active) {
            const btn = document.getElementById('btnAgent');
            if (active) { btn.classList.add('active'); btn.innerHTML = 'Agent 已开启'; }
            else { btn.classList.remove('active'); btn.innerHTML = 'Agent 模式'; }
        };


        function renderMessages() {
            const c = document.getElementById('chatContainer');
            c.innerHTML = App.messages.map((m, i) => {
                if (m.type === 'system') {
                    return `<div class="msg-system fade-in" style="animation-delay:${i*0.02}s">${esc(m.content)}</div>`;
                }
                if (m.type === 'user') {
                    return `<div class="msg-user fade-in" style="animation-delay:${i*0.02}s"><div class="bubble">${md(m.content)}</div></div>`;
                }
                return `
                    <div class="msg-agent fade-in" style="animation-delay:${i*0.02}s">
                        <div class="avatar">${(m.name||'M')[0]}</div>
                        <div class="body">
                            <div class="name">${esc(m.name||'Open Aries AI')}${m.status ? `<span style="color:var(--text-muted);font-weight:400;margin-left:4px;">${esc(m.status)}</span>` : ''}</div>
                            ${m.thinking ? `<details class="thinking-block"><summary>思考过程</summary><div class="thinking-content">${esc(m.thinking)}</div></details>` : ''}
                            ${m.toolResults ? m.toolResults.map(t => t.imageBase64 ? `<details class="tool-block"><summary>📸 ${esc(t.name)}</summary><div class="tool-content" style="padding:8px;"><img src="data:image/png;base64,${t.imageBase64}" style="max-width:100%;border-radius:6px;" /></div></details>` : `<details class="tool-block"><summary>🔧 ${esc(t.name)}</summary><div class="tool-content">${esc(t.result)}</div></details>`).join('') : ''}
                            ${m.content ? `<div class="bubble">${md(m.content)}</div>` : (m.streaming ? '' : '<div class="bubble" style="color:var(--text-muted);">...</div>')}
                            ${m.showActions ? `<div class="actions">
                                <button onclick="copyMsg('${m.id}')">复制</button>
                            </div>` : ''}
                        </div>
                    </div>`;
            }).join('');
            c.scrollTop = c.scrollHeight;
        }

        function renderUser() {
            if (!App.user) return;
            document.getElementById('sideUser').innerHTML = `
                <div class="avatar">${icons.user}</div>
                <span class="user-name">${esc(App.user.name)}</span>
                <button class="btn-settings" title="设置" onclick="openSettings()">&#9881;</button>`;
        }

        let _settingsTab = 'api';
        function moveIndicator() {
            var activeTab = document.querySelector('.vtab.active');
            if (!activeTab) return;
            var container = activeTab.parentElement;
            var containerRect = container.getBoundingClientRect();
            var tabRect = activeTab.getBoundingClientRect();
            var top = tabRect.top - containerRect.top;
            document.getElementById('vtabIndicator').style.top = top + 'px';
        }
        function switchSettingsTab(tab) {
            _settingsTab = tab;
            document.getElementById('tabApi').classList.toggle('active', tab === 'api');
            document.getElementById('tabMcp').classList.toggle('active', tab === 'mcp');
            document.getElementById('tabAppearance').classList.toggle('active', tab === 'appearance');
            document.getElementById('tabContentApi').style.display = tab === 'api' ? '' : 'none';
            document.getElementById('tabContentMcp').style.display = tab === 'mcp' ? '' : 'none';
            document.getElementById('tabContentAppearance').style.display = tab === 'appearance' ? '' : 'none';
            moveIndicator();
            if (tab === 'mcp') post('getMcpConfig', {});
        }
        function openSettings() {
            document.getElementById('settingsModal').classList.add('active');
            switchSettingsTab('api');
            post('getConfig', {});
        }
        window.showConfig = function(cfg) {
            document.getElementById('cfgHost').value = cfg.host || '';
            document.getElementById('cfgKey').value = cfg.key || '';
            document.getElementById('cfgModel').value = cfg.model || '';
        };
        function applyTheme(name) {
            var root = document.documentElement;
            root.classList.toggle('dark', name === 'dark');
            var chips = document.querySelectorAll('.theme-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.toggle('active', chips[i].dataset.theme === name);
            }
            try { localStorage.setItem('aries-theme', name); } catch(e) {}
        }
        // Init on DOM ready
        function initTheme() {
            var saved = 'dark';
            try { saved = localStorage.getItem('aries-theme') || 'dark'; } catch(e) {}
            applyTheme(saved);
        }
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', initTheme);
        } else {
            initTheme();
        }

        function closeSettings() {
            document.getElementById('settingsModal').classList.remove('active');
        }
        // ---- MCP server list ----
        window.showMcpConfig = function(servers) {
            var list = document.getElementById('mcpServerList');
            list.innerHTML = '';
            if (!servers || !servers.length) {
                list.innerHTML = '<div style="font-size:12px;color:var(--text-muted);padding:12px;text-align:center;">暂无 MCP 服务器</div>';
                return;
            }
            servers.forEach(function(s, i) {
                addMcpRow(s.label, s.command, s.args || '');
            });
        };
        function addMcpRow(label, command, args) {
            label = label || ''; command = command || ''; args = args || '';
            var row = document.createElement('div');
            row.className = 'mcp-server-row';
            row.innerHTML =
                '<input class="mcp-label" placeholder="标识" value="' + escAttr(label) + '">' +
                '<input class="mcp-cmd" placeholder="demo\\mcp_server.exe" value="' + escAttr(command) + '">' +
                '<input class="mcp-args" placeholder="参数 (可选)" value="' + escAttr(args) + '">' +
                '<button class="btn-danger" onclick="this.parentElement.remove()" title="移除">✕</button>';
            document.getElementById('mcpServerList').appendChild(row);
        }
        function escAttr(s) {
            return s.replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
        }
        function collectMcpServers() {
            var rows = document.querySelectorAll('#mcpServerList .mcp-server-row');
            var servers = [];
            rows.forEach(function(row) {
                var inputs = row.querySelectorAll('input');
                var label = inputs[0].value.trim();
                var cmd = inputs[1].value.trim();
                var args = inputs[2].value.trim();
                if (label || cmd) servers.push({label: label, command: cmd, args: args});
            });
            return servers;
        }
        function testConnection() {
            const btn = document.getElementById('btnTestConn');
            const span = document.getElementById('testResult');
            btn.disabled = true; btn.textContent = '测试中...'; span.textContent = '';
            post('testConnection', {
                host: document.getElementById('cfgHost').value.trim(),
                key: document.getElementById('cfgKey').value.trim(),
                model: document.getElementById('cfgModel').value.trim()
            });
        }
        window.showTestResult = function(ok, msg) {
            const btn = document.getElementById('btnTestConn');
            const span = document.getElementById('testResult');
            btn.disabled = false; btn.textContent = '测试连接';
            span.textContent = msg;
            span.style.color = ok ? '#16a34a' : '#dc2626';
        };
        function saveSettings() {
            post('saveConfig', {
                host: document.getElementById('cfgHost').value.trim(),
                key: document.getElementById('cfgKey').value.trim(),
                model: document.getElementById('cfgModel').value.trim()
            });
            // Also save MCP config
            post('saveMcpConfig', { servers: collectMcpServers() });
            closeSettings();
        }
        let _confirmType = 'ps'; // 'ps' or 'kill'
        document.addEventListener('click', function(e) {
            if (e.target.id === 'settingsModal') closeSettings();
            if (e.target.id === 'psConfirmModal') { denyConfirm(); }
        });

        window.confirmPowerShell = function(script) {
            _confirmType = 'ps';
            document.getElementById('confirmTitle').textContent = 'PowerShell 执行确认';
            document.getElementById('confirmDesc').textContent = 'AI 请求执行以下 PowerShell 命令：';
            document.getElementById('psScript').textContent = script;
            document.getElementById('psConfirmModal').classList.add('active');
        };
        window.confirmKill = function(name, pid) {
            _confirmType = 'kill';
            document.getElementById('confirmTitle').textContent = '终止进程确认';
            document.getElementById('confirmDesc').textContent = 'AI 请求终止以下进程（PID: ' + pid + '）：';
            document.getElementById('psScript').textContent = name;
            document.getElementById('rememberRow').style.display = 'none';
            document.getElementById('psConfirmModal').classList.add('active');
        };
        window.confirmPerm = function(permission, pattern) {
            _confirmType = 'perm';
            _permPermission = permission;
            _permPattern = pattern;
            var titles = {execute:'操作确认',write:'写入确认',edit:'编辑确认','delete':'删除确认'};
            document.getElementById('confirmTitle').textContent = titles[permission] || '权限确认';
            document.getElementById('confirmDesc').textContent = 'AI 请求执行 ' + permission + ' 操作：';
            document.getElementById('psScript').textContent = pattern.length > 200 ? pattern.substr(0,200)+'...' : pattern;
            document.getElementById('rememberRow').style.display = 'flex';
            document.getElementById('rememberCheck').checked = false;
            document.getElementById('psConfirmModal').classList.add('active');
        };
        function approveConfirm() {
            document.getElementById('psConfirmModal').classList.remove('active');
            var remember = document.getElementById('rememberCheck').checked;
            if (_confirmType === 'kill') {
                post('confirmKill', {approved: true});
            } else if (_confirmType === 'perm') {
                post('confirmPerm', {approved: true, remember: remember});
            } else {
                post('confirmRunPs', {approved: true});
            }
        }
        function denyConfirm() {
            document.getElementById('psConfirmModal').classList.remove('active');
            var remember = document.getElementById('rememberCheck').checked;
            if (_confirmType === 'kill') {
                post('confirmKill', {approved: false});
            } else if (_confirmType === 'perm') {
                post('confirmPerm', {approved: false, remember: remember});
            } else {
                post('confirmRunPs', {approved: false});
            }
        }

        function esc(s) {
            const d = document.createElement('div');
            d.textContent = s;
            return d.innerHTML;
        }

        function md(s) {
            let html = esc(s);
            const blocks = [];

            // Protect fenced code blocks (```lang\ncode\n```)
            html = html.replace(/```(\w*)\n?([\s\S]*?)```/g, (_, lang, code) => {
                blocks.push(`<pre><code class="lang-${lang}">${code.replace(/\n$/, '')}</code></pre>`);
                return `B${blocks.length - 1}E`;
            });

            // Protect inline code (`code`)
            html = html.replace(/`([^`\n]+)`/g, (_, code) => {
                blocks.push(`<code>${code}</code>`);
                return `B${blocks.length - 1}E`;
            });

            // Tables — process line by line
            const lines = html.split('\n');
            let out = [];
            let tableRows = [];
            let inTable = false;

            function flushTable() {
                if (tableRows.length < 2) { out.push(...tableRows); tableRows = []; return; }
                // First row is header, second is separator
                const header = tableRows[0];
                const sep = tableRows[1];
                if (!/^\|.*\|$/.test(header) || !/^\|[\-\| :]+\|$/.test(sep)) {
                    out.push(...tableRows); tableRows = []; return;
                }
                // Parse alignments
                const aligns = sep.slice(1, -1).split('|').map(c => {
                    const t = c.trim();
                    if (t.startsWith(':') && t.endsWith(':')) return 'center';
                    if (t.endsWith(':')) return 'right';
                    return 'left';
                });
                let tbl = '<table><thead><tr>';
                header.slice(1, -1).split('|').forEach((h, i) => {
                    tbl += `<th style="text-align:${aligns[i]||'left'}">${h.trim()}</th>`;
                });
                tbl += '</tr></thead><tbody>';
                for (let r = 2; r < tableRows.length; r++) {
                    tbl += '<tr>';
                    tableRows[r].slice(1, -1).split('|').forEach((c, i) => {
                        tbl += `<td style="text-align:${aligns[i]||'left'}">${c.trim()}</td>`;
                    });
                    tbl += '</tr>';
                }
                tbl += '</tbody></table>';
                out.push(tbl);
                tableRows = [];
            }

            for (const line of lines) {
                const isPipeRow = /^\|.*\|$/.test(line.trim());
                if (isPipeRow) {
                    tableRows.push(line);
                } else {
                    flushTable();
                    out.push(line);
                }
            }
            flushTable();
            html = out.join('\n');

            // Strikethrough
            html = html.replace(/~~([^~\n]+)~~/g, '<del>$1</del>');
            // Bold
            html = html.replace(/\*\*([^*\n]+)\*\*/g, '<strong>$1</strong>');
            // Italic
            html = html.replace(/\*([^*\n]+)\*/g, '<em>$1</em>');
            // Images before links
            html = html.replace(/!\[([^\]]*)\]\(([^)\s]+)\)/g, '<img src="$2" alt="$1" loading="lazy">');
            // Links
            html = html.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, '<a href="$2" target="_blank">$1</a>');
            // Headers
            html = html.replace(/^#### (.+)$/gm, '<h4>$1</h4>');
            html = html.replace(/^### (.+)$/gm, '<h3>$1</h3>');
            html = html.replace(/^## (.+)$/gm, '<h2>$1</h2>');
            html = html.replace(/^# (.+)$/gm, '<h1>$1</h1>');
            // Blockquotes
            html = html.replace(/^&gt; ?(.+)$/gm, '<blockquote><p>$1</p></blockquote>');
            html = html.replace(/<\/blockquote>\n<blockquote>/g, '\n');
            // Horizontal rules
            html = html.replace(/^(---|\*\*\*|___)$/gm, '<hr>');
            // Task lists (before regular lists)
            html = html.replace(/^- \[x\] (.+)$/gm, '<li class="task-done">✔ $1</li>');
            html = html.replace(/^- \[ \] (.+)$/gm, '<li class="task-pending">○ $1</li>');
            // Unordered lists
            html = html.replace(/^[\-\*] (.+)$/gm, '<li>$1</li>');
            // Ordered lists
            html = html.replace(/^\d+\. (.+)$/gm, '<li>$1</li>');
            // Wrap consecutive <li> in <ul>
            html = html.replace(/((?:<li[^>]*>.*<\/li>\n?)+)/g, '<ul>$1</ul>');
            // Paragraphs
            html = html.replace(/\n\n+/g, '</p><p>');
            html = '<p>' + html + '</p>';
            html = html.replace(/<p><\/p>/g, '');
            // Restore protected blocks
            html = html.replace(/B(\d+)E/g, (_, i) => blocks[parseInt(i)] || '');
            // Clean up empty tags
            html = html.replace(/<p><(h[1-4]|hr|ul|ol|table|blockquote|pre)/g, '<$1');
            html = html.replace(/(<\/[^>]+>)<\/p>/g, '$1');
            return html;
        }

        // === File handling ===
        window.addFileContext = function(name, preview) {
            App.pendingFiles.push({ name, preview });
            renderFileChips();
        };
        function removeFile(idx) {
            App.pendingFiles.splice(idx, 1);
            renderFileChips();
            post('removeFile', { index: idx });
        }
        window.showToast = function(msg) {
            const t = document.createElement('div');
            t.className = 'toast'; t.textContent = msg;
            document.body.appendChild(t);
            setTimeout(() => { t.style.opacity = '0'; setTimeout(() => t.remove(), 300); }, 2000);
        };
        function renderFileChips() {
            const area = document.getElementById('fileChips');
            if (!App.pendingFiles.length) { area.style.display = 'none'; area.innerHTML = ''; return; }
            area.style.display = 'flex';
            area.innerHTML = App.pendingFiles.map((f, i) => `
                <span class="file-chip">
                    <span class="file-chip-icon">📄</span>
                    <span class="file-chip-name" title="${esc(f.name)}">${esc(f.name.length > 18 ? f.name.substring(0,15) + '...' : f.name)}</span>
                    <button class="file-chip-remove" onclick="removeFile(${i})">✕</button>
                </span>
            `).join('');
        }

        // === Actions ===
        function sendMsg() {
            if (_sending) {
                // Stop: abort current AI generation
                post('abort', {});
                setSendState(false);
                setLoading(false);
                return;
            }
            const input = document.getElementById('msgInput');
            const text = input.value.trim();
            if (!text && !App.pendingFiles.length) return;
            const finalText = text || '请帮我看看这个文件';

            // Auto-title: use first message as title
            const s = App.sessions.find(x => x.id === App.activeSessionId);
            if (s && !s.title_set) { s.title = finalText.substring(0, 20); s.title_set = true; renderNav(); }
            const fileNote = App.pendingFiles.length ? ' [已附加 ' + App.pendingFiles.length + ' 个文件]' : '';
            App.messages.push({ id: 't_' + Date.now(), type: 'user', content: finalText + fileNote });
            App.pendingFiles = [];
            renderFileChips();
            saveCurrentMessages();
            renderMessages();
            input.value = '';
            input.dispatchEvent(new Event('input'));
            post('sendMessage', { sessionId: App.activeSessionId, content: finalText });
        }

        function post(action, data) {
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage(JSON.stringify({ action, data, timestamp: Date.now() }));
            } else {
                console.log('[→]', action, data);
            }
        }

        // === External API (called by C#) ===
        window.updateAppData = function(data) {
            if (data.user) { App.user = data.user; renderUser(); }
            if (data.messages) { App.messages = data.messages; renderMessages(); }
            if (data.currentSessionId) App.currentSessionId = data.currentSessionId;
        };
        function copyMsg(id) {
            const m = App.messages.find(x => x.id === id);
            if (!m) return;
            const text = (m.thinking ? m.thinking + '\n\n' : '') + (m.content || '');
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard.writeText(text).then(() => {
                    const btn = document.querySelector(`button[onclick="copyMsg('${id}')"]`);
                    if (btn) { btn.textContent = '已复制'; setTimeout(() => btn.textContent = '复制', 1500); }
                }).catch(() => {});
            } else {
                const ta = document.createElement('textarea');
                ta.value = text; ta.style.position = 'fixed'; ta.style.left = '-9999px';
                document.body.appendChild(ta); ta.select();
                document.execCommand('copy'); document.body.removeChild(ta);
                const btn = document.querySelector(`button[onclick="copyMsg('${id}')"]`);
                if (btn) { btn.textContent = '已复制'; setTimeout(() => btn.textContent = '复制', 1500); }
            }
        }
        var _streamEl = null;

        function ensureStreamEl(msg) {
            document.getElementById('loader')?.remove();
            if (_streamEl) return _streamEl;
            const c = document.getElementById('chatContainer');
            // Build the agent message DOM element
            const div = document.createElement('div');
            div.className = 'msg-agent fade-in';
            div.innerHTML = `
                <div class="avatar">${(msg.name||'O')[0]}</div>
                <div class="body">
                    <div class="name">${esc(msg.name||'Open Aries AI')}</div>
                    <div class="thinking-wrap"></div>
                    <div class="tool-wrap"></div>
                    <div class="bubble-wrap"></div>
                    <div class="actions" style="display:none;">
                        <button>复制</button>
                    </div>
                </div>`;
            c.appendChild(div);
            _streamEl = div;
            return div;
        }

        function finishStream() {
            if (!_streamEl) return;
            // Show actions
            const actions = _streamEl.querySelector('.actions');
            if (actions) actions.style.display = 'flex';
            // Wire up buttons
            const lastMsg = App.messages[App.messages.length - 1];
            if (lastMsg) {
                const btns = actions?.querySelectorAll('button');
                if (btns && btns.length >= 1) {
                    btns[0].setAttribute('onclick', `copyMsg('${lastMsg.id}')`);
                    btns[0].textContent = '复制';
                }
            }
            _streamEl = null;
        }

        window.appendMessage = function(msg) { App.messages.push(msg); saveCurrentMessages(); renderMessages(); };
        window.appendStreamMessage = function(msg) {
            const last = App.messages[App.messages.length - 1];
            if (last && last.type === 'agent' && last.streaming) {
                last.content += msg.content;
            } else {
                msg.streaming = true;
                App.messages.push(msg);
            }
            const el = ensureStreamEl(msg);
            // Collapse thinking when actual response starts
            const details = el.querySelector('.thinking-block');
            if (details) details.removeAttribute('open');
            const bw = el.querySelector('.bubble-wrap');
            const cur = App.messages[App.messages.length - 1];
            if (bw && cur) bw.innerHTML = '<div class="bubble">' + md(cur.content) + '</div>';
            saveCurrentMessages();
            const c = document.getElementById('chatContainer');
            c.scrollTop = c.scrollHeight;
        };
        window.appendThinking = function(msgId, text) {
            const last = App.messages[App.messages.length - 1];
            if (last && last.type === 'agent' && last.streaming) {
                if (!last.thinking) last.thinking = '';
                last.thinking += text;
            } else {
                App.messages.push({ id: msgId, type: 'agent', name: 'Open Aries AI', content: '', thinking: text, streaming: true, showActions: true });
            }
            const el = ensureStreamEl({name:'Open Aries AI'});
            let tw = el.querySelector('.thinking-wrap');
            if (tw) {
                if (!tw.querySelector('details')) {
                    tw.innerHTML = `<details class="thinking-block" open><summary>思考过程</summary><div class="thinking-content"></div></details>`;
                }
                const tc = tw.querySelector('.thinking-content');
                const cur = App.messages[App.messages.length - 1];
                if (tc && cur && cur.thinking) tc.textContent = cur.thinking;
            }
            saveCurrentMessages();
            const c = document.getElementById('chatContainer');
            c.scrollTop = c.scrollHeight;
        };
        window.showToolCalling = function(toolName) {
            const last = App.messages[App.messages.length - 1];
            if (!last || last.type !== 'agent' || !last.streaming) {
                App.messages.push({ id: 'a', type: 'agent', name: 'Open Aries AI', content: '', thinking: '', toolResults: [], streaming: true, showActions: true });
            }
            const msg = App.messages[App.messages.length - 1];
            if (!msg.toolResults) msg.toolResults = [];
            // Avoid duplicate if same tool already shown
            const lastTool = msg.toolResults[msg.toolResults.length - 1];
            if (!lastTool || lastTool.name !== toolName || lastTool.result !== '⏳') {
                msg.toolResults.push({ name: toolName, result: '⏳' });
            }
            const el = ensureStreamEl({name:'Open Aries AI'});
            let tw = el.querySelector('.tool-wrap');
            if (tw) {
                const blocks = msg.toolResults.map(t =>
                    t.result === '⏳'
                        ? `<details class="tool-block" open><summary>⏳ ${esc(t.name)} 执行中...</summary><div class="tool-content" style="color:#f0c040;"><span class="tool-args"></span></div></details>`
                        : `<details class="tool-block"><summary>🔧 ${esc(t.name)}</summary><div class="tool-content">${esc(t.result)}</div></details>`
                ).join('');
                tw.innerHTML = blocks;
            }
            saveCurrentMessages();
            const c = document.getElementById('chatContainer');
            c.scrollTop = c.scrollHeight;
        };
        window._pendingToolArgs = '';
        window._argsRafId = 0;
        window.appendToolArgs = function(text) {
            window._pendingToolArgs += text;
            if (!window._argsRafId) {
                window._argsRafId = requestAnimationFrame(function() {
                    window._argsRafId = 0;
                    var el = document.querySelector('.tool-block[open] .tool-args');
                    if (el) {
                        el.textContent += window._pendingToolArgs;
                        window._pendingToolArgs = '';
                        var c = document.getElementById('chatContainer');
                        c.scrollTop = c.scrollHeight;
                    }
                });
            }
        };
        window.addToolBlock = function(toolName, result) {
            // Ensure agent message in App.messages
            const last = App.messages[App.messages.length - 1];
            if (!last || last.type !== 'agent' || !last.streaming) {
                App.messages.push({ id: 'a', type: 'agent', name: 'Open Aries AI', content: '', thinking: '', toolResults: [], streaming: true, showActions: true });
            }
            const msg = App.messages[App.messages.length - 1];
            if (!msg.toolResults) msg.toolResults = [];
            // Replace placeholder from showToolCalling if present
            const lastTool = msg.toolResults[msg.toolResults.length - 1];
            if (lastTool && lastTool.name === toolName && lastTool.result === '⏳') {
                lastTool.result = result;
            } else {
                msg.toolResults.push({ name: toolName, result: result });
            }

            // Create/reuse streaming DOM element
            const el = ensureStreamEl({name:'Open Aries AI'});
            let tw = el.querySelector('.tool-wrap');
            if (tw) {
                const blocks = msg.toolResults.map(t =>
                    `<details class="tool-block"><summary>🔧 ${esc(t.name)}</summary><div class="tool-content">${esc(t.result)}</div></details>`
                ).join('');
                tw.innerHTML = blocks;
            }
            saveCurrentMessages();
            const c = document.getElementById('chatContainer');
            c.scrollTop = c.scrollHeight;
        };
        window.addImageBlock = function(name, base64) {
            const last = App.messages[App.messages.length - 1];
            if (!last || last.type !== 'agent' || !last.streaming) {
                App.messages.push({ id: 'a', type: 'agent', name: 'Open Aries AI', content: '', thinking: '', toolResults: [], streaming: true, showActions: true });
            }
            const msg = App.messages[App.messages.length - 1];
            if (!msg.toolResults) msg.toolResults = [];
            msg.toolResults.push({ name: name, imageBase64: base64 });
            const el = ensureStreamEl({name:'Open Aries AI'});
            let tw = el.querySelector('.tool-wrap');
            if (tw) {
                const blocks = msg.toolResults.map(t => {
                    if (t.imageBase64) return `<details class="tool-block" open><summary>📸 ${esc(t.name)}</summary><div class="tool-content" style="padding:8px;"><img src="data:image/png;base64,${t.imageBase64}" style="max-width:100%;border-radius:6px;display:block;" /></div></details>`;
                    return `<details class="tool-block"><summary>🔧 ${esc(t.name)}</summary><div class="tool-content">${esc(t.result)}</div></details>`;
                }).join('');
                tw.innerHTML = blocks;
            }
            saveCurrentMessages();
            document.getElementById('chatContainer').scrollTop = document.getElementById('chatContainer').scrollHeight;
        };
        window.updateMessageStatus = function(id, status) {
            const m = App.messages.find(x => x.id === id);
            if (m) { m.status = status; renderMessages(); }
        };
        let _sending = false;
        function setSendState(sending) {
            _sending = sending;
            const btn = document.getElementById('sendBtn');
            const sendIcon = document.getElementById('sendIcon');
            const stopIcon = document.getElementById('stopIcon');
            if (sending) {
                btn.classList.add('btn-stop');
                btn.disabled = false;
                sendIcon.style.display = 'none';
                stopIcon.style.display = '';
            } else {
                btn.classList.remove('btn-stop');
                btn.disabled = !document.getElementById('msgInput').value.trim();
                sendIcon.style.display = '';
                stopIcon.style.display = 'none';
            }
        }
        window.setLoading = function(v) {
            const c = document.getElementById('chatContainer');
            if (v) {
                _streamEl = null;
                setSendState(true);
                c.innerHTML += `<div class="msg-agent fade-in" id="loader"><div class="avatar" style="background:var(--code-bg);"><div style="width:13px;height:13px;border:2px solid var(--code-text);border-top-color:transparent;border-radius:50%;animation:spin .6s linear infinite;"></div></div><div class="body" style="padding-top:6px;"><div class="skeleton" style="width:120px;height:12px;margin-bottom:6px;"></div><div class="skeleton" style="width:180px;height:12px;"></div></div></div>`;
                c.scrollTop = c.scrollHeight;
            } else {
                setSendState(false);
                const last = App.messages[App.messages.length - 1];
                if (last && last.streaming) last.streaming = false;
                finishStream();
                document.getElementById('loader')?.remove();
                saveCurrentMessages();
            }
        };

        window.loadSavedSessions = function(jsonArrayStr, activeId) {
            try {
                var saved = JSON.parse(jsonArrayStr);
                if (!Array.isArray(saved) || saved.length === 0) return;
                for (var i = 0; i < saved.length; i++) {
                    var s = saved[i];
                    var raw = s.messages || [];
                    var msgs = [];
                    var pendingTools = [];  // tool results waiting for an agent message
                    for (var j = 0; j < raw.length; j++) {
                        var m = raw[j];
                        var role = m.role;
                        if (role === 'user') {
                            pendingTools = [];
                            msgs.push({ id: 's_' + s.id + '_' + j, type: 'user', content: m.content });
                        } else if (role === 'assistant') {
                            var last2 = msgs[msgs.length - 1];
                            // If previous msg is an agent placeholder (from tool results), merge content
                            if (last2 && last2.type === 'agent' && last2._placeholder && m.content) {
                                last2.content = m.content;
                                last2._placeholder = false;
                                // Also attach any pending tools
                                if (pendingTools.length > 0) {
                                    if (!last2.toolResults) last2.toolResults = [];
                                    last2.toolResults = last2.toolResults.concat(pendingTools);
                                }
                            } else {
                                var agentMsg = { id: 's_' + s.id + '_' + j, type: 'agent', name: 'Open Aries AI', content: m.content, showActions: true };
                                if (pendingTools.length > 0) {
                                    agentMsg.toolResults = pendingTools.slice();
                                }
                                msgs.push(agentMsg);
                            }
                            pendingTools = [];
                        } else if (role === 'system' && m.content.indexOf('工具执行结果') === 0) {
                            var toolMatch = m.content.match(/^工具执行结果 \(([^)]+)\):\n([\s\S]*)/);
                            var toolName = toolMatch ? toolMatch[1] : '?';
                            var toolResult = toolMatch ? toolMatch[2].replace(/\n\n请继续处理[\s\S]*$/, '') : '';
                            var last3 = msgs[msgs.length - 1];
                            if (last3 && last3.type === 'agent') {
                                if (!last3.toolResults) last3.toolResults = [];
                                last3.toolResults.push({ name: toolName, result: toolResult });
                            } else {
                                // No agent yet — create placeholder
                                pendingTools.push({ name: toolName, result: toolResult });
                                var ph = { id: 's_' + s.id + '_' + j, type: 'agent', name: 'Open Aries AI', content: '', showActions: true, _placeholder: true, toolResults: pendingTools.slice() };
                                msgs.push(ph);
                            }
                        }
                    }
                    var existing = App.sessions.find(function(x) { return x.id === s.id; });
                    if (existing) {
                        existing.messages = msgs;
                        existing.title_set = true;
                    } else {
                        var title = '新对话';
                        var fu = msgs.find(function(m) { return m.type === 'user'; });
                        if (fu) title = fu.content.substring(0, 20);
                        App.sessions.unshift({ id: s.id, title: title, messages: msgs, title_set: true });
                    }
                }
                if (activeId) App.activeSessionId = activeId;
                var cur = App.sessions.find(function(x) { return x.id === App.activeSessionId; });
                if (cur) App.messages = cur.messages.slice();
                renderNav();
                renderMessages();
                post('switchSession', { sessionId: App.activeSessionId });
            } catch(e) {}
        };

        // === Init ===
        document.getElementById('msgInput').addEventListener('input', function() {
            this.style.height = 'auto';
            this.style.height = Math.min(this.scrollHeight, 120) + 'px';
            document.getElementById('sendBtn').disabled = !this.value.trim();
        });
        document.getElementById('msgInput').addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMsg(); }
        });
        // === Window resize via edge handles ===
        (function() {
            var handles = document.querySelectorAll('.resize-handle');
            var edge = '', sx = 0, sy = 0, lastPost = 0;
            handles.forEach(function(h) {
                h.addEventListener('mousedown', function(e) {
                    edge = h.id.replace('rh-', '');
                    sx = e.screenX; sy = e.screenY;
                    e.preventDefault();
                    e.stopPropagation();
                    document.body.style.userSelect = 'none';
                    document.body.style.webkitUserSelect = 'none';
                });
            });
            window.addEventListener('mousemove', function(e) {
                if (!edge) return;
                e.preventDefault();
                var dx = e.screenX - sx, dy = e.screenY - sy;
                if (dx === 0 && dy === 0) return;
                var now = Date.now();
                if (now - lastPost < 30) return;
                lastPost = now;
                post('resize', { edge: edge, dx: dx, dy: dy });
                sx = e.screenX; sy = e.screenY;
            });
            window.addEventListener('mouseup', function() {
                edge = '';
                document.body.style.userSelect = '';
                document.body.style.webkitUserSelect = '';
            });
        })();

        document.getElementById('sendBtn').addEventListener('click', sendMsg);
        document.getElementById('attachBtn').addEventListener('click', () => post('attachFile', {}));

        // Titlebar drag
        document.getElementById('titlebar').addEventListener('mousedown', (e) => {
            if (e.target.closest('.titlebar-btn, .agent-toggle')) return;
            post('dragStart', { x: e.screenX, y: e.screenY });
        });

        document.getElementById('btnMin').addEventListener('click', () => post('windowControl', { action: 'minimize' }));
        document.getElementById('btnMax').addEventListener('click', () => post('windowControl', { action: 'maximize' }));
        document.getElementById('btnClose').addEventListener('click', () => post('windowControl', { action: 'close' }));

        window.addEventListener('DOMContentLoaded', () => post('init', { ready: true }));

        // === Init ===
        App.user = { name: '用户' };
        App.sessions = [{ id: 'default', title: '默认对话', messages: [
            { id:'1', type:'agent', name:'Open Aries AI', content:'你好，我是 Open Aries AI。有什么可以帮你的？', showActions: true },
        ]}];
        App.messages = [...App.sessions[0].messages];
        renderUser();
        renderNav();
        renderMessages();
        post('switchSession', { sessionId: 'default' });
    </script>

    <div class="modal-overlay" id="settingsModal">
        <div class="modal-box" style="display:flex;flex-direction:row;gap:24px;width:600px;padding:20px 20px 60px 20px;">
            <!-- Left: vertical tabs -->
            <div class="vtabs" style="display:flex;flex-direction:column;gap:2px;flex-shrink:0;width:80px;padding-top:4px;border-right:1px solid var(--border);margin-right:4px;">
                <div class="vtab active" onclick="switchSettingsTab('api')" id="tabApi">API</div>
                <div class="vtab" onclick="switchSettingsTab('mcp')" id="tabMcp">MCP</div>
                <div class="vtab" onclick="switchSettingsTab('appearance')" id="tabAppearance">外观</div>
                <div class="vtabs-indicator" id="vtabIndicator"></div>
            </div>
            <!-- Right: content area (fixed height) -->
            <div style="flex:1;min-width:0;min-height:340px;">
                <!-- API Tab -->
                <div id="tabContentApi">
                    <h3 style="margin-top:0;">API 设置</h3>
                    <label>API 地址</label>
                    <input type="text" id="cfgHost" placeholder="api.siliconflow.cn/v1">
                    <label>API Key</label>
                    <input type="password" id="cfgKey" placeholder="sk-...">
                    <label>模型</label>
                    <input type="text" id="cfgModel" placeholder="Pro/zai-org/GLM-5">
                    <div style="margin-top:12px;">
                        <button class="btn-primary" id="btnTestConn" onclick="testConnection()" style="width:100%;">测试连接</button>
                        <span id="testResult" style="font-size:12px;margin-left:10px;"></span>
                        <div style="font-size:11px;color:var(--text-secondary);margin-top:4px;">注：视觉能力检测通过发送 1×1 像素 PNG 探测，每次约消耗 100~150 tokens。</div>
                    </div>
                </div>
                <!-- MCP Tab -->
                <div id="tabContentMcp" style="display:none;">
                    <h3 style="margin-top:0;">MCP 服务器</h3>
                    <div class="mcp-header">
                        <span>标识</span><span>命令 (command)</span><span>参数 (可选)</span>
                    </div>
                    <div id="mcpServerList" style="min-height:180px;"></div>
                    <button class="btn-sm" onclick="addMcpRow()" style="margin-top:6px;width:100%;">+ 添加 MCP 服务器</button>
                    <div style="font-size:11px;color:var(--text-muted);margin-top:8px;">
                        相对路径相对于应用所在目录。修改后需重启应用生效。
                    </div>
                </div>
                <!-- Appearance Tab -->
                <div id="tabContentAppearance" style="display:none;">
                    <h3 style="margin-top:0;">外观</h3>
                    <label>主题配色</label>
                    <div style="display:flex;gap:10px;margin-top:6px;">
                        <div class="theme-chip" data-theme="light" onclick="applyTheme('light')" style="background:#f8f8f7;border:2px solid #d4d2ce;" title="亮色"></div>
                        <div class="theme-chip active" data-theme="dark" onclick="applyTheme('dark')" style="background:#0F141B;border:2px solid #A9C7FF;" title="暗色"></div>
                    </div>
                    <div style="font-size:11px;color:var(--text-muted);margin-top:8px;">
                        切换后即时生效，偏好自动保存。
                    </div>
                </div>
            </div>
            <!-- Bottom actions (absolute) -->
            <div class="modal-actions" style="position:absolute;bottom:20px;right:20px;left:20px;">
                <div></div>
                <div style="display:flex;gap:8px;">
                    <button class="btn-cancel" onclick="closeSettings()">取消</button>
                    <button class="btn-primary" onclick="saveSettings()">保存</button>
                </div>
            </div>
        </div>
    </div>

    <div class="modal-overlay" id="psConfirmModal">
        <div class="modal-box">
            <h3 id="confirmTitle">PowerShell 执行确认</h3>
            <p style="font-size:13px;color:var(--text-secondary);margin-bottom:8px;" id="confirmDesc">AI 请求执行以下 PowerShell 命令：</p>
            <div class="ps-script" id="psScript"></div>
            <div class="modal-actions" style="justify-content:space-between;">
                <div id="rememberRow" style="display:none;align-items:center;gap:6px;font-size:12px;color:var(--text-secondary);">
                    <input type="checkbox" id="rememberCheck" style="width:14px;height:14px;">
                    <label for="rememberCheck">记住此选择</label>
                </div>
                <div style="display:flex;gap:8px;">
                    <button class="btn-cancel" onclick="denyConfirm()">拒绝</button>
                    <button class="btn-primary" onclick="approveConfirm()">允许</button>
                </div>
            </div>
        </div>
    </div>

</body>
</html>
)HTML";
