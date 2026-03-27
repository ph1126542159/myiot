<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from 'vue'
import { featurePackages, formatPackageStatus, getPackageStatusTone } from './core/packageRegistry'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const banner = ref({
  type: 'info',
  text: '正在同步会话并准备主页面...'
})
const logProcesses = ref([])
const selectedProcessId = ref('')
const logsMessage = ref('正在同步后端日志...')
const logsUpdatedAt = ref('')
const logsLoading = ref(false)
const logsError = ref('')
let logTimer = null
let logsRequestInFlight = false

const readyPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.status === 'active').length
)

const downstreamPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.id !== 'myiot.home')
)

const bundleListPackage = computed(() =>
  featurePackages.find((featurePackage) => featurePackage.id === 'myiot.bundle-list') ?? null
)

const registryPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.id !== 'myiot.home')
)

const launchablePackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.entryPath)
)

const signalItems = computed(() => [
  { label: '会话链路', value: '稳定', icon: 'mdi-lan-connect', tone: 'primary' },
  { label: '组件同步', value: `${featurePackages.length} 个就绪`, icon: 'mdi-layers-triple-outline', tone: 'secondary' },
  { label: '访问协议', value: (sessionState.accessProtocol || 'http').toUpperCase(), icon: 'mdi-shield-check-outline', tone: 'info' }
])

const timeline = [
  { time: '00.8 ms', event: '登录会话已确认' },
  { time: '01.6 ms', event: '主页面数据已同步' },
  { time: '02.4 ms', event: '功能包索引已刷新' },
  { time: '03.1 ms', event: '控制台视图已就绪' }
]

const totalLogLines = computed(() =>
  logProcesses.value.reduce((total, process) => total + (process.lineCount ?? 0), 0)
)

const selectedProcess = computed(() =>
  logProcesses.value.find((process) => process.id === selectedProcessId.value) ?? logProcesses.value[0] ?? null
)

async function loadLogs() {
  if (logsRequestInFlight) return

  logsRequestInFlight = true
  logsLoading.value = true

  try {
    const response = await fetch(`/myiot/home/logs.json?lines=40&_=${Date.now()}`, {
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
    logsMessage.value = payload.message ?? '后端日志同步正常。'
    logsUpdatedAt.value = payload.updatedAt ?? ''
    logsError.value = ''
    await nextTick()
    document.querySelectorAll('.log-console').forEach((element) => {
      element.scrollTop = element.scrollHeight
    })
  } catch {
    logsError.value = '后端日志同步失败，请稍后重试。'
  } finally {
    logsLoading.value = false
    logsRequestInFlight = false
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
    text: `欢迎回来，${sessionState.username}。主页面已经完成加载。`
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
              <p class="eyebrow">MYIOT Home</p>
              <h1>工业控制台主页面</h1>
              <p class="brand-copy">
                当前这套科技感页面已经独立为 `Home` 包。它负责登录后的主界面展示，
                统一承载功能包导航、平台状态和后续扩展入口。
              </p>
            </div>
          </div>

          <div class="header-pills">
            <div class="meta-pill">
              <v-icon icon="mdi-layers-triple-outline" size="18"></v-icon>
              <span>{{ featurePackages.length }} 个功能包</span>
            </div>
            <div class="meta-pill">
              <v-icon icon="mdi-check-decagram-outline" size="18"></v-icon>
              <span>{{ readyPackages }} 个已启用</span>
            </div>
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
              退出登录
            </v-btn>
          </div>
        </header>

        <div class="shell-grid">
          <aside class="registry-rail">
            <div class="panel-head">
              <div>
                <p class="section-kicker">功能包清单</p>
                <h2>已发现的模块</h2>
              </div>
            </div>

            <div v-if="bundleListPackage" class="rail-action">
              <p class="section-kicker">功能入口</p>
              <a :href="bundleListPackage.entryPath" class="rail-action-button">
                <span class="rail-action-copy">
                  <strong>查看系统包列表</strong>
                  <small>点击进入运行时 bundle 巡检页面</small>
                </span>
                <v-icon :icon="bundleListPackage.icon" size="20"></v-icon>
              </a>
            </div>

            <div class="registry-list">
              <div
                v-for="featurePackage in registryPackages"
                :key="featurePackage.id"
                class="registry-card"
              >
                <div class="registry-card-top">
                  <div class="registry-card-title">
                    <v-icon :icon="featurePackage.icon" size="18"></v-icon>
                    <strong>{{ featurePackage.title }}</strong>
                  </div>
                  <v-chip
                    size="x-small"
                    variant="tonal"
                    :color="getPackageStatusTone(featurePackage.status)"
                  >
                    {{ formatPackageStatus(featurePackage.status) }}
                  </v-chip>
                </div>

                <p class="registry-card-copy">{{ featurePackage.description }}</p>

                <div class="registry-card-meta">
                  <span>{{ featurePackage.category }}</span>
                  <span>v{{ featurePackage.version }}</span>
                </div>

                <a
                  v-if="featurePackage.entryPath"
                  :href="featurePackage.entryPath"
                  class="registry-link"
                >
                  打开入口
                </a>
              </div>
            </div>
          </aside>

          <main class="viewport-panel">
            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">主页面</p>
                <h2>控制台总览</h2>
              </div>

              <div class="nav-pills">
                <a
                  v-for="featurePackage in featurePackages.filter((entry) => entry.entryPath)"
                  :key="featurePackage.id"
                  :href="featurePackage.entryPath"
                  class="nav-pill"
                  :class="{ 'nav-pill-active': featurePackage.id === 'myiot.home' }"
                >
                  <v-icon :icon="featurePackage.icon" size="16"></v-icon>
                  <span>{{ featurePackage.name }}</span>
                </a>
              </div>
            </div>

            <div class="route-panel route-shell">
              <section class="feature-panel">
                <div class="feature-frame"></div>

                <p class="section-kicker">系统概览</p>
                <div class="title-line">
                  <h3>已进入 MyIoT 控制台</h3>
                  <div class="pulse-dot"></div>
                </div>

                <p class="section-copy">
                  登录成功后，默认会进入这个 `Home` 包。页面聚合了当前功能包、
                  平台状态和后续扩展入口，后面再加更多页面包时也可以沿用这个结构。
                </p>

                <v-alert
                  :type="banner.type"
                  variant="tonal"
                  border="start"
                  class="mt-6"
                >
                  {{ banner.text }}
                </v-alert>

                <div class="signal-row">
                  <div
                    v-for="item in signalItems"
                    :key="item.label"
                    class="signal-card"
                  >
                    <v-icon :icon="item.icon" :color="item.tone" size="22"></v-icon>
                    <div>
                      <p>{{ item.label }}</p>
                      <strong>{{ item.value }}</strong>
                    </div>
                  </div>
                </div>

                <div class="d-grid ga-4 mt-6">
                  <div class="signal-row">
                    <div class="signal-card">
                      <v-icon icon="mdi-account-badge-outline" color="secondary" size="22"></v-icon>
                      <div>
                        <p>当前账号</p>
                        <strong>{{ sessionState.username }}</strong>
                      </div>
                    </div>
                    <div class="signal-card">
                      <v-icon icon="mdi-door-open" color="primary" size="22"></v-icon>
                      <div>
                        <p>控制台状态</p>
                        <strong>在线</strong>
                      </div>
                    </div>
                    <div class="signal-card">
                      <v-icon icon="mdi-view-grid-plus-outline" color="info" size="22"></v-icon>
                      <div>
                        <p>可进入功能包</p>
                        <strong>{{ launchablePackages.length }}</strong>
                      </div>
                    </div>
                  </div>

                  <div class="package-stack">
                    <div
                      v-for="featurePackage in featurePackages"
                      :key="featurePackage.id"
                      class="package-tile"
                    >
                      <div class="package-tile-top">
                        <div class="package-tile-title">
                          <v-icon :icon="featurePackage.icon" size="18"></v-icon>
                          <strong>{{ featurePackage.title }}</strong>
                        </div>
                        <v-chip
                          size="x-small"
                          variant="tonal"
                          :color="getPackageStatusTone(featurePackage.status)"
                        >
                          {{ formatPackageStatus(featurePackage.status) }}
                        </v-chip>
                      </div>
                      <p class="package-tile-copy">{{ featurePackage.description }}</p>
                      <div class="package-tile-meta">
                        <span>{{ featurePackage.category }}</span>
                        <span>v{{ featurePackage.version }}</span>
                      </div>
                    </div>
                  </div>
                </div>
              </section>

              <aside class="telemetry-column">
                <section class="feature-panel">
                  <div class="feature-frame"></div>

                  <p class="section-kicker">平台状态</p>
                  <div class="title-line">
                    <h3>实时遥测</h3>
                  </div>
                  <p class="section-copy">
                    右侧展示当前会话、入口注册和功能包数量，用来承接后续更多包的运行状态。
                  </p>

                  <div class="telemetry-ring mt-6">
                    <div class="ring-core">
                      <span>{{ featurePackages.length }}</span>
                      <small>模块已就绪</small>
                    </div>
                  </div>

                  <div class="d-grid ga-3 mt-4">
                    <div class="telemetry-item">
                      <span>登录鉴权</span>
                      <strong>已建立</strong>
                    </div>
                    <div class="telemetry-item">
                      <span>入口注册</span>
                      <strong>已加载</strong>
                    </div>
                    <div class="telemetry-item">
                      <span>预留扩展位</span>
                      <strong>{{ downstreamPackages.length }}</strong>
                    </div>
                  </div>
                </section>

                <section class="feature-panel">
                  <div class="feature-frame"></div>

                  <p class="section-kicker">启动追踪</p>
                  <div class="timeline mt-4">
                    <div v-for="entry in timeline" :key="entry.time" class="timeline-item">
                      <span>{{ entry.time }}</span>
                      <p>{{ entry.event }}</p>
                    </div>
                  </div>
                </section>
              </aside>
            </div>

            <section class="feature-panel">
              <div class="feature-frame"></div>

              <div class="panel-head panel-head-inline">
                <div>
                  <p class="section-kicker">后端日志</p>
                  <h2>实时同步面板</h2>
                </div>

                <div class="header-pills">
                  <div class="meta-pill">
                    <v-icon icon="mdi-text-box-search-outline" size="18"></v-icon>
                    <span>{{ totalLogLines }} 行缓冲</span>
                  </div>
                  <div class="meta-pill">
                    <v-icon icon="mdi-view-list-outline" size="18"></v-icon>
                    <span>{{ logProcesses.length }} 个进程</span>
                  </div>
                  <div class="meta-pill" v-if="logsUpdatedAt">
                    <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
                    <span>{{ logsUpdatedAt }}</span>
                  </div>
                </div>
              </div>

              <p class="section-copy">
                这里实时轮询后端日志接口，展示当前应用目录下最近的服务端日志输出，
                方便在主页面直接观察运行状态。
              </p>

              <v-alert
                :type="logsError ? 'error' : 'info'"
                variant="tonal"
                border="start"
                class="mt-6"
              >
                {{ logsError || logsMessage }}
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
                        {{ process.fileCount }} 文件
                      </v-chip>
                    </div>
                    <p>{{ process.lineCount }} 行缓冲</p>
                  </button>

                  <div v-if="!logProcesses.length && !logsLoading" class="log-empty-state">
                    当前没有可展示的进程日志。
                  </div>
                </aside>

                <div class="log-stage">
                  <div v-if="selectedProcess" class="log-stage-head">
                    <div>
                      <strong>{{ selectedProcess.name }}</strong>
                      <p>当前显示这个进程的日志文件。</p>
                    </div>
                    <v-chip size="small" variant="tonal" color="primary">
                      {{ selectedProcess.lineCount }} 行
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
                            {{ file.lines?.length ?? 0 }} 行
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
                          当前日志文件还没有内容。
                        </div>
                      </div>
                    </div>
                  </div>

                  <div v-else-if="!logsLoading" class="log-empty-state">
                    请选择左侧进程卡片查看日志。
                  </div>
                </div>
              </div>
            </section>
          </main>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.log-board {
  display: grid;
  gap: 18px;
}

.rail-action {
  margin-bottom: 18px;
  padding: 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
}

.rail-action-button {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 14px;
  padding: 16px 18px;
  border-radius: 16px;
  border: 1px solid rgba(112, 240, 193, 0.24);
  background: linear-gradient(135deg, rgba(58, 216, 255, 0.12), rgba(9, 22, 39, 0.82));
  color: #e6f4ff;
  box-shadow: inset 0 0 0 1px rgba(58, 216, 255, 0.08);
}

.rail-action-copy {
  display: grid;
  gap: 6px;
}

.rail-action-copy small {
  color: rgba(210, 232, 255, 0.62);
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
.log-stage-head p {
  margin: 10px 0 0;
  color: rgba(210, 232, 255, 0.62);
}

.log-stage {
  display: grid;
  gap: 18px;
}

.log-stage-head {
  padding: 16px 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
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
  max-height: 280px;
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

@media (max-width: 780px) {
  .process-log-shell {
    grid-template-columns: 1fr;
  }

  .log-line {
    grid-template-columns: 1fr;
  }
}
</style>
