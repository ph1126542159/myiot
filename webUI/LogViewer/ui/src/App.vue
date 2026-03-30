<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from 'vue'
import { featurePackages as rawFeaturePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const { isZh } = useUiLocale()
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
  currentUser: '当前账号'
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
  currentUser: 'Current User'
}

const text = computed(() => (isZh.value ? zh : en))
const featurePackages = computed(() =>
  rawFeaturePackages.map((featurePackage) => localizeFeaturePackage(featurePackage, locale.value))
)

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const logProcesses = ref([])
const selectedProcessId = ref('')
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

const selectedProcess = computed(() =>
  logProcesses.value.find((process) => process.id === selectedProcessId.value) ?? logProcesses.value[0] ?? null
)

function scrollLogConsoles() {
  document.querySelectorAll('.log-console').forEach((element) => {
    element.scrollTop = element.scrollHeight
  })
}

async function loadLogs() {
  if (logsRequestInFlight) return

  logsRequestInFlight = true
  logsLoading.value = true

  try {
    const response = await fetch(`/myiot/home/logs.json?lines=60&_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: { Accept: 'application/json' }
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`logs status ${response.status}`)

    const payload = await response.json()
    logProcesses.value = payload.processes ?? []
    if (!selectedProcessId.value || !logProcesses.value.some((process) => process.id === selectedProcessId.value)) {
      selectedProcessId.value = logProcesses.value[0]?.id ?? ''
    }
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
            <a
              v-for="featurePackage in launchablePackages"
              :key="featurePackage.id"
              :href="featurePackage.entryPath"
              class="meta-pill meta-pill-link"
              :class="{ 'meta-pill-active': featurePackage.id === 'myiot.log-viewer' }"
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
                <p class="section-kicker">{{ text.logStream }}</p>
                <h2>{{ text.realtimePreview }}</h2>
              </div>
              <div class="header-pills">
                <div class="meta-pill">
                  <v-icon icon="mdi-text-box-search-outline" size="18"></v-icon>
                  <span>{{ totalLogLines }} {{ text.cachedLines }}</span>
                </div>
                <div class="meta-pill">
                  <v-icon icon="mdi-view-list-outline" size="18"></v-icon>
                  <span>{{ logProcesses.length }} {{ text.processes }}</span>
                </div>
                <div class="meta-pill" v-if="logsUpdatedAt">
                  <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
                  <span>{{ logsUpdatedAt }}</span>
                </div>
              </div>
            </div>

            <v-alert
              :type="logsError ? 'error' : banner.type"
              variant="tonal"
              border="start"
              class="mt-4"
            >
              {{ logsError || banner.text }}
            </v-alert>

            <div class="process-log-shell mt-6">
              <aside class="process-rail">
                <button
                  v-for="process in logProcesses"
                  :key="process.id"
                  type="button"
                  class="process-card"
                  :class="{ 'process-card-active': process.id === selectedProcessId }"
                  @click="selectedProcessId = process.id"
                >
                  <div class="process-card-top">
                    <strong>{{ process.name }}</strong>
                    <v-chip size="x-small" variant="tonal" color="secondary">
                      {{ process.fileCount }} {{ text.files }}
                    </v-chip>
                  </div>
                  <p>{{ process.lineCount }} {{ text.lines }}</p>
                </button>

                <div v-if="!logProcesses.length && !logsLoading" class="log-empty-state">
                  {{ text.noLogs }}
                </div>
              </aside>

              <div class="log-stage">
                <div v-if="selectedProcess" class="log-stage-head">
                  <div>
                    <strong>{{ selectedProcess.name }}</strong>
                    <p>{{ text.processHint }}</p>
                  </div>
                  <v-chip size="small" variant="tonal" color="primary">
                    {{ selectedProcess.lineCount }} {{ text.lines }}
                  </v-chip>
                </div>

                <div v-if="selectedProcess" class="log-board">
                  <div
                    v-for="file in selectedProcess.files"
                    :key="file.path"
                    class="log-file-card"
                  >
                    <div class="log-file-head">
                      <div>
                        <strong>{{ file.name }}</strong>
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

                    <div class="log-console">
                      <div
                        v-for="(line, index) in file.lines"
                        :key="`${file.path}-${index}`"
                        class="log-line"
                      >
                        <span class="log-source">{{ file.stream }}</span>
                        <code>{{ line }}</code>
                      </div>

                      <div v-if="!file.lines?.length" class="log-empty">
                        {{ text.emptyFile }}
                      </div>
                    </div>
                  </div>
                </div>

                <div v-else-if="!logsLoading" class="log-empty-state">
                  {{ text.selectProcess }}
                </div>
              </div>
            </div>
          </section>

          <aside class="side-column">
            <section class="feature-panel">
              <div class="feature-frame"></div>

              <p class="section-kicker">{{ text.logStream }}</p>
              <div class="status-card-list">
                <div class="status-card">
                  <span>{{ text.refreshCadence }}</span>
                  <strong>1 second</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.sourceEndpoint }}</span>
                  <strong>/myiot/home/logs.json</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.currentState }}</span>
                  <strong>{{ logsError ? text.degraded : text.online }}</strong>
                </div>
              </div>
            </section>

            <section class="feature-panel">
              <div class="feature-frame"></div>

              <p class="section-kicker">{{ text.operatorNotes }}</p>
              <div class="status-card-list">
                <div class="status-card">
                  <span>{{ text.layout }}</span>
                  <strong>{{ text.layoutValue }}</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.bestUse }}</span>
                  <strong>{{ text.bestUseValue }}</strong>
                </div>
                <div class="status-card">
                  <span>{{ text.statusMessage }}</span>
                  <strong>{{ logsError || logsMessage }}</strong>
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
.layout-grid {
  display: grid;
  grid-template-columns: minmax(0, 1.7fr) minmax(280px, 0.8fr);
  gap: 20px;
  margin-top: 24px;
}

.side-column,
.status-card-list,
.log-board {
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

.meta-pill-link {
  color: #d8ecff;
  text-decoration: none;
}

.meta-pill-active {
  border-color: rgba(112, 240, 193, 0.34);
  color: #70f0c1;
}

.process-log-shell {
  display: grid;
  grid-template-columns: minmax(220px, 280px) minmax(0, 1fr);
  gap: 18px;
}

.process-rail {
  display: grid;
  gap: 12px;
  align-content: start;
}

.process-card {
  padding: 16px;
  text-align: left;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
  color: #e6f4ff;
  cursor: pointer;
  transition: border-color 160ms ease, transform 160ms ease, box-shadow 160ms ease;
}

.process-card:hover,
.process-card-active {
  border-color: rgba(112, 240, 193, 0.32);
  box-shadow: inset 0 0 0 1px rgba(112, 240, 193, 0.2), 0 0 24px rgba(58, 216, 255, 0.08);
  transform: translateY(-1px);
}

.process-card-top,
.log-stage-head,
.file-chip-group {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.process-card p,
.log-stage-head p,
.status-card span {
  margin: 10px 0 0;
  color: rgba(210, 232, 255, 0.62);
}

.log-stage {
  display: grid;
  gap: 18px;
}

.log-stage-head,
.status-card {
  padding: 16px 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
}

.status-card {
  display: grid;
  gap: 8px;
}

.log-file-card {
  padding: 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
}

.log-file-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 16px;
  margin-bottom: 14px;
}

.log-file-head p {
  margin: 8px 0 0;
  color: rgba(210, 232, 255, 0.56);
  word-break: break-all;
}

.log-console {
  display: grid;
  gap: 10px;
  max-height: 320px;
  overflow: auto;
  padding: 14px;
  border-radius: 16px;
  background: rgba(1, 9, 17, 0.88);
  border: 1px solid rgba(78, 188, 255, 0.1);
}

.log-line {
  display: grid;
  grid-template-columns: 168px minmax(0, 1fr);
  gap: 12px;
  align-items: start;
  font-size: 0.84rem;
}

.log-source {
  color: #70f0c1;
  font-weight: 700;
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

@media (max-width: 980px) {
  .layout-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 780px) {
  .process-log-shell {
    grid-template-columns: 1fr;
  }

  .process-card-top,
  .log-stage-head,
  .log-file-head,
  .file-chip-group {
    flex-direction: column;
    align-items: stretch;
  }

  .log-line {
    grid-template-columns: 1fr;
  }
}
</style>
