<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'
import { featurePackages } from './core/packageRegistry'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const channelPalette = ['#38d6ff', '#8cf2b2', '#f7a94a', '#ff6e88', '#7ab8ff', '#ffd166', '#d48bff', '#8df4ff']
const channelOptions = [
  { title: 'All', value: 'all' },
  { title: 'CH1', value: '0' },
  { title: 'CH2', value: '1' },
  { title: 'CH3', value: '2' },
  { title: 'CH4', value: '3' },
  { title: 'CH5', value: '4' },
  { title: 'CH6', value: '5' },
  { title: 'CH7', value: '6' },
  { title: 'CH8', value: '7' },
]
const dividerOptions = [1, 2, 3, 4, 5, 10].map((value) => ({ title: `${value}`, value }))

const banner = ref({
  type: 'info',
  text: 'Connecting to JNDM123 control service...'
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

let pollTimer = null

const currentDeviceAddress = computed(() =>
  sessionState.serverAddress ||
  (sessionState.deviceIp
    ? `${sessionState.deviceIp}${sessionState.devicePort ? `:${sessionState.devicePort}` : ''}`
    : 'unknown')
)

const outputs = computed(() => dividerStatus.value?.outputs ?? [])
const chips = computed(() => acquisitionState.value?.chips ?? [])
const acquisitionRunning = computed(() => Boolean(acquisitionState.value?.running))
const previewEnabled = computed(() => activeView.value === 'preview' && document.visibilityState === 'visible')
const selectedOutput = computed(() => outputs.value.find((entry) => entry.index === selectedOutputIndex.value) ?? null)

const launchablePackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.entryPath)
)

const acquisitionMetrics = computed(() => [
  {
    label: 'Capture',
    value: acquisitionRunning.value ? 'Running' : 'Stopped',
    helper: acquisitionState.value?.message ?? 'Waiting for operator action'
  },
  {
    label: 'Frames',
    value: formatInteger(acquisitionState.value?.totalFrames),
    helper: acquisitionState.value?.lastFrameAt ? `Last frame ${formatDateTime(acquisitionState.value.lastFrameAt)}` : 'No frames yet'
  },
  {
    label: 'Recoveries',
    value: formatInteger(acquisitionState.value?.recoveries),
    helper: acquisitionState.value?.lastError || 'No recovery warning'
  },
  {
    label: 'Queue',
    value: formatInteger(acquisitionState.value?.queueDepth),
    helper: acquisitionState.value?.previewActive ? 'Preview cache active' : 'Preview cache paused'
  }
])

function formatInteger(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return '0'
  return new Intl.NumberFormat('en-US').format(Math.round(numeric))
}

function formatFrequency(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric) || numeric <= 0) return '--'
  if (numeric >= 1000000) return `${(numeric / 1000000).toFixed(3)} MHz`
  if (numeric >= 1000) return `${(numeric / 1000).toFixed(2)} kHz`
  return `${numeric.toFixed(0)} Hz`
}

function formatDateTime(value) {
  if (!value) return 'not updated'
  return new Date(value).toLocaleString('zh-CN', { hour12: false })
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
      text: error.message || 'Unable to read divider status.'
    }
  }
}

async function loadAcquisitionSnapshot() {
  if (pollInFlight.value) return
  pollInFlight.value = true

  try {
    const query = previewEnabled.value ? '?includeWaveform=1' : ''
    const payload = await requestJson(`/myiot/jndm123/acquisition.json${query}`)
    acquisitionState.value = payload
    syncUdpForm()
  } catch (error) {
    banner.value = {
      type: 'warning',
      text: error.message || 'Unable to synchronize acquisition state.'
    }
  } finally {
    pollInFlight.value = false
  }
}

async function applyDivider() {
  dividerBusy.value = true
  banner.value = {
    type: 'info',
    text: `Applying divider ${selectedDivider.value} to ${selectedOutput.value?.name || `Y${selectedOutputIndex.value + 1}`}...`
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
      text: payload.message || 'Divider update complete.'
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || 'Divider update failed.'
    }
  } finally {
    dividerBusy.value = false
  }
}

async function setAcquisition(action) {
  acquisitionBusy.value = true
  banner.value = {
    type: 'info',
    text: action === 'start' ? 'Starting AD7606 capture...' : 'Stopping AD7606 capture...'
  }

  try {
    const body = new URLSearchParams()
    body.set('action', action)
    if (previewEnabled.value) body.set('includeWaveform', '1')
    const payload = await requestJson('/myiot/jndm123/acquisition.json', {
      method: 'POST',
      body
    })
    acquisitionState.value = payload
    syncUdpForm()
    banner.value = {
      type: 'success',
      text: payload.message || (action === 'start' ? 'Capture started.' : 'Capture stopped.')
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || 'Capture control failed.'
    }
  } finally {
    acquisitionBusy.value = false
  }
}

async function saveUdpConfig() {
  udpBusy.value = true
  banner.value = {
    type: 'info',
    text: 'Saving UDP broadcast configuration...'
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
    acquisitionState.value = payload
    syncUdpForm()
    banner.value = {
      type: 'success',
      text: payload.message || 'UDP configuration saved.'
    }
  } catch (error) {
    banner.value = {
      type: 'error',
      text: error.message || 'Unable to save UDP configuration.'
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
  }, previewEnabled.value ? 280 : 1200)
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
    text: `Welcome ${sessionState.username}. JNDM123 hardware state is synchronizing.`
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
              <p class="eyebrow">JNDM123 Lab</p>
              <h1>Clock Divider and 6x AD7606 Capture</h1>
              <p class="brand-copy">
                Divider updates follow the local CDCE937 I2C test flow. FIFO capture follows the packet-mode
                implementation from `work.c`, removes console/file output, and feeds the UI plus UDP broadcast.
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
              Sign Out
            </v-btn>
          </div>
        </header>

        <div class="layout-grid">
          <div class="left-stack">
            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Clock</p>
                  <h2>CDCE937 Divider Control</h2>
                  <p class="panel-copy">
                    Press execute to apply the selected divider. The backend pauses acquisition, writes the divider,
                    reads the actual result back, and restarts capture when needed.
                  </p>
                </div>
              </div>

              <v-alert :type="banner.type" variant="tonal" border="start" class="mb-5">
                {{ banner.text }}
              </v-alert>

              <div class="control-grid">
                <v-text-field
                  v-model="devicePath"
                  label="I2C Device"
                  prepend-inner-icon="mdi-expansion-card-variant"
                  placeholder="/dev/i2c-0"
                />

                <v-select
                  v-model="selectedOutputIndex"
                  :items="outputs.map((output) => ({ title: `${output.name} / Pin ${output.pin}`, value: output.index }))"
                  label="Output"
                  prepend-inner-icon="mdi-vector-polyline"
                />

                <v-select
                  v-model="selectedDivider"
                  :items="dividerOptions"
                  label="Divider"
                  prepend-inner-icon="mdi-tune-variant"
                />

                <div class="panel-actions">
                  <v-btn color="primary" block :loading="dividerBusy" :disabled="dividerBusy || !outputs.length" @click="applyDivider">
                    Execute
                  </v-btn>
                  <v-btn variant="outlined" color="secondary" block :disabled="dividerBusy" @click="loadDividerStatus">
                    Refresh
                  </v-btn>
                </div>
              </div>

              <div class="status-grid mt-5">
                <article class="metric-card">
                  <div class="meta-copy">Device</div>
                  <div class="metric-value">{{ dividerStatus?.deviceType || '--' }}</div>
                  <div class="meta-copy small">{{ dividerStatus?.address || '--' }} / {{ dividerStatus?.inputClock || '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">Selected Output</div>
                  <div class="metric-value">{{ selectedOutput?.divider ?? '--' }} / {{ formatFrequency(selectedOutput?.frequencyHz) }}</div>
                  <div class="meta-copy small">{{ selectedOutput?.name || '--' }} / {{ selectedOutput?.pdiv || '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">EEPROM / Lock</div>
                  <div class="metric-value">Busy {{ dividerStatus?.eepBusy ? '1' : '0' }} / Lock {{ dividerStatus?.eepLock ? '1' : '0' }}</div>
                  <div class="meta-copy small">Revision {{ dividerStatus?.revisionId ?? '--' }}</div>
                </article>
                <article class="metric-card">
                  <div class="meta-copy">Power</div>
                  <div class="metric-value">{{ dividerStatus?.powerDown ? 'Down' : 'Active' }}</div>
                  <div class="meta-copy small">Actual readback is shown in every output card below.</div>
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
                  <div class="meta-copy small">Pin {{ output.pin }} / {{ formatFrequency(output.frequencyHz) }}</div>
                </article>
              </div>
            </section>

            <section class="lab-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Acquisition</p>
                  <h2>AD7606 Capture Runtime</h2>
                  <p class="panel-copy">
                    The reader thread stays lean, pushes full frames into `Poco::NotificationQueue`, and leaves
                    waveform packaging and UDP broadcasting to the consumer side.
                  </p>
                </div>

                <div class="panel-actions">
                  <v-btn color="primary" :loading="acquisitionBusy" :disabled="acquisitionBusy || acquisitionRunning" @click="setAcquisition('start')">
                    Start
                  </v-btn>
                  <v-btn color="warning" variant="outlined" :loading="acquisitionBusy" :disabled="acquisitionBusy || !acquisitionRunning" @click="setAcquisition('stop')">
                    Stop
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
                  <p class="eyebrow">Broadcast</p>
                  <h2>UDP External Interface</h2>
                  <p class="panel-copy">
                    Every dequeued frame can be broadcast as a packed 48-channel UDP payload even when preview rendering
                    is paused on the front end.
                  </p>
                </div>
              </div>

              <div class="config-grid">
                <v-switch v-model="udpForm.enabled" color="primary" inset label="Enable UDP Broadcast" />
                <v-text-field v-model="udpForm.host" label="UDP Host" prepend-inner-icon="mdi-access-point-network" />
                <v-text-field v-model="udpForm.port" type="number" label="UDP Port" prepend-inner-icon="mdi-connection" />
                <div class="panel-actions">
                  <v-btn color="primary" block :loading="udpBusy" @click="saveUdpConfig">
                    Save UDP
                  </v-btn>
                </div>
              </div>

              <div class="legend-row">
                <span class="legend-chip">Last UDP: {{ acquisitionState?.udp?.lastBroadcastAt ? formatDateTime(acquisitionState.udp.lastBroadcastAt) : 'none' }}</span>
                <span class="legend-chip">Target: {{ acquisitionState?.udp?.host || udpForm.host }}:{{ acquisitionState?.udp?.port || udpForm.port }}</span>
              </div>
            </section>
          </div>

          <div class="right-stack">
            <section class="lab-panel chart-panel">
              <div class="panel-head">
                <div>
                  <p class="eyebrow">Waveforms</p>
                  <h2>6 Independent Charts</h2>
                  <p class="panel-copy">
                    Each chart maps one AD7606 chip. Choose `All` or a single channel from `CH1~CH8`. When preview is
                    not active, the backend stops packaging waveform history for the browser but keeps UDP broadcast alive.
                  </p>
                </div>

                <div class="view-toggle">
                  <v-btn :variant="activeView === 'preview' ? 'flat' : 'outlined'" color="primary" @click="activeView = 'preview'">
                    Preview
                  </v-btn>
                  <v-btn :variant="activeView === 'overview' ? 'flat' : 'outlined'" color="secondary" @click="activeView = 'overview'">
                    Overview
                  </v-btn>
                </div>
              </div>

              <div v-if="activeView === 'preview'" class="charts-grid">
                <article v-for="chip in chips" :key="chip.index" class="chart-card">
                  <div class="chart-head">
                    <div class="chart-title">
                      <strong>{{ chip.name }}</strong>
                      <span>{{ visibleSeries(chip).length }} visible lines / {{ chip.channels?.[0]?.samples?.length ?? 0 }} points</span>
                    </div>

                    <v-select
                      v-model="channelSelection[chip.index]"
                      :items="channelOptions"
                      label="Channels"
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
                    No waveform data yet. Start capture or keep the preview page active until the history window fills.
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
                  <p class="eyebrow">Navigation</p>
                  <h2>Other Packages</h2>
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
