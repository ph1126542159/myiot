<script setup>
import { computed, onMounted, ref } from 'vue'
import { featurePackages, formatPackageStatus, getPackageStatusTone } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'

const { isZh, toggleLocale } = useUiLocale()

const zh = {
  preparing: '正在同步会话并准备工作台...',
  welcome: (username) => `欢迎回来，${username}。主页工作台已经就绪。`,
  homeEyebrow: 'MYIOT 首页',
  homeTitle: '工业控制工作台',
  homeCopy: '主页负责聚合导航、系统状态和诊断入口。实时日志已拆分为独立窗口，操作其他页面时也能持续观察输出。',
  packageCount: '个功能包',
  activeCount: '个已启用',
  signOut: '退出登录',
  language: 'EN',
  catalog: '功能目录',
  installedModules: '已安装模块',
  bundleList: '系统包列表',
  openBundleCatalog: '打开包目录',
  bundleCatalogDesc: '查看当前 OSP Bundle 的安装与运行状态。',
  liveLogs: '实时日志',
  openLogWindow: '打开日志窗口',
  openLogWindowDesc: '将实时日志窗口独立打开，便于边操作边观察输出。',
  open: '打开',
  home: '主页',
  overviewTitle: '控制台总览',
  workspace: '工作台',
  operatorHome: 'MyIoT 操作主页',
  workspaceCopy: '这个主页把主要包入口集中起来，并把日志预览拆分成独立包。打开一次日志窗口后，就可以在硬件页、监控页或进程控制台之间继续工作。',
  sessionLink: '会话链路',
  stable: '稳定',
  packagesOnline: '在线功能包',
  accessProtocol: '访问协议',
  currentUser: '当前用户',
  controlPlane: '控制面',
  online: '在线',
  launchablePackages: '可打开页面数',
  platform: '平台状态',
  realtimeTelemetry: '实时遥测',
  telemetryCopy: '右侧展示当前会话和功能包概况，同时保留日志入口，不再让首页被大块日志区域占满。',
  modulesReady: '模块就绪',
  authentication: '鉴权状态',
  connected: '已连接',
  entrypoints: '入口注册',
  registered: '已完成',
  expandableSlots: '扩展位',
  startupTrace: '启动轨迹',
  diagnostics: '诊断入口',
  dedicatedLogWindow: '独立日志预览窗口',
  independentWindow: '独立窗口',
  parallelWithOps: '可与操作页面并行',
  diagnosticsCopy: '实时日志已经拆分为专用功能包。打开后可与 JNDM123、系统包管理或进程控制台并排查看。',
  openRealtimeLogPreview: '打开实时日志预览',
  openRealtimeLogPreviewDesc: '启动独立日志窗口，在另一个页面工作时也能持续观察输出。',
  processConsole: '进程控制台',
  openProcessDiagnostics: '打开进程诊断页',
  openProcessDiagnosticsDesc: '需要交互式排查时，可进入独立的命令式诊断页面。',
  reviewRuntimeBundles: '查看运行包状态',
  reviewRuntimeBundlesDesc: '从系统包列表查看功能包版本、状态和入口信息。',
  trace: [
    '会话已恢复并校验完成。',
    '主页工作台初始化完成。',
    'Bundle 目录已刷新。',
    '诊断入口已准备就绪。'
  ]
}

const en = {
  preparing: 'Synchronizing the session and preparing the workspace...',
  welcome: (username) => `Welcome back, ${username}. The home workspace is ready.`,
  homeEyebrow: 'MYIOT Home',
  homeTitle: 'Industrial Control Workspace',
  homeCopy: 'The home package keeps navigation, system status, and diagnostics entry points together. Live logs now run in a dedicated window so operators can keep watching output while using other pages.',
  packageCount: 'packages',
  activeCount: 'active',
  signOut: 'Sign Out',
  language: '中文',
  catalog: 'Catalog',
  installedModules: 'Installed Modules',
  bundleList: 'Bundle List',
  openBundleCatalog: 'Open Bundle Catalog',
  bundleCatalogDesc: 'Review installed OSP bundles and runtime status.',
  liveLogs: 'Live Logs',
  openLogWindow: 'Open Log Window',
  openLogWindowDesc: 'Keep the realtime log viewer open beside operation pages.',
  open: 'Open',
  home: 'Home',
  overviewTitle: 'Control Console Overview',
  workspace: 'Workspace',
  operatorHome: 'MyIoT operator home',
  workspaceCopy: 'This landing page keeps the main packages within reach and moves live log preview to a dedicated package. Open the log window once and keep it visible while you continue working in hardware, monitoring, or process-console pages.',
  sessionLink: 'Session Link',
  stable: 'Stable',
  packagesOnline: 'Packages Online',
  accessProtocol: 'Access Protocol',
  currentUser: 'Current User',
  controlPlane: 'Control Plane',
  online: 'Online',
  launchablePackages: 'Launchable Packages',
  platform: 'Platform',
  realtimeTelemetry: 'Realtime telemetry',
  telemetryCopy: 'The right column summarizes session state and available modules. It also keeps the live log entry close so diagnostics remain available without taking over the home page.',
  modulesReady: 'modules ready',
  authentication: 'Authentication',
  connected: 'Connected',
  entrypoints: 'Entrypoints',
  registered: 'Registered',
  expandableSlots: 'Expandable Slots',
  startupTrace: 'Startup Trace',
  diagnostics: 'Diagnostics',
  dedicatedLogWindow: 'Dedicated Log Preview Window',
  independentWindow: 'Independent window',
  parallelWithOps: 'Parallel with operations',
  diagnosticsCopy: 'Live log preview is now a dedicated package. Open it once and place it beside the current operation page so you can keep watching output while adjusting hardware or using diagnostics.',
  openRealtimeLogPreview: 'Open realtime log preview',
  openRealtimeLogPreviewDesc: 'Launch the dedicated log window and keep it visible while you work in another package.',
  processConsole: 'Process Console',
  openProcessDiagnostics: 'Open process diagnostics',
  openProcessDiagnosticsDesc: 'Use command-based diagnostics in a dedicated page when you need interactive inspection.',
  reviewRuntimeBundles: 'Review runtime bundles',
  reviewRuntimeBundlesDesc: 'Check package versions, status, and available entry points from the system bundle list.',
  trace: [
    'Session restored and verified.',
    'Home workspace initialized.',
    'Bundle catalog refreshed.',
    'Diagnostic entry points are ready.'
  ]
}

const text = computed(() => (isZh.value ? zh : en))

const banner = ref({
  type: 'info',
  text: text.value.preparing
})

const readyPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.status === 'active').length
)

const downstreamPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.id !== 'myiot.home')
)

const bundleListPackage = computed(() =>
  featurePackages.find((featurePackage) => featurePackage.id === 'myiot.bundle-list') ?? null
)

const logViewerPackage = computed(() =>
  featurePackages.find((featurePackage) => featurePackage.id === 'myiot.log-viewer') ?? null
)

const processConsolePackage = computed(() =>
  featurePackages.find((featurePackage) => featurePackage.id === 'myiot.process-console-ui') ?? null
)

const registryPackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.id !== 'myiot.home')
)

const launchablePackages = computed(() =>
  featurePackages.filter((featurePackage) => featurePackage.entryPath)
)

const signalItems = computed(() => [
  { label: text.value.sessionLink, value: text.value.stable, icon: 'mdi-lan-connect', tone: 'primary' },
  { label: text.value.packagesOnline, value: `${featurePackages.length}`, icon: 'mdi-layers-triple-outline', tone: 'secondary' },
  { label: text.value.accessProtocol, value: (sessionState.accessProtocol || 'http').toUpperCase(), icon: 'mdi-shield-check-outline', tone: 'info' }
])

const timeline = computed(() => ([
  { time: '00.8 ms', event: text.value.trace[0] },
  { time: '01.6 ms', event: text.value.trace[1] },
  { time: '02.4 ms', event: text.value.trace[2] },
  { time: '03.1 ms', event: text.value.trace[3] }
]))

async function handleSignOut() {
  await signOut()
  window.location.replace('/myiot/login/index.html')
}

function openLogViewer() {
  const target = logViewerPackage.value?.entryPath || '/myiot/logs/index.html'
  window.open(target, '_blank', 'noopener,noreferrer')
}

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
})
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
              <p class="eyebrow">{{ text.homeEyebrow }}</p>
              <h1>{{ text.homeTitle }}</h1>
              <p class="brand-copy">{{ text.homeCopy }}</p>
            </div>
          </div>

          <div class="header-pills">
            <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
              {{ text.language }}
            </v-btn>
            <div class="meta-pill">
              <v-icon icon="mdi-layers-triple-outline" size="18"></v-icon>
              <span>{{ featurePackages.length }} {{ text.packageCount }}</span>
            </div>
            <div class="meta-pill">
              <v-icon icon="mdi-check-decagram-outline" size="18"></v-icon>
              <span>{{ readyPackages }} {{ text.activeCount }}</span>
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
              {{ text.signOut }}
            </v-btn>
          </div>
        </header>

        <div class="shell-grid">
          <aside class="registry-rail">
            <div class="panel-head">
              <div>
                <p class="section-kicker">{{ text.catalog }}</p>
                <h2>{{ text.installedModules }}</h2>
              </div>
            </div>

            <div v-if="bundleListPackage" class="rail-action">
              <p class="section-kicker">{{ text.bundleList }}</p>
              <a :href="bundleListPackage.entryPath" class="rail-action-button">
                <span class="rail-action-copy">
                  <strong>{{ text.openBundleCatalog }}</strong>
                  <small>{{ text.bundleCatalogDesc }}</small>
                </span>
                <v-icon :icon="bundleListPackage.icon" size="20"></v-icon>
              </a>
            </div>

            <div v-if="logViewerPackage" class="rail-action">
              <p class="section-kicker">{{ text.liveLogs }}</p>
              <button type="button" class="rail-action-button rail-action-button-inline" @click="openLogViewer">
                <span class="rail-action-copy">
                  <strong>{{ text.openLogWindow }}</strong>
                  <small>{{ text.openLogWindowDesc }}</small>
                </span>
                <v-icon :icon="logViewerPackage.icon" size="20"></v-icon>
              </button>
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
                  {{ text.open }}
                </a>
              </div>
            </div>
          </aside>

          <main class="viewport-panel">
            <div class="panel-head panel-head-inline">
              <div>
                <p class="section-kicker">{{ text.home }}</p>
                <h2>{{ text.overviewTitle }}</h2>
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

                <p class="section-kicker">{{ text.workspace }}</p>
                <div class="title-line">
                  <h3>{{ text.operatorHome }}</h3>
                  <div class="pulse-dot"></div>
                </div>

                <p class="section-copy">{{ text.workspaceCopy }}</p>

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
                        <p>{{ text.currentUser }}</p>
                        <strong>{{ sessionState.username }}</strong>
                      </div>
                    </div>
                    <div class="signal-card">
                      <v-icon icon="mdi-door-open" color="primary" size="22"></v-icon>
                      <div>
                        <p>{{ text.controlPlane }}</p>
                        <strong>{{ text.online }}</strong>
                      </div>
                    </div>
                    <div class="signal-card">
                      <v-icon icon="mdi-view-grid-plus-outline" color="info" size="22"></v-icon>
                      <div>
                        <p>{{ text.launchablePackages }}</p>
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

                  <p class="section-kicker">{{ text.platform }}</p>
                  <div class="title-line">
                    <h3>{{ text.realtimeTelemetry }}</h3>
                  </div>
                  <p class="section-copy">{{ text.telemetryCopy }}</p>

                  <div class="telemetry-ring mt-6">
                    <div class="ring-core">
                      <span>{{ featurePackages.length }}</span>
                      <small>{{ text.modulesReady }}</small>
                    </div>
                  </div>

                  <div class="d-grid ga-3 mt-4">
                    <div class="telemetry-item">
                      <span>{{ text.authentication }}</span>
                      <strong>{{ text.connected }}</strong>
                    </div>
                    <div class="telemetry-item">
                      <span>{{ text.entrypoints }}</span>
                      <strong>{{ text.registered }}</strong>
                    </div>
                    <div class="telemetry-item">
                      <span>{{ text.expandableSlots }}</span>
                      <strong>{{ downstreamPackages.length }}</strong>
                    </div>
                  </div>
                </section>

                <section class="feature-panel">
                  <div class="feature-frame"></div>

                  <p class="section-kicker">{{ text.startupTrace }}</p>
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
                  <p class="section-kicker">{{ text.diagnostics }}</p>
                  <h2>{{ text.dedicatedLogWindow }}</h2>
                </div>
                <div class="header-pills">
                  <div class="meta-pill">
                    <v-icon icon="mdi-open-in-new" size="18"></v-icon>
                    <span>{{ text.independentWindow }}</span>
                  </div>
                  <div class="meta-pill">
                    <v-icon icon="mdi-console-network-outline" size="18"></v-icon>
                    <span>{{ text.parallelWithOps }}</span>
                  </div>
                </div>
              </div>

              <p class="section-copy">{{ text.diagnosticsCopy }}</p>

              <div class="utility-grid mt-6">
                <button type="button" class="utility-card utility-card-primary" @click="openLogViewer">
                  <div class="utility-row">
                    <v-icon icon="mdi-file-document-refresh-outline" size="24"></v-icon>
                    <span>{{ text.liveLogs }}</span>
                  </div>
                  <strong>{{ text.openRealtimeLogPreview }}</strong>
                  <p>{{ text.openRealtimeLogPreviewDesc }}</p>
                </button>

                <a
                  v-if="processConsolePackage?.entryPath"
                  :href="processConsolePackage.entryPath"
                  class="utility-card"
                >
                  <div class="utility-row">
                    <v-icon icon="mdi-console-network-outline" size="24"></v-icon>
                    <span>{{ text.processConsole }}</span>
                  </div>
                  <strong>{{ text.openProcessDiagnostics }}</strong>
                  <p>{{ text.openProcessDiagnosticsDesc }}</p>
                </a>

                <a
                  v-if="bundleListPackage?.entryPath"
                  :href="bundleListPackage.entryPath"
                  class="utility-card"
                >
                  <div class="utility-row">
                    <v-icon icon="mdi-package-variant-closed" size="24"></v-icon>
                    <span>{{ text.bundleList }}</span>
                  </div>
                  <strong>{{ text.reviewRuntimeBundles }}</strong>
                  <p>{{ text.reviewRuntimeBundlesDesc }}</p>
                </a>
              </div>
            </section>
          </main>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.rail-action {
  margin-bottom: 18px;
  padding: 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
}

.rail-action-button {
  display: flex;
  width: 100%;
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

.rail-action-button-inline {
  cursor: pointer;
}

.rail-action-copy {
  display: grid;
  gap: 6px;
  text-align: left;
}

.rail-action-copy small {
  color: rgba(210, 232, 255, 0.62);
}

.utility-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 18px;
}

.utility-card {
  display: grid;
  gap: 12px;
  padding: 22px;
  border: 1px solid rgba(78, 188, 255, 0.14);
  border-radius: 22px;
  background: rgba(8, 22, 40, 0.72);
  color: #e6f4ff;
  text-align: left;
  box-shadow: inset 0 0 0 1px rgba(58, 216, 255, 0.06);
}

.utility-card-primary {
  cursor: pointer;
  background: linear-gradient(135deg, rgba(16, 54, 74, 0.92), rgba(6, 20, 36, 0.86));
  border-color: rgba(112, 240, 193, 0.24);
}

.utility-row {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  color: #70f0c1;
}

.utility-card p {
  margin: 0;
  color: rgba(210, 232, 255, 0.66);
  line-height: 1.7;
}

@media (max-width: 1100px) {
  .utility-grid {
    grid-template-columns: 1fr;
  }
}
</style>
