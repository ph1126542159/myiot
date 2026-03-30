<script setup>
import { computed, nextTick, onMounted, ref } from 'vue'
import { featurePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const { isZh } = useUiLocale()

const zh = {
  connecting: '正在连接终端服务...',
  busy: '命令执行中...',
  waiting: '等待输入命令',
  terminalReady: (username) => `欢迎回来，${username}。终端页面已经就绪。`,
  terminalHealthy: '终端连接正常。',
  requestFailed: '控制台请求失败',
  requestFailedLines: ['当前命令没有执行成功，请检查服务状态后重试。'],
  requestFailedBanner: '终端服务请求失败，请稍后重试。',
  title: '终端交互页面',
  copy: '这里是独立打开的进程终端页面。它直接连接当前应用的诊断服务，适合持续输入命令、查看返回结果和做运行时排查。',
  signOut: '退出登录',
  terminalStatus: '终端状态',
  currentWindow: '当前进程交互窗口',
  prompt: '终端提示符',
  connection: '连接状态',
  online: '在线',
  recentStatus: '最近状态',
  examples: {
    help: '帮助',
    summary: '概要',
    threads: '线程',
    modules: '模块',
    stack: '堆栈',
    functions: '函数',
    clear: '清屏'
  },
  hint: '支持方向键浏览历史命令，按 Tab 快速补全，输入 clear 可清屏。',
  inputLabel: '输入命令',
  inputPlaceholder: '例如：help、summary、threads 10',
  execute: '执行',
  commandHelp: '命令说明',
  accessInfo: '访问说明',
  pagePath: '页面地址',
  serviceEndpoint: '服务接口',
  currentUser: '当前账号',
  introLines: ['当前页面已经连接到进程诊断终端。你可以直接输入命令，或点击下方快捷指令开始。'],
  commandDescriptions: {
    help: '查看当前终端可用命令。',
    summary: '查看当前进程摘要信息。',
    threads: '查看线程列表，适合做线程排查。',
    modules: '查看进程已加载模块。',
    stack: '查看当前请求线程调用栈。',
    functions: '快速查看当前调用栈函数名。'
  }
}

const en = {
  connecting: 'Connecting to the process console service...',
  busy: 'Command is running...',
  waiting: 'Waiting for a command',
  terminalReady: (username) => `Welcome back, ${username}. The process console is ready.`,
  terminalHealthy: 'Terminal connection is healthy.',
  requestFailed: 'Console request failed',
  requestFailedLines: ['The command did not complete successfully. Please check the service status and try again.'],
  requestFailedBanner: 'The process console request failed. Please retry later.',
  title: 'Process Console',
  copy: 'This is the dedicated process console page. It talks directly to the in-process diagnostic service and is useful for interactive commands, immediate responses, and runtime inspection.',
  signOut: 'Sign Out',
  terminalStatus: 'Terminal Status',
  currentWindow: 'Current process interaction window',
  prompt: 'Prompt',
  connection: 'Connection',
  online: 'Online',
  recentStatus: 'Recent status',
  examples: {
    help: 'Help',
    summary: 'Summary',
    threads: 'Threads',
    modules: 'Modules',
    stack: 'Stack',
    functions: 'Functions',
    clear: 'Clear'
  },
  hint: 'Use the arrow keys to browse history, press Tab for quick completion, and enter clear to reset the transcript.',
  inputLabel: 'Enter command',
  inputPlaceholder: 'Examples: help, summary, threads 10',
  execute: 'Run',
  commandHelp: 'Command Help',
  accessInfo: 'Access Info',
  pagePath: 'Page Path',
  serviceEndpoint: 'Service Endpoint',
  currentUser: 'Current User',
  introLines: ['This page is connected to the process diagnostics console. Enter commands directly or start with one of the shortcuts below.'],
  commandDescriptions: {
    help: 'Show the commands available in the console.',
    summary: 'Display a summary of the current process.',
    threads: 'Inspect the thread list for debugging.',
    modules: 'Show loaded modules for the process.',
    stack: 'Capture the current request-thread stack.',
    functions: 'Quickly list function names from the current stack.'
  }
}

const text = computed(() => (isZh.value ? zh : en))

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const consoleCommand = ref('')
const consoleBusy = ref(false)
const consoleOutput = ref([])
const consoleLastUpdatedAt = ref('')
const consoleInput = ref(null)
const consoleHistory = ref([])
const consoleHistoryIndex = ref(-1)

const consolePresetCommands = computed(() => [
  { label: text.value.examples.help, command: 'help' },
  { label: text.value.examples.summary, command: 'summary' },
  { label: text.value.examples.threads, command: 'threads 10' },
  { label: text.value.examples.modules, command: 'modules 12' },
  { label: text.value.examples.stack, command: 'stack 12' },
  { label: text.value.examples.functions, command: 'functions 12' },
  { label: text.value.examples.clear, command: 'clear' }
])

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : 'unknown')
)

const statusText = computed(() => {
  if (consoleBusy.value) return text.value.busy
  if (consoleLastUpdatedAt.value) return consoleLastUpdatedAt.value
  return text.value.waiting
})

const launchablePackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.entryPath)
)

const normalizedConsoleCommand = computed(() => consoleCommand.value.trim().toLowerCase())

const consoleSuggestions = computed(() => {
  if (!normalizedConsoleCommand.value) return consolePresetCommands.value

  return consolePresetCommands.value
    .filter((item) => item.command.toLowerCase().startsWith(normalizedConsoleCommand.value))
    .slice(0, 6)
})

function appendConsoleBlock(block) {
  consoleOutput.value.push({
    id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
    ...block
  })
}

function scrollConsoleTranscript() {
  document.querySelectorAll('.console-transcript').forEach((element) => {
    element.scrollTop = element.scrollHeight
  })
}

function rememberConsoleCommand(command) {
  if (!command) return
  if (consoleHistory.value[consoleHistory.value.length - 1] !== command) {
    consoleHistory.value.push(command)
  }
  consoleHistoryIndex.value = consoleHistory.value.length
}

async function focusConsoleInput() {
  await nextTick()
  const inputElement = consoleInput.value?.$el?.querySelector('input') ?? consoleInput.value?.$el?.querySelector('textarea')
  inputElement?.focus()
}

async function executeConsoleCommand(command, options = {}) {
  const normalizedCommand = command.trim()
  if (!normalizedCommand || consoleBusy.value) return
  const shouldRemember = options.remember !== false

  if (shouldRemember) {
    rememberConsoleCommand(normalizedCommand)
  }

  appendConsoleBlock({
    kind: 'command',
    title: `process-console> ${normalizedCommand}`,
    lines: []
  })

  if (['clear', 'cls'].includes(normalizedCommand.toLowerCase())) {
    consoleOutput.value = []
    consoleLastUpdatedAt.value = ''
    consoleCommand.value = ''
    await nextTick()
    focusConsoleInput()
    return
  }

  consoleBusy.value = true

  try {
    const body = new URLSearchParams()
    body.set('command', normalizedCommand)

    const response = await fetch('/myiot/services/process-console/exec', {
      method: 'POST',
      credentials: 'same-origin',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        Accept: 'application/json'
      },
      body
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`process console status ${response.status}`)

    const payload = await response.json()
    appendConsoleBlock({
      kind: payload.ok ? 'response' : 'error',
      title: payload.message || payload.normalizedCommand || normalizedCommand,
      lines: payload.output ?? []
    })
    consoleLastUpdatedAt.value = payload.updatedAt ?? ''
    banner.value = {
      type: payload.ok ? 'success' : 'warning',
      text: payload.message ?? text.value.terminalHealthy
    }
  } catch {
    appendConsoleBlock({
      kind: 'error',
      title: text.value.requestFailed,
      lines: text.value.requestFailedLines
    })
    banner.value = {
      type: 'error',
      text: text.value.requestFailedBanner
    }
  } finally {
    consoleBusy.value = false
    consoleCommand.value = ''
    await nextTick()
    scrollConsoleTranscript()
    focusConsoleInput()
  }
}

function handleConsoleSubmit() {
  executeConsoleCommand(consoleCommand.value)
}

function applySuggestion(command) {
  consoleCommand.value = command
  focusConsoleInput()
}

function handleConsoleHistory(step) {
  if (!consoleHistory.value.length) return

  const nextIndex = Math.min(
    consoleHistory.value.length,
    Math.max(0, consoleHistoryIndex.value + step)
  )

  consoleHistoryIndex.value = nextIndex
  consoleCommand.value = nextIndex === consoleHistory.value.length
    ? ''
    : consoleHistory.value[nextIndex]
}

function handleConsoleInputKeydown(event) {
  if (event.key === 'ArrowUp') {
    event.preventDefault()
    handleConsoleHistory(-1)
    return
  }

  if (event.key === 'ArrowDown') {
    event.preventDefault()
    handleConsoleHistory(1)
    return
  }

  if (event.key === 'Tab') {
    if (!consoleSuggestions.value.length) return
    event.preventDefault()
    applySuggestion(consoleSuggestions.value[0].command)
  }
}

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  banner.value = {
    type: 'success',
    text: text.value.terminalReady(sessionState.username)
  }

  appendConsoleBlock({
    kind: 'response',
    title: 'MyIoT Process Console',
    lines: text.value.introLines
  })
  await executeConsoleCommand('help', { remember: false })
})

async function handleSignOut() {
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
              <p class="eyebrow">MYIOT Process Console</p>
              <h1>{{ text.title }}</h1>
              <p class="brand-copy">{{ text.copy }}</p>
            </div>
          </div>

          <div class="header-pills">
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

            <div class="console-status-bar mt-4">
              <div class="status-card">
                <span>{{ text.prompt }}</span>
                <strong>process-console&gt;</strong>
              </div>
              <div class="status-card">
                <span>{{ text.connection }}</span>
                <strong>{{ consoleBusy ? text.busy : text.online }}</strong>
              </div>
              <div class="status-card">
                <span>{{ text.recentStatus }}</span>
                <strong>{{ statusText }}</strong>
              </div>
            </div>

            <div class="console-transcript mt-4">
              <div
                v-for="block in consoleOutput"
                :key="block.id"
                class="console-block"
                :class="`console-block-${block.kind}`"
              >
                <div class="console-block-title">{{ block.title }}</div>
                <div v-if="block.lines?.length" class="console-block-lines">
                  <div
                    v-for="(line, index) in block.lines"
                    :key="`${block.id}-${index}`"
                    class="console-line"
                  >
                    <code>{{ line }}</code>
                  </div>
                </div>
              </div>
            </div>

            <div class="console-toolbar mt-4">
              <div class="console-chip-row">
                <button
                  v-for="item in consolePresetCommands"
                  :key="item.command"
                  type="button"
                  class="console-chip"
                  @click="applySuggestion(item.command)"
                >
                  {{ item.label }}
                </button>
              </div>
              <p class="console-hint">{{ text.hint }}</p>
            </div>

            <form class="console-command-bar mt-4" @submit.prevent="handleConsoleSubmit">
              <v-text-field
                ref="consoleInput"
                v-model="consoleCommand"
                :label="text.inputLabel"
                variant="outlined"
                density="comfortable"
                hide-details
                :placeholder="text.inputPlaceholder"
                class="console-command-input"
                :disabled="consoleBusy"
                @keydown="handleConsoleInputKeydown"
              ></v-text-field>
              <v-btn
                type="submit"
                color="primary"
                variant="elevated"
                :loading="consoleBusy"
                :disabled="!consoleCommand.trim()"
              >
                {{ text.execute }}
              </v-btn>
            </form>

            <div v-if="consoleSuggestions.length && consoleCommand.trim()" class="console-suggestion-list mt-4">
              <button
                v-for="item in consoleSuggestions"
                :key="`suggest-${item.command}`"
                type="button"
                class="console-suggestion"
                @click="applySuggestion(item.command)"
              >
                <strong>{{ item.command }}</strong>
                <span>{{ item.label }}</span>
              </button>
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
                  <strong>stack [limit]</strong>
                  <p>{{ text.commandDescriptions.stack }}</p>
                </div>
                <div class="help-card">
                  <strong>functions [limit]</strong>
                  <p>{{ text.commandDescriptions.functions }}</p>
                </div>
              </div>
            </section>

            <section class="feature-panel">
              <div class="feature-frame"></div>

              <p class="section-kicker">{{ text.accessInfo }}</p>
              <div class="status-card-list">
                <div class="status-card">
                  <span>{{ text.pagePath }}</span>
                  <strong>/myiot/console/index.html</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.serviceEndpoint }}</span>
                  <strong>/myiot/services/process-console/exec</strong>
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
.console-command-bar,
.console-chip-row,
.console-suggestion-list,
.console-status-bar {
  display: flex;
  gap: 14px;
}

.shell-header,
.panel-head-inline,
.console-command-bar {
  align-items: center;
  justify-content: space-between;
}

.brand-block {
  align-items: flex-start;
}

.header-pills,
.console-chip-row,
.console-suggestion-list {
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

.console-status-bar {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
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

.console-transcript {
  display: grid;
  gap: 14px;
  min-height: 420px;
  max-height: 60vh;
  overflow: auto;
  padding: 18px;
  border-radius: 22px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background: linear-gradient(180deg, rgba(3, 10, 19, 0.98), rgba(1, 6, 12, 0.98));
}

.console-block {
  display: grid;
  gap: 10px;
  padding: 14px 16px;
  border-radius: 16px;
  border: 1px solid rgba(78, 188, 255, 0.1);
  background: rgba(8, 20, 35, 0.78);
}

.console-block-command {
  border-color: rgba(112, 240, 193, 0.24);
  background: rgba(7, 27, 31, 0.72);
}

.console-block-error {
  border-color: rgba(255, 132, 132, 0.22);
  background: rgba(47, 14, 18, 0.72);
}

.console-block-title {
  color: #70f0c1;
  font-weight: 700;
}

.console-block-lines {
  display: grid;
  gap: 8px;
}

.console-line code,
.console-hint code,
.console-hint kbd {
  font-family: Consolas, "Courier New", monospace;
}

.console-line code {
  color: #d8ecff;
  white-space: pre-wrap;
  word-break: break-word;
}

.console-chip,
.console-suggestion {
  border: 1px solid rgba(78, 188, 255, 0.14);
  background: rgba(8, 20, 35, 0.82);
  color: #d8ecff;
  cursor: pointer;
  transition: border-color 160ms ease, transform 160ms ease, background 160ms ease;
}

.console-chip {
  padding: 8px 14px;
  border-radius: 999px;
}

.console-suggestion {
  display: grid;
  gap: 4px;
  min-width: 150px;
  padding: 10px 14px;
  border-radius: 16px;
  text-align: left;
}

.console-chip:hover,
.console-suggestion:hover,
.meta-pill-link:hover {
  border-color: rgba(112, 240, 193, 0.32);
  transform: translateY(-1px);
}

.console-command-input {
  flex: 1 1 auto;
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
  .console-command-bar,
  .panel-head-inline {
    flex-direction: column;
    align-items: stretch;
  }

  .console-status-bar {
    grid-template-columns: 1fr;
  }

  .console-suggestion-list {
    flex-direction: column;
  }
}
</style>
