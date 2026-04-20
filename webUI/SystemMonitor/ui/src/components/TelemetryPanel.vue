<script setup>
import { onBeforeUnmount, onMounted, ref, computed } from 'vue'
import { createUiLocaleHeaders } from '../core/requestLocale.js'
import { useUiLocale } from '../core/locale.js'

const { isZh } = useUiLocale()

const zh = {
  kicker: '遥测',
  title: '业务调用与实时遥测',
  copy: '这里按 1 秒轮询后端遥测快照，集中展示实时日志、业务调用链步骤和最新性能指标，不自动滚动到底部，方便人工排查。',
  syncing: '正在同步遥测快照...',
  synced: '遥测快照同步正常。',
  failed: '遥测快照同步失败，请稍后重试。',
  unavailable: '遥测服务暂不可用。',
  activeTraces: '活跃调用',
  recentLogs: '最近日志',
  metricSeries: '指标样本',
  logsTitle: '实时日志',
  tracesTitle: '业务调用链',
  metricsTitle: '性能指标',
  noLogs: '当前还没有采集到日志。',
  noTraces: '当前还没有业务调用记录。',
  noMetrics: '当前还没有性能指标样本。',
  input: '输入',
  output: '输出',
  error: '错误',
  steps: '步骤',
  source: '来源',
  value: '数值',
  duration: '耗时'
}

const en = {
  kicker: 'Telemetry',
  title: 'Business Flow and Realtime Telemetry',
  copy: 'This section polls the backend telemetry snapshot once per second and keeps logs, business traces, and the latest performance metrics together without forcing auto-scroll.',
  syncing: 'Synchronizing telemetry snapshot...',
  synced: 'Telemetry snapshot synchronized.',
  failed: 'Failed to synchronize telemetry snapshot. Please retry later.',
  unavailable: 'Telemetry service is unavailable.',
  activeTraces: 'Active traces',
  recentLogs: 'Recent logs',
  metricSeries: 'Metric samples',
  logsTitle: 'Realtime Logs',
  tracesTitle: 'Business Traces',
  metricsTitle: 'Performance Metrics',
  noLogs: 'No logs have been captured yet.',
  noTraces: 'No business traces are available yet.',
  noMetrics: 'No metric samples are available yet.',
  input: 'Input',
  output: 'Output',
  error: 'Error',
  steps: 'Steps',
  source: 'Source',
  value: 'Value',
  duration: 'Duration'
}

const text = computed(() => (isZh.value ? zh : en))
const loading = ref(false)
const errorText = ref('')
const banner = ref(text.value.syncing)
const snapshot = ref({
  updatedAt: '',
  activeTraceCount: 0,
  logs: [],
  traces: [],
  metrics: [],
})

let pollTimer = null
let inFlight = false

function formatAttributes(attributes) {
  if (!Array.isArray(attributes) || !attributes.length) return ''
  return attributes.map((attribute) => `${attribute.key}=${attribute.value}`).join(' | ')
}

function formatMetricValue(metric) {
  const numeric = Number(metric?.value)
  const valueText = Number.isFinite(numeric)
    ? (numeric >= 100 ? numeric.toFixed(0) : numeric.toFixed(2))
    : String(metric?.value ?? '')
  return metric?.unit ? `${valueText} ${metric.unit}` : valueText
}

async function loadTelemetry() {
  if (inFlight) return

  loading.value = true
  inFlight = true

  try {
    const response = await fetch(`/myiot/monitor/telemetry.json?_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({ Accept: 'application/json' })
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    const payload = await response.json()
    if (!response.ok || payload.available === false) {
      throw new Error(payload.message ?? text.value.unavailable)
    }

    snapshot.value = {
      updatedAt: payload.updatedAt ?? '',
      activeTraceCount: Number(payload.activeTraceCount ?? 0),
      logs: Array.isArray(payload.logs) ? payload.logs : [],
      traces: Array.isArray(payload.traces) ? payload.traces : [],
      metrics: Array.isArray(payload.metrics) ? payload.metrics : [],
    }

    banner.value = payload.message ?? text.value.synced
    errorText.value = ''
  } catch (error) {
    errorText.value = error instanceof Error && error.message ? error.message : text.value.failed
    banner.value = errorText.value
  } finally {
    loading.value = false
    inFlight = false
  }
}

onMounted(async () => {
  await loadTelemetry()
  pollTimer = window.setInterval(loadTelemetry, 1000)
})

onBeforeUnmount(() => {
  if (pollTimer) {
    window.clearInterval(pollTimer)
    pollTimer = null
  }
})
</script>

<template>
  <section class="feature-panel">
    <div class="feature-frame"></div>

    <div class="panel-head">
      <div>
        <p class="section-kicker">{{ text.kicker }}</p>
        <h2>{{ text.title }}</h2>
        <p class="telemetry-copy">{{ text.copy }}</p>
      </div>
      <div class="telemetry-meta">
        <div class="meta-pill">
          <v-icon icon="mdi-transit-connection-variant" size="18"></v-icon>
          <span>{{ snapshot.activeTraceCount }} {{ text.activeTraces }}</span>
        </div>
        <div class="meta-pill">
          <v-icon icon="mdi-file-document-multiple-outline" size="18"></v-icon>
          <span>{{ snapshot.logs.length }} {{ text.recentLogs }}</span>
        </div>
        <div class="meta-pill">
          <v-icon icon="mdi-chart-scatter-plot" size="18"></v-icon>
          <span>{{ snapshot.metrics.length }} {{ text.metricSeries }}</span>
        </div>
        <div class="meta-pill" v-if="snapshot.updatedAt">
          <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
          <span>{{ snapshot.updatedAt }}</span>
        </div>
      </div>
    </div>

    <v-alert :type="errorText ? 'error' : 'success'" variant="tonal" border="start">
      {{ banner }}
    </v-alert>

    <div class="telemetry-grid">
      <article class="telemetry-card">
        <header class="telemetry-card-head">
          <div>
            <p>{{ text.logsTitle }}</p>
            <strong>{{ snapshot.logs.length }}</strong>
          </div>
        </header>

        <div v-if="snapshot.logs.length" class="telemetry-scroll">
          <div v-for="log in snapshot.logs" :key="`${log.timestamp}-${log.source}-${log.message}`" class="telemetry-log-item">
            <div class="telemetry-line">
              <span class="telemetry-badge">{{ log.level }}</span>
              <span>{{ log.timestamp }}</span>
            </div>
            <div class="telemetry-line telemetry-line-strong">
              <span>{{ text.source }}: {{ log.source || '-' }}</span>
            </div>
            <p class="telemetry-message">{{ log.message }}</p>
            <p v-if="formatAttributes(log.attributes)" class="telemetry-attributes">{{ formatAttributes(log.attributes) }}</p>
          </div>
        </div>

        <div v-else-if="!loading" class="empty-state">
          {{ text.noLogs }}
        </div>
      </article>

      <article class="telemetry-card">
        <header class="telemetry-card-head">
          <div>
            <p>{{ text.tracesTitle }}</p>
            <strong>{{ snapshot.traces.length }}</strong>
          </div>
        </header>

        <div v-if="snapshot.traces.length" class="telemetry-scroll">
          <div v-for="trace in snapshot.traces" :key="trace.activityId" class="telemetry-trace-item">
            <div class="telemetry-line">
              <span class="telemetry-badge">{{ trace.status }}</span>
              <span>{{ trace.startedAt }}</span>
            </div>
            <h3>{{ trace.name }}</h3>
            <p class="telemetry-secondary">{{ text.duration }}: {{ trace.durationMs }} ms</p>
            <p v-if="trace.input" class="telemetry-block"><strong>{{ text.input }}:</strong> {{ trace.input }}</p>
            <p v-if="trace.output" class="telemetry-block"><strong>{{ text.output }}:</strong> {{ trace.output }}</p>
            <p v-if="trace.error" class="telemetry-block telemetry-error"><strong>{{ text.error }}:</strong> {{ trace.error }}</p>
            <div v-if="trace.steps?.length" class="telemetry-steps">
              <p class="telemetry-secondary">{{ text.steps }}</p>
              <div v-for="step in trace.steps" :key="`${trace.activityId}-${step.timestamp}-${step.name}`" class="telemetry-step">
                <div class="telemetry-line">
                  <span class="telemetry-badge">{{ step.status }}</span>
                  <span>{{ step.timestamp }}</span>
                </div>
                <p class="telemetry-step-name">{{ step.name }}</p>
                <p v-if="step.detail" class="telemetry-step-detail">{{ step.detail }}</p>
                <p v-if="formatAttributes(step.attributes)" class="telemetry-attributes">{{ formatAttributes(step.attributes) }}</p>
              </div>
            </div>
            <p v-if="formatAttributes(trace.attributes)" class="telemetry-attributes">{{ formatAttributes(trace.attributes) }}</p>
          </div>
        </div>

        <div v-else-if="!loading" class="empty-state">
          {{ text.noTraces }}
        </div>
      </article>

      <article class="telemetry-card">
        <header class="telemetry-card-head">
          <div>
            <p>{{ text.metricsTitle }}</p>
            <strong>{{ snapshot.metrics.length }}</strong>
          </div>
        </header>

        <div v-if="snapshot.metrics.length" class="telemetry-scroll">
          <div v-for="metric in snapshot.metrics" :key="`${metric.timestamp}-${metric.name}`" class="telemetry-metric-item">
            <div class="telemetry-line">
              <span class="telemetry-badge">{{ text.value }}</span>
              <span>{{ metric.timestamp }}</span>
            </div>
            <h3>{{ metric.name }}</h3>
            <p class="telemetry-metric-value">{{ formatMetricValue(metric) }}</p>
            <p v-if="metric.description" class="telemetry-secondary">{{ metric.description }}</p>
            <p v-if="formatAttributes(metric.attributes)" class="telemetry-attributes">{{ formatAttributes(metric.attributes) }}</p>
          </div>
        </div>

        <div v-else-if="!loading" class="empty-state">
          {{ text.noMetrics }}
        </div>
      </article>
    </div>
  </section>
</template>

<style scoped>
.telemetry-copy {
  margin: 10px 0 0;
  max-width: 68rem;
  color: rgba(210, 232, 255, 0.72);
  line-height: 1.72;
}

.telemetry-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  justify-content: flex-end;
}

.telemetry-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 16px;
  margin-top: 18px;
}

.telemetry-card {
  border: 1px solid rgba(78, 188, 255, 0.14);
  border-radius: 22px;
  background: rgba(8, 22, 40, 0.72);
  box-shadow: var(--shell-shadow);
  padding: 20px;
}

.telemetry-card-head {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  align-items: flex-start;
}

.telemetry-card-head p,
.telemetry-secondary,
.telemetry-attributes,
.telemetry-step-detail {
  margin: 0;
  color: rgba(210, 232, 255, 0.66);
}

.telemetry-card-head strong,
.telemetry-metric-value,
.telemetry-trace-item h3,
.telemetry-metric-item h3 {
  display: block;
  margin-top: 6px;
}

.telemetry-scroll {
  max-height: 32rem;
  overflow: auto;
  margin-top: 18px;
  padding-right: 6px;
}

.telemetry-log-item,
.telemetry-trace-item,
.telemetry-metric-item,
.telemetry-step {
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(3, 12, 22, 0.78);
  padding: 14px;
}

.telemetry-log-item + .telemetry-log-item,
.telemetry-trace-item + .telemetry-trace-item,
.telemetry-metric-item + .telemetry-metric-item,
.telemetry-step + .telemetry-step {
  margin-top: 12px;
}

.telemetry-line {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  align-items: center;
  font-size: 0.8rem;
  color: rgba(210, 232, 255, 0.74);
}

.telemetry-line-strong {
  margin-top: 6px;
  color: #e6f4ff;
}

.telemetry-badge {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 28px;
  padding: 0 10px;
  border-radius: 999px;
  border: 1px solid rgba(112, 240, 193, 0.2);
  background: rgba(16, 36, 58, 0.84);
  color: #70f0c1;
  text-transform: uppercase;
}

.telemetry-message,
.telemetry-block {
  margin: 10px 0 0;
  line-height: 1.68;
}

.telemetry-error {
  color: #ff8d6d;
}

.telemetry-steps {
  margin-top: 12px;
}

.telemetry-step-name {
  margin: 8px 0 0;
  font-weight: 700;
}

.telemetry-step-detail,
.telemetry-attributes {
  margin-top: 8px;
  word-break: break-word;
  line-height: 1.6;
}

@media (width <= 1200px) {
  .telemetry-grid {
    grid-template-columns: 1fr;
  }
}
</style>
