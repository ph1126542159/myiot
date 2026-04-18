<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch, watchEffect } from 'vue'
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
  timedOut: '实时日志拉取超时，正在继续重试。',
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
  pollingMode: '前端轮询',
  pollingCadence: '每 1 秒主动拉取',
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
  packageSources: '包来源',
  unifiedTerminal: '主日志终端',
  unifiedHint: '所有包的日志都按主日志顺序汇总到这一个窗口里，标签仅用于标记来源。',
  followingLatest: '跟随最新',
  pausedScroll: '已暂停跟随',
  jumpToLatest: '回到最新',
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
  timedOut: 'Realtime log polling timed out and will keep retrying.',
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
  pollingMode: 'Client Polling',
  pollingCadence: 'Fetch every second',
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
  packageSources: 'Package Sources',
  unifiedTerminal: 'Main Log Terminal',
  unifiedHint: 'All package logs are merged into this single main terminal. Tags only indicate the source package.',
  followingLatest: 'Following Latest',
  pausedScroll: 'Follow Paused',
  jumpToLatest: 'Jump to Latest',
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
const logConsoleElement = ref(null)
const autoFollowTail = ref(true)
let logTimer = null
let logsRequestInFlight = false
let logRequestAbortController = null
let logRequestTimeout = null
const logLineLimit = 500
const logFollowThreshold = 24
const logPollIntervalMs = 1000
const logRequestTimeoutMs = 8000
const logSnapshotEndpoint = '/myiot/home/logs.json'

function trimBundleToken(value) {
  const firstToken = value.split(/[ /,:;]/, 1)[0] || ''
  return firstToken.replace(/\.+$/, '')
}

function extractBundleFromMessage(message, marker) {
  const markerIndex = message.indexOf(marker)
  if (markerIndex === -1) return ''
  return trimBundleToken(message.slice(markerIndex + marker.length))
}

function detectBundleName(source, message) {
  if (source.startsWith('osp.bundle.')) {
    return source.slice('osp.bundle.'.length)
  }

  return (
    extractBundleFromMessage(message, 'registered by bundle ') ||
    extractBundleFromMessage(message, 'mapped by bundle ') ||
    extractBundleFromMessage(message, 'Loaded bundle ') ||
    extractBundleFromMessage(message, 'Bundle ')
  )
}

function parseLogEntry(rawLine, fallbackKey, previousEntry = null) {
  const entry = {
    key: fallbackKey,
    raw: rawLine,
    timestamp: '',
    effectiveTimestamp: previousEntry?.effectiveTimestamp ?? '',
    level: '',
    source: '',
    bundle: '',
    message: rawLine,
  }

  const levelStart = rawLine.indexOf('[')
  const levelEnd = levelStart === -1 ? -1 : rawLine.indexOf(']', levelStart + 1)

  if (levelStart > 0 && levelEnd > levelStart) {
    entry.timestamp = rawLine.slice(0, levelStart).trim()
    entry.level = rawLine.slice(levelStart + 1, levelEnd).trim()

    const remainder = rawLine.slice(levelEnd + 1).trim()
    const separatorIndex = remainder.indexOf(': ')
    if (separatorIndex !== -1) {
      const sourceWithThread = remainder.slice(0, separatorIndex).trim()
      const threadMarkerIndex = sourceWithThread.lastIndexOf('<')
      entry.source =
        threadMarkerIndex !== -1 && sourceWithThread.endsWith('>')
          ? sourceWithThread.slice(0, threadMarkerIndex)
          : sourceWithThread
      entry.message = remainder.slice(separatorIndex + 2)
    }
  }

  if (entry.timestamp) {
    entry.effectiveTimestamp = entry.timestamp
  }

  entry.bundle = detectBundleName(entry.source, entry.message)
  return entry
}

function parseFileEntries(file) {
  const entries = []
  let previousEntry = null

  ;(file.lines ?? []).forEach((line, index) => {
    const entry = parseLogEntry(line, `${file.path}-${index}`, previousEntry)
    entries.push(entry)
    previousEntry = entry
  })

  return entries
}

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
      processId: process.id,
      entries: parseFileEntries(file),
    }))
  )
)

const packageSources = computed(() => {
  const packageCounts = new Map()

  logFiles.value.forEach((file) => {
    file.entries.forEach((entry) => {
      if (!entry.bundle) return
      packageCounts.set(entry.bundle, (packageCounts.get(entry.bundle) ?? 0) + 1)
    })
  })

  return Array.from(packageCounts.entries())
    .map(([name, count]) => ({ name, count }))
    .sort((left, right) => {
      if (right.count !== left.count) return right.count - left.count
      return left.name.localeCompare(right.name)
    })
})

const mergedLogEntries = computed(() => {
  const entries = []

  logFiles.value.forEach((file, fileIndex) => {
    file.entries.forEach((entry, entryIndex) => {
      entries.push({
        ...entry,
        sequence: `${String(fileIndex).padStart(4, '0')}-${String(entryIndex).padStart(4, '0')}`,
        processName: file.processName,
        processId: file.processId,
        fileName: file.name,
        filePath: file.path,
        stream: file.stream ?? 'main',
      })
    })
  })

  return entries.sort((left, right) => {
    const leftTimestamp = left.effectiveTimestamp || ''
    const rightTimestamp = right.effectiveTimestamp || ''

    if (leftTimestamp && rightTimestamp && leftTimestamp !== rightTimestamp) {
      return leftTimestamp.localeCompare(rightTimestamp)
    }

    if (leftTimestamp && !rightTimestamp) return -1
    if (!leftTimestamp && rightTimestamp) return 1
    return left.sequence.localeCompare(right.sequence)
  })
})

function isNearBottom(element) {
  if (!element) return true
  return element.scrollHeight - element.scrollTop - element.clientHeight <= logFollowThreshold
}

function captureLogConsoleState() {
  const element = logConsoleElement.value
  return {
    shouldFollow: isNearBottom(element),
    scrollTop: element?.scrollTop ?? 0,
  }
}

function scheduleFollowToLatest() {
  if (typeof window === 'undefined') return

  const alignToBottom = () => {
    const element = logConsoleElement.value
    if (!element || !autoFollowTail.value) return
    element.scrollTop = element.scrollHeight
    element.lastElementChild?.scrollIntoView({ block: 'end' })
  }

  alignToBottom()
  window.requestAnimationFrame(() => {
    alignToBottom()
    window.setTimeout(alignToBottom, 60)
  })
}

function restoreLogConsoleState(state) {
  const element = logConsoleElement.value
  if (!element) return

  if (state.shouldFollow) {
    autoFollowTail.value = true
    scheduleFollowToLatest()
    return
  }

  const maxScrollTop = Math.max(element.scrollHeight - element.clientHeight, 0)
  element.scrollTop = Math.min(state.scrollTop, maxScrollTop)
  autoFollowTail.value = false
}

function handleLogConsoleScroll() {
  autoFollowTail.value = isNearBottom(logConsoleElement.value)
}

function jumpToLatest() {
  autoFollowTail.value = true
  scheduleFollowToLatest()
}

function openFullPage() {
  window.location.assign('/myiot/logs/index.html')
}

function closeWindow() {
  window.close()
}

function clearLogRequestTimeout() {
  if (!logRequestTimeout || typeof window === 'undefined') return
  window.clearTimeout(logRequestTimeout)
  logRequestTimeout = null
}

function abortPendingLogRequest() {
  clearLogRequestTimeout()

  if (!logRequestAbortController) return
  logRequestAbortController.abort()
  logRequestAbortController = null
}

async function applyLogsPayload(payload) {
  const scrollState = captureLogConsoleState()
  logProcesses.value = payload.processes ?? []
  const hasLogs = logProcesses.value.length > 0
  logsMessage.value = hasLogs ? text.value.synced : text.value.noLogs
  logsUpdatedAt.value = payload.updatedAt ?? ''
  logsError.value = ''
  banner.value = {
    type: hasLogs ? 'success' : 'info',
    text: hasLogs ? text.value.synced : text.value.noLogs
  }
  await nextTick()
  restoreLogConsoleState(scrollState)
  if (scrollState.shouldFollow) {
    scheduleFollowToLatest()
  }
}

async function loadLogs() {
  if (logsRequestInFlight) return

  logsRequestInFlight = true
  logsLoading.value = true

  if (typeof AbortController !== 'undefined') {
    logRequestAbortController = new AbortController()
  }

  if (typeof window !== 'undefined') {
    logRequestTimeout = window.setTimeout(() => {
      logRequestAbortController?.abort()
    }, logRequestTimeoutMs)
  }

  try {
    const response = await fetch(`${logSnapshotEndpoint}?lines=${logLineLimit}&locale=${locale.value}&_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({ Accept: 'application/json' }),
      signal: logRequestAbortController?.signal
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`logs status ${response.status}`)

    const payload = await response.json()
    await applyLogsPayload(payload)
  } catch (error) {
    logsError.value = error?.name === 'AbortError' ? text.value.timedOut : text.value.failed
    banner.value = {
      type: 'error',
      text: error?.name === 'AbortError' ? text.value.timedOut : error.message || logsError.value
    }
  } finally {
    clearLogRequestTimeout()
    logRequestAbortController = null
    logsLoading.value = false
    logsRequestInFlight = false
  }
}

function stopLogPolling() {
  if (!logTimer) return
  window.clearInterval(logTimer)
  logTimer = null
}

function startLogPolling() {
  if (logTimer) return

  void loadLogs()
  logTimer = window.setInterval(() => {
    loadLogs()
  }, logPollIntervalMs)
}

function handleWindowFocus() {
  void loadLogs()
}

function handleVisibilityChange() {
  if (typeof document === 'undefined' || document.hidden) return
  void loadLogs()
}

function handlePageShow() {
  void loadLogs()
}

function handleNetworkReconnect() {
  void loadLogs()
}

async function handleSignOut() {
  stopLogPolling()
  abortPendingLogRequest()
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

  startLogPolling()

  window.addEventListener('focus', handleWindowFocus)
  window.addEventListener('pageshow', handlePageShow)
  window.addEventListener('online', handleNetworkReconnect)
  document.addEventListener('visibilitychange', handleVisibilityChange)
})

onBeforeUnmount(() => {
  stopLogPolling()
  abortPendingLogRequest()

  if (typeof window !== 'undefined') {
    window.removeEventListener('focus', handleWindowFocus)
    window.removeEventListener('pageshow', handlePageShow)
    window.removeEventListener('online', handleNetworkReconnect)
  }

  if (typeof document !== 'undefined') {
    document.removeEventListener('visibilitychange', handleVisibilityChange)
  }
})

watch(locale, () => {
  if (!sessionState.authenticated) return
  void loadLogs()
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
                <div class="meta-pill">
                  <v-icon icon="mdi-refresh-auto" size="18"></v-icon>
                  <span>{{ text.pollingMode }}</span>
                </div>
                <div class="meta-pill">
                  <v-icon :icon="autoFollowTail ? 'mdi-arrow-collapse-down' : 'mdi-hand-back-right-outline'" size="18"></v-icon>
                  <span>{{ autoFollowTail ? text.followingLatest : text.pausedScroll }}</span>
                </div>
                <div class="meta-pill" v-if="logsUpdatedAt">
                  <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
                  <span>{{ logsUpdatedAt }}</span>
                </div>
              </div>
            </div>

            <div class="log-window-status">
              <span>{{ text.sourceEndpoint }}: <code>{{ logSnapshotEndpoint }}</code></span>
              <span>{{ text.pollingCadence }}</span>
              <span>{{ logsError || logsMessage }}</span>
            </div>

            <div v-if="packageSources.length" class="log-source-row">
              <span class="log-source-label">{{ text.packageSources }}</span>
              <div
                v-for="packageSource in packageSources"
                :key="packageSource.name"
                class="package-source-chip"
              >
                <strong>{{ packageSource.name }}</strong>
                <small>{{ packageSource.count }}</small>
              </div>
            </div>

            <v-alert
              :type="logsError ? 'error' : banner.type"
              variant="tonal"
              border="start"
            >
              {{ logsError || banner.text }}
            </v-alert>

            <div class="filter-hint">
              {{ text.unifiedHint }}
            </div>

            <div v-if="!autoFollowTail" class="follow-actions">
              <v-btn variant="outlined" color="primary" size="small" @click="jumpToLatest">
                {{ text.jumpToLatest }}
              </v-btn>
            </div>

            <div v-if="mergedLogEntries.length" class="single-log-shell">
              <div class="single-log-head">
                <strong>{{ text.unifiedTerminal }}</strong>
                <span>{{ mergedLogEntries.length }} {{ text.lines }}</span>
              </div>

              <div
                ref="logConsoleElement"
                class="log-console unified-log-console"
                :class="{ 'log-console-popup': isPopupMode }"
                @scroll="handleLogConsoleScroll"
              >
                <div
                  v-for="entry in mergedLogEntries"
                  :key="entry.key"
                  class="log-line"
                >
                  <div class="log-line-meta">
                    <span v-if="entry.timestamp" class="log-tag">{{ entry.timestamp }}</span>
                    <span v-if="entry.level" class="log-tag log-tag-level">{{ entry.level }}</span>
                    <span v-if="entry.bundle" class="log-tag log-tag-bundle">{{ entry.bundle }}</span>
                    <span v-else-if="entry.source" class="log-tag">{{ entry.source }}</span>
                    <span v-if="entry.stream && entry.stream !== 'main'" class="log-tag">{{ entry.stream }}</span>
                  </div>
                  <code>{{ entry.message || entry.raw }}</code>
                  <p
                    v-if="entry.bundle && entry.source && entry.source !== `osp.bundle.${entry.bundle}`"
                    class="log-source"
                  >
                    {{ entry.source }}
                  </p>
                </div>
              </div>
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
.log-source-row {
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

.log-source-row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 10px;
  padding: 12px 14px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 12px;
  background: rgba(5, 16, 29, 0.58);
}

.log-source-label,
.filter-hint {
  color: rgba(210, 232, 255, 0.7);
}

.filter-hint {
  font-size: 0.84rem;
}

.follow-actions {
  display: flex;
  justify-content: flex-end;
}

.package-source-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 9px 12px;
  border-radius: 999px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background: rgba(8, 22, 40, 0.74);
  color: #d8ecff;
}

.package-source-chip small {
  color: rgba(210, 232, 255, 0.72);
}

.single-log-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 12px 14px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 12px 12px 0 0;
  background: rgba(5, 16, 29, 0.78);
  color: rgba(214, 235, 255, 0.88);
}

.log-console {
  display: grid;
  gap: 6px;
  max-height: 58vh;
  overflow: auto;
  overflow-anchor: none;
  padding: 12px;
  border-radius: 0 0 12px 12px;
  background: rgba(1, 9, 17, 0.88);
  border: 1px solid rgba(78, 188, 255, 0.1);
}

.single-log-shell .log-console {
  border-top: 0;
}

.log-console-popup {
  max-height: 62vh;
}

.log-line {
  display: grid;
  gap: 6px;
  padding: 8px 0;
  border-bottom: 1px solid rgba(78, 188, 255, 0.08);
  font-size: 0.84rem;
}

.log-line:last-child {
  border-bottom: 0;
}

.log-line-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.log-tag {
  display: inline-flex;
  align-items: center;
  min-height: 24px;
  padding: 0 10px;
  border-radius: 999px;
  background: rgba(78, 188, 255, 0.12);
  color: rgba(214, 235, 255, 0.88);
  font-size: 0.76rem;
}

.log-tag-level {
  background: rgba(255, 209, 102, 0.14);
  color: #ffe29b;
}

.log-tag-bundle {
  background: rgba(112, 240, 193, 0.14);
  color: #70f0c1;
}

.log-line code {
  color: #d8ecff;
  white-space: pre-wrap;
  word-break: break-word;
  font-family: Consolas, "Courier New", monospace;
}

.log-source {
  margin: 0;
  color: rgba(210, 232, 255, 0.56);
  font-size: 0.76rem;
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
  .single-log-head {
    flex-direction: column;
    align-items: flex-start;
  }
}
</style>
