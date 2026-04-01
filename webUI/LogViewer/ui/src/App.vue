<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watchEffect } from 'vue'
import { featurePackages as rawFeaturePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'
import { createUiLocaleHeaders } from './core/requestLocale.js'

const { isZh, toggleLocale } = useUiLocale()
const locale = computed(() => (isZh.value ? 'zh' : 'en'))

const zh = {
  connecting: '正在连接实时日志服务...',
  synced: '实时日志预览连接正常。',
  failed: '实时日志同步失败，请稍后重试。',
  brandEyebrow: 'MYIOT 实时日志',
  title: '独立后端日志窗口',
  copy: '保持这个页面与操作页面并排打开，可以在修改硬件参数、查看系统包或执行诊断命令时持续观察后端输出。',
  signOut: '退出登录',
  logStream: '日志流',
  realtimePreview: '实时进程预览',
  cachedLines: '行缓存',
  processes: '个进程',
  files: '个文件',
  lines: '行',
  sourceEndpoint: '数据接口',
  currentState: '当前状态',
  online: '在线',
  degraded: '异常',
  operatorNotes: '使用建议',
  layout: '布局建议',
  layoutValue: '把这个窗口放在 JNDM123 或诊断页面旁边。',
  bestUse: '推荐用途',
  bestUseValue: '在执行硬件操作时观察后端输出。',
  statusMessage: '状态消息',
  noLogs: '当前没有可读取的后端日志文件。',
  processHint: '当前区域展示这个进程对应的日志文件。',
  emptyFile: '当前日志文件还没有内容。',
  selectProcess: '请先从左侧选择一个进程查看日志。',
  refreshCadence: '刷新频率',
  currentUser: '当前账号',
  language: 'EN',
  documentTitle: 'MyIoT 实时日志',
  popupMode: '悬浮窗口',
  popupHint: '这个页面已切换为独立悬浮模式，适合边操作边观察日志。',
  refreshNow: '立即刷新',
  openFullPage: '完整页',
  closeWindow: '关闭窗口',
  noLogsHint: '如果这里始终为空，请检查服务是否已经启动，以及日志文件是否已经生成。'
}

const en = {
  connecting: 'Connecting to the realtime log service...',
  synced: 'Realtime log preview is healthy.',
  failed: 'Realtime log synchronization failed. Please retry shortly.',
  brandEyebrow: 'MYIOT Realtime Logs',
  title: 'Dedicated Backend Log Window',
  copy: 'Keep this page open beside operation pages to watch backend logs continuously while making hardware changes, checking packages, or using the process console.',
  signOut: 'Sign Out',
  logStream: 'Log Stream',
  realtimePreview: 'Realtime process preview',
  cachedLines: 'cached lines',
  processes: 'processes',
  files: 'files',
  lines: 'lines',
  sourceEndpoint: 'Source endpoint',
  currentState: 'Current state',
  online: 'Online',
  degraded: 'Degraded',
  operatorNotes: 'Operator Notes',
  layout: 'Layout',
  layoutValue: 'Keep this window beside JNDM123 or diagnostics pages.',
  bestUse: 'Best use',
  bestUseValue: 'Watch backend output while applying hardware actions.',
  statusMessage: 'Status message',
  noLogs: 'No readable backend log files are available right now.',
  processHint: 'The current stage shows the log files discovered for this process.',
  emptyFile: 'This log file is currently empty.',
  selectProcess: 'Select a process on the left to review its log output.',
  refreshCadence: 'Refresh cadence',
  currentUser: 'Current User',
  language: '中文',
  documentTitle: 'MyIoT Realtime Logs',
  popupMode: 'Floating Window',
  popupHint: 'This page is running in a detached popup layout so you can keep logs visible beside operation pages.',
  refreshNow: 'Refresh',
  openFullPage: 'Full Page',
  closeWindow: 'Close Window',
  noLogsHint: 'If this stays empty, check whether the service is running and whether log files have been created.'
}

const text = computed(() => (isZh.value ? zh : en))
const isPopupMode = computed(() => {
  if (typeof window === 'undefined') return false
  return new URLSearchParams(window.location.search).get('popup') === '1'
})

watchEffect(() => {
  if (typeof document !== 'undefined') {
    document.title = isPopupMode.value ? `${text.value.documentTitle} · ${text.value.popupMode}` : text.value.documentTitle
  }
})
const featurePackages = computed(() =>
  rawFeaturePackages.map((featurePackage) => localizeFeaturePackage(featurePackage, locale.value))
)

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const logProcesses = ref([])
const logsMessage = ref(text.value.connecting)
const logsUpdatedAt = ref('')
const logsLoading = ref(false)
const logsError = ref('')
let logTimer = null
let logsRequestInFlight = false

const launchablePackages = computed(() =>
  featurePackages.value.filter((featurePackage) => featurePackage.entryPath)
)

const totalLogLines = computed(() =>
  logProcesses.value.reduce((total, process) => total + (process.lineCount ?? 0), 0)
)

const logFiles = computed(() =>
  logProcesses.value.flatMap((process) =>
    (process.files ?? []).map((file) => ({
      ...file,
      processName: process.name,
      processId: process.id
    }))
  )
)

function scrollLogConsoles() {
  document.querySelectorAll('.log-console').forEach((element) => {
    element.scrollTop = element.scrollHeight
  })
}

function openFullPage() {
  window.location.assign('/myiot/logs/index.html')
}

function closeWindow() {
  window.close()
}

async function loadLogs() {
  if (logsRequestInFlight) return

  logsRequestInFlight = true
  logsLoading.value = true

  try {
    const response = await fetch(`/myiot/home/logs.json?lines=60&_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({ Accept: 'application/json' })
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`logs status ${response.status}`)

    const payload = await response.json()
    logProcesses.value = payload.processes ?? []
    logsMessage.value = payload.message ?? text.value.synced
    logsUpdatedAt.value = payload.updatedAt ?? ''
    logsError.value = ''
    banner.value = {
      type: 'success',
      text: payload.message ?? text.value.synced
    }
    await nextTick()
    scrollLogConsoles()
  } catch (error) {
    logsError.value = text.value.failed
    banner.value = {
      type: 'error',
      text: error.message || logsError.value
    }
  } finally {
    logsLoading.value = false
    logsRequestInFlight = false
  }
}

async function handleSignOut() {
  await signOut()
  window.location.replace('/myiot/login/index.html')
}

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  banner.value = {
    type: 'success',
    text: text.value.synced
  }

  await loadLogs()
  logTimer = window.setInterval(() => {
    loadLogs()
  }, 1000)
})

onBeforeUnmount(() => {
  if (logTimer) {
    window.clearInterval(logTimer)
    logTimer = null
  }
})
</script>

<template>
  <v-app>
    <div class="shell-scene">
      <div class="ambient-grid" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-a" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-b" aria-hidden="true"></div>

      <v-container fluid class="shell-container">
        <header class="shell-header" :class="{ 'shell-header-popup': isPopupMode }">
          <div class="brand-block">
            <div class="brand-mark">
              <span></span>
              <span></span>
              <span></span>
            </div>

            <div>
              <p class="eyebrow">{{ text.brandEyebrow }}</p>
              <h1>{{ text.title }}</h1>
              <p class="brand-copy">{{ isPopupMode ? text.popupHint : text.copy }}</p>
            </div>
          </div>

          <div class="header-pills">
            <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
              {{ text.language }}
            </v-btn>
            <a
              v-if="!isPopupMode"
              v-for="featurePackage in launchablePackages"
              :key="featurePackage.id"
              :href="featurePackage.entryPath"
              class="meta-pill meta-pill-link"
              :class="{ 'meta-pill-active': featurePackage.id === 'myiot.log-viewer' }"
            >
              <v-icon :icon="featurePackage.icon" size="18"></v-icon>
              <span>{{ featurePackage.name }}</span>
            </a>
            <div v-if="isPopupMode" class="meta-pill meta-pill-active">
              <v-icon icon="mdi-open-in-new" size="18"></v-icon>
              <span>{{ text.popupMode }}</span>
            </div>
            <div class="meta-pill">
              <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
              <span>{{ sessionState.username }}</span>
            </div>
            <v-btn variant="outlined" color="info" size="small" @click="loadLogs">
              {{ text.refreshNow }}
            </v-btn>
            <v-btn v-if="isPopupMode" variant="outlined" color="primary" size="small" @click="openFullPage">
              {{ text.openFullPage }}
            </v-btn>
            <v-btn v-if="isPopupMode" variant="tonal" color="secondary" size="small" @click="closeWindow">
              {{ text.closeWindow }}
            </v-btn>
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

        <div class="layout-grid" :class="{ 'layout-grid-popup': isPopupMode }">
          <section class="feature-panel log-window-panel">
            <div class="feature-frame"></div>

            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">{{ text.logStream }}</p>
                <h2>{{ text.realtimePreview }}</h2>
                <p class="panel-copy">{{ isPopupMode ? text.popupHint : text.copy }}</p>
              </div>
              <div class="header-pills log-toolbar-pills">
                <div class="meta-pill">
                  <v-icon icon="mdi-text-box-search-outline" size="18"></v-icon>
                  <span>{{ totalLogLines }} {{ text.cachedLines }}</span>
                </div>
                <div class="meta-pill">
                  <v-icon icon="mdi-file-document-multiple-outline" size="18"></v-icon>
                  <span>{{ logFiles.length }} {{ text.files }}</span>
                </div>
                <div class="meta-pill">
                  <v-icon icon="mdi-heart-pulse" size="18"></v-icon>
                  <span>{{ logsError ? text.degraded : text.online }}</span>
                </div>
                <div class="meta-pill" v-if="logsUpdatedAt">
                  <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
                  <span>{{ logsUpdatedAt }}</span>
                </div>
              </div>
            </div>

            <div class="log-window-status">
              <span>{{ text.sourceEndpoint }}: <code>/myiot/home/logs.json</code></span>
              <span>{{ logsError || logsMessage }}</span>
            </div>

            <v-alert
              :type="logsError ? 'error' : banner.type"
              variant="tonal"
              border="start"
            >
              {{ logsError || banner.text }}
            </v-alert>

            <div v-if="logFiles.length" class="log-board">
              <article
                v-for="file in logFiles"
                :key="file.path"
                class="log-file-card"
              >
                <div class="log-file-head">
                  <div>
                    <div class="log-file-title">
                      <strong>{{ file.processName }}</strong>
                      <span>{{ file.name }}</span>
                    </div>
                    <p>{{ file.path }}</p>
                  </div>
                  <div class="file-chip-group">
                    <v-chip size="small" variant="tonal" color="info">
                      {{ file.stream }}
                    </v-chip>
                    <v-chip size="small" variant="tonal" color="secondary">
                      {{ file.lines?.length ?? 0 }} {{ text.lines }}
                    </v-chip>
                  </div>
                </div>

                <div class="log-console" :class="{ 'log-console-popup': isPopupMode }">
                  <div
                    v-for="(line, index) in file.lines"
                    :key="`${file.path}-${index}`"
                    class="log-line"
                  >
                    <code>{{ line }}</code>
                  </div>

                  <div v-if="!file.lines?.length" class="log-empty">
                    {{ text.emptyFile }}
                  </div>
                </div>
              </article>
            </div>

            <div v-else-if="!logsLoading" class="log-empty-state">
              <strong>{{ text.noLogs }}</strong>
              <span>{{ text.noLogsHint }}</span>
            </div>
          </section>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.layout-grid {
  display: grid;
  grid-template-columns: 1fr;
  gap: 16px;
  margin-top: 20px;
}

.layout-grid-popup {
  grid-template-columns: 1fr;
}

.log-board {
  display: grid;
  gap: 12px;
}

.feature-panel {
  position: relative;
  display: grid;
  gap: 12px;
  padding: 18px;
  border-radius: 18px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background: rgba(10, 23, 41, 0.9);
  box-shadow: 0 14px 34px rgba(0, 0, 0, 0.2);
  overflow: hidden;
}

.feature-frame {
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid rgba(112, 240, 193, 0.08);
  pointer-events: none;
}

.shell-header-popup {
  align-items: flex-start;
}

.log-window-panel {
  max-width: 1180px;
}

.panel-copy {
  margin-top: 6px;
  color: rgba(210, 232, 255, 0.62);
}

.meta-pill-link {
  color: #d8ecff;
  text-decoration: none;
}

.meta-pill-active {
  border-color: rgba(112, 240, 193, 0.34);
  color: #70f0c1;
}

.log-toolbar-pills,
.file-chip-group {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 12px;
}

.log-window-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 12px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 12px;
  background: rgba(5, 16, 29, 0.72);
  color: rgba(210, 232, 255, 0.68);
  font-size: 0.84rem;
}

.log-file-card {
  padding: 14px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 14px;
  background: rgba(5, 16, 29, 0.72);
}

.log-file-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 16px;
  margin-bottom: 14px;
}

.log-file-title {
  display: flex;
  align-items: baseline;
  flex-wrap: wrap;
  gap: 8px;
}

.log-file-title span {
  color: rgba(210, 232, 255, 0.72);
  font-size: 0.82rem;
}

.log-file-head p {
  margin: 4px 0 0;
  color: rgba(210, 232, 255, 0.56);
  word-break: break-all;
  font-size: 0.8rem;
}

.log-console {
  display: grid;
  gap: 6px;
  max-height: 260px;
  overflow: auto;
  padding: 12px;
  border-radius: 12px;
  background: rgba(1, 9, 17, 0.88);
  border: 1px solid rgba(78, 188, 255, 0.1);
}

.log-console-popup {
  max-height: 46vh;
}

.log-line {
  display: block;
  font-size: 0.84rem;
}

.log-line code {
  color: #d8ecff;
  white-space: pre-wrap;
  word-break: break-word;
  font-family: Consolas, "Courier New", monospace;
}

.log-empty,
.log-empty-state {
  color: rgba(210, 232, 255, 0.62);
}

.log-empty-state {
  display: grid;
  gap: 8px;
  padding: 24px;
  border: 1px dashed rgba(78, 188, 255, 0.18);
  border-radius: 14px;
  background: rgba(5, 16, 29, 0.42);
}

@media (max-width: 980px) {
  .log-window-status {
    flex-direction: column;
    align-items: flex-start;
  }
}

@media (max-width: 780px) {
  .log-file-head,
  .file-chip-group {
    flex-direction: column;
    align-items: stretch;
  }
}
</style>
