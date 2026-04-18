<script setup>
import { computed, onBeforeUnmount, onMounted, ref, watchEffect } from 'vue'
import { Terminal } from 'xterm'
import { FitAddon } from 'xterm-addon-fit'
import 'xterm/css/xterm.css'

import { featurePackages as rawFeaturePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const { isZh, toggleLocale } = useUiLocale()
const locale = computed(() => (isZh.value ? 'zh' : 'en'))

const zh = {
  connecting: '正在连接 PTY 终端服务...',
  terminalReady: (username) => `欢迎回来，${username}。PTY 终端会话已经建立。`,
  terminalDisconnected: 'PTY 终端连接已断开，请刷新页面后重试。',
  brandEyebrow: 'MYIOT PTY 终端控制台',
  title: '终端交互页面',
  copy: '这里直接连接后端 PTY 会话，不再模拟“假终端”。像 vim、less、方向键分页、全屏程序这样的交互都会直接进入 shell。',
  signOut: '退出登录',
  terminalStatus: '终端状态',
  currentWindow: '当前进程交互窗口',
  online: '已连接',
  offline: '未连接',
  commandPalette: '快捷命令',
  hint: '当前是原生 PTY 会话：终端按键会直接发送到后端 shell。若系统环境里没有真实 Linux top，会自动注入兼容版 top 命令。',
  examples: {
    pwd: '当前目录',
    ls: '列目录',
    top: 'top 视图',
    cdParent: '上一级',
    help: '帮助',
    summary: '概要',
    threads: '线程',
    modules: '模块',
    clear: '清屏'
  },
  commandHelp: '命令说明',
  accessInfo: '访问说明',
  workingDirectory: '当前目录',
  shellBackend: 'Shell 后端',
  pagePath: '页面地址',
  serviceEndpoint: '终端接口',
  currentUser: '当前账号',
  unknown: '未知',
  commandDescriptions: {
    help: 'help 会输出内置诊断命令和 shell 使用说明。',
    summary: '内置诊断命令，会输出当前后端进程摘要。',
    threads: '内置诊断命令，会输出线程列表。',
    modules: '内置诊断命令，会输出已加载模块。',
    top: '优先调用系统自带 top；若当前环境没有，则启用兼容版 top。',
    vim: '用于验证 PTY 是否支持全屏交互程序。'
  },
  terminalWindow: 'PTY 终端窗口',
  shellSession: 'shell 会话',
  language: 'EN',
  documentTitle: 'MyIoT PTY 终端交互'
}

const en = {
  connecting: 'Connecting to the PTY terminal service...',
  terminalReady: (username) => `Welcome back, ${username}. The PTY terminal session is ready.`,
  terminalDisconnected: 'The PTY terminal connection was closed. Refresh the page to reconnect.',
  brandEyebrow: 'MYIOT PTY Process Console',
  title: 'Process Console',
  copy: 'This page connects directly to a backend PTY instead of simulating a fake transcript. Interactive apps such as vim, less, arrow-key navigation, and full-screen programs run inside the shell session.',
  signOut: 'Sign Out',
  terminalStatus: 'Terminal Status',
  currentWindow: 'Current process interaction window',
  online: 'Connected',
  offline: 'Offline',
  commandPalette: 'Quick Commands',
  hint: 'This is a native PTY session. Keyboard input is forwarded directly to the backend shell. If the host does not provide a real Linux top binary, a compatibility top command is injected automatically.',
  examples: {
    pwd: 'PWD',
    ls: 'LS',
    top: 'Top',
    cdParent: 'Parent',
    help: 'Help',
    summary: 'Summary',
    threads: 'Threads',
    modules: 'Modules',
    clear: 'Clear'
  },
  commandHelp: 'Command Help',
  accessInfo: 'Access Info',
  workingDirectory: 'Working Directory',
  shellBackend: 'Shell Backend',
  pagePath: 'Page Path',
  serviceEndpoint: 'Terminal Endpoint',
  currentUser: 'Current User',
  unknown: 'unknown',
  commandDescriptions: {
    help: 'help prints built-in diagnostics and shell guidance.',
    summary: 'Built-in diagnostic command that prints a backend process summary.',
    threads: 'Built-in diagnostic command that prints thread information.',
    modules: 'Built-in diagnostic command that prints loaded modules.',
    top: 'Uses the system top command when available, otherwise falls back to a compatibility top command.',
    vim: 'Use this to verify full-screen PTY support.'
  },
  terminalWindow: 'PTY Terminal Window',
  shellSession: 'Shell Session',
  language: '中文',
  documentTitle: 'MyIoT PTY Terminal'
}

const text = computed(() => (isZh.value ? zh : en))

watchEffect(() => {
  if (typeof document !== 'undefined') {
    document.title = text.value.documentTitle
  }
})

const featurePackages = computed(() =>
  rawFeaturePackages.map((featurePackage) => localizeFeaturePackage(featurePackage, locale.value))
)

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const terminalHost = ref(null)
const currentWorkingDirectory = ref('')
const shellBackendDisplayName = ref('')
const terminalConnected = ref(false)
const terminalStatusLabel = ref(text.value.connecting)

let terminal = null
let fitAddon = null
let terminalSocket = null
let resizeObserver = null
const terminalDecoder = new TextDecoder()

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : text.value.unknown)
)

const launchablePackages = computed(() =>
  featurePackages.value.filter((featurePackage) => featurePackage.entryPath)
)

const consolePresetCommands = computed(() => [
  { label: text.value.examples.pwd, command: 'pwd' },
  { label: text.value.examples.ls, command: 'ls' },
  { label: text.value.examples.top, command: 'top' },
  { label: text.value.examples.cdParent, command: 'cd ..' },
  { label: text.value.examples.help, command: 'help' },
  { label: text.value.examples.summary, command: 'summary' },
  { label: text.value.examples.threads, command: 'threads 10' },
  { label: text.value.examples.modules, command: 'modules 12' },
  { label: text.value.examples.clear, command: 'clear' }
])

const statusText = computed(() => terminalStatusLabel.value || text.value.connecting)

function normalizeDisplayPath(value) {
  return String(value || '').replace(/\\/g, '/')
}

function formatShellLocation(pathValue) {
  const normalizedPath = normalizeDisplayPath(pathValue).trim()
  return normalizedPath || '~/diagnostics'
}

function focusTerminal() {
  terminal?.focus()
}

function sendSocketPayload(payload) {
  if (!terminalSocket || terminalSocket.readyState !== WebSocket.OPEN) return
  terminalSocket.send(JSON.stringify(payload))
}

function syncTerminalSize() {
  if (!terminal || !fitAddon || !terminalSocket || terminalSocket.readyState !== WebSocket.OPEN) return
  fitAddon.fit()
  sendSocketPayload({
    type: 'resize',
    cols: terminal.cols,
    rows: terminal.rows
  })
}

function handleTerminalOutput(chunk) {
  const cleanedChunk = chunk.replace(/\u001b]633;Cwd=([^\u0007]*)\u0007/g, (_, cwd) => {
    currentWorkingDirectory.value = cwd
    return ''
  })

  if (cleanedChunk) {
    terminal?.write(cleanedChunk)
  }
}

function handleControlMessage(payload) {
  if (payload.type === 'ready') {
    terminalConnected.value = true
    currentWorkingDirectory.value = payload.displayWorkingDirectory ?? currentWorkingDirectory.value
    shellBackendDisplayName.value = payload.shellBackendDisplayName ?? shellBackendDisplayName.value
    terminalStatusLabel.value = payload.shellBackendDisplayName || text.value.online
    banner.value = {
      type: 'success',
      text: text.value.terminalReady(sessionState.username)
    }
    syncTerminalSize()
    return
  }

  if (payload.type === 'error') {
    terminalConnected.value = false
    terminalStatusLabel.value = payload.message || text.value.terminalDisconnected
    banner.value = {
      type: 'error',
      text: payload.message || text.value.terminalDisconnected
    }
    if (payload.message) {
      terminal?.writeln(`\r\n${payload.message}\r\n`)
    }
    return
  }

  if (payload.type === 'exit') {
    terminalConnected.value = false
    terminalStatusLabel.value = text.value.terminalDisconnected
    banner.value = {
      type: 'warning',
      text: text.value.terminalDisconnected
    }
  }
}

function connectTerminalSocket() {
  if (terminalSocket) {
    terminalSocket.close()
  }

  const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws'
  terminalSocket = new WebSocket(`${protocol}://${window.location.host}/myiot/services/process-console/pty`)
  terminalSocket.binaryType = 'arraybuffer'

  terminalSocket.addEventListener('open', () => {
    terminalStatusLabel.value = text.value.connecting
    syncTerminalSize()
    focusTerminal()
  })

  terminalSocket.addEventListener('message', (event) => {
    if (typeof event.data === 'string') {
      try {
        handleControlMessage(JSON.parse(event.data))
      } catch {
      }
      return
    }

    if (event.data instanceof ArrayBuffer) {
      handleTerminalOutput(terminalDecoder.decode(new Uint8Array(event.data)))
    }
  })

  terminalSocket.addEventListener('close', () => {
    terminalConnected.value = false
    terminalStatusLabel.value = text.value.terminalDisconnected
    banner.value = {
      type: 'warning',
      text: text.value.terminalDisconnected
    }
  })

  terminalSocket.addEventListener('error', () => {
    terminalConnected.value = false
    terminalStatusLabel.value = text.value.terminalDisconnected
    banner.value = {
      type: 'error',
      text: text.value.terminalDisconnected
    }
  })
}

function initializeTerminal() {
  if (!terminalHost.value || terminal) return

  terminal = new Terminal({
    cursorBlink: true,
    convertEol: false,
    fontFamily: 'Consolas, "Courier New", monospace',
    fontSize: 14,
    lineHeight: 1.35,
    scrollback: 6000,
    theme: {
      background: '#020812',
      foreground: '#d8ecff',
      cursor: '#70f0c1',
      black: '#020812',
      red: '#ff7b72',
      green: '#70f0c1',
      yellow: '#ffd166',
      blue: '#49d3ff',
      magenta: '#8fb7ff',
      cyan: '#8fe8ff',
      white: '#eef7ff',
      brightBlack: '#546173',
      brightRed: '#ff9aab',
      brightGreen: '#91ffd1',
      brightYellow: '#ffe19b',
      brightBlue: '#8fe8ff',
      brightMagenta: '#b7c8ff',
      brightCyan: '#c5fbff',
      brightWhite: '#ffffff'
    }
  })

  fitAddon = new FitAddon()
  terminal.loadAddon(fitAddon)
  terminal.open(terminalHost.value)
  terminal.focus()

  terminal.onData((data) => {
    sendSocketPayload({
      type: 'input',
      data
    })
  })

  resizeObserver = new ResizeObserver(() => {
    syncTerminalSize()
  })
  resizeObserver.observe(terminalHost.value)
}

function executeTerminalCommand(command) {
  if (!command.trim()) return
  sendSocketPayload({
    type: 'command',
    data: command
  })
  focusTerminal()
}

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  initializeTerminal()
  connectTerminalSocket()
})

onBeforeUnmount(() => {
  resizeObserver?.disconnect()
  resizeObserver = null

  if (terminalSocket) {
    terminalSocket.close()
    terminalSocket = null
  }

  terminal?.dispose()
  terminal = null
  fitAddon = null
})

async function handleSignOut() {
  if (terminalSocket) {
    terminalSocket.close()
  }
  await signOut()
  window.location.replace('/myiot/login/index.html')
}
</script>

<template>
  <v-app>
    <div class="shell-scene">
      <div class="ambient-grid" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-a" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-b" aria-hidden="true"></div>

      <v-container fluid class="shell-container">
        <header class="shell-header">
          <div class="brand-block">
            <div class="brand-mark">
              <span></span>
              <span></span>
              <span></span>
            </div>

            <div>
              <p class="eyebrow">{{ text.brandEyebrow }}</p>
              <h1>{{ text.title }}</h1>
              <p class="brand-copy">{{ text.copy }}</p>
            </div>
          </div>

          <div class="header-pills">
            <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
              {{ text.language }}
            </v-btn>
            <a
              v-for="featurePackage in launchablePackages"
              :key="featurePackage.id"
              :href="featurePackage.entryPath"
              class="meta-pill meta-pill-link"
              :class="{ 'meta-pill-active': featurePackage.id === 'myiot.process-console-ui' }"
            >
              <v-icon :icon="featurePackage.icon" size="18"></v-icon>
              <span>{{ featurePackage.name }}</span>
            </a>
            <div class="meta-pill">
              <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
              <span>{{ sessionState.username }}</span>
            </div>
            <v-btn
              variant="tonal"
              color="secondary"
              size="small"
              class="logout-button"
              @click="handleSignOut"
            >
              {{ text.signOut }}
            </v-btn>
          </div>
        </header>

        <div class="layout-grid">
          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">{{ text.terminalStatus }}</p>
                <h2>{{ text.currentWindow }}</h2>
              </div>
              <div class="meta-pill">
                <v-icon icon="mdi-lan-connect" size="18"></v-icon>
                <span>{{ currentDeviceAddress }}</span>
              </div>
            </div>

            <v-alert
              :type="banner.type"
              variant="tonal"
              border="start"
              class="mt-4"
            >
              {{ banner.text }}
            </v-alert>

            <div class="terminal-shell mt-4" @click="focusTerminal">
              <div class="terminal-shell-head">
                <div class="terminal-window-controls" aria-hidden="true">
                  <span class="terminal-control terminal-control-close"></span>
                  <span class="terminal-control terminal-control-minimize"></span>
                  <span class="terminal-control terminal-control-maximize"></span>
                </div>

                <div class="terminal-window-title">
                  <strong>{{ text.terminalWindow }}</strong>
                  <span>{{ text.shellSession }} · {{ shellBackendDisplayName || currentDeviceAddress }}</span>
                </div>

                <div class="terminal-window-state">
                  <span class="terminal-state-pill">
                    {{ terminalConnected ? text.online : text.offline }}
                  </span>
                  <span class="terminal-state-pill">
                    {{ statusText }}
                  </span>
                </div>
              </div>

              <div ref="terminalHost" class="terminal-xterm"></div>
            </div>

            <div class="console-toolbar mt-4">
              <div>
                <p class="section-kicker">{{ text.commandPalette }}</p>
                <div class="console-chip-row">
                  <button
                    v-for="item in consolePresetCommands"
                    :key="item.command"
                    type="button"
                    class="console-chip"
                    @click="executeTerminalCommand(item.command)"
                  >
                    {{ item.label }}
                  </button>
                </div>
              </div>
              <p class="console-hint">{{ text.hint }}</p>
            </div>
          </section>

          <aside class="side-column">
            <section class="feature-panel">
              <div class="feature-frame"></div>

              <p class="section-kicker">{{ text.commandHelp }}</p>
              <div class="command-help-list">
                <div class="help-card">
                  <strong>help</strong>
                  <p>{{ text.commandDescriptions.help }}</p>
                </div>
                <div class="help-card">
                  <strong>summary</strong>
                  <p>{{ text.commandDescriptions.summary }}</p>
                </div>
                <div class="help-card">
                  <strong>threads [limit]</strong>
                  <p>{{ text.commandDescriptions.threads }}</p>
                </div>
                <div class="help-card">
                  <strong>modules [limit]</strong>
                  <p>{{ text.commandDescriptions.modules }}</p>
                </div>
                <div class="help-card">
                  <strong>top</strong>
                  <p>{{ text.commandDescriptions.top }}</p>
                </div>
                <div class="help-card">
                  <strong>vim</strong>
                  <p>{{ text.commandDescriptions.vim }}</p>
                </div>
              </div>
            </section>

            <section class="feature-panel">
              <div class="feature-frame"></div>

              <p class="section-kicker">{{ text.accessInfo }}</p>
              <div class="status-card-list">
                <div class="status-card">
                  <span>{{ text.workingDirectory }}</span>
                  <strong>{{ formatShellLocation(currentWorkingDirectory) }}</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.shellBackend }}</span>
                  <strong>{{ shellBackendDisplayName || text.unknown }}</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.pagePath }}</span>
                  <strong>/myiot/console/index.html</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.serviceEndpoint }}</span>
                  <strong>/myiot/services/process-console/pty</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.currentUser }}</span>
                  <strong>{{ sessionState.username }}</strong>
                </div>
              </div>
            </section>
          </aside>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.shell-scene {
  min-height: 100vh;
  background:
    radial-gradient(circle at top left, rgba(58, 216, 255, 0.08), transparent 22%),
    radial-gradient(circle at bottom right, rgba(112, 240, 193, 0.08), transparent 28%),
    linear-gradient(180deg, #040b15 0%, #071424 50%, #09192d 100%);
  color: #e8f3ff;
}

.ambient-grid,
.ambient-glow {
  position: fixed;
  inset: 0;
  pointer-events: none;
}

.ambient-grid {
  opacity: 0.12;
  background-image:
    linear-gradient(rgba(73, 211, 255, 0.12) 1px, transparent 1px),
    linear-gradient(90deg, rgba(73, 211, 255, 0.12) 1px, transparent 1px);
  background-size: 40px 40px;
}

.ambient-glow-a {
  background: radial-gradient(circle at 18% 20%, rgba(73, 211, 255, 0.12), transparent 26%);
}

.ambient-glow-b {
  background: radial-gradient(circle at 82% 12%, rgba(112, 240, 193, 0.12), transparent 24%);
}

.shell-container {
  position: relative;
  z-index: 1;
  padding: 32px 24px 40px;
}

.shell-header,
.brand-block,
.header-pills,
.panel-head-inline,
.console-chip-row,
.terminal-shell-head,
.terminal-window-controls,
.terminal-window-state {
  display: flex;
  gap: 14px;
}

.shell-header,
.panel-head-inline,
.terminal-shell-head {
  align-items: center;
  justify-content: space-between;
}

.brand-block {
  align-items: flex-start;
}

.header-pills,
.console-chip-row {
  flex-wrap: wrap;
}

.brand-mark {
  display: grid;
  grid-template-columns: repeat(2, 18px);
  gap: 8px;
  padding-top: 8px;
}

.brand-mark span {
  width: 18px;
  height: 18px;
  border-radius: 5px;
  background: linear-gradient(135deg, #49d3ff, #70f0c1);
  box-shadow: 0 0 16px rgba(73, 211, 255, 0.32);
}

.eyebrow,
.section-kicker,
.brand-copy,
.console-hint,
.help-card p,
.status-card span {
  color: rgba(213, 231, 255, 0.68);
}

.eyebrow,
.section-kicker {
  margin: 0 0 10px;
  text-transform: uppercase;
  letter-spacing: 0.18em;
  font-size: 12px;
}

.brand-copy,
.help-card p {
  line-height: 1.8;
}

.layout-grid {
  display: grid;
  grid-template-columns: minmax(0, 1.7fr) minmax(300px, 0.9fr);
  gap: 20px;
  margin-top: 24px;
}

.side-column,
.command-help-list,
.status-card-list {
  display: grid;
  gap: 16px;
}

.feature-panel {
  position: relative;
  display: grid;
  gap: 12px;
  padding: 24px;
  border-radius: 28px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background: rgba(10, 23, 41, 0.9);
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.22);
  overflow: hidden;
}

.feature-frame {
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid rgba(112, 240, 193, 0.08);
  pointer-events: none;
}

.meta-pill,
.meta-pill-link,
.status-card,
.help-card {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 12px 14px;
  border-radius: 16px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background: rgba(8, 18, 34, 0.8);
}

.meta-pill-link {
  color: #d8ecff;
  text-decoration: none;
}

.meta-pill-active {
  border-color: rgba(112, 240, 193, 0.34);
  color: #70f0c1;
}

.status-card,
.help-card {
  display: grid;
  align-items: start;
}

.status-card strong,
.help-card strong {
  color: #eef7ff;
  word-break: break-word;
}

.terminal-shell {
  overflow: hidden;
  border-radius: 24px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background:
    radial-gradient(circle at top left, rgba(73, 211, 255, 0.12), transparent 24%),
    linear-gradient(180deg, rgba(3, 10, 19, 0.98), rgba(1, 6, 12, 0.98));
  box-shadow: inset 0 1px 0 rgba(112, 240, 193, 0.06);
}

.terminal-shell-head {
  padding: 14px 18px;
  border-bottom: 1px solid rgba(78, 188, 255, 0.12);
  background: rgba(6, 16, 30, 0.92);
}

.terminal-window-controls {
  align-items: center;
}

.terminal-control {
  width: 12px;
  height: 12px;
  border-radius: 999px;
  box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.18) inset;
}

.terminal-control-close {
  background: #ff7b72;
}

.terminal-control-minimize {
  background: #ffd166;
}

.terminal-control-maximize {
  background: #70f0c1;
}

.terminal-window-title {
  display: grid;
  gap: 4px;
  min-width: 0;
  flex: 1 1 auto;
}

.terminal-window-title strong {
  color: #eef7ff;
  font-size: 0.96rem;
}

.terminal-window-title span {
  color: rgba(213, 231, 255, 0.58);
  font-size: 0.82rem;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.terminal-window-state {
  flex-wrap: wrap;
  justify-content: flex-end;
}

.terminal-state-pill {
  display: inline-flex;
  align-items: center;
  min-height: 32px;
  padding: 0 12px;
  border-radius: 999px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background: rgba(8, 18, 34, 0.82);
  color: #d8ecff;
  font-size: 0.82rem;
  font-family: Consolas, "Courier New", monospace;
}

.terminal-xterm {
  min-height: 520px;
  max-height: 66vh;
  overflow: hidden;
  padding: 18px 12px 12px;
}

.terminal-xterm :deep(.xterm) {
  height: 100%;
}

.terminal-xterm :deep(.xterm-viewport) {
  border-radius: 14px;
  background: transparent !important;
}

.terminal-xterm :deep(.xterm-screen),
.terminal-xterm :deep(.xterm-viewport),
.terminal-xterm :deep(.xterm-scroll-area) {
  width: 100% !important;
}

.console-chip {
  border: 1px solid rgba(78, 188, 255, 0.14);
  background: rgba(8, 20, 35, 0.82);
  color: #d8ecff;
  cursor: pointer;
  transition: border-color 160ms ease, transform 160ms ease, background 160ms ease;
  padding: 8px 14px;
  border-radius: 999px;
}

.console-chip:hover,
.meta-pill-link:hover {
  border-color: rgba(112, 240, 193, 0.32);
  transform: translateY(-1px);
}

.console-toolbar {
  display: grid;
  gap: 12px;
  padding: 16px 18px;
  border-radius: 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background: rgba(8, 18, 34, 0.74);
}

.console-hint {
  margin: 0;
  line-height: 1.7;
}

.mt-4 {
  margin-top: 16px;
}

@media (max-width: 980px) {
  .layout-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 780px) {
  .shell-header,
  .brand-block,
  .panel-head-inline,
  .terminal-shell-head {
    flex-direction: column;
    align-items: stretch;
  }

  .terminal-window-state {
    justify-content: flex-start;
  }
}
</style>
