<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch, watchEffect } from 'vue'
import { featurePackages as rawFeaturePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'
import { createUiLocaleHeaders } from './core/requestLocale.js'

const { isZh, toggleLocale } = useUiLocale()
const locale = computed(() => (isZh.value ? 'zh' : 'en'))

const zh = {
  connecting: '正在连接 JNDM123 控制服务...',
  unknown: '未知',
  signOut: '退出登录',
  language: 'EN',
  labEyebrow: 'JNDM123 实验台',
  labTitle: '时钟分频与 6 路 AD7606 采集',
  labCopy: '分频器调整遵循本地 CDCE937 I2C 测试流程。FIFO 采集沿用 `work.c` 的 packet-mode 实现，去掉控制台/文件输出后，统一喂给页面预览和 UDP 广播。',
  clockEyebrow: '时钟',
  clockTitle: 'CDCE937 分频控制',
  clockCopy: '点击执行后，后端会暂停采集、写入分频值、回读实际结果，并在需要时恢复采集。',
  i2cDevice: 'I2C 设备',
  output: '输出',
  divider: '分频值',
  execute: '执行',
  refresh: '刷新',
  device: '设备',
  selectedOutput: '当前输出',
  eepromLock: 'EEPROM / 锁定',
  busy: '忙',
  lock: '锁定',
  revision: '版本',
  pin: '引脚',
  power: '电源',
  active: '活动',
  down: '关闭',
  actualReadback: '下方每个输出卡片都会展示实际回读结果。',
  acquisitionEyebrow: '采集',
  acquisitionTitle: 'AD7606 采集运行态',
  acquisitionCopy: '读取线程只负责搬运整帧数据进入 `Poco::NotificationQueue`，波形缓存和 UDP 广播都交给消费侧处理。浏览器预览每秒刷新一次缓存快照，UDP 则持续走完整出队链路。',
  start: '启动',
  stop: '停止',
  broadcastEyebrow: '广播',
  broadcastTitle: 'UDP 外部接口',
  broadcastCopy: '即使前端暂停波形预览，每一帧完整 48 通道数据仍可按打包格式持续广播到 UDP。',
  enableUdp: '启用 UDP 广播',
  udpHost: 'UDP 主机',
  udpPort: 'UDP 端口',
  saveUdp: '保存 UDP 配置',
  lastUdp: '最近 UDP',
  none: '无',
  target: '目标',
  udpPackets: 'UDP 累计包数',
  udpBytes: 'UDP 累计字节',
  udpPacketsRate: 'UDP 包/秒',
  udpBytesRate: 'UDP 字节/秒',
  udpPacketsTrend: '每秒发包速率',
  udpBytesTrend: '每秒字节速率',
  collectingUdpStats: '正在累计 UDP 每秒统计，请保持页面打开几秒钟。',
  waveformsEyebrow: '波形',
  waveformsTitle: '6 组独立图表',
  waveformsCopy: '每张图对应一个 AD7606 芯片。可选择“全部”或单独 `CH1~CH8`。预览关闭时，后端会停止为浏览器打包历史波形，但仍保留 UDP 广播；预览开启时，缓存波形每秒刷新一次。',
  preview: '预览',
  overview: '概览',
  channels: '通道',
  noWaveform: '暂无波形数据。请先启动采集，或保持当前预览页面激活直到历史窗口填满。',
  visibleLines: (lines, points) => `${lines} 条可见曲线 / ${points} 个采样点`,
  navigationEyebrow: '导航',
  navigationTitle: '其他功能包',
  all: '全部',
  capture: '采集',
  running: '运行中',
  stopped: '已停止',
  waitingForOperatorAction: '等待操作指令',
  frames: '帧数',
  lastFrame: (value) => `最近一帧 ${value}`,
  noFramesYet: '还没有帧数据',
  recoveries: '恢复次数',
  noRecoveryWarning: '暂无恢复告警',
  queue: '队列',
  previewCacheActive: '预览缓存已启用',
  previewCachePaused: '预览缓存已暂停',
  hz: 'Hz',
  khz: 'kHz',
  mhz: 'MHz',
  notUpdated: '未更新',
  unauthorized: '未授权',
  unableReadDividerStatus: '无法读取分频器状态。',
  unableSyncAcquisitionState: '无法同步采集状态。',
  applyingDivider: (divider, output) => `正在把分频值 ${divider} 应用到 ${output}...`,
  dividerUpdateComplete: '分频更新完成。',
  dividerUpdateFailed: '分频更新失败。',
  startingCapture: '正在启动 AD7606 采集...',
  stoppingCapture: '正在停止 AD7606 采集...',
  captureStarted: '采集已启动。',
  captureStopped: '采集已停止。',
  captureControlFailed: '采集控制失败。',
  savingUdpConfig: '正在保存 UDP 广播配置...',
  udpConfigSaved: 'UDP 配置已保存。',
  unableSaveUdpConfig: '无法保存 UDP 配置。',
  welcome: (username) => `欢迎 ${username}，正在同步 JNDM123 硬件状态。`,
  documentTitle: 'MyIoT JNDM123 控制台'
}

const en = {
  connecting: 'Connecting to JNDM123 control service...',
  unknown: 'unknown',
  signOut: 'Sign Out',
  language: '中文',
  labEyebrow: 'JNDM123 Lab',
  labTitle: 'Clock Divider and 6x AD7606 Capture',
  labCopy: 'Divider updates follow the local CDCE937 I2C test flow. FIFO capture follows the packet-mode implementation from `work.c`, removes console/file output, and feeds the UI plus UDP broadcast.',
  clockEyebrow: 'Clock',
  clockTitle: 'CDCE937 Divider Control',
  clockCopy: 'Press execute to apply the selected divider. The backend pauses acquisition, writes the divider, reads the actual result back, and restarts capture when needed.',
  i2cDevice: 'I2C Device',
  output: 'Output',
  divider: 'Divider',
  execute: 'Execute',
  refresh: 'Refresh',
  device: 'Device',
  selectedOutput: 'Selected Output',
  eepromLock: 'EEPROM / Lock',
  busy: 'Busy',
  lock: 'Lock',
  revision: 'Revision',
  pin: 'Pin',
  power: 'Power',
  active: 'Active',
  down: 'Down',
  actualReadback: 'Actual readback is shown in every output card below.',
  acquisitionEyebrow: 'Acquisition',
  acquisitionTitle: 'AD7606 Capture Runtime',
  acquisitionCopy: 'The reader thread stays lean, pushes full frames into `Poco::NotificationQueue`, and leaves waveform caching and UDP broadcasting to the consumer side. Browser preview snapshots are published from cache once per second, while UDP stays on the full dequeue path.',
  start: 'Start',
  stop: 'Stop',
  broadcastEyebrow: 'Broadcast',
  broadcastTitle: 'UDP External Interface',
  broadcastCopy: 'Every dequeued frame can be broadcast as a packed 48-channel UDP payload even when preview rendering is paused on the front end.',
  enableUdp: 'Enable UDP Broadcast',
  udpHost: 'UDP Host',
  udpPort: 'UDP Port',
  saveUdp: 'Save UDP',
  lastUdp: 'Last UDP',
  none: 'none',
  target: 'Target',
  udpPackets: 'UDP Packets',
  udpBytes: 'UDP Bytes',
  udpPacketsRate: 'UDP Packets/s',
  udpBytesRate: 'UDP Bytes/s',
  udpPacketsTrend: 'Packets Per Second',
  udpBytesTrend: 'Bytes Per Second',
  collectingUdpStats: 'Collecting UDP per-second statistics. Keep this page open for a few seconds.',
  waveformsEyebrow: 'Waveforms',
  waveformsTitle: '6 Independent Charts',
  waveformsCopy: 'Each chart maps one AD7606 chip. Choose `All` or a single channel from `CH1~CH8`. When preview is not active, the backend stops packaging waveform history for the browser but keeps UDP broadcast alive. When preview is active, cached waveform data is refreshed to the browser once per second.',
  preview: 'Preview',
  overview: 'Overview',
  channels: 'Channels',
  noWaveform: 'No waveform data yet. Start capture or keep the preview page active until the history window fills.',
  visibleLines: (lines, points) => `${lines} visible lines / ${points} points`,
  navigationEyebrow: 'Navigation',
  navigationTitle: 'Other Packages',
  all: 'All',
  capture: 'Capture',
  running: 'Running',
  stopped: 'Stopped',
  waitingForOperatorAction: 'Waiting for operator action',
  frames: 'Frames',
  lastFrame: (value) => `Last frame ${value}`,
  noFramesYet: 'No frames yet',
  recoveries: 'Recoveries',
  noRecoveryWarning: 'No recovery warning',
  queue: 'Queue',
  previewCacheActive: 'Preview cache active',
  previewCachePaused: 'Preview cache paused',
  hz: 'Hz',
  khz: 'kHz',
  mhz: 'MHz',
  notUpdated: 'not updated',
  unauthorized: 'unauthorized',
  unableReadDividerStatus: 'Unable to read divider status.',
  unableSyncAcquisitionState: 'Unable to synchronize acquisition state.',
  applyingDivider: (divider, output) => `Applying divider ${divider} to ${output}...`,
  dividerUpdateComplete: 'Divider update complete.',
  dividerUpdateFailed: 'Divider update failed.',
  startingCapture: 'Starting AD7606 capture...',
  stoppingCapture: 'Stopping AD7606 capture...',
  captureStarted: 'Capture started.',
  captureStopped: 'Capture stopped.',
  captureControlFailed: 'Capture control failed.',
  savingUdpConfig: 'Saving UDP broadcast configuration...',
  udpConfigSaved: 'UDP configuration saved.',
  unableSaveUdpConfig: 'Unable to save UDP configuration.',
  welcome: (username) => `Welcome ${username}. JNDM123 hardware state is synchronizing.`,
  documentTitle: 'MyIoT JNDM123 Acquisition'
}

const text = computed(() => (isZh.value ? zh : en))

watchEffect(() => {
  if (typeof document !== 'undefined') {
    document.title = text.value.documentTitle
  }
})
const uiLocale = computed(() => (isZh.value ? 'zh-CN' : 'en-US'))
const featurePackages = computed(() =>
  rawFeaturePackages.map((featurePackage) => localizeFeaturePackage(featurePackage, locale.value))
)

const channelPalette = ['#38d6ff', '#8cf2b2', '#f7a94a', '#ff6e88', '#7ab8ff', '#ffd166', '#d48bff', '#8df4ff']
const channelOptions = computed(() => [
  { title: text.value.all, value: 'all' },
  { title: 'CH1', value: '0' },
  { title: 'CH2', value: '1' },
  { title: 'CH3', value: '2' },
  { title: 'CH4', value: '3' },
  { title: 'CH5', value: '4' },
  { title: 'CH6', value: '5' },
  { title: 'CH7', value: '6' },
  { title: 'CH8', value: '7' },
])
const dividerOptions = [1, 2, 3, 4, 5, 10].map((value) => ({ title: `${value}`, value }))
const chartPlot = Object.freeze({ left: 8, right: 97, top: 12, bottom: 82 })
const udpRateHistoryLimit = 30

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const devicePath = ref('/dev/i2c-0')
const dividerBusy = ref(false)
const acquisitionBusy = ref(false)
const udpBusy = ref(false)
const pollInFlight = ref(false)
const dividerStatus = ref({ outputs: [] })
const acquisitionState = ref({ chips: [], udp: { enabled: true, host: '255.255.255.255', port: 19048 } })
const selectedOutputIndex = ref(0)
const selectedDivider = ref(1)
const activeView = ref('preview')
const channelSelection = reactive({
  0: 'all',
  1: 'all',
  2: 'all',
  3: 'all',
  4: 'all',
  5: 'all'
})
const udpForm = reactive({
  enabled: true,
  host: '255.255.255.255',
  port: 19048
})
const udpRateHistory = ref([])
const udpRateBaseline = ref(null)

let pollTimer = null

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : text.value.unknown)
)

const outputs = computed(() => dividerStatus.value?.outputs ?? [])
const chips = computed(() => acquisitionState.value?.chips ?? [])
const acquisitionRunning = computed(() => Boolean(acquisitionState.value?.running))
const previewEnabled = computed(() => activeView.value === 'preview' && document.visibilityState === 'visible')
const selectedOutput = computed(() => outputs.value.find((entry) => entry.index === selectedOutputIndex.value) ?? null)
const outputOptions = computed(() =>
  outputs.value.map((output) => ({
    title: `${output.name} / ${text.value.pin} ${output.pin}`,
    value: output.index
  }))
)

const launchablePackages = computed(() =>
  featurePackages.value.filter((featurePackage) => featurePackage.entryPath)
)
const waveformTimelineUs = computed(() => Array.isArray(acquisitionState.value?.timelineUs) ? acquisitionState.value.timelineUs : [])
const udpTimelineMs = computed(() => udpRateHistory.value.map((point) => point.timeMs))
const udpPacketSeries = computed(() => udpRateHistory.value.map((point) => point.packetsPerSecond))
const udpByteSeries = computed(() => udpRateHistory.value.map((point) => point.bytesPerSecond))
const udpPacketRange = computed(() => rateChartRange(udpPacketSeries.value))
const udpByteRange = computed(() => rateChartRange(udpByteSeries.value))
const latestUdpRatePoint = computed(() => udpRateHistory.value[udpRateHistory.value.length - 1] ?? null)

const acquisitionMetrics = computed(() => [
  {
    label: text.value.capture,
    value: acquisitionRunning.value ? text.value.running : text.value.stopped,
    helper: acquisitionState.value?.message ?? text.value.waitingForOperatorAction
  },
  {
    label: text.value.frames,
    value: formatInteger(acquisitionState.value?.totalFrames),
    helper: acquisitionState.value?.lastFrameAt ? text.value.lastFrame(formatDateTime(acquisitionState.value.lastFrameAt)) : text.value.noFramesYet
  },
  {
    label: text.value.recoveries,
    value: formatInteger(acquisitionState.value?.recoveries),
    helper: acquisitionState.value?.lastError || text.value.noRecoveryWarning
  },
  {
    label: text.value.queue,
    value: formatInteger(acquisitionState.value?.queueDepth),
    helper: acquisitionState.value?.previewActive ? text.value.previewCacheActive : text.value.previewCachePaused
  }
])
const udpMetrics = computed(() => [
  {
    label: text.value.udpPacketsRate,
    value: formatPacketRate(latestUdpRatePoint.value?.packetsPerSecond),
    helper: acquisitionState.value?.udp?.lastBroadcastAt ? formatDateTime(acquisitionState.value.udp.lastBroadcastAt) : text.value.none
  },
  {
    label: text.value.udpBytesRate,
    value: formatBytesRate(latestUdpRatePoint.value?.bytesPerSecond),
    helper: acquisitionState.value?.udp?.enabled ? `${acquisitionState.value?.udp?.host || udpForm.host}:${acquisitionState.value?.udp?.port || udpForm.port}` : text.value.none
  },
  {
    label: text.value.udpPackets,
    value: formatInteger(acquisitionState.value?.udp?.packetsSent),
    helper: acquisitionState.value?.udp?.lastBroadcastAt ? formatDateTime(acquisitionState.value.udp.lastBroadcastAt) : text.value.none
  },
  {
    label: text.value.udpBytes,
    value: formatBytes(acquisitionState.value?.udp?.bytesSent),
    helper: acquisitionState.value?.udp?.enabled ? text.value.running : text.value.stopped
  }
])

function formatInteger(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '0'
  return new Intl.NumberFormat(uiLocale.value).format(Math.round(numeric))
}

function formatDecimal(value, maximumFractionDigits = 1) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '0'
  return new Intl.NumberFormat(uiLocale.value, {
    minimumFractionDigits: 0,
    maximumFractionDigits
  }).format(numeric)
}

function formatFrequency(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric) || numeric <= 0) return '--'
  if (numeric >= 1000000) return `${(numeric / 1000000).toFixed(3)} ${text.value.mhz}`
  if (numeric >= 1000) return `${(numeric / 1000).toFixed(2)} ${text.value.khz}`
  return `${numeric.toFixed(0)} ${text.value.hz}`
}

function formatDateTime(value) {
  if (!value) return text.value.notUpdated
  return new Date(value).toLocaleString(uiLocale.value, { hour12: false })
}

function formatTimelineLabel(value, unit = 'us') {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '--'

  const date = new Date(unit === 'us' ? numeric / 1000 : numeric)
  const clock = date.toLocaleTimeString(uiLocale.value, {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  })
  return `${clock}.${String(date.getMilliseconds()).padStart(3, '0')}`
}

function formatBytes(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric) || numeric <= 0) return '0 B'

  const units = ['B', 'KB', 'MB', 'GB']
  let scaled = numeric
  let unitIndex = 0
  while (scaled >= 1024 && unitIndex < units.length - 1) {
    scaled /= 1024
    unitIndex += 1
  }

  const digits = scaled >= 100 || unitIndex === 0 ? 0 : 1
  return `${formatDecimal(scaled, digits)} ${units[unitIndex]}`
}

function formatPacketRate(value) {
  return `${formatDecimal(value, 1)} /s`
}

function formatBytesRate(value) {
  return `${formatBytes(value)}/s`
}

function formatPacketAxisValue(value) {
  return formatDecimal(value, 1)
}

function formatAxisValue(value, formatter = formatInteger) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '--'
  return formatter(numeric)
}

function syncSelectedDivider() {
  const current = selectedOutput.value
  if (current && dividerOptions.some((option) => option.value === current.divider)) {
    selectedDivider.value = current.divider
  }
}

function syncUdpForm() {
  const udp = acquisitionState.value?.udp
  if (!udp) return
  udpForm.enabled = Boolean(udp.enabled)
  udpForm.host = udp.host || '255.255.255.255'
  udpForm.port = Number(udp.port) || 19048
}

function syncAcquisitionState(payload) {
  acquisitionState.value = payload
  syncUdpForm()
  updateUdpRateHistory(payload)
}

function updateUdpRateHistory(payload) {
  const udp = payload?.udp
  if (!udp) return

  const packetsSent = Number(udp.packetsSent) || 0
  const bytesSent = Number(udp.bytesSent) || 0
  const timeMs = Date.parse(payload.updatedAt || udp.lastBroadcastAt || new Date().toISOString())
  const previous = udpRateBaseline.value

  if (!Number.isFinite(timeMs)) return

  if (!previous || packetsSent < previous.packetsSent || bytesSent < previous.bytesSent) {
    udpRateBaseline.value = { timeMs, packetsSent, bytesSent }
    udpRateHistory.value = []
    return
  }

  let elapsedMs = timeMs - previous.timeMs
  if (!(elapsedMs > 0)) elapsedMs = 1000

  udpRateHistory.value = [
    ...udpRateHistory.value,
    {
      timeMs,
      packetsPerSecond: Math.max(0, ((packetsSent - previous.packetsSent) * 1000) / elapsedMs),
      bytesPerSecond: Math.max(0, ((bytesSent - previous.bytesSent) * 1000) / elapsedMs)
    }
  ].slice(-udpRateHistoryLimit)

  udpRateBaseline.value = { timeMs, packetsSent, bytesSent }
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, {
    credentials: 'same-origin',
    headers: createUiLocaleHeaders({
      Accept: 'application/json',
      ...(options.body ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {})
    }),
    ...options
  })

  if (response.status === 401) {
    window.location.replace('/myiot/login/index.html')
    throw new Error('unauthorized')
  }

  let payload = {}
  try {
    payload = await response.json()
  } catch {
  }

  if (!response.ok || payload.ok === false) {
    throw new Error(payload.message || `request status ${response.status}`)
  }

  return payload
}

async function loadDividerStatus() {
  try {
    const payload = await requestJson(`/myiot/jndm123/divider.json?devicePath=${encodeURIComponent(devicePath.value)}`)
    dividerStatus.value = payload
    syncSelectedDivider()
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || text.value.unableReadDividerStatus
    }
  }
}

async function loadAcquisitionSnapshot() {
  if (pollInFlight.value) return
  pollInFlight.value = true

  try {
    const query = previewEnabled.value ? '?includeWaveform=1' : ''
    const payload = await requestJson(`/myiot/jndm123/acquisition.json${query}`)
    syncAcquisitionState(payload)
  } catch (error) {
    banner.value = {
      type: 'warning',
      text: error.message || text.value.unableSyncAcquisitionState
    }
  } finally {
    pollInFlight.value = false
  }
}

async function applyDivider() {
  dividerBusy.value = true
  banner.value = {
    type: 'info',
    text: text.value.applyingDivider(selectedDivider.value, selectedOutput.value?.name || `Y${selectedOutputIndex.value + 1}`)
  }

  try {
    const body = new URLSearchParams()
    body.set('devicePath', devicePath.value)
    body.set('outputIndex', String(selectedOutputIndex.value))
    body.set('divider', String(selectedDivider.value))
    const payload = await requestJson('/myiot/jndm123/divider.json', {
      method: 'POST',
      body
    })

    dividerStatus.value = payload
    syncSelectedDivider()
    await loadAcquisitionSnapshot()

    banner.value = {
      type: 'success',
      text: payload.message || text.value.dividerUpdateComplete
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || text.value.dividerUpdateFailed
    }
  } finally {
    dividerBusy.value = false
  }
}

async function setAcquisition(action) {
  acquisitionBusy.value = true
  banner.value = {
    type: 'info',
    text: action === 'start' ? text.value.startingCapture : text.value.stoppingCapture
  }

  try {
    const body = new URLSearchParams()
    body.set('action', action)
    if (previewEnabled.value) body.set('includeWaveform', '1')
    const payload = await requestJson('/myiot/jndm123/acquisition.json', {
      method: 'POST',
      body
    })
    syncAcquisitionState(payload)
    banner.value = {
      type: 'success',
      text: payload.message || (action === 'start' ? text.value.captureStarted : text.value.captureStopped)
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || text.value.captureControlFailed
    }
  } finally {
    acquisitionBusy.value = false
  }
}

async function saveUdpConfig() {
  udpBusy.value = true
  banner.value = {
    type: 'info',
    text: text.value.savingUdpConfig
  }

  try {
    const body = new URLSearchParams()
    body.set('udpEnabled', udpForm.enabled ? '1' : '0')
    body.set('udpHost', udpForm.host)
    body.set('udpPort', String(udpForm.port))
    if (previewEnabled.value) body.set('includeWaveform', '1')
    const payload = await requestJson('/myiot/jndm123/acquisition.json', {
      method: 'POST',
      body
    })
    syncAcquisitionState(payload)
    banner.value = {
      type: 'success',
      text: payload.message || text.value.udpConfigSaved
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || text.value.unableSaveUdpConfig
    }
  } finally {
    udpBusy.value = false
  }
}

function visibleSeries(chip) {
  const selectedMode = channelSelection[chip.index] ?? 'all'
  const channels = Array.isArray(chip.channels) ? chip.channels : []
  const visibleChannels = selectedMode === 'all'
    ? channels
    : channels.filter((channel) => String(channel.index) === selectedMode)

  return visibleChannels.map((channel) => ({
    ...channel,
    color: channelPalette[channel.index % channelPalette.length],
    samples: Array.isArray(channel.samples) ? channel.samples : []
  }))
}

function chartRange(chip) {
  const values = visibleSeries(chip).flatMap((series) => series.samples)
  if (!values.length) return { min: -1, max: 1 }

  const min = Math.min(...values)
  const max = Math.max(...values)
  if (min === max) return { min: min - 1, max: max + 1 }

  const padding = Math.max((max - min) * 0.1, 1)
  return { min: min - padding, max: max + padding }
}

function rateChartRange(values) {
  if (!values.length) return { min: 0, max: 1 }

  const max = Math.max(...values)
  const paddedMax = max <= 0 ? 1 : max + Math.max(max * 0.12, 1)
  return { min: 0, max: paddedMax }
}

function normalizeY(value, minValue, maxValue) {
  const safeSpan = Math.max(maxValue - minValue, 1)
  const ratio = (value - minValue) / safeSpan
  return chartPlot.bottom - (ratio * (chartPlot.bottom - chartPlot.top))
}

function resolveX(index, length, timeline = []) {
  if (length <= 1) return (chartPlot.left + chartPlot.right) / 2
  if (timeline.length === length) {
    const start = Number(timeline[0])
    const end = Number(timeline[length - 1])
    const current = Number(timeline[index])
    if (Number.isFinite(start) && Number.isFinite(end) && Number.isFinite(current) && end > start) {
      const ratio = (current - start) / (end - start)
      return chartPlot.left + (ratio * (chartPlot.right - chartPlot.left))
    }
  }

  return chartPlot.left + ((index / (length - 1)) * (chartPlot.right - chartPlot.left))
}

function linePoints(samples, minValue, maxValue, timeline = []) {
  if (!samples.length) return ''

  return samples.map((sample, index) => {
    const x = resolveX(index, samples.length, timeline)
    const y = normalizeY(sample, minValue, maxValue)
    return `${x.toFixed(2)},${y.toFixed(2)}`
  }).join(' ')
}

function zeroLineY(minValue, maxValue) {
  if (minValue > 0 || maxValue < 0) return null
  return normalizeY(0, minValue, maxValue).toFixed(2)
}

function chartStartLabel(timeline, unit = 'us') {
  return timeline.length ? formatTimelineLabel(timeline[0], unit) : '--'
}

function chartEndLabel(timeline, unit = 'us') {
  return timeline.length ? formatTimelineLabel(timeline[timeline.length - 1], unit) : '--'
}

function chipTimeline(chip) {
  const expectedLength = chip?.channels?.[0]?.samples?.length ?? 0
  if (!expectedLength) return []
  if (waveformTimelineUs.value.length === expectedLength) return waveformTimelineUs.value
  if (waveformTimelineUs.value.length > expectedLength) return waveformTimelineUs.value.slice(waveformTimelineUs.value.length - expectedLength)
  return waveformTimelineUs.value
}

function latestChannelValue(series) {
  if (series.samples?.length) return String(series.samples[series.samples.length - 1])
  if (series.hasValue) return String(series.value)
  return '--'
}

function resetPolling() {
  if (pollTimer) {
    window.clearInterval(pollTimer)
    pollTimer = null
  }

  pollTimer = window.setInterval(() => {
    loadAcquisitionSnapshot()
  }, previewEnabled.value ? 1000 : 1200)
}

function handleVisibilityChange() {
  resetPolling()
  loadAcquisitionSnapshot()
}

async function handleSignOut() {
  await signOut()
  window.location.replace('/myiot/login/index.html')
}

watch(activeView, () => {
  resetPolling()
  loadAcquisitionSnapshot()
})

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  banner.value = {
    type: 'success',
    text: text.value.welcome(sessionState.username)
  }

  await loadDividerStatus()
  await loadAcquisitionSnapshot()
  resetPolling()
  document.addEventListener('visibilitychange', handleVisibilityChange)
})

onBeforeUnmount(() => {
  if (pollTimer) {
    window.clearInterval(pollTimer)
    pollTimer = null
  }
  document.removeEventListener('visibilitychange', handleVisibilityChange)
})
</script>

<template>
  <v-app>
    <div class="lab-scene">
      <div class="lab-grid" aria-hidden="true"></div>
      <div class="lab-glow lab-glow-a" aria-hidden="true"></div>
      <div class="lab-glow lab-glow-b" aria-hidden="true"></div>

      <v-container fluid class="lab-container">
        <header class="lab-header">
          <div class="brand-block">
            <div class="brand-mark"></div>
            <div>
              <p class="eyebrow">{{ text.labEyebrow }}</p>
              <h1>{{ text.labTitle }}</h1>
              <p class="brand-copy">{{ text.labCopy }}</p>
            </div>
          </div>

          <div class="header-actions">
            <div class="header-pills">
              <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
                {{ text.language }}
              </v-btn>
              <div class="meta-pill">
                <v-icon icon="mdi-lan-connect" size="18"></v-icon>
                <span>{{ currentDeviceAddress }}</span>
              </div>
              <div class="meta-pill">
                <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
                <span>{{ sessionState.username }}</span>
              </div>
            </div>

            <v-btn color="secondary" variant="tonal" size="small" @click="handleSignOut">
              {{ text.signOut }}
            </v-btn>
          </div>
        </header>

        <div class="layout-grid">
          <div class="left-stack">
            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">{{ text.clockEyebrow }}</p>
                  <h2>{{ text.clockTitle }}</h2>
                  <p class="panel-copy">{{ text.clockCopy }}</p>
                </div>
              </div>

              <v-alert :type="banner.type" variant="tonal" border="start" class="mb-5">
                {{ banner.text }}
              </v-alert>

              <div class="control-grid">
                <v-text-field
                  v-model="devicePath"
                  :label="text.i2cDevice"
                  prepend-inner-icon="mdi-expansion-card-variant"
                  placeholder="/dev/i2c-0"
                />

                <v-select
                  v-model="selectedOutputIndex"
                  :items="outputOptions"
                  :label="text.output"
                  prepend-inner-icon="mdi-vector-polyline"
                />

                <v-select
                  v-model="selectedDivider"
                  :items="dividerOptions"
                  :label="text.divider"
                  prepend-inner-icon="mdi-tune-variant"
                />

                <div class="panel-actions">
                  <v-btn color="primary" block :loading="dividerBusy" :disabled="dividerBusy || !outputs.length" @click="applyDivider">
                    {{ text.execute }}
                  </v-btn>
                  <v-btn variant="outlined" color="secondary" block :disabled="dividerBusy" @click="loadDividerStatus">
                    {{ text.refresh }}
                  </v-btn>
                </div>
              </div>

              <div class="status-grid mt-5">
                <article class="metric-card">
                  <div class="meta-copy">{{ text.device }}</div>
                  <div class="metric-value">{{ dividerStatus?.deviceType || '--' }}</div>
                  <div class="meta-copy small">{{ dividerStatus?.address || '--' }} / {{ dividerStatus?.inputClock || '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">{{ text.selectedOutput }}</div>
                  <div class="metric-value">{{ selectedOutput?.divider ?? '--' }} / {{ formatFrequency(selectedOutput?.frequencyHz) }}</div>
                  <div class="meta-copy small">{{ selectedOutput?.name || '--' }} / {{ selectedOutput?.pdiv || '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">{{ text.eepromLock }}</div>
                  <div class="metric-value">{{ text.busy }} {{ dividerStatus?.eepBusy ? '1' : '0' }} / {{ text.lock }} {{ dividerStatus?.eepLock ? '1' : '0' }}</div>
                  <div class="meta-copy small">{{ text.revision }} {{ dividerStatus?.revisionId ?? '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">{{ text.power }}</div>
                  <div class="metric-value">{{ dividerStatus?.powerDown ? text.down : text.active }}</div>
                  <div class="meta-copy small">{{ text.actualReadback }}</div>
                </article>
              </div>

              <div class="outputs-grid">
                <article
                  v-for="output in outputs"
                  :key="output.index"
                  class="output-card"
                  :class="{ active: output.index === selectedOutputIndex }"
                >
                  <div class="meta-copy">{{ output.name }} / {{ output.pdiv }}</div>
                  <div class="output-line">{{ output.divider || '--' }}</div>
                  <div class="meta-copy small">{{ text.pin }} {{ output.pin }} / {{ formatFrequency(output.frequencyHz) }}</div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">{{ text.acquisitionEyebrow }}</p>
                  <h2>{{ text.acquisitionTitle }}</h2>
                  <p class="panel-copy">{{ text.acquisitionCopy }}</p>
                </div>

                <div class="panel-actions">
                  <v-btn color="primary" :loading="acquisitionBusy" :disabled="acquisitionBusy || acquisitionRunning" @click="setAcquisition('start')">
                    {{ text.start }}
                  </v-btn>
                  <v-btn color="warning" variant="outlined" :loading="acquisitionBusy" :disabled="acquisitionBusy || !acquisitionRunning" @click="setAcquisition('stop')">
                    {{ text.stop }}
                  </v-btn>
                </div>
              </div>

              <div class="metrics-grid">
                <article v-for="metric in acquisitionMetrics" :key="metric.label" class="metric-card">
                  <div class="meta-copy">{{ metric.label }}</div>
                  <div class="metric-value">{{ metric.value }}</div>
                  <div class="meta-copy small">{{ metric.helper }}</div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">{{ text.broadcastEyebrow }}</p>
                  <h2>{{ text.broadcastTitle }}</h2>
                  <p class="panel-copy">{{ text.broadcastCopy }}</p>
                </div>
              </div>

              <div class="config-grid">
                <v-switch v-model="udpForm.enabled" color="primary" inset :label="text.enableUdp" />
                <v-text-field v-model="udpForm.host" :label="text.udpHost" prepend-inner-icon="mdi-access-point-network" />
                <v-text-field v-model="udpForm.port" type="number" :label="text.udpPort" prepend-inner-icon="mdi-connection" />
                <div class="panel-actions">
                  <v-btn color="primary" block :loading="udpBusy" @click="saveUdpConfig">
                    {{ text.saveUdp }}
                  </v-btn>
                </div>
              </div>

              <div class="legend-row">
                <span class="legend-chip">{{ text.lastUdp }}: {{ acquisitionState?.udp?.lastBroadcastAt ? formatDateTime(acquisitionState.udp.lastBroadcastAt) : text.none }}</span>
                <span class="legend-chip">{{ text.target }}: {{ acquisitionState?.udp?.host || udpForm.host }}:{{ acquisitionState?.udp?.port || udpForm.port }}</span>
              </div>

              <div class="metrics-grid udp-metrics-grid">
                <article v-for="metric in udpMetrics" :key="metric.label" class="metric-card">
                  <div class="meta-copy">{{ metric.label }}</div>
                  <div class="metric-value">{{ metric.value }}</div>
                  <div class="meta-copy small">{{ metric.helper }}</div>
                </article>
              </div>

              <div class="udp-rate-grid">
                <article class="chart-card compact-chart-card">
                  <div class="chart-head compact-chart-head">
                    <div class="chart-title">
                      <strong>{{ text.udpPacketsTrend }}</strong>
                      <span>{{ udpRateHistory.length ? text.visibleLines(1, udpPacketSeries.length) : text.collectingUdpStats }}</span>
                    </div>
                  </div>

                  <div v-if="udpPacketSeries.length" class="chart-surface compact-chart-surface">
                    <div class="chart-axis chart-axis-y-top">{{ formatAxisValue(udpPacketRange.max, formatPacketAxisValue) }}</div>
                    <div class="chart-axis chart-axis-y-bottom">{{ formatAxisValue(udpPacketRange.min, formatPacketAxisValue) }}</div>
                    <div class="chart-axis chart-axis-x-left">{{ chartStartLabel(udpTimelineMs, 'ms') }}</div>
                    <div class="chart-axis chart-axis-x-right">{{ chartEndLabel(udpTimelineMs, 'ms') }}</div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg compact-chart-svg">
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.top"
                        :y2="chartPlot.top"
                        class="chart-boundary-line"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.bottom"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line chart-boundary-line-strong"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.left"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line"
                      />
                      <polyline
                        :points="linePoints(udpPacketSeries, udpPacketRange.min, udpPacketRange.max, udpTimelineMs)"
                        fill="none"
                        stroke="#8cf2b2"
                        stroke-width="0.72"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      />
                    </svg>
                  </div>

                  <div v-else class="chart-placeholder compact-chart-placeholder">
                    {{ text.collectingUdpStats }}
                  </div>
                </article>

                <article class="chart-card compact-chart-card">
                  <div class="chart-head compact-chart-head">
                    <div class="chart-title">
                      <strong>{{ text.udpBytesTrend }}</strong>
                      <span>{{ udpRateHistory.length ? text.visibleLines(1, udpByteSeries.length) : text.collectingUdpStats }}</span>
                    </div>
                  </div>

                  <div v-if="udpByteSeries.length" class="chart-surface compact-chart-surface">
                    <div class="chart-axis chart-axis-y-top">{{ formatAxisValue(udpByteRange.max, formatBytes) }}</div>
                    <div class="chart-axis chart-axis-y-bottom">{{ formatAxisValue(udpByteRange.min, formatBytes) }}</div>
                    <div class="chart-axis chart-axis-x-left">{{ chartStartLabel(udpTimelineMs, 'ms') }}</div>
                    <div class="chart-axis chart-axis-x-right">{{ chartEndLabel(udpTimelineMs, 'ms') }}</div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg compact-chart-svg">
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.top"
                        :y2="chartPlot.top"
                        class="chart-boundary-line"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.bottom"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line chart-boundary-line-strong"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.left"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line"
                      />
                      <polyline
                        :points="linePoints(udpByteSeries, udpByteRange.min, udpByteRange.max, udpTimelineMs)"
                        fill="none"
                        stroke="#f7a94a"
                        stroke-width="0.72"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      />
                    </svg>
                  </div>

                  <div v-else class="chart-placeholder compact-chart-placeholder">
                    {{ text.collectingUdpStats }}
                  </div>
                </article>
              </div>
            </section>
          </div>

          <div class="right-stack">
            <section class="lab-panel chart-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">{{ text.waveformsEyebrow }}</p>
                  <h2>{{ text.waveformsTitle }}</h2>
                  <p class="panel-copy">{{ text.waveformsCopy }}</p>
                </div>

                <div class="view-toggle">
                  <v-btn :variant="activeView === 'preview' ? 'flat' : 'outlined'" color="primary" @click="activeView = 'preview'">
                    {{ text.preview }}
                  </v-btn>
                  <v-btn :variant="activeView === 'overview' ? 'flat' : 'outlined'" color="secondary" @click="activeView = 'overview'">
                    {{ text.overview }}
                  </v-btn>
                </div>
              </div>

              <div v-if="activeView === 'preview'" class="charts-grid">
                <article v-for="chip in chips" :key="chip.index" class="chart-card">
                  <div class="chart-head">
                    <div class="chart-title">
                      <strong>{{ chip.name }}</strong>
                      <span>{{ text.visibleLines(visibleSeries(chip).length, chip.channels?.[0]?.samples?.length ?? 0) }}</span>
                    </div>

                    <v-select
                      v-model="channelSelection[chip.index]"
                      :items="channelOptions"
                      :label="text.channels"
                      style="max-width: 160px"
                    />
                  </div>

                  <div v-if="visibleSeries(chip).some((series) => series.samples.length)" class="chart-surface">
                    <div class="chart-axis chart-axis-y-top">{{ formatAxisValue(chartRange(chip).max) }}</div>
                    <div class="chart-axis chart-axis-y-bottom">{{ formatAxisValue(chartRange(chip).min) }}</div>
                    <div class="chart-axis chart-axis-x-left">{{ chartStartLabel(chipTimeline(chip), 'us') }}</div>
                    <div class="chart-axis chart-axis-x-right">{{ chartEndLabel(chipTimeline(chip), 'us') }}</div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg">
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.top"
                        :y2="chartPlot.top"
                        class="chart-boundary-line"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="chartPlot.bottom"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line chart-boundary-line-strong"
                      />
                      <line
                        :x1="chartPlot.left"
                        :x2="chartPlot.left"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-boundary-line"
                      />
                      <line
                        v-if="zeroLineY(chartRange(chip).min, chartRange(chip).max) !== null"
                        :y1="zeroLineY(chartRange(chip).min, chartRange(chip).max)"
                        :y2="zeroLineY(chartRange(chip).min, chartRange(chip).max)"
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        class="chart-guide-line"
                      />
                      <polyline
                        v-for="series in visibleSeries(chip)"
                        :key="series.index"
                        :points="linePoints(series.samples, chartRange(chip).min, chartRange(chip).max, chipTimeline(chip))"
                        fill="none"
                        :stroke="series.color"
                        stroke-width="0.72"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      />
                    </svg>
                  </div>

                  <div v-else class="chart-placeholder">
                    {{ text.noWaveform }}
                  </div>

                  <div class="legend-row">
                    <span v-for="series in visibleSeries(chip)" :key="series.index" class="legend-chip">
                      <span class="legend-swatch" :style="{ background: series.color }"></span>
                      {{ series.name }} / {{ latestChannelValue(series) }}
                    </span>
                  </div>
                </article>
              </div>

              <div v-else class="detail-grid">
                <article v-for="chip in chips" :key="chip.index" class="metric-card">
                  <div class="meta-copy">{{ chip.name }}</div>
                  <div class="value-grid">
                    <span v-for="channel in chip.channels" :key="channel.index" class="value-cell">
                      {{ channel.name }} {{ channel.hasValue ? channel.value : '--' }}
                    </span>
                  </div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">{{ text.navigationEyebrow }}</p>
                  <h2>{{ text.navigationTitle }}</h2>
                </div>
              </div>

              <div class="legend-row">
                <a
                  v-for="featurePackage in launchablePackages.filter((entry) => entry.id !== 'myiot.jndm123')"
                  :key="featurePackage.id"
                  :href="featurePackage.entryPath"
                  class="legend-chip"
                >
                  <v-icon :icon="featurePackage.icon" size="16"></v-icon>
                  {{ featurePackage.title }}
                </a>
              </div>
            </section>
          </div>
        </div>
      </v-container>
    </div>
  </v-app>
</template>
