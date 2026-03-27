<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { featurePackages, formatPackageStatus, getPackageStatusTone } from './core/packageRegistry'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const banner = ref({
  type: 'info',
  text: '正在同步系统监控数据...'
})
const latestMetrics = ref(null)
const metricsUpdatedAt = ref('')
const pollIntervalMs = ref(1000)
const loading = ref(false)
const errorText = ref('')
const historyPoints = ref([])
let pollTimer = null
let requestInFlight = false

const MAX_HISTORY_POINTS = 60

function toNumber(value, fallback = 0) {
  const numeric = Number(value)
  return Number.isFinite(numeric) ? numeric : fallback
}

function formatPercent(value) {
  return `${toNumber(value).toFixed(1)}%`
}

function formatTemperature(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '暂不可用'
  return `${numeric.toFixed(1)}°C`
}

function formatCount(value) {
  return new Intl.NumberFormat('zh-CN').format(Math.round(toNumber(value)))
}

function formatBytes(value) {
  const numeric = toNumber(value)
  if (numeric <= 0) return '0 B'

  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  const exponent = Math.min(Math.floor(Math.log(numeric) / Math.log(1024)), units.length - 1)
  const scaled = numeric / (1024 ** exponent)
  return `${scaled.toFixed(scaled >= 100 ? 0 : scaled >= 10 ? 1 : 2)} ${units[exponent]}`
}

function formatRate(value) {
  return `${formatBytes(value)}/s`
}

function sanitizeSample(payload) {
  return {
    cpuPercent: toNumber(payload.cpu?.usagePercent),
    cpuTemperatureCelsius: payload.cpu?.temperatureAvailable ? Number(payload.cpu?.temperatureCelsius) : Number.NaN,
    memoryPercent: toNumber(payload.memory?.usagePercent),
    processCount: toNumber(payload.counts?.processes),
    threadCount: toNumber(payload.counts?.threads),
    networkBytesPerSec: toNumber(payload.network?.totalBytesPerSec),
    ioBytesPerSec: toNumber(payload.io?.totalBytesPerSec),
    timestamp: payload.updatedAt || new Date().toISOString()
  }
}

function chartPolyline(values, maxValue) {
  if (!values.length) return ''

  const height = 64
  const width = 100
  const denominator = Math.max(maxValue, 1)

  return values
    .map((value, index) => {
      const x = values.length === 1 ? width / 2 : (index / (values.length - 1)) * width
      const y = height - (Math.min(value, denominator) / denominator) * height
      return `${x.toFixed(2)},${y.toFixed(2)}`
    })
    .join(' ')
}

function chartArea(values, maxValue) {
  if (!values.length) return ''

  const height = 64
  const denominator = Math.max(maxValue, 1)
  const coordinates = values.map((value, index) => {
    const x = values.length === 1 ? 50 : (index / (values.length - 1)) * 100
    const y = height - (Math.min(value, denominator) / denominator) * height
    return { x, y }
  })
  const firstX = coordinates[0].x
  const lastX = coordinates[coordinates.length - 1].x
  const segments = coordinates.map((point) => `L ${point.x.toFixed(2)} ${point.y.toFixed(2)}`).join(' ')
  return `M ${firstX.toFixed(2)} ${height} ${segments} L ${lastX.toFixed(2)} ${height} Z`
}

function metricMax(values, floor, ratio = 1.15) {
  const maximum = Math.max(...values, floor)
  return maximum * ratio
}

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : '未识别')
)

const diskCards = computed(() =>
  (latestMetrics.value?.disks ?? []).map((disk) => ({
    ...disk,
    title: disk.name,
    usageText: formatPercent(disk.usagePercent),
    totalText: formatBytes(disk.totalBytes),
    usedText: formatBytes(disk.usedBytes),
    freeText: formatBytes(disk.freeBytes)
  }))
)

const overviewCards = computed(() => {
  const metrics = latestMetrics.value
  if (!metrics) return []

  return [
    {
      key: 'cpu',
      title: 'CPU',
      value: formatPercent(metrics.cpu?.usagePercent),
      subtitle: '系统总占用'
    },
    {
      key: 'temperature',
      title: 'CPU 温度',
      value: formatTemperature(metrics.cpu?.temperatureCelsius),
      subtitle: metrics.cpu?.temperatureAvailable ? 'WMI 热区采样' : '当前设备暂未提供温度传感器'
    },
    {
      key: 'memory',
      title: '内存',
      value: formatBytes(metrics.memory?.usedBytes),
      subtitle: `${formatPercent(metrics.memory?.usagePercent)} / 总内存 ${formatBytes(metrics.memory?.totalBytes)}`
    },
    {
      key: 'processes',
      title: '进程',
      value: formatCount(metrics.counts?.processes),
      subtitle: '当前系统进程总数'
    },
    {
      key: 'threads',
      title: '线程',
      value: formatCount(metrics.counts?.threads),
      subtitle: '当前系统线程总数'
    }
  ]
})

const chartSeries = computed(() => {
  const metrics = latestMetrics.value
  if (!metrics) return []

  const samples = historyPoints.value
  const cpuValues = samples.map((entry) => entry.cpuPercent)
  const cpuTemperatureValues = samples
    .map((entry) => entry.cpuTemperatureCelsius)
    .filter((value) => Number.isFinite(value))
  const memoryValues = samples.map((entry) => entry.memoryPercent)
  const processValues = samples.map((entry) => entry.processCount)
  const threadValues = samples.map((entry) => entry.threadCount)
  const networkValues = samples.map((entry) => entry.networkBytesPerSec)
  const ioValues = samples.map((entry) => entry.ioBytesPerSec)

  return [
    {
      key: 'cpu',
      title: 'CPU 曲线',
      accent: 'chart-primary',
      currentValue: formatPercent(metrics.cpu?.usagePercent),
      helper: '总使用率',
      values: cpuValues,
      maxValue: 100
    },
    {
      key: 'memory',
      title: '内存曲线',
      accent: 'chart-secondary',
      currentValue: formatPercent(metrics.memory?.usagePercent),
      helper: `${formatBytes(metrics.memory?.usedBytes)} / ${formatBytes(metrics.memory?.totalBytes)}`,
      values: memoryValues,
      maxValue: 100
    },
    {
      key: 'temperature',
      title: 'CPU 温度',
      accent: 'chart-danger',
      currentValue: formatTemperature(metrics.cpu?.temperatureCelsius),
      helper: metrics.cpu?.temperatureAvailable ? '摄氏度' : '当前设备暂未提供温度',
      values: samples.map((entry) => Number.isFinite(entry.cpuTemperatureCelsius) ? entry.cpuTemperatureCelsius : 0),
      maxValue: metricMax(cpuTemperatureValues, 70, 1.08)
    },
    {
      key: 'processes',
      title: '进程数',
      accent: 'chart-info',
      currentValue: formatCount(metrics.counts?.processes),
      helper: '系统活跃进程总量',
      values: processValues,
      maxValue: metricMax(processValues, 16)
    },
    {
      key: 'threads',
      title: '线程数',
      accent: 'chart-warning',
      currentValue: formatCount(metrics.counts?.threads),
      helper: '系统线程总量',
      values: threadValues,
      maxValue: metricMax(threadValues, 64)
    },
    {
      key: 'network',
      title: '网络吞吐',
      accent: 'chart-primary',
      currentValue: formatRate(metrics.network?.totalBytesPerSec),
      helper: `收 ${formatRate(metrics.network?.receiveBytesPerSec)} / 发 ${formatRate(metrics.network?.sendBytesPerSec)}`,
      values: networkValues,
      maxValue: metricMax(networkValues, 1024 * 64)
    },
    {
      key: 'io',
      title: '磁盘 IO',
      accent: 'chart-secondary',
      currentValue: formatRate(metrics.io?.totalBytesPerSec),
      helper: `读 ${formatRate(metrics.io?.readBytesPerSec)} / 写 ${formatRate(metrics.io?.writeBytesPerSec)}`,
      values: ioValues,
      maxValue: metricMax(ioValues, 1024 * 64)
    }
  ]
})

async function loadMetrics() {
  if (requestInFlight) return

  requestInFlight = true
  loading.value = true

  try {
    const response = await fetch(`/myiot/monitor/metrics.json?_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: { Accept: 'application/json' }
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`metrics status ${response.status}`)

    const payload = await response.json()
    latestMetrics.value = payload
    metricsUpdatedAt.value = payload.updatedAt ?? ''
    pollIntervalMs.value = toNumber(payload.sampleIntervalMs, 1000)

    historyPoints.value = [
      ...historyPoints.value,
      sanitizeSample(payload)
    ].slice(-MAX_HISTORY_POINTS)

    banner.value = {
      type: 'success',
      text: payload.message ?? '系统监控指标同步正常。'
    }
    errorText.value = ''
  } catch {
    errorText.value = '系统监控数据同步失败，请稍后重试。'
    banner.value = {
      type: 'error',
      text: errorText.value
    }
  } finally {
    loading.value = false
    requestInFlight = false
  }
}

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  await loadMetrics()
  pollTimer = window.setInterval(() => {
    loadMetrics()
  }, 1000)
})

onBeforeUnmount(() => {
  if (pollTimer) {
    window.clearInterval(pollTimer)
    pollTimer = null
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
              <p class="eyebrow">MYIOT System Monitor</p>
              <h1>系统资源实时监控</h1>
              <p class="brand-copy">
                这里集中显示当前系统的磁盘、内存、线程、进程、IO、网络和 CPU 变化趋势。
                页面以 1 秒节奏轮询后端指标接口，并在浏览器内维持最近 60 个采样点。
              </p>
            </div>
          </div>

          <div class="header-pills">
            <a href="/myiot/home/index.html" class="meta-pill">
              <v-icon icon="mdi-view-dashboard-outline" size="18"></v-icon>
              <span>返回主页面</span>
            </a>
            <a href="/myiot/packages/index.html" class="meta-pill">
              <v-icon icon="mdi-package-variant-closed" size="18"></v-icon>
              <span>系统包列表</span>
            </a>
            <div class="meta-pill">
              <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
              <span>{{ sessionState.username }}</span>
            </div>
            <div class="meta-pill" v-if="metricsUpdatedAt">
              <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
              <span>{{ metricsUpdatedAt }}</span>
            </div>
            <v-btn variant="tonal" color="secondary" size="small" @click="handleSignOut">
              退出登录
            </v-btn>
          </div>
        </header>

        <main class="viewport-panel">
          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">运行态</p>
                <h2>监控入口概览</h2>
              </div>

              <div class="header-pills">
                <div class="meta-pill">
                  <v-icon icon="mdi-lan-connect" size="18"></v-icon>
                  <span>{{ currentDeviceAddress }}</span>
                </div>
                <div class="meta-pill">
                  <v-icon icon="mdi-timer-sand" size="18"></v-icon>
                  <span>{{ pollIntervalMs }} ms</span>
                </div>
                <div class="meta-pill">
                  <v-icon icon="mdi-chart-line" size="18"></v-icon>
                  <span>{{ historyPoints.length }} 个采样点</span>
                </div>
              </div>
            </div>

            <v-alert :type="errorText ? 'error' : banner.type" variant="tonal" border="start">
              {{ errorText || banner.text }}
            </v-alert>
          </section>

          <section class="stats-grid">
            <article v-for="card in overviewCards" :key="card.key" class="stat-card">
              <p>{{ card.title }}</p>
              <strong>{{ card.value }}</strong>
              <span>{{ card.subtitle }}</span>
            </article>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">曲线视图</p>
                <h2>实时资源走势</h2>
              </div>
            </div>

            <div v-if="chartSeries.length" class="chart-grid">
              <article
                v-for="series in chartSeries"
                :key="series.key"
                class="chart-card"
                :class="series.accent"
              >
                <div class="chart-card-head">
                  <div>
                    <p>{{ series.title }}</p>
                    <strong>{{ series.currentValue }}</strong>
                  </div>
                  <span>{{ series.helper }}</span>
                </div>

                <div class="chart-surface">
                  <svg viewBox="0 0 100 64" preserveAspectRatio="none" class="chart-svg">
                    <defs>
                      <linearGradient :id="`gradient-${series.key}`" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="0%" stop-color="currentColor" stop-opacity="0.42"></stop>
                        <stop offset="100%" stop-color="currentColor" stop-opacity="0.02"></stop>
                      </linearGradient>
                    </defs>
                    <path
                      :d="chartArea(series.values, series.maxValue)"
                      :fill="`url(#gradient-${series.key})`"
                      class="chart-area"
                    ></path>
                    <polyline
                      :points="chartPolyline(series.values, series.maxValue)"
                      class="chart-line"
                    ></polyline>
                  </svg>
                </div>
              </article>
            </div>

            <div v-else-if="!loading" class="empty-state">
              当前还没有可展示的监控曲线。
            </div>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">磁盘视图</p>
                <h2>卷使用情况</h2>
              </div>
            </div>

            <div v-if="diskCards.length" class="disk-grid">
              <article v-for="disk in diskCards" :key="disk.name" class="disk-card">
                <div class="disk-card-head">
                  <div>
                    <strong>{{ disk.title }}</strong>
                    <p>{{ disk.usageText }}</p>
                  </div>
                  <v-chip size="small" variant="tonal" color="secondary">
                    {{ disk.freeText }} 可用
                  </v-chip>
                </div>

                <v-progress-linear
                  :model-value="disk.usagePercent"
                  color="primary"
                  height="12"
                  rounded
                  class="mt-4"
                ></v-progress-linear>

                <div class="disk-meta-row">
                  <span>已用 {{ disk.usedText }}</span>
                  <span>总量 {{ disk.totalText }}</span>
                </div>
              </article>
            </div>

            <div v-else-if="!loading" class="empty-state">
              当前没有可展示的磁盘信息。
            </div>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">WebUI 包</p>
                <h2>当前已注册的页面入口</h2>
              </div>
            </div>

            <div class="package-grid">
              <article
                v-for="featurePackage in featurePackages"
                :key="featurePackage.id"
                class="package-card"
              >
                <div class="package-card-head">
                  <div class="package-title">
                    <v-icon :icon="featurePackage.icon" size="18"></v-icon>
                    <strong>{{ featurePackage.title }}</strong>
                  </div>
                  <v-chip
                    size="small"
                    variant="tonal"
                    :color="getPackageStatusTone(featurePackage.status)"
                  >
                    {{ formatPackageStatus(featurePackage.status) }}
                  </v-chip>
                </div>

                <p class="package-copy">{{ featurePackage.description }}</p>

                <div class="package-meta">
                  <span>{{ featurePackage.category }}</span>
                  <span>v{{ featurePackage.version }}</span>
                  <a :href="featurePackage.entryPath">打开页面</a>
                </div>
              </article>
            </div>
          </section>
        </main>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 18px;
}

.stat-card,
.chart-card,
.disk-card,
.package-card {
  border: 1px solid rgba(78, 188, 255, 0.14);
  border-radius: 22px;
  background: rgba(8, 22, 40, 0.72);
  box-shadow: var(--shell-shadow);
}

.stat-card {
  display: grid;
  gap: 8px;
  padding: 22px;
}

.stat-card p,
.chart-card-head p,
.disk-card-head p,
.package-copy {
  margin: 0;
  color: var(--shell-text-muted);
}

.stat-card strong {
  font-size: 2rem;
}

.chart-grid,
.disk-grid,
.package-grid {
  display: grid;
  gap: 16px;
}

.chart-grid {
  grid-template-columns: repeat(2, minmax(0, 1fr));
}

.disk-grid {
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
}

.chart-card,
.disk-card,
.package-card {
  padding: 20px;
}

.chart-card-head,
.disk-card-head,
.package-card-head,
.package-meta,
.disk-meta-row {
  display: flex;
  gap: 12px;
  justify-content: space-between;
}

.chart-card-head,
.disk-card-head,
.package-card-head {
  align-items: flex-start;
}

.chart-card-head strong {
  display: block;
  margin-top: 6px;
  font-size: 1.5rem;
}

.chart-card-head span {
  max-width: 16rem;
  color: rgba(210, 232, 255, 0.58);
  text-align: right;
  font-size: 0.84rem;
  line-height: 1.6;
}

.chart-surface {
  margin-top: 16px;
  border-radius: 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background:
    linear-gradient(180deg, rgba(1, 8, 14, 0.96), rgba(3, 13, 24, 0.96));
  overflow: hidden;
}

.chart-svg {
  display: block;
  width: 100%;
  height: 180px;
}

.chart-card {
  color: #3ad8ff;
}

.chart-secondary {
  color: #70f0c1;
}

.chart-warning {
  color: #ffd166;
}

.chart-info {
  color: #84d5ff;
}

.chart-danger {
  color: #ff8d6d;
}

.chart-line {
  fill: none;
  stroke: currentColor;
  stroke-width: 2.2;
  stroke-linecap: round;
  stroke-linejoin: round;
}

.chart-area {
  stroke: none;
}

.disk-card-head strong,
.package-title strong {
  color: var(--shell-text);
}

.disk-meta-row,
.package-meta {
  margin-top: 14px;
  flex-wrap: wrap;
  color: rgba(202, 226, 255, 0.58);
  font-size: 0.84rem;
}

.package-title {
  display: inline-flex;
  align-items: center;
  gap: 10px;
}

.package-copy {
  margin-top: 12px;
  line-height: 1.72;
}

.empty-state {
  color: rgba(210, 232, 255, 0.62);
}

@media (max-width: 1160px) {
  .chart-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 760px) {
  .stats-grid,
  .chart-grid {
    grid-template-columns: 1fr;
  }

  .chart-card-head,
  .disk-card-head,
  .package-card-head {
    flex-direction: column;
    align-items: flex-start;
  }

  .chart-card-head span {
    text-align: left;
  }
}
</style>
