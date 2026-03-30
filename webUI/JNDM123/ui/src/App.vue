<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref } from 'vue'
import { featurePackages } from './core/packageRegistry'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const channelPalette = ['#38d6ff', '#8cf2b2', '#f7a94a', '#ff6e88', '#7ab8ff', '#ffd166', '#d48bff', '#8df4ff']
const channelOptions = [
  { title: '全部', value: 'all' },
  { title: 'CH1', value: '0' },
  { title: 'CH2', value: '1' },
  { title: 'CH3', value: '2' },
  { title: 'CH4', value: '3' },
  { title: 'CH5', value: '4' },
  { title: 'CH6', value: '5' },
  { title: 'CH7', value: '6' },
  { title: 'CH8', value: '7' },
]
const dividerOptions = [
  { title: '1 分频', value: 1 },
  { title: '2 分频', value: 2 },
  { title: '3 分频', value: 3 },
  { title: '4 分频', value: 4 },
  { title: '5 分频', value: 5 },
  { title: '10 分频', value: 10 },
]

const banner = ref({
  type: 'info',
  text: '正在连接 JNDM123 硬件控制台...'
})
const devicePath = ref('/dev/i2c-0')
const dividerBusy = ref(false)
const dividerStatus = ref(null)
const acquisitionBusy = ref(false)
const acquisitionState = ref(null)
const pollInFlight = ref(false)
const selectedOutputIndex = ref(0)
const selectedDivider = ref(1)
const channelSelection = reactive({
  0: 'all',
  1: 'all',
  2: 'all',
  3: 'all',
  4: 'all',
  5: 'all'
})
let pollTimer = null

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : '未识别')
)

const outputs = computed(() => dividerStatus.value?.outputs ?? [])
const chips = computed(() => acquisitionState.value?.chips ?? [])
const acquisitionRunning = computed(() => Boolean(acquisitionState.value?.running))

const launchablePackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.entryPath)
)

const acquisitionMetrics = computed(() => [
  {
    label: '采集状态',
    value: acquisitionRunning.value ? '运行中' : '已停止',
    helper: acquisitionState.value?.message ?? '等待采集指令'
  },
  {
    label: '累计帧数',
    value: formatInteger(acquisitionState.value?.totalFrames),
    helper: acquisitionState.value?.updatedAt ? `最近帧：${formatDateTime(acquisitionState.value.updatedAt)}` : '尚无数据'
  },
  {
    label: '恢复次数',
    value: formatInteger(acquisitionState.value?.recoveries),
    helper: acquisitionState.value?.lastError || '当前没有恢复告警'
  },
  {
    label: '显示窗口',
    value: `${acquisitionState.value?.historyLimit ?? 0} 点`,
    helper: '每通道保留最近采样点'
  }
])

function formatInteger(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '0'
  return new Intl.NumberFormat('zh-CN').format(Math.round(numeric))
}

function formatFrequency(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric) || numeric <= 0) return '--'
  if (numeric >= 1000000) return `${(numeric / 1000000).toFixed(3)} MHz`
  if (numeric >= 1000) return `${(numeric / 1000).toFixed(2)} kHz`
  return `${numeric.toFixed(0)} Hz`
}

function formatDateTime(value) {
  if (!value) return '未更新'
  return new Date(value).toLocaleString('zh-CN', { hour12: false })
}

function syncSelectedDivider() {
  const current = outputs.value.find((entry) => entry.index === selectedOutputIndex.value)
  if (!current) return
  if (dividerOptions.some((option) => option.value === current.divider)) {
    selectedDivider.value = current.divider
  }
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, {
    credentials: 'same-origin',
    headers: {
      Accept: 'application/json',
      ...(options.body ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {})
    },
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
      text: error.message || '分频状态读取失败。'
    }
  }
}

async function loadAcquisitionSnapshot() {
  if (pollInFlight.value) return
  pollInFlight.value = true

  try {
    const payload = await requestJson('/myiot/jndm123/acquisition.json')
    acquisitionState.value = payload
  } catch (error) {
    banner.value = {
      type: 'warning',
      text: error.message || '采集状态同步失败。'
    }
  } finally {
    pollInFlight.value = false
  }
}

async function applyDivider() {
  dividerBusy.value = true
  banner.value = {
    type: 'info',
    text: `正在对 Y${selectedOutputIndex.value + 1} 应用 ${selectedDivider.value} 分频...`
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
      text: payload.message || '分频设置完成。'
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || '分频设置失败。'
    }
  } finally {
    dividerBusy.value = false
  }
}

async function setAcquisition(action, silent = false) {
  acquisitionBusy.value = true

  if (!silent) {
    banner.value = {
      type: 'info',
      text: action === 'start' ? '正在启动 AD7606 采集...' : '正在停止 AD7606 采集...'
    }
  }

  try {
    const body = new URLSearchParams()
    body.set('action', action)
    const payload = await requestJson('/myiot/jndm123/acquisition.json', {
      method: 'POST',
      body
    })
    acquisitionState.value = payload

    if (!silent) {
      banner.value = {
        type: 'success',
        text: payload.message || (action === 'start' ? '采集已启动。' : '采集已停止。')
      }
    }
  } catch (error) {
    if (!silent) {
      banner.value = {
        type: 'error',
        text: error.message || '采集控制失败。'
      }
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

function chartRange(chip) {
  const values = visibleSeries(chip).flatMap((series) => series.samples)
  if (!values.length) return { min: -1, max: 1 }

  const min = Math.min(...values)
  const max = Math.max(...values)
  if (min === max) return { min: min - 1, max: max + 1 }

  const padding = Math.max((max - min) * 0.1, 1)
  return { min: min - padding, max: max + padding }
}

function linePoints(samples, minValue, maxValue) {
  if (!samples.length) return ''

  return samples.map((sample, index) => {
    const x = samples.length === 1 ? 50 : (index / (samples.length - 1)) * 100
    const ratio = (sample - minValue) / Math.max(maxValue - minValue, 1)
    const y = 100 - (ratio * 100)
    return `${x.toFixed(2)},${y.toFixed(2)}`
  }).join(' ')
}

function latestChannelValue(series) {
  if (!series.samples?.length) return '--'
  return String(series.samples[series.samples.length - 1])
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
    text: `欢迎回来，${sessionState.username}。JNDM123 控制台正在同步板级状态。`
  }

  await loadDividerStatus()
  await loadAcquisitionSnapshot()

  if (!acquisitionRunning.value) {
    await setAcquisition('start', true)
    await loadAcquisitionSnapshot()
  }

  pollTimer = window.setInterval(() => {
    loadAcquisitionSnapshot()
  }, 600)
})

onBeforeUnmount(() => {
  if (pollTimer) {
    window.clearInterval(pollTimer)
    pollTimer = null
  }
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
              <p class="eyebrow">JNDM123 Lab</p>
              <h1>分频与 6 路 AD7606 联机控制台</h1>
              <p class="brand-copy">
                当前页面把 CDCE937 分频设置和 6 片 AD7606 的实时采集合在一个操作面板里。分频动作会在后端临时暂停采集，
                完成后再恢复，避免在切频过程中读到混杂数据。
              </p>
            </div>
          </div>

          <div class="header-actions">
            <div class="header-pills">
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
              退出登录
            </v-btn>
          </div>
        </header>

        <div class="layout-grid">
          <div class="left-stack">
            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Clock</p>
                  <h2>CDCE937 分频设置</h2>
                  <p class="panel-copy">
                    参考 `cdce937_i2c_test.c` 的 `set-div/status` 逻辑，直接对目标输出设置允许的分频值，并回读当前实际结果。
                  </p>
                </div>
              </div>

              <v-alert :type="banner.type" variant="tonal" border="start" class="mb-5">
                {{ banner.text }}
              </v-alert>

              <div class="control-grid">
                <v-text-field
                  v-model="devicePath"
                  label="I2C 设备"
                  prepend-inner-icon="mdi-expansion-card-variant"
                  placeholder="/dev/i2c-0"
                />

                <v-select
                  v-model="selectedOutputIndex"
                  :items="outputs.map((output) => ({ title: `${output.name} · Pin ${output.pin}`, value: output.index }))"
                  label="目标输出"
                  prepend-inner-icon="mdi-vector-polyline"
                />

                <v-select
                  v-model="selectedDivider"
                  :items="dividerOptions"
                  label="目标分频"
                  prepend-inner-icon="mdi-tune-variant"
                />

                <div class="panel-actions">
                  <v-btn
                    color="primary"
                    block
                    :loading="dividerBusy"
                    :disabled="dividerBusy || !outputs.length"
                    @click="applyDivider"
                  >
                    执行分频
                  </v-btn>
                  <v-btn variant="outlined" color="secondary" block :disabled="dividerBusy" @click="loadDividerStatus">
                    回读状态
                  </v-btn>
                </div>
              </div>

              <div class="status-grid mt-5">
                <div class="metric-card">
                  <div class="meta-copy">器件信息</div>
                  <div class="metric-value">{{ dividerStatus?.deviceType || '未识别' }}</div>
                  <div class="meta-copy small">
                    地址 {{ dividerStatus?.address || '--' }} · 输入 {{ dividerStatus?.inputClock || '--' }}
                  </div>
                </div>
                <div class="metric-card">
                  <div class="meta-copy">EEPROM / 电源状态</div>
                  <div class="metric-value">
                    Busy {{ dividerStatus?.eepBusy ? '1' : '0' }} · Lock {{ dividerStatus?.eepLock ? '1' : '0' }}
                  </div>
                  <div class="meta-copy small">
                    PowerDown {{ dividerStatus?.powerDown ? '1' : '0' }} · Rev {{ dividerStatus?.revisionId ?? '--' }}
                  </div>
                </div>
              </div>

              <div class="outputs-grid">
                <article
                  v-for="output in outputs"
                  :key="output.index"
                  class="output-card"
                  :class="{ active: output.index === selectedOutputIndex }"
                >
                  <div class="meta-copy">{{ output.name }} / {{ output.pdiv }}</div>
                  <div class="output-line">{{ output.divider || '--' }} 分频</div>
                  <div class="meta-copy small">Pin {{ output.pin }} · {{ formatFrequency(output.frequencyHz) }}</div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Acquisition</p>
                  <h2>AD7606 采集状态</h2>
                  <p class="panel-copy">
                    采集线程按 `work.c` 的 AXI FIFO packet 读取路径实现，只保留最近历史样本，不做终端打印，也不落地文件。
                  </p>
                </div>

                <div class="panel-actions">
                  <v-btn color="primary" :loading="acquisitionBusy" :disabled="acquisitionBusy || acquisitionRunning" @click="setAcquisition('start')">
                    启动采集
                  </v-btn>
                  <v-btn color="warning" variant="outlined" :loading="acquisitionBusy" :disabled="acquisitionBusy || !acquisitionRunning" @click="setAcquisition('stop')">
                    停止采集
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
          </div>

          <div class="right-stack">
            <section class="lab-panel chart-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Waveforms</p>
                  <h2>6 片 AD7606 实时曲线</h2>
                  <p class="panel-copy">
                    每张 chart 对应一片芯片，可切换显示 `全部` 或单独 `CH1~CH8`。当前实现按 48 列样本顺序映射为 6 片 × 8 通道。
                  </p>
                </div>
              </div>

              <div class="charts-grid">
                <article v-for="chip in chips" :key="chip.index" class="chart-card">
                  <div class="chart-head">
                    <div class="chart-title">
                      <strong>{{ chip.name }}</strong>
                      <span>{{ visibleSeries(chip).length }} 条曲线 · 最近 {{ chip.channels?.[0]?.samples?.length ?? 0 }} 点</span>
                    </div>

                    <v-select
                      v-model="channelSelection[chip.index]"
                      :items="channelOptions"
                      label="显示通道"
                      style="max-width: 160px"
                    />
                  </div>

                  <div v-if="visibleSeries(chip).some((series) => series.samples.length)" class="chart-surface">
                    <svg viewBox="0 0 100 100" preserveAspectRatio="none" class="chart-svg">
                      <polyline
                        v-for="series in visibleSeries(chip)"
                        :key="series.index"
                        :points="linePoints(series.samples, chartRange(chip).min, chartRange(chip).max)"
                        fill="none"
                        :stroke="series.color"
                        stroke-width="1.6"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      />
                    </svg>
                  </div>

                  <div v-else class="chart-placeholder">
                    当前芯片还没有可显示的数据，可能采集尚未启动，或最近窗口还未累积到有效样本。
                  </div>

                  <div class="legend-row">
                    <span v-for="series in visibleSeries(chip)" :key="series.index" class="legend-chip">
                      <span class="legend-swatch" :style="{ background: series.color }"></span>
                      {{ series.name }} · {{ latestChannelValue(series) }}
                    </span>
                  </div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Navigation</p>
                  <h2>其它功能包</h2>
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
