<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { featurePackages, formatPackageStatus, getPackageStatusTone } from './core/packageRegistry'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const banner = ref({
  type: 'info',
  text: '正在同步系统包列表...'
})
const bundlePayload = ref([])
const bundleUpdatedAt = ref('')
const bundleCount = ref(0)
const stateSummary = ref({})
const loading = ref(false)
const errorText = ref('')
const expandedBundles = ref([])
const preferenceDrafts = ref({})
const dirtyDrafts = ref({})
const actionBusyMap = ref({})
let refreshTimer = null
let requestInFlight = false

const stateChips = computed(() =>
  Object.entries(stateSummary.value).map(([label, total]) => ({ label, total }))
)

const activeBundles = computed(() =>
  bundlePayload.value.filter((bundle) => String(bundle.state).toLowerCase() === 'active').length
)

const manageableBundles = computed(() =>
  bundlePayload.value.filter((bundle) => bundle.manageable).length
)

const protectedBundles = computed(() =>
  bundlePayload.value.filter((bundle) => !bundle.manageable).length
)

function normalizePreferences(preferences = []) {
  if (!preferences.length) return [{ key: '', value: '' }]
  return preferences.map((entry) => ({
    key: entry.key ?? '',
    value: entry.value ?? ''
  }))
}

function syncDrafts(payloadBundles) {
  const nextDrafts = { ...preferenceDrafts.value }
  const nextDirty = { ...dirtyDrafts.value }
  const visibleSymbols = new Set()

  payloadBundles.forEach((bundle) => {
    visibleSymbols.add(bundle.symbolicName)
    if (!nextDirty[bundle.symbolicName]) {
      nextDrafts[bundle.symbolicName] = normalizePreferences(bundle.preferences)
    }
  })

  Object.keys(nextDrafts).forEach((symbolicName) => {
    if (!visibleSymbols.has(symbolicName)) delete nextDrafts[symbolicName]
  })
  Object.keys(nextDirty).forEach((symbolicName) => {
    if (!visibleSymbols.has(symbolicName)) delete nextDirty[symbolicName]
  })

  preferenceDrafts.value = nextDrafts
  dirtyDrafts.value = nextDirty
}

function isExpanded(symbolicName) {
  return expandedBundles.value.includes(symbolicName)
}

function toggleBundle(symbolicName) {
  if (isExpanded(symbolicName)) {
    expandedBundles.value = expandedBundles.value.filter((entry) => entry !== symbolicName)
  } else {
    expandedBundles.value = [...expandedBundles.value, symbolicName]
  }
}

function markDraftDirty(symbolicName) {
  dirtyDrafts.value = {
    ...dirtyDrafts.value,
    [symbolicName]: true
  }
}

function preferenceRows(symbolicName) {
  return preferenceDrafts.value[symbolicName] ?? [{ key: '', value: '' }]
}

function updatePreference(symbolicName, index, field, value) {
  const nextRows = preferenceRows(symbolicName).map((row, rowIndex) =>
    rowIndex === index ? { ...row, [field]: value } : row
  )
  preferenceDrafts.value = {
    ...preferenceDrafts.value,
    [symbolicName]: nextRows
  }
  markDraftDirty(symbolicName)
}

function addPreference(symbolicName) {
  const nextRows = [...preferenceRows(symbolicName), { key: '', value: '' }]
  preferenceDrafts.value = {
    ...preferenceDrafts.value,
    [symbolicName]: nextRows
  }
  markDraftDirty(symbolicName)
}

function removePreference(symbolicName, index) {
  const currentRows = preferenceRows(symbolicName)
  const nextRows = currentRows.filter((_, rowIndex) => rowIndex !== index)
  preferenceDrafts.value = {
    ...preferenceDrafts.value,
    [symbolicName]: nextRows.length ? nextRows : [{ key: '', value: '' }]
  }
  markDraftDirty(symbolicName)
}

function stateTone(bundle) {
  const state = String(bundle.state ?? '').toLowerCase()
  if (state === 'active') return 'success'
  if (state === 'resolved') return 'info'
  if (state === 'installed') return 'warning'
  if (state === 'starting' || state === 'stopping') return 'secondary'
  return 'error'
}

function setBusy(symbolicName, busy) {
  actionBusyMap.value = {
    ...actionBusyMap.value,
    [symbolicName]: busy
  }
}

function isBusy(symbolicName) {
  return Boolean(actionBusyMap.value[symbolicName])
}

async function loadBundles() {
  if (requestInFlight) return

  requestInFlight = true
  loading.value = true

  try {
    const response = await fetch(`/myiot/packages/data.json?_=${Date.now()}`, {
      credentials: 'same-origin',
      headers: { Accept: 'application/json' }
    })

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return
    }

    if (!response.ok) throw new Error(`bundle status ${response.status}`)

    const payload = await response.json()
    const bundles = payload.bundles ?? []
    bundlePayload.value = bundles
    bundleCount.value = payload.bundleCount ?? bundles.length
    bundleUpdatedAt.value = payload.updatedAt ?? ''
    stateSummary.value = payload.stateSummary ?? {}
    syncDrafts(bundles)
    errorText.value = ''
    banner.value = {
      type: 'success',
      text: payload.message ?? '系统包列表同步完成。'
    }
  } catch {
    errorText.value = '系统包列表同步失败，请稍后重试。'
    banner.value = {
      type: 'error',
      text: errorText.value
    }
  } finally {
    loading.value = false
    requestInFlight = false
  }
}

async function postBundleAction(symbolicName, action, extraFields = {}) {
  setBusy(symbolicName, true)

  try {
    const body = new URLSearchParams()
    body.set('symbolicName', symbolicName)
    body.set('action', action)
    Object.entries(extraFields).forEach(([key, value]) => {
      body.set(key, value)
    })

    const response = await fetch('/myiot/packages/manage.json', {
      method: 'POST',
      credentials: 'same-origin',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        Accept: 'application/json'
      },
      body
    })

    const payload = await response.json().catch(() => ({
      ok: false,
      message: `管理请求失败，状态码 ${response.status}`
    }))

    if (response.status === 401) {
      window.location.replace('/myiot/login/index.html')
      return false
    }

    if (!response.ok || payload.ok === false) {
      banner.value = {
        type: 'error',
        text: payload.message ?? '管理请求失败。'
      }
      errorText.value = payload.message ?? '管理请求失败。'
      return false
    }

    errorText.value = ''
    banner.value = {
      type: 'success',
      text: payload.message ?? '操作已完成。'
    }
    return true
  } catch {
    banner.value = {
      type: 'error',
      text: '管理请求失败，请稍后重试。'
    }
    errorText.value = '管理请求失败，请稍后重试。'
    return false
  } finally {
    setBusy(symbolicName, false)
  }
}

async function handleBundleCommand(bundle, action) {
  const ok = await postBundleAction(bundle.symbolicName, action)
  if (ok) {
    await loadBundles()
  }
}

async function handleSavePreferences(bundle) {
  const sanitized = preferenceRows(bundle.symbolicName)
    .map((row) => ({
      key: row.key.trim(),
      value: row.value ?? ''
    }))
    .filter((row) => row.key)

  const ok = await postBundleAction(bundle.symbolicName, 'savePreferences', {
    preferences: JSON.stringify(sanitized)
  })

  if (ok) {
    dirtyDrafts.value = {
      ...dirtyDrafts.value,
      [bundle.symbolicName]: false
    }
    await loadBundles()
  }
}

onMounted(async () => {
  const payload = await refreshSession()
  if (!payload.authenticated) {
    window.location.replace('/myiot/login/index.html')
    return
  }

  await loadBundles()
  refreshTimer = window.setInterval(() => {
    loadBundles()
  }, 3000)
})

onBeforeUnmount(() => {
  if (refreshTimer) {
    window.clearInterval(refreshTimer)
    refreshTimer = null
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
              <p class="eyebrow">MYIOT Bundle Catalog</p>
              <h1>包管理与配置控制台</h1>
              <p class="brand-copy">
                这里统一查看当前系统已加载的 bundle，并对非核心包执行解析、激活、停用和配置参数编辑。
                系统核心包会保留只读视图，不进入控制范围。
              </p>
            </div>
          </div>

          <div class="header-pills">
            <a href="/myiot/home/index.html" class="meta-pill">
              <v-icon icon="mdi-view-dashboard-outline" size="18"></v-icon>
              <span>返回主页面</span>
            </a>
            <div class="meta-pill">
              <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
              <span>{{ sessionState.username }}</span>
            </div>
            <div class="meta-pill" v-if="bundleUpdatedAt">
              <v-icon icon="mdi-refresh-circle" size="18"></v-icon>
              <span>{{ bundleUpdatedAt }}</span>
            </div>
            <v-btn variant="tonal" color="secondary" size="small" @click="handleSignOut">
              退出登录
            </v-btn>
          </div>
        </header>

        <main class="viewport-panel">
          <div class="bundle-stats-grid">
            <div class="bundle-stat-card">
              <p>已加载包总数</p>
              <strong>{{ bundleCount }}</strong>
            </div>
            <div class="bundle-stat-card">
              <p>可管理包</p>
              <strong>{{ manageableBundles }}</strong>
            </div>
            <div class="bundle-stat-card">
              <p>核心只读包</p>
              <strong>{{ protectedBundles }}</strong>
            </div>
            <div class="bundle-stat-card">
              <p>活跃状态包</p>
              <strong>{{ activeBundles }}</strong>
            </div>
          </div>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">运行时摘要</p>
                <h2>状态概览</h2>
              </div>

              <div class="bundle-summary-row">
                <div v-for="item in stateChips" :key="item.label" class="bundle-summary-chip">
                  <v-icon icon="mdi-chart-donut" size="16"></v-icon>
                  <span>{{ item.label }}: {{ item.total }}</span>
                </div>
              </div>
            </div>

            <v-alert :type="errorText ? 'error' : banner.type" variant="tonal" border="start">
              {{ errorText || banner.text }}
            </v-alert>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">系统包清单</p>
                <h2>管理动作与配置参数</h2>
              </div>
            </div>

            <div v-if="bundlePayload.length" class="bundle-grid">
              <article
                v-for="bundle in bundlePayload"
                :key="`${bundle.id}-${bundle.symbolicName}`"
                class="bundle-card"
              >
                <div class="bundle-card-head">
                  <div class="bundle-heading">
                    <v-icon icon="mdi-package-variant-closed" size="20"></v-icon>
                    <div>
                      <strong>{{ bundle.name || bundle.symbolicName }}</strong>
                      <p>{{ bundle.symbolicName }}</p>
                    </div>
                  </div>

                  <div class="bundle-chip-row">
                    <v-chip size="small" variant="tonal" :color="stateTone(bundle)">
                      {{ bundle.state }}
                    </v-chip>
                    <v-chip
                      size="small"
                      variant="tonal"
                      :color="bundle.manageable ? 'secondary' : 'warning'"
                    >
                      {{ bundle.manageable ? '可管理' : '核心只读' }}
                    </v-chip>
                  </div>
                </div>

                <div class="bundle-card-meta">
                  <span>ID {{ bundle.id }}</span>
                  <span>v{{ bundle.version }}</span>
                  <span>RunLevel {{ bundle.runLevel || 'default' }}</span>
                  <span>{{ bundle.extensionBundle ? '扩展包' : '普通包' }}</span>
                </div>

                <p class="bundle-copy">
                  供应商：{{ bundle.vendor || '未标注' }}
                </p>

                <div class="bundle-path">{{ bundle.path }}</div>

                <div v-if="bundle.manageReason" class="bundle-note">
                  {{ bundle.manageReason }}
                </div>

                <div v-if="bundle.activeDependents?.length" class="bundle-note bundle-note-warning">
                  依赖当前包的运行中模块：{{ bundle.activeDependents.join('，') }}
                </div>

                <div class="bundle-action-row">
                  <v-btn
                    size="small"
                    variant="tonal"
                    color="primary"
                    :disabled="!bundle.canResolve || isBusy(bundle.symbolicName)"
                    @click="handleBundleCommand(bundle, 'resolve')"
                  >
                    解析
                  </v-btn>
                  <v-btn
                    size="small"
                    variant="tonal"
                    color="success"
                    :disabled="!bundle.canStart || isBusy(bundle.symbolicName)"
                    @click="handleBundleCommand(bundle, 'start')"
                  >
                    激活
                  </v-btn>
                  <v-btn
                    size="small"
                    variant="tonal"
                    color="warning"
                    :disabled="!bundle.canStop || isBusy(bundle.symbolicName)"
                    @click="handleBundleCommand(bundle, 'stop')"
                  >
                    停用
                  </v-btn>
                  <v-btn
                    size="small"
                    variant="outlined"
                    color="secondary"
                    @click="toggleBundle(bundle.symbolicName)"
                  >
                    {{ isExpanded(bundle.symbolicName) ? '隐藏配置' : '展开配置' }}
                  </v-btn>
                </div>

                <div v-if="isExpanded(bundle.symbolicName)" class="bundle-config-panel">
                  <div class="bundle-config-head">
                    <div>
                      <strong>配置参数</strong>
                      <p>这里编辑 bundle 偏好参数。核心包只读展示，普通包可以保存。</p>
                    </div>
                    <div class="bundle-chip-row">
                      <v-chip size="small" variant="tonal" color="info">
                        {{ preferenceRows(bundle.symbolicName).length }} 项
                      </v-chip>
                      <v-chip
                        size="small"
                        variant="tonal"
                        :color="dirtyDrafts[bundle.symbolicName] ? 'warning' : 'success'"
                      >
                        {{ dirtyDrafts[bundle.symbolicName] ? '未保存' : '已同步' }}
                      </v-chip>
                    </div>
                  </div>

                  <div class="bundle-config-grid">
                    <div
                      v-for="(entry, index) in preferenceRows(bundle.symbolicName)"
                      :key="`${bundle.symbolicName}-${index}`"
                      class="bundle-config-row"
                    >
                      <v-text-field
                        label="参数名"
                        :model-value="entry.key"
                        :disabled="!bundle.configurable"
                        @update:model-value="updatePreference(bundle.symbolicName, index, 'key', $event)"
                      />
                      <v-text-field
                        label="参数值"
                        :model-value="entry.value"
                        :disabled="!bundle.configurable"
                        @update:model-value="updatePreference(bundle.symbolicName, index, 'value', $event)"
                      />
                      <v-btn
                        size="small"
                        variant="text"
                        color="error"
                        :disabled="!bundle.configurable"
                        @click="removePreference(bundle.symbolicName, index)"
                      >
                        删除
                      </v-btn>
                    </div>
                  </div>

                  <div class="bundle-action-row">
                    <v-btn
                      size="small"
                      variant="tonal"
                      color="secondary"
                      :disabled="!bundle.configurable"
                      @click="addPreference(bundle.symbolicName)"
                    >
                      新增参数
                    </v-btn>
                    <v-btn
                      size="small"
                      variant="flat"
                      color="primary"
                      :disabled="!bundle.configurable || isBusy(bundle.symbolicName)"
                      @click="handleSavePreferences(bundle)"
                    >
                      保存配置
                    </v-btn>
                  </div>
                </div>
              </article>
            </div>

            <div v-else-if="!loading" class="bundle-empty-state">
              当前没有可展示的系统包信息。
            </div>
          </section>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div class="panel-head">
              <div>
                <p class="section-kicker">前端功能包</p>
                <h2>已注册的 WebUI 页面包</h2>
              </div>
            </div>

            <div class="bundle-grid">
              <article
                v-for="featurePackage in featurePackages"
                :key="featurePackage.id"
                class="bundle-card"
              >
                <div class="bundle-card-head">
                  <div class="bundle-heading">
                    <v-icon :icon="featurePackage.icon" size="18"></v-icon>
                    <div>
                      <strong>{{ featurePackage.title }}</strong>
                      <p>{{ featurePackage.name }}</p>
                    </div>
                  </div>
                  <v-chip
                    size="small"
                    variant="tonal"
                    :color="getPackageStatusTone(featurePackage.status)"
                  >
                    {{ formatPackageStatus(featurePackage.status) }}
                  </v-chip>
                </div>

                <p class="bundle-copy">{{ featurePackage.description }}</p>

                <div class="bundle-card-meta">
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
.bundle-stats-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 18px;
  margin-top: 24px;
}

.bundle-stat-card,
.bundle-card,
.bundle-summary-chip {
  border: 1px solid rgba(78, 188, 255, 0.14);
  background: rgba(8, 22, 40, 0.72);
}

.bundle-stat-card {
  padding: 22px;
  border-radius: 22px;
  box-shadow: var(--shell-shadow);
}

.bundle-stat-card p {
  margin: 0 0 10px;
  color: var(--shell-text-muted);
}

.bundle-stat-card strong {
  font-size: 2rem;
}

.bundle-summary-row {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
}

.bundle-summary-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-height: 40px;
  padding: 0 14px;
  border-radius: 999px;
  color: var(--shell-text);
}

.bundle-grid {
  display: grid;
  gap: 16px;
}

.bundle-card {
  padding: 18px;
  border-radius: 18px;
}

.bundle-card-head,
.bundle-card-meta,
.bundle-heading,
.bundle-chip-row,
.bundle-action-row,
.bundle-config-head {
  display: flex;
  gap: 12px;
}

.bundle-card-head,
.bundle-card-meta,
.bundle-config-head {
  justify-content: space-between;
}

.bundle-card-head,
.bundle-config-head {
  align-items: flex-start;
}

.bundle-heading {
  align-items: flex-start;
}

.bundle-heading p,
.bundle-copy {
  margin: 10px 0 0;
  color: var(--shell-text-muted);
}

.bundle-chip-row,
.bundle-action-row {
  flex-wrap: wrap;
}

.bundle-card-meta {
  margin-top: 14px;
  flex-wrap: wrap;
  color: rgba(202, 226, 255, 0.54);
  font-size: 0.84rem;
}

.bundle-path {
  margin-top: 12px;
  padding: 12px 14px;
  border-radius: 14px;
  background: rgba(1, 9, 17, 0.88);
  border: 1px solid rgba(78, 188, 255, 0.1);
  color: rgba(210, 232, 255, 0.78);
  font-family: Consolas, "Courier New", monospace;
  font-size: 0.82rem;
  word-break: break-all;
}

.bundle-note {
  margin-top: 12px;
  padding: 12px 14px;
  border-radius: 14px;
  background: rgba(11, 24, 40, 0.72);
  color: rgba(210, 232, 255, 0.72);
}

.bundle-note-warning {
  border: 1px solid rgba(255, 209, 102, 0.2);
  color: rgba(255, 223, 158, 0.9);
}

.bundle-action-row {
  margin-top: 16px;
}

.bundle-config-panel {
  margin-top: 18px;
  padding: 18px;
  border-radius: 18px;
  border: 1px solid rgba(112, 240, 193, 0.16);
  background: rgba(4, 16, 28, 0.78);
}

.bundle-config-head p {
  margin: 8px 0 0;
  color: var(--shell-text-muted);
}

.bundle-config-grid {
  display: grid;
  gap: 12px;
  margin-top: 16px;
}

.bundle-config-row {
  display: grid;
  grid-template-columns: minmax(220px, 0.9fr) minmax(260px, 1.3fr) auto;
  gap: 12px;
  align-items: center;
}

.bundle-empty-state {
  color: rgba(210, 232, 255, 0.62);
}

@media (max-width: 1160px) {
  .bundle-stats-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 960px) {
  .bundle-summary-row {
    justify-content: flex-start;
  }

  .bundle-config-row {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 640px) {
  .bundle-stats-grid {
    grid-template-columns: 1fr;
  }

  .bundle-card-head,
  .bundle-config-head {
    flex-direction: column;
    align-items: flex-start;
  }
}
</style>
