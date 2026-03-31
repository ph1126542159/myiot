<script setup>
import { computed, onBeforeUnmount, onMounted, ref, watchEffect } from 'vue'
import { featurePackages as rawFeaturePackages, getPackageStatusTone } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { formatPackageStatus as formatLocalizedPackageStatus, localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'
import { createUiLocaleHeaders } from './core/requestLocale.js'

const { isZh, toggleLocale } = useUiLocale()
const locale = computed(() => (isZh.value ? 'zh' : 'en'))

const zh = {
  syncing: '正在同步系统监控数据...',
  synced: '系统监控指标同步正常。',
  failed: '系统监控数据同步失败，请稍后重试。',
  brandEyebrow: 'MYIOT 系统监控',
  unavailable: '暂不可用',
  unknown: '未知',
  title: '系统资源实时监控',
  copy: '这里集中显示当前系统的磁盘、内存、线程、进程、IO、网络和 CPU 变化趋势。页面以 1 秒节奏轮询后端指标接口，并在浏览器内维持最近 60 个采样点。',
  backHome: '返回主页',
  bundleList: '系统包列表',
  signOut: '退出登录',
  runtime: '运行态',
  overview: '监控入口概览',
  samplePoints: '个采样点',
  cpu: 'CPU',
  cpuTemp: 'CPU 温度',
  memory: '内存',
  processes: '进程',
  threads: '线程',
  totalCpu: '系统总占用',
  tempSource: 'WMI 热区采样',
  tempUnavailable: '当前设备暂未提供温度传感器',
  processCount: '当前系统进程总数',
  threadCount: '当前系统线程总数',
  charts: '曲线视图',
  chartTitle: '实时资源走势',
  cpuCurve: 'CPU 曲线',
  memoryCurve: '内存曲线',
  processCurve: '进程数',
  threadCurve: '线程数',
  networkThroughput: '网络吞吐',
  diskIo: '磁盘 IO',
  totalUsage: '总使用率',
  activeProcessTotal: '系统活跃进程总量',
  activeThreadTotal: '系统线程总量',
  cpuTempHelper: '摄氏度',
  noCharts: '当前还没有可展示的监控曲线。',
  disks: '磁盘视图',
  diskUsage: '卷使用情况',
  available: '可用',
  used: '已用',
  total: '总量',
  noDisks: '当前没有可展示的磁盘信息。',
  webuiPackages: 'WebUI 包',
  packageEntries: '当前已注册的页面入口',
  openPage: '打开页面',
  language: 'EN',
  documentTitle: 'MyIoT 系统监控'
}

const en = {
  syncing: 'Synchronizing system monitor data...',
  synced: 'System monitoring metrics are synchronized.',
  failed: 'Failed to synchronize monitor data. Please retry later.',
  brandEyebrow: 'MYIOT System Monitor',
  unavailable: 'Unavailable',
  unknown: 'unknown',
  title: 'Realtime System Resource Monitor',
  copy: 'This page brings together disk, memory, thread, process, IO, network, and CPU trends. It polls the backend metrics endpoint once per second and keeps the latest 60 samples in the browser.',
  backHome: 'Back to Home',
  bundleList: 'Bundle List',
  signOut: 'Sign Out',
  runtime: 'Runtime',
  overview: 'Monitor Overview',
  samplePoints: 'sample points',
  cpu: 'CPU',
  cpuTemp: 'CPU Temperature',
  memory: 'Memory',
  processes: 'Processes',
  threads: 'Threads',
  totalCpu: 'Total system usage',
  tempSource: 'WMI thermal sampling',
  tempUnavailable: 'No temperature sensor is currently available',
  processCount: 'Total process count',
  threadCount: 'Total thread count',
  charts: 'Charts',
  chartTitle: 'Realtime Resource Trends',
  cpuCurve: 'CPU Curve',
  memoryCurve: 'Memory Curve',
  processCurve: 'Process Count',
  threadCurve: 'Thread Count',
  networkThroughput: 'Network Throughput',
  diskIo: 'Disk IO',
  totalUsage: 'Total usage',
  activeProcessTotal: 'Total active processes',
  activeThreadTotal: 'Total active threads',
  cpuTempHelper: 'Celsius',
  noCharts: 'No monitoring curves are available yet.',
  disks: 'Disks',
  diskUsage: 'Volume Usage',
  available: 'available',
  used: 'used',
  total: 'total',
  noDisks: 'No disk information is available right now.',
  webuiPackages: 'WebUI Packages',
  packageEntries: 'Registered page entry points',
  openPage: 'Open Page',
  language: '中文',
  documentTitle: 'MyIoT System Monitor'
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
const formatPackageStatus = (status) => formatLocalizedPackageStatus(status, locale.value)

const banner = ref({
  type: 'info',
  text: text.value.syncing
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
  if (!Number.isFinite(numeric)) return text.value.unavailable
  return `${numeric.toFixed(1)}°C`
}

function formatCount(value) {
  return new Intl.NumberFormat(isZh.value ? 'zh-CN' : 'en-US').format(Math.round(toNumber(value)))
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
    : text.value.unknown)
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
      title: text.value.cpu,
      value: formatPercent(metrics.cpu?.usagePercent),
      subtitle: text.value.totalCpu
    },
    {
      key: 'temperature',
      title: text.value.cpuTemp,
      value: formatTemperature(metrics.cpu?.temperatureCelsius),
      subtitle: metrics.cpu?.temperatureAvailable ? text.value.tempSource : text.value.tempUnavailable
    },
    {
      key: 'memory',
      title: text.value.memory,
      value: formatBytes(metrics.memory?.usedBytes),
      subtitle: `${formatPercent(metrics.memory?.usagePercent)} / ${formatBytes(metrics.memory?.totalBytes)}`
    },
    {
      key: 'processes',
      title: text.value.processes,
      value: formatCount(metrics.counts?.processes),
      subtitle: text.value.processCount
    },
    {
      key: 'threads',
      title: text.value.threads,
      value: formatCount(metrics.counts?.threads),
      subtitle: text.value.threadCount
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
      title: text.value.cpuCurve,
      accent: 'chart-primary',
      currentValue: formatPercent(metrics.cpu?.usagePercent),
      helper: text.value.totalUsage,
      values: cpuValues,
      maxValue: 100
    },
    {
      key: 'memory',
      title: text.value.memoryCurve,
      accent: 'chart-secondary',
      currentValue: formatPercent(metrics.memory?.usagePercent),
      helper: `${formatBytes(metrics.memory?.usedBytes)} / ${formatBytes(metrics.memory?.totalBytes)}`,
      values: memoryValues,
      maxValue: 100
    },
    {
      key: 'temperature',
      title: text.value.cpuTemp,
      accent: 'chart-danger',
      currentValue: formatTemperature(metrics.cpu?.temperatureCelsius),
      helper: metrics.cpu?.temperatureAvailable ? text.value.cpuTempHelper : text.value.tempUnavailable,
      values: samples.map((entry) => Number.isFinite(entry.cpuTemperatureCelsius) ? entry.cpuTemperatureCelsius : 0),
      maxValue: metricMax(cpuTemperatureValues, 70, 1.08)
    },
    {
      key: 'processes',
      title: text.value.processCurve,
      accent: 'chart-info',
      currentValue: formatCount(metrics.counts?.processes),
      helper: text.value.activeProcessTotal,
      values: processValues,
      maxValue: metricMax(processValues, 16)
    },
    {
      key: 'threads',
      title: text.value.threadCurve,
      accent: 'chart-warning',
      currentValue: formatCount(metrics.counts?.threads),
      helper: text.value.activeThreadTotal,
      values: threadValues,
      maxValue: metricMax(threadValues, 64)
    },
    {
      key: 'network',
      title: text.value.networkThroughput,
      accent: 'chart-primary',
      currentValue: formatRate(metrics.network?.totalBytesPerSec),
      helper: `${formatRate(metrics.network?.receiveBytesPerSec)} / ${formatRate(metrics.network?.sendBytesPerSec)}`,
      values: networkValues,
      maxValue: metricMax(networkValues, 1024 * 64)
    },
    {
      key: 'io',
      title: text.value.diskIo,
      accent: 'chart-secondary',
      currentValue: formatRate(metrics.io?.totalBytesPerSec),
      helper: `${formatRate(metrics.io?.readBytesPerSec)} / ${formatRate(metrics.io?.writeBytesPerSec)}`,
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
      headers: createUiLocaleHeaders({ Accept: 'application/json' })
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
      text: payload.message ?? text.value.synced
    }
    errorText.value = ''
  } catch {
    errorText.value = text.value.failed
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
              <p class="eyebrow">{{ text.brandEyebrow }}</p>
              <h1>{{ text.title }}</h1>
              <p class="brand-copy">{{ text.copy }}</p>
            </div>
          </div>

          <div class="header-pills">
            <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
              {{ text.language }}
            </v-btn>
            <a href="/myiot/home/index.html" class="meta-pill">
              <v-icon icon="mdi-view-dashboard-outline" size="18"></v-icon>
              <span>{{ text.backHome }}</span>
            </a>
            <a href="/myiot/packages/index.html" class="meta-pill">
              <v-icon icon="mdi-package-variant-closed" size="18"></v-icon>
              <span>{{ text.bundleList }}</span>
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
              {{ text.signOut }}
            </v-btn>
          </div>
        </header>

        <main class="viewport-panel">
          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">{{ text.runtime }}</p>
                <h2>{{ text.overview }}</h2>
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
                  <span>{{ historyPoints.length }} {{ text.samplePoints }}</span>
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
                <p class="section-kicker">{{ text.charts }}</p>
                <h2>{{ text.chartTitle }}</h2>
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
              {{ text.noCharts }}
            </div>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">{{ text.disks }}</p>
                <h2>{{ text.diskUsage }}</h2>
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
                    {{ disk.freeText }} {{ text.available }}
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
                  <span>{{ text.used }} {{ disk.usedText }}</span>
                  <span>{{ text.total }} {{ disk.totalText }}</span>
                </div>
              </article>
            </div>

            <div v-else-if="!loading" class="empty-state">
              {{ text.noDisks }}
            </div>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">{{ text.webuiPackages }}</p>
                <h2>{{ text.packageEntries }}</h2>
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
                  <a :href="featurePackage.entryPath">{{ text.openPage }}</a>
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
