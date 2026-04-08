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
  labCopy: '分频器调整遵循本地 CDCE937 I2C 测试流程。FIFO 采集沿用 `work.c` 的 packet-mode 实现，去掉控制台/文件输出后，统一喂给页面预览。',
  clockEyebrow: '时钟',
  clockTitle: 'CDCE937 分频控制',
  clockCopy: '可同时勾选多个输出一次性下发同一个分频值。点击执行后，后端会暂停采集线程、写入分频、回读结果，并在成功后恢复采集。',
  i2cDevice: 'I2C 设备',
  referenceClockHz: '外部时钟 Hz',
  referenceClockHint: '例如 25000000 表示 25 MHz。',
  output: '输出选择',
  divider: '分频值',
  execute: '执行',
  saveClock: '保存时钟',
  refresh: '刷新',
  device: '设备',
  referenceClock: '外部时钟',
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
  acquisitionCopy: '读取线程只负责搬运整帧数据进入 `Poco::NotificationQueue`，波形缓存交给消费侧处理。浏览器预览按 100ms 刷新缓存快照。',
  runtimeChartsTitle: '实时性能',
  runtimeChartsCopy: '基于运行态快照的累计帧数差值，按秒估算当前帧率和浏览器看到的数据吞吐。',
  start: '启动',
  stop: '停止',
  restartProcess: '重启采集进程',
  waveformsEyebrow: '波形',
  waveformsTitle: '6 组独立图表',
  waveformsCopy: '每张图对应一个 AD7606 芯片。可选择“全部”或单独 `CH1~CH8`。采集线程将整帧数据先放入队列，再由发送线程按 100ms 节奏整理并通过 DDS 推送给页面。',
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
  frameRate: '每秒帧率',
  throughputMBps: '每秒吞吐',
  lastFrame: (value) => `最近一帧 ${value}`,
  noFramesYet: '还没有帧数据',
  recoveries: '恢复次数',
  noRecoveryWarning: '暂无恢复告警',
  queue: '队列',
  statPerSecond: '最近 60 秒',
  statUpdatedAt: (value) => `更新于 ${value}`,
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
  savingClockConfig: '正在保存外部时钟配置...',
  clockConfigSaved: '外部时钟配置已保存。',
  unableSaveClockConfig: '无法保存外部时钟配置。',
  startingCapture: '正在启动 AD7606 采集...',
  stoppingCapture: '正在停止 AD7606 采集...',
  restartingCaptureProcess: '正在重启采集进程...',
  captureStarted: '采集已启动。',
  captureStopped: '采集已停止。',
  captureProcessRestarted: '采集进程已重启。',
  captureControlFailed: '采集控制失败。',
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
  labCopy: 'Divider updates follow the local CDCE937 I2C test flow. FIFO capture follows the packet-mode implementation from `work.c`, removes console/file output, and feeds the UI preview.',
  clockEyebrow: 'Clock',
  clockTitle: 'CDCE937 Divider Control',
  clockCopy: 'Select one or more outputs to push the same divider in one shot. The backend pauses acquisition threads, writes the divider, reads back the actual state, and restarts capture on success.',
  i2cDevice: 'I2C Device',
  referenceClockHz: 'External Clock Hz',
  referenceClockHint: 'For example, 25000000 means 25 MHz.',
  output: 'Outputs',
  divider: 'Divider',
  execute: 'Execute',
  saveClock: 'Save Clock',
  refresh: 'Refresh',
  device: 'Device',
  referenceClock: 'Reference Clock',
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
  acquisitionCopy: 'The reader thread stays lean, pushes full 96-byte frames into `Poco::NotificationQueue`, and a sender thread batches cached waveform data into 100 ms DDS updates for the browser.',
  runtimeChartsTitle: 'Live Performance',
  runtimeChartsCopy: 'Based on cumulative frame deltas from runtime snapshots, the UI estimates current FPS and browser-visible throughput once per second.',
  start: 'Start',
  stop: 'Stop',
  restartProcess: 'Restart Agent',
  waveformsEyebrow: 'Waveforms',
  waveformsTitle: '6 Independent Charts',
  waveformsCopy: 'Each chart maps one AD7606 chip. Choose `All` or a single channel from `CH1~CH8`. The reader thread queues each 96-byte frame immediately, and a sender thread publishes cached waveform data to the browser over DDS every 100 ms.',
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
  frameRate: 'FPS',
  throughputMBps: 'Throughput',
  lastFrame: (value) => `Last frame ${value}`,
  noFramesYet: 'No frames yet',
  recoveries: 'Recoveries',
  noRecoveryWarning: 'No recovery warning',
  queue: 'Queue',
  statPerSecond: 'Last 60 seconds',
  statUpdatedAt: (value) => `Updated ${value}`,
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
  savingClockConfig: 'Saving external reference clock...',
  clockConfigSaved: 'External reference clock saved.',
  unableSaveClockConfig: 'Unable to save external reference clock.',
  startingCapture: 'Starting AD7606 capture...',
  stoppingCapture: 'Stopping AD7606 capture...',
  restartingCaptureProcess: 'Restarting the acquisition process...',
  captureStarted: 'Capture started.',
  captureStopped: 'Capture stopped.',
  captureProcessRestarted: 'Acquisition process restarted.',
  captureControlFailed: 'Capture control failed.',
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
const adcFullScaleVoltage = 10
const adcCodeFullScale = 32768
const framePayloadBytes = 96
const performanceHistoryLimit = 60

const banner = ref({
  type: 'info',
  text: text.value.connecting
})
const devicePath = ref('/dev/i2c-0')
const dividerBusy = ref(false)
const clockBusy = ref(false)
const acquisitionBusy = ref(false)
const pollInFlight = ref(false)
const dividerStatus = ref({ outputs: [] })
const acquisitionState = ref({ chips: [] })
const frameRateHistory = ref([])
const throughputHistory = ref([])
const previousPerformanceSnapshot = ref(null)
const selectedOutputIndices = ref([0])
const selectedDivider = ref('1')
const referenceClockHz = ref('')
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

let pollTimer = null
let debugSnapshotCount = 0

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

const runtimeCharts = computed(() => [
  {
    key: 'fps',
    label: text.value.frameRate,
    value: frameRateHistory.value.length
      ? `${formatDecimal(frameRateHistory.value[frameRateHistory.value.length - 1].value, 1)} fps`
      : '0 fps',
    helper: frameRateHistory.value.length
      ? text.value.statUpdatedAt(formatDateTime(frameRateHistory.value[frameRateHistory.value.length - 1].timestampIso))
      : text.value.statPerSecond,
    history: frameRateHistory.value,
    color: '#57d2ff',
    formatter: (value) => `${formatDecimal(value, 1)}`
  },
  {
    key: 'mbps',
    label: text.value.throughputMBps,
    value: throughputHistory.value.length
      ? `${formatDecimal(throughputHistory.value[throughputHistory.value.length - 1].value, 3)} MB/s`
      : '0 MB/s',
    helper: throughputHistory.value.length
      ? text.value.statUpdatedAt(formatDateTime(throughputHistory.value[throughputHistory.value.length - 1].timestampIso))
      : text.value.statPerSecond,
    history: throughputHistory.value,
    color: '#8cf2b2',
    formatter: (value) => formatDecimal(value, 3)
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

function appendPerformancePoint(targetRef, point) {
  const next = [...targetRef.value, point]
  if (next.length > performanceHistoryLimit) {
    next.splice(0, next.length - performanceHistoryLimit)
  }
  targetRef.value = next
}

function resetPerformanceHistory() {
  frameRateHistory.value = []
  throughputHistory.value = []
  previousPerformanceSnapshot.value = null
}

function updatePerformanceHistory(payload) {
  const totalFrames = Number(payload?.totalFrames)
  const updatedAtMs = Date.parse(payload?.updatedAt ?? '')
  if (!Number.isFinite(totalFrames) || !Number.isFinite(updatedAtMs)) {
    return
  }

  const previous = previousPerformanceSnapshot.value
  if (previous && totalFrames < previous.totalFrames) {
    resetPerformanceHistory()
  }

  const effectivePrevious = previousPerformanceSnapshot.value
  if (effectivePrevious) {
    const deltaFrames = Math.max(0, totalFrames - effectivePrevious.totalFrames)
    const deltaSeconds = Math.max((updatedAtMs - effectivePrevious.updatedAtMs) / 1000, 0)
    if (deltaSeconds >= 0.1) {
      const fps = deltaFrames / deltaSeconds
      const throughputMbps = (deltaFrames * framePayloadBytes) / deltaSeconds / 1000 / 1000
      const point = {
        timestampMs: updatedAtMs,
        timestampIso: payload.updatedAt,
        fps,
        throughputMbps
      }
      appendPerformancePoint(frameRateHistory, {
        timestampMs: point.timestampMs,
        timestampIso: point.timestampIso,
        value: point.fps
      })
      appendPerformancePoint(throughputHistory, {
        timestampMs: point.timestampMs,
        timestampIso: point.timestampIso,
        value: point.throughputMbps
      })
    }
  }

  previousPerformanceSnapshot.value = {
    totalFrames,
    updatedAtMs
  }
}

function metricTimeline(history) {
  return history.map((point) => point.timestampMs)
}

function metricValues(history) {
  return history.map((point) => Number(point.value) || 0)
}

function metricRange(history) {
  const values = metricValues(history)
  if (!values.length) return { min: 0, max: 1 }

  const min = Math.min(...values)
  const max = Math.max(...values)
  if (min === max) {
    return { min: Math.max(0, min - 1), max: max + 1 }
  }

  const padding = Math.max((max - min) * 0.15, 0.1)
  return { min: Math.max(0, min - padding), max: max + padding }
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

function formatRelativeWaveformLabel(value, start) {
  const current = Number(value)
  const origin = Number(start)
  if (!Number.isFinite(current) || !Number.isFinite(origin)) return '--'

  const deltaMs = (current - origin) / 1000
  if (Math.abs(deltaMs) >= 1000) return `${formatDecimal(deltaMs / 1000, 3)} s`
  return `${formatDecimal(deltaMs, 1)} ms`
}

function normalizeSignedSample(rawValue) {
  const numeric = Number(rawValue)
  if (!Number.isFinite(numeric)) return 0
  return Math.max(-adcCodeFullScale, Math.min(adcCodeFullScale - 1, numeric))
}

function rawToVoltage(rawValue) {
  return (normalizeSignedSample(rawValue) / adcCodeFullScale) * adcFullScaleVoltage
}

function waveformModeForChip(chipIndex) {
  return waveformValueModeByChip[chipIndex] === 'voltage' ? 'voltage' : 'raw'
}

function waveformDisplayValue(rawValue, mode = 'raw') {
  return mode === 'voltage' ? rawToVoltage(rawValue) : normalizeSignedSample(rawValue)
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

function parseReferenceClockHz() {
  const value = Number.parseInt(String(referenceClockHz.value ?? '').trim(), 10)
  if (!Number.isInteger(value) || value <= 0 || value > 1000000000) return null
  return value
}

function toggleOutputSelection(outputIndex, enabled) {
  if (enabled) {
    selectedOutputIndices.value = [...new Set([...selectedOutputIndices.value, outputIndex])].sort((left, right) => left - right)
    return
  }

  selectedOutputIndices.value = selectedOutputIndices.value.filter((value) => value !== outputIndex)
}

function syncAcquisitionState(payload) {
  debugSnapshotCount += 1
  if (payload?.debug && (debugSnapshotCount <= 5 || debugSnapshotCount % 20 === 0)) {
    const chip0 = Array.isArray(payload?.chips) ? payload.chips[0] : null
    const channelValues = Array.isArray(chip0?.channels)
      ? chip0.channels.slice(0, 4).map((channel) => Number(channel?.value ?? 0))
      : []
    console.info('[JNDM123 debug] snapshot', {
      count: debugSnapshotCount,
      debug: payload.debug,
      chip0Values: channelValues
    })
  }
  updatePerformanceHistory(payload)
  acquisitionState.value = payload
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

async function loadDividerStatus(useCurrentDevicePath = true) {
  try {
    const query = useCurrentDevicePath
      ? `?devicePath=${encodeURIComponent(devicePath.value)}`
      : ''
    const payload = await requestJson(`/myiot/jndm123/divider.json${query}`)
    dividerStatus.value = payload
    if (payload?.devicePath) {
      devicePath.value = payload.devicePath
    }
    if (payload?.referenceClockHz) {
      referenceClockHz.value = String(payload.referenceClockHz)
    }
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

  const parsedReferenceClockHz = parseReferenceClockHz()
  if (parsedReferenceClockHz === null) {
    banner.value = {
      type: 'error',
      text: `${text.value.referenceClockHz} must be in 1..1000000000.`
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
    body.set('referenceClockHz', String(parsedReferenceClockHz))
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

async function saveReferenceClock() {
  const parsedReferenceClockHz = parseReferenceClockHz()
  if (parsedReferenceClockHz === null) {
    banner.value = {
      type: 'error',
      text: `${text.value.referenceClockHz} must be in 1..1000000000.`
    }
    return
  }

  clockBusy.value = true
  banner.value = {
    type: 'info',
    text: text.value.savingClockConfig
  }

  try {
    const body = new URLSearchParams()
    body.set('devicePath', devicePath.value)
    body.set('referenceClockHz', String(parsedReferenceClockHz))
    const payload = await requestJson('/myiot/jndm123/divider.json', {
      method: 'POST',
      body
    })

    dividerStatus.value = payload
    if (payload?.devicePath) {
      devicePath.value = payload.devicePath
    }
    if (payload?.referenceClockHz) {
      referenceClockHz.value = String(payload.referenceClockHz)
    }
    syncSelectedOutputs()
    syncSelectedDivider()

    banner.value = {
      type: 'success',
      text: payload.message || text.value.clockConfigSaved
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || text.value.unableSaveClockConfig
    }
  } finally {
    clockBusy.value = false
  }
}

async function setAcquisition(action) {
  acquisitionBusy.value = true
  banner.value = {
    type: 'info',
    text:
      action === 'start'
        ? text.value.startingCapture
        : action === 'restart-process'
          ? text.value.restartingCaptureProcess
          : text.value.stoppingCapture
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
      text:
        payload.message ||
        (action === 'start'
          ? text.value.captureStarted
          : action === 'restart-process'
            ? text.value.captureProcessRestarted
            : text.value.captureStopped)
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
      label: Number.isFinite(tickValue)
        ? (unit === 'us' ? formatRelativeWaveformLabel(tickValue, start) : formatTimelineLabel(tickValue, unit))
        : '--'
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
  if (!timeline.length) return '--'
  return unit === 'us'
    ? formatRelativeWaveformLabel(timeline[0], timeline[0])
    : formatTimelineLabel(timeline[0], unit)
}

function chartEndLabel(timeline, unit = 'us') {
  if (!timeline.length) return '--'
  return unit === 'us'
    ? formatRelativeWaveformLabel(timeline[timeline.length - 1], timeline[0])
    : formatTimelineLabel(timeline[timeline.length - 1], unit)
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
  }, previewEnabled.value ? 100 : 400)
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

  await loadDividerStatus(false)
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

                <v-text-field
                  v-model="referenceClockHz"
                  :label="text.referenceClockHz"
                  prepend-inner-icon="mdi-sine-wave"
                  type="number"
                  min="1"
                  max="1000000000"
                  :hint="text.referenceClockHint"
                  persistent-hint
                />

                <div class="panel-actions">
                  <v-btn color="primary" block :loading="dividerBusy" :disabled="dividerBusy || !outputs.length || !selectedOutputIndices.length" @click="applyDivider">
                    {{ text.execute }}
                  </v-btn>
                  <v-btn variant="tonal" color="secondary" block :loading="clockBusy" :disabled="dividerBusy || clockBusy" @click="saveReferenceClock">
                    {{ text.saveClock }}
                  </v-btn>
                  <v-btn variant="outlined" color="secondary" block :disabled="dividerBusy || clockBusy" @click="loadDividerStatus">
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
                  <div class="meta-copy">{{ text.referenceClock }}</div>
                  <div class="metric-value">{{ formatFrequency(dividerStatus?.referenceClockHz) }}</div>
                  <div class="meta-copy small">{{ dividerStatus?.referenceClockHz || '--' }} Hz</div>
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
                  <v-btn color="secondary" variant="outlined" :loading="acquisitionBusy" :disabled="acquisitionBusy" @click="setAcquisition('restart-process')">
                    {{ text.restartProcess }}
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

              <div class="runtime-charts-block">
                <div class="meta-copy">{{ text.runtimeChartsTitle }}</div>
                <div class="meta-copy runtime-charts-copy">{{ text.runtimeChartsCopy }}</div>
                <div class="runtime-charts-grid">
                  <article v-for="chart in runtimeCharts" :key="chart.key" class="runtime-chart-card">
                    <div class="chart-head runtime-chart-head">
                      <div class="chart-title">
                        <strong>{{ chart.label }}</strong>
                        <span>{{ chart.value }}</span>
                      </div>
                      <span class="chart-mode-chip">{{ chart.helper }}</span>
                    </div>

                    <div class="chart-surface runtime-chart-surface">
                      <div
                        v-for="tick in chartYTicks(metricRange(chart.history), chart.formatter, 3)"
                        :key="`${chart.key}-y-${tick.key}`"
                        class="chart-axis chart-axis-y-tick"
                        :style="{ top: `${tick.position}%` }"
                      >
                        {{ tick.label }}
                      </div>
                      <div
                        v-for="tick in chartXTicks(metricTimeline(chart.history), 'ms', 3)"
                        :key="`${chart.key}-x-${tick.key}`"
                        class="chart-axis chart-axis-x-tick"
                        :style="{ left: `${tick.position}%` }"
                      >
                        {{ tick.label }}
                      </div>

                      <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg">
                        <line
                          v-for="tick in chartYTicks(metricRange(chart.history), chart.formatter, 3)"
                          :key="`${chart.key}-grid-y-${tick.key}`"
                          :x1="chartPlot.left"
                          :x2="chartPlot.right"
                          :y1="tick.position"
                          :y2="tick.position"
                          class="chart-grid-line"
                        />
                        <line
                          v-for="tick in chartXTicks(metricTimeline(chart.history), 'ms', 3)"
                          :key="`${chart.key}-grid-x-${tick.key}`"
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
                        <polyline
                          v-if="chart.history.length"
                          :points="linePoints(metricValues(chart.history), metricRange(chart.history).min, metricRange(chart.history).max, metricTimeline(chart.history))"
                          fill="none"
                          :stroke="chart.color"
                          stroke-width="0.9"
                          vector-effect="non-scaling-stroke"
                          stroke-linecap="round"
                          stroke-linejoin="round"
                          class="chart-signal-line"
                        />
                      </svg>

                      <div v-if="!chart.history.length" class="chart-placeholder runtime-chart-placeholder">
                        {{ text.statPerSecond }}
                      </div>
                    </div>
                  </article>
                </div>
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

.runtime-charts-block {
  margin-top: 22px;
}

.runtime-charts-copy {
  margin-bottom: 12px;
}

.runtime-charts-grid {
  display: grid;
  grid-template-columns: 1fr;
  gap: 16px;
}

.runtime-chart-card {
  border: 1px solid rgba(104, 146, 227, 0.18);
  border-radius: 20px;
  background: linear-gradient(180deg, rgba(13, 23, 40, 0.96), rgba(9, 17, 31, 0.98));
  overflow: hidden;
}

.runtime-chart-head {
  padding-top: 16px;
}

.runtime-chart-surface {
  min-height: 220px;
  margin-top: 10px;
}

.runtime-chart-placeholder {
  min-height: 220px;
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

.chart-head {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  padding: 18px 20px 0;
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

  .chart-placeholder {
    min-height: 280px;
  }

  .runtime-chart-surface {
    min-height: 240px;
  }

  .runtime-chart-placeholder {
    min-height: 240px;
  }
}
</style>
