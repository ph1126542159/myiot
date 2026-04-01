<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch, watchEffect } from 'vue'
import { useUiLocale } from './core/locale'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'
import { createUiLocaleHeaders } from './core/requestLocale.js'

const { isZh, toggleLocale } = useUiLocale()

const zh = {
  connecting: '正在连接 JNDM123 控制服务...',
  unknown: '未知',
  signOut: '退出登录',
  openLogWindow: '悬浮日志',
  openConfigCenter: '配置中心',
  language: 'EN',
  labEyebrow: 'JNDM123 实验台',
  labTitle: '时钟分频与 6 路 AD7606 采集',
  labCopy: '分频器调整遵循本地 CDCE937 I2C 测试流程。FIFO 采集沿用 `work.c` 的 packet-mode 实现，去掉控制台/文件输出后，统一喂给页面预览和 UDP 广播。',
  clockEyebrow: '时钟',
  clockTitle: 'CDCE937 分频控制',
  clockCopy: '可同时勾选多个输出一次性下发同一个分频值。点击执行后，后端会暂停采集线程、写入分频、回读结果，并在成功后恢复采集。',
  i2cDevice: 'I2C 设备',
  output: '输出选择',
  divider: '分频值',
  execute: '执行',
  refresh: '刷新',
  device: '设备',
  selectedOutput: '已选输出',
  chooseOutputs: '勾选需要一起分频的输出',
  selectedCount: (count) => `已选 ${count} 路`,
  noneSelected: '请至少选择一个输出。',
  mixedDivider: '当前选中输出的分频值不一致，输入新值后可一次性下发。',
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
  udpFixedPayload: '固定包长下，字节速率曲线与发包速率同趋势。',
  collectingUdpStats: '正在累计 UDP 每秒统计，请保持页面打开几秒钟。',
  waveformsEyebrow: '波形',
  waveformsTitle: '6 组独立图表',
  waveformsCopy: '每张图对应一个 AD7606 芯片。可选择“全部”或单独 `CH1~CH8`。预览关闭时，后端会停止为浏览器打包历史波形，但仍保留 UDP 广播；预览开启时，缓存波形每秒刷新一次。',
  rawScale: '原始值',
  voltageScale: '电压值',
  voltageEstimate: '电压视图按 +/-10V 满量程估算。',
  timeAxis: '时间轴',
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
  openLogWindow: 'Floating Logs',
  openConfigCenter: 'Config Center',
  language: '中文',
  labEyebrow: 'JNDM123 Lab',
  labTitle: 'Clock Divider and 6x AD7606 Capture',
  labCopy: 'Divider updates follow the local CDCE937 I2C test flow. FIFO capture follows the packet-mode implementation from `work.c`, removes console/file output, and feeds the UI plus UDP broadcast.',
  clockEyebrow: 'Clock',
  clockTitle: 'CDCE937 Divider Control',
  clockCopy: 'Select one or more outputs to push the same divider in one shot. The backend pauses acquisition threads, writes the divider, reads back the actual state, and restarts capture on success.',
  i2cDevice: 'I2C Device',
  output: 'Outputs',
  divider: 'Divider',
  execute: 'Execute',
  refresh: 'Refresh',
  device: 'Device',
  selectedOutput: 'Selected Outputs',
  chooseOutputs: 'Pick the outputs to update together',
  selectedCount: (count) => `${count} selected`,
  noneSelected: 'Select at least one output.',
  mixedDivider: 'Selected outputs currently use different dividers. Enter a new value to apply them together.',
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
  udpFixedPayload: 'With a fixed packet size, bytes/s naturally follows the packets/s trend.',
  collectingUdpStats: 'Collecting UDP per-second statistics. Keep this page open for a few seconds.',
  waveformsEyebrow: 'Waveforms',
  waveformsTitle: '6 Independent Charts',
  waveformsCopy: 'Each chart maps one AD7606 chip. Choose `All` or a single channel from `CH1~CH8`. When preview is not active, the backend stops packaging waveform history for the browser but keeps UDP broadcast alive. When preview is active, cached waveform data is refreshed to the browser once per second.',
  rawScale: 'Raw',
  voltageScale: 'Voltage',
  voltageEstimate: 'Voltage view assumes a +/-10V full-scale input.',
  timeAxis: 'Time Axis',
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
const chartPlot = Object.freeze({ left: 14, right: 96, top: 10, bottom: 74 })
const udpRateHistoryLimit = 30
const adcFullScaleVoltage = 10
const adcCodeFullScale = 32768

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
const selectedOutputIndices = ref([0])
const selectedDivider = ref('1')
const activeView = ref('preview')
const channelSelection = reactive({
  0: 'all',
  1: 'all',
  2: 'all',
  3: 'all',
  4: 'all',
  5: 'all'
})
const waveformValueModeByChip = reactive({
  0: 'raw',
  1: 'raw',
  2: 'raw',
  3: 'raw',
  4: 'raw',
  5: 'raw'
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
const selectedOutputs = computed(() =>
  outputs.value.filter((entry) => selectedOutputIndices.value.includes(entry.index))
)
const selectedDividerMax = computed(() => {
  if (!selectedOutputIndices.value.length) return null
  return Math.min(...selectedOutputIndices.value.map((outputIndex) => (outputIndex === 0 ? 1023 : 127)))
})
const selectedOutputSummary = computed(() =>
  selectedOutputs.value.length ? selectedOutputs.value.map((output) => output.name).join(', ') : '--'
)
const selectedOutputMetric = computed(() => {
  if (!selectedOutputs.value.length) return '--'
  return text.value.selectedCount(selectedOutputs.value.length)
})
const selectionHasMixedDividers = computed(() => {
  const dividers = selectedOutputs.value
    .map((output) => Number(output.divider))
    .filter((divider) => Number.isInteger(divider) && divider > 0)
  return dividers.length > 1 && new Set(dividers).size > 1
})

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
  const minutes = String(date.getMinutes()).padStart(2, '0')
  const seconds = String(date.getSeconds()).padStart(2, '0')
  return `${minutes}:${seconds}`
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

function rawToVoltage(rawValue) {
  const numeric = Number(rawValue)
  if (!Number.isFinite(numeric)) return 0
  return (numeric / adcCodeFullScale) * adcFullScaleVoltage
}

function waveformModeForChip(chipIndex) {
  return waveformValueModeByChip[chipIndex] === 'voltage' ? 'voltage' : 'raw'
}

function waveformDisplayValue(rawValue, mode = 'raw') {
  return mode === 'voltage' ? rawToVoltage(rawValue) : Number(rawValue)
}

function formatVoltageValue(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '--'

  const abs = Math.abs(numeric)
  if (abs >= 1) return `${formatDecimal(numeric, 3)} V`
  if (abs >= 0.001) return `${formatDecimal(numeric * 1000, 1)} mV`
  return `${formatDecimal(numeric * 1000000, 0)} uV`
}

function formatWaveformAxisValue(value, mode = 'raw') {
  return mode === 'voltage' ? formatVoltageValue(value) : formatInteger(value)
}

function formatWaveformLegendValue(rawValue, mode = 'raw') {
  return mode === 'voltage'
    ? formatVoltageValue(rawToVoltage(rawValue))
    : formatInteger(rawValue)
}

function formatPacketAxisValue(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric) || numeric === 0) return '0'

  const exponent = Math.floor(Math.log10(Math.abs(numeric)))
  const mantissa = numeric / (10 ** exponent)
  return `${formatDecimal(mantissa, 1)}x10^${exponent}`
}

function formatAxisValue(value, formatter = formatInteger) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '--'
  return formatter(numeric)
}

function syncSelectedOutputs() {
  const availableIndexes = new Set(outputs.value.map((output) => output.index))
  const normalized = selectedOutputIndices.value
    .filter((outputIndex) => availableIndexes.has(outputIndex))
    .sort((left, right) => left - right)

  const unique = [...new Set(normalized)]
  if (!unique.length && outputs.value.length) {
    unique.push(outputs.value[0].index)
  }

  selectedOutputIndices.value = unique
}

function syncSelectedDivider() {
  if (!selectedOutputs.value.length) return

  const dividers = selectedOutputs.value
    .map((output) => Number(output.divider))
    .filter((divider) => Number.isInteger(divider) && divider > 0)

  if (!dividers.length) return

  const uniqueDividers = [...new Set(dividers)]
  if (uniqueDividers.length === 1) {
    selectedDivider.value = String(uniqueDividers[0])
  } else {
    selectedDivider.value = ''
  }
}

function parseSelectedDivider() {
  if (!selectedOutputIndices.value.length || !selectedDividerMax.value) return null
  const divider = Number.parseInt(String(selectedDivider.value ?? '').trim(), 10)
  if (!Number.isInteger(divider)) return null
  if (divider < 1 || divider > selectedDividerMax.value) return null
  return divider
}

function toggleOutputSelection(outputIndex, enabled) {
  if (enabled) {
    selectedOutputIndices.value = [...new Set([...selectedOutputIndices.value, outputIndex])].sort((left, right) => left - right)
    return
  }

  selectedOutputIndices.value = selectedOutputIndices.value.filter((value) => value !== outputIndex)
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
    syncSelectedOutputs()
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
  if (!selectedOutputIndices.value.length) {
    banner.value = {
      type: 'error',
      text: text.value.noneSelected
    }
    return
  }

  const divider = parseSelectedDivider()
  if (divider === null) {
    banner.value = {
      type: 'error',
      text: `${text.value.divider} must be in 1..${selectedDividerMax.value}.`
    }
    return
  }

  dividerBusy.value = true
  banner.value = {
    type: 'info',
    text: text.value.applyingDivider(divider, selectedOutputSummary.value)
  }

  try {
    const body = new URLSearchParams()
    body.set('devicePath', devicePath.value)
    body.set('outputIndices', selectedOutputIndices.value.join(','))
    body.set('divider', String(divider))
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

function chartRange(chip, mode = waveformModeForChip(chip?.index)) {
  const values = visibleSeries(chip).flatMap((series) =>
    series.samples.map((sample) => waveformDisplayValue(sample, mode))
  )
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

function chartXTicks(timeline, unit = 'us', segments = 4) {
  const start = Number(timeline?.[0])
  const end = Number(timeline?.[timeline.length - 1])

  return Array.from({ length: segments + 1 }, (_, index) => {
    const ratio = segments === 0 ? 0 : index / segments
    const position = chartPlot.left + (ratio * (chartPlot.right - chartPlot.left))
    const tickValue = Number.isFinite(start) && Number.isFinite(end)
      ? start + ((end - start) * ratio)
      : NaN

    return {
      key: `${unit}-${index}-${start}-${end}`,
      position,
      label: Number.isFinite(tickValue) ? formatTimelineLabel(tickValue, unit) : '--'
    }
  })
}

function chartYTicks(range, formatter = formatAxisValue, segments = 4) {
  const min = Number(range?.min)
  const max = Number(range?.max)

  return Array.from({ length: segments + 1 }, (_, index) => {
    const ratio = segments === 0 ? 0 : index / segments
    const position = chartPlot.top + (ratio * (chartPlot.bottom - chartPlot.top))
    const value = Number.isFinite(min) && Number.isFinite(max)
      ? max - ((max - min) * ratio)
      : NaN

    return {
      key: `${index}-${min}-${max}`,
      position,
      label: Number.isFinite(value) ? formatter(value) : '--'
    }
  })
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

function linePoints(samples, minValue, maxValue, timeline = [], projector = (value) => value) {
  if (!samples.length) return ''

  return samples.map((sample, index) => {
    const x = resolveX(index, samples.length, timeline)
    const y = normalizeY(projector(sample), minValue, maxValue)
    return `${x.toFixed(2)},${y.toFixed(2)}`
  }).join(' ')
}

function areaPoints(samples, minValue, maxValue, timeline = [], projector = (value) => value) {
  if (!samples.length) return ''

  const line = samples.map((sample, index) => {
    const x = resolveX(index, samples.length, timeline)
    const y = normalizeY(projector(sample), minValue, maxValue)
    return `${x.toFixed(2)},${y.toFixed(2)}`
  })

  const startX = resolveX(0, samples.length, timeline)
  const endX = resolveX(samples.length - 1, samples.length, timeline)

  return [
    `${startX.toFixed(2)},${chartPlot.bottom.toFixed(2)}`,
    ...line,
    `${endX.toFixed(2)},${chartPlot.bottom.toFixed(2)}`
  ].join(' ')
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

function latestChannelValue(series, mode = 'raw') {
  if (series.samples?.length) return formatWaveformLegendValue(series.samples[series.samples.length - 1], mode)
  if (series.hasValue) return formatWaveformLegendValue(series.value, mode)
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

function openPopupWindow(url, name, width = 1180, height = 820) {
  const left = Math.max(Math.round((window.screen.width - width) / 2), 32)
  const top = Math.max(Math.round((window.screen.height - height) / 2), 32)
  const features = [
    'popup=yes',
    'resizable=yes',
    'scrollbars=yes',
    'toolbar=no',
    'menubar=no',
    'location=no',
    'status=no',
    `width=${width}`,
    `height=${height}`,
    `left=${left}`,
    `top=${top}`,
  ].join(',')

  return window.open(url, name, features)
}

function openLogWindow() {
  const baseTarget = '/myiot/logs/index.html'
  const popup = openPopupWindow(`${baseTarget}?popup=1`, 'myiot-log-viewer')
  if (!popup) {
    window.open(baseTarget, '_blank', 'noopener,noreferrer')
  }
}

function openConfigCenter() {
  window.open('/myiot/config/index.html', '_blank', 'noopener,noreferrer')
}

watch(activeView, () => {
  resetPolling()
  loadAcquisitionSnapshot()
})

watch(selectedOutputIndices, () => {
  syncSelectedDivider()
}, { deep: true })

watch(outputs, () => {
  syncSelectedOutputs()
  syncSelectedDivider()
}, { deep: true })

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
              <v-btn variant="outlined" color="info" size="small" @click="openLogWindow">
                {{ text.openLogWindow }}
              </v-btn>
              <v-btn variant="outlined" color="secondary" size="small" @click="openConfigCenter">
                {{ text.openConfigCenter }}
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

                <v-text-field
                  v-model="selectedDivider"
                  :label="text.divider"
                  prepend-inner-icon="mdi-tune-variant"
                  type="number"
                  min="1"
                  :max="selectedDividerMax"
                  :hint="selectedDividerMax ? `1..${selectedDividerMax}` : text.noneSelected"
                  persistent-hint
                />

                <div class="panel-actions">
                  <v-btn color="primary" block :loading="dividerBusy" :disabled="dividerBusy || !outputs.length || !selectedOutputIndices.length" @click="applyDivider">
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
                  <div class="metric-value">{{ selectedOutputMetric }}</div>
                  <div class="meta-copy small">{{ selectedOutputSummary }}</div>
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

              <div class="meta-copy small mt-5">{{ text.chooseOutputs }}</div>
              <div v-if="selectionHasMixedDividers" class="meta-copy small mt-2">{{ text.mixedDivider }}</div>

              <div class="outputs-grid">
                <article
                  v-for="output in outputs"
                  :key="output.index"
                  class="output-card"
                  :class="{ active: selectedOutputIndices.includes(output.index) }"
                  @click="toggleOutputSelection(output.index, !selectedOutputIndices.includes(output.index))"
                >
                  <div class="d-flex align-center justify-space-between ga-3">
                    <div class="meta-copy">{{ output.name }} / {{ output.pdiv }}</div>
                    <v-checkbox-btn
                      :model-value="selectedOutputIndices.includes(output.index)"
                      color="primary"
                      density="compact"
                      @click.stop
                      @update:model-value="toggleOutputSelection(output.index, $event)"
                    />
                  </div>
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

                  <div v-if="udpPacketSeries.length" class="chart-surface chart-surface-cool compact-chart-surface">
                    <div
                      v-for="tick in chartYTicks(udpPacketRange, formatPacketAxisValue)"
                      :key="`udp-packet-y-${tick.key}`"
                      class="chart-axis chart-axis-y-tick"
                      :style="{ top: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>
                    <div
                      v-for="tick in chartXTicks(udpTimelineMs, 'ms')"
                      :key="`udp-packet-x-${tick.key}`"
                      class="chart-axis chart-axis-x-tick"
                      :style="{ left: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg compact-chart-svg">
                      <line
                        v-for="tick in chartYTicks(udpPacketRange, formatPacketAxisValue)"
                        :key="`udp-packet-grid-y-${tick.key}`"
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="tick.position"
                        :y2="tick.position"
                        class="chart-grid-line"
                      />
                      <line
                        v-for="tick in chartXTicks(udpTimelineMs, 'ms')"
                        :key="`udp-packet-grid-x-${tick.key}`"
                        :x1="tick.position"
                        :x2="tick.position"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-grid-line chart-grid-line-vertical"
                      />
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
                        stroke-width="0.92"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        class="chart-signal-line chart-signal-line-packet"
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
                      <span class="chart-note">{{ text.udpFixedPayload }}</span>
                    </div>
                  </div>

                  <div v-if="udpByteSeries.length" class="chart-surface chart-surface-warm compact-chart-surface">
                    <div
                      v-for="tick in chartYTicks(udpByteRange, formatBytes)"
                      :key="`udp-byte-y-${tick.key}`"
                      class="chart-axis chart-axis-y-tick"
                      :style="{ top: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>
                    <div
                      v-for="tick in chartXTicks(udpTimelineMs, 'ms')"
                      :key="`udp-byte-x-${tick.key}`"
                      class="chart-axis chart-axis-x-tick"
                      :style="{ left: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg compact-chart-svg">
                      <defs>
                        <linearGradient id="udp-byte-fill" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="0%" stop-color="#f7a94a" stop-opacity="0.44" />
                          <stop offset="72%" stop-color="#f7a94a" stop-opacity="0.14" />
                          <stop offset="100%" stop-color="#f7a94a" stop-opacity="0.03" />
                        </linearGradient>
                      </defs>
                      <line
                        v-for="tick in chartYTicks(udpByteRange, formatBytes)"
                        :key="`udp-byte-grid-y-${tick.key}`"
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="tick.position"
                        :y2="tick.position"
                        class="chart-grid-line"
                      />
                      <line
                        v-for="tick in chartXTicks(udpTimelineMs, 'ms')"
                        :key="`udp-byte-grid-x-${tick.key}`"
                        :x1="tick.position"
                        :x2="tick.position"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-grid-line chart-grid-line-vertical"
                      />
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
                      <polygon
                        :points="areaPoints(udpByteSeries, udpByteRange.min, udpByteRange.max, udpTimelineMs)"
                        fill="url(#udp-byte-fill)"
                        class="chart-signal-area"
                      />
                      <polyline
                        :points="linePoints(udpByteSeries, udpByteRange.min, udpByteRange.max, udpTimelineMs)"
                        fill="none"
                        stroke="#f7a94a"
                        stroke-width="1.02"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        class="chart-signal-line chart-signal-line-byte"
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
                      <span class="chart-mode-chip">{{ waveformModeForChip(chip.index) === 'raw' ? text.rawScale : text.voltageScale }} / {{ text.timeAxis }}</span>
                      <span v-if="waveformModeForChip(chip.index) === 'voltage'" class="chart-note">{{ text.voltageEstimate }}</span>
                    </div>

                    <div class="chart-head-actions">
                      <div class="axis-toggle axis-toggle-compact">
                        <v-btn :variant="waveformModeForChip(chip.index) === 'raw' ? 'flat' : 'outlined'" color="primary" size="small" @click="waveformValueModeByChip[chip.index] = 'raw'">
                          {{ text.rawScale }}
                        </v-btn>
                        <v-btn :variant="waveformModeForChip(chip.index) === 'voltage' ? 'flat' : 'outlined'" color="secondary" size="small" @click="waveformValueModeByChip[chip.index] = 'voltage'">
                          {{ text.voltageScale }}
                        </v-btn>
                      </div>

                      <v-select
                        v-model="channelSelection[chip.index]"
                        :items="channelOptions"
                        :label="text.channels"
                        style="max-width: 160px"
                      />
                    </div>
                  </div>

                  <div v-if="visibleSeries(chip).some((series) => series.samples.length)" class="chart-surface">
                    <div
                      v-for="tick in chartYTicks(chartRange(chip, waveformModeForChip(chip.index)), (value) => formatWaveformAxisValue(value, waveformModeForChip(chip.index)))"
                      :key="`wave-y-${chip.index}-${tick.key}`"
                      class="chart-axis chart-axis-y-tick"
                      :style="{ top: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>
                    <div
                      v-for="tick in chartXTicks(chipTimeline(chip), 'us')"
                      :key="`wave-x-${chip.index}-${tick.key}`"
                      class="chart-axis chart-axis-x-tick"
                      :style="{ left: `${tick.position}%` }"
                    >
                      {{ tick.label }}
                    </div>

                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg">
                      <line
                        v-for="tick in chartYTicks(chartRange(chip, waveformModeForChip(chip.index)), (value) => formatWaveformAxisValue(value, waveformModeForChip(chip.index)))"
                        :key="`wave-grid-y-${chip.index}-${tick.key}`"
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        :y1="tick.position"
                        :y2="tick.position"
                        class="chart-grid-line"
                      />
                      <line
                        v-for="tick in chartXTicks(chipTimeline(chip), 'us')"
                        :key="`wave-grid-x-${chip.index}-${tick.key}`"
                        :x1="tick.position"
                        :x2="tick.position"
                        :y1="chartPlot.top"
                        :y2="chartPlot.bottom"
                        class="chart-grid-line chart-grid-line-vertical"
                      />
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
                        v-if="zeroLineY(chartRange(chip, waveformModeForChip(chip.index)).min, chartRange(chip, waveformModeForChip(chip.index)).max) !== null"
                        :y1="zeroLineY(chartRange(chip, waveformModeForChip(chip.index)).min, chartRange(chip, waveformModeForChip(chip.index)).max)"
                        :y2="zeroLineY(chartRange(chip, waveformModeForChip(chip.index)).min, chartRange(chip, waveformModeForChip(chip.index)).max)"
                        :x1="chartPlot.left"
                        :x2="chartPlot.right"
                        class="chart-guide-line"
                      />
                      <polyline
                        v-for="series in visibleSeries(chip)"
                        :key="series.index"
                        :points="linePoints(series.samples, chartRange(chip, waveformModeForChip(chip.index)).min, chartRange(chip, waveformModeForChip(chip.index)).max, chipTimeline(chip), (sample) => waveformDisplayValue(sample, waveformModeForChip(chip.index)))"
                        fill="none"
                        :stroke="series.color"
                        stroke-width="0.86"
                        vector-effect="non-scaling-stroke"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        class="chart-signal-line"
                      />
                    </svg>
                  </div>

                  <div v-else class="chart-placeholder">
                    {{ text.noWaveform }}
                  </div>

                  <div class="legend-row">
                    <span v-for="series in visibleSeries(chip)" :key="series.index" class="legend-chip">
                      <span class="legend-swatch" :style="{ background: series.color }"></span>
                      {{ series.name }} / {{ latestChannelValue(series, waveformModeForChip(chip.index)) }}
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

          </div>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.waveform-toolbar {
  display: flex;
  flex-wrap: wrap;
  justify-content: flex-end;
  align-items: center;
  gap: 12px;
}

.axis-toggle,
.view-toggle {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.axis-toggle-compact {
  justify-content: flex-end;
}

.chart-head-actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
}

.waveform-note {
  padding: 8px 12px;
  border: 1px solid rgba(91, 141, 239, 0.18);
  border-radius: 999px;
  background: linear-gradient(135deg, rgba(12, 31, 56, 0.9), rgba(20, 45, 74, 0.72));
  color: rgba(227, 237, 255, 0.82);
}

.chart-card {
  border: 1px solid rgba(104, 146, 227, 0.18);
  border-radius: 24px;
  background:
    linear-gradient(180deg, rgba(15, 26, 45, 0.96), rgba(9, 17, 31, 0.98)),
    radial-gradient(circle at top right, rgba(72, 165, 255, 0.14), transparent 42%);
  box-shadow:
    0 24px 50px rgba(5, 12, 23, 0.38),
    inset 0 1px 0 rgba(255, 255, 255, 0.04);
  overflow: hidden;
}

.compact-chart-card {
  min-height: 360px;
}

.chart-head {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  padding: 18px 20px 0;
}

.compact-chart-head {
  padding-bottom: 6px;
}

.chart-title {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.chart-title strong {
  font-size: 1.05rem;
  letter-spacing: 0.02em;
}

.chart-title span {
  color: rgba(226, 234, 248, 0.7);
  font-size: 0.88rem;
}

.chart-note {
  color: rgba(185, 202, 233, 0.66);
  font-size: 0.8rem;
  max-width: 38ch;
}

.chart-mode-chip {
  display: inline-flex;
  align-items: center;
  width: fit-content;
  padding: 4px 10px;
  border-radius: 999px;
  background: rgba(61, 122, 221, 0.14);
  border: 1px solid rgba(92, 153, 255, 0.24);
  color: rgba(223, 232, 252, 0.92);
}

.chart-surface {
  position: relative;
  min-height: 335px;
  margin: 14px 20px 10px;
  border-radius: 20px;
  background:
    linear-gradient(180deg, rgba(7, 14, 28, 0.98), rgba(11, 21, 39, 0.94)),
    radial-gradient(circle at top, rgba(95, 183, 255, 0.08), transparent 44%);
  border: 1px solid rgba(96, 132, 194, 0.18);
  overflow: hidden;
}

.chart-surface-cool {
  background:
    linear-gradient(180deg, rgba(7, 18, 30, 0.98), rgba(10, 27, 37, 0.95)),
    radial-gradient(circle at top, rgba(95, 255, 183, 0.08), transparent 46%);
}

.chart-surface-warm {
  background:
    linear-gradient(180deg, rgba(26, 14, 7, 0.98), rgba(36, 22, 11, 0.95)),
    radial-gradient(circle at top, rgba(255, 194, 107, 0.12), transparent 46%);
}

.compact-chart-surface {
  min-height: 260px;
}

.chart-surface::before {
  content: "";
  position: absolute;
  inset: 0;
  background:
    linear-gradient(180deg, rgba(255, 255, 255, 0.03), transparent 30%),
    radial-gradient(circle at 15% 12%, rgba(115, 229, 211, 0.08), transparent 24%);
  pointer-events: none;
}

.chart-svg {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
}

.compact-chart-svg {
  opacity: 0.96;
}

.chart-axis {
  position: absolute;
  z-index: 2;
  color: rgba(212, 223, 243, 0.74);
  font-size: 0.76rem;
  line-height: 1;
  letter-spacing: 0.02em;
  pointer-events: none;
}

.chart-axis-y-tick {
  left: 10px;
  transform: translateY(-50%);
}

.chart-axis-x-tick {
  bottom: 10px;
  transform: translateX(-50%);
  white-space: nowrap;
}

.chart-grid-line {
  stroke: rgba(163, 188, 232, 0.18);
  stroke-width: 0.32;
  stroke-dasharray: 1.2 1.8;
}

.chart-grid-line-vertical {
  stroke: rgba(115, 149, 208, 0.14);
}

.chart-boundary-line {
  stroke: rgba(196, 215, 248, 0.22);
  stroke-width: 0.38;
}

.chart-boundary-line-strong {
  stroke: rgba(222, 234, 255, 0.42);
}

.chart-guide-line {
  stroke: rgba(255, 193, 115, 0.38);
  stroke-width: 0.34;
  stroke-dasharray: 1.6 1.6;
}

.chart-signal-line {
  filter: drop-shadow(0 0 6px rgba(113, 209, 255, 0.22));
}

.chart-signal-line-packet {
  filter: drop-shadow(0 0 8px rgba(140, 242, 178, 0.24));
}

.chart-signal-line-byte {
  filter: drop-shadow(0 0 10px rgba(247, 169, 74, 0.28));
}

.chart-signal-area {
  opacity: 0.96;
}

.chart-placeholder {
  display: grid;
  place-items: center;
  min-height: 335px;
  padding: 32px;
  text-align: center;
  color: rgba(214, 225, 245, 0.68);
}

.compact-chart-placeholder {
  min-height: 260px;
}

.legend-row {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  padding: 0 20px 20px;
}

.legend-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  border-radius: 999px;
  border: 1px solid rgba(102, 140, 209, 0.18);
  background: rgba(14, 24, 42, 0.7);
  color: rgba(230, 238, 255, 0.85);
  text-decoration: none;
}

.legend-swatch {
  width: 10px;
  height: 10px;
  border-radius: 999px;
  box-shadow: 0 0 10px currentColor;
}

@media (max-width: 960px) {
  .waveform-toolbar {
    justify-content: flex-start;
  }

  .chart-head-actions {
    justify-content: flex-start;
  }

  .chart-surface {
    min-height: 280px;
    margin-left: 14px;
    margin-right: 14px;
  }

  .compact-chart-surface,
  .compact-chart-placeholder {
    min-height: 240px;
  }

  .chart-placeholder {
    min-height: 280px;
  }
}
</style>
