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
            --avatar-icon: url(data:image/png;base64,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);


)">
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
            background: var(--avatar-icon) center/cover;
            flex-shrink: 0;
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
                <div class="avatar"></div>
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
