<script setup>
import { computed, onMounted, reactive, ref, watch, watchEffect } from 'vue'
import { featurePackages as rawFeaturePackages } from './core/packageRegistry'
import { useUiLocale } from './core/locale'
import { localizeFeaturePackage } from './core/packageLocalization.js'
import { refreshSession, sessionState, signOut } from './core/sessionGateway'
import { createUiLocaleHeaders } from './core/requestLocale.js'

const { isZh, toggleLocale } = useUiLocale()
const locale = computed(() => (isZh.value ? 'zh' : 'en'))

const zh = {
  connecting: '正在同步全局配置...',
  synced: '全局配置已同步。',
  failed: '读取全局配置失败，请稍后重试。',
  signOut: '退出登录',
  language: 'EN',
  title: '全局配置中心',
  eyebrow: 'MYIOT 配置',
  copy: '左侧使用树状结构组织系统配置，点击带属性的节点后，右侧会自动生成对应的编辑界面。',
  openLogWindow: '悬浮日志',
  treeTitle: '配置树',
  appConfigPath: '应用配置文件',
  reload: '刷新',
  save: '保存当前节点',
  addEntry: '新增配置项',
  search: '筛选配置项',
  searchPlaceholder: '按键名或值搜索',
  sourcePath: '配置来源',
  helper: '说明',
  editable: '可编辑',
  readonly: '只读',
  key: '键',
  value: '值',
  remove: '删除',
  noEntries: '当前节点还没有配置项，可以直接新增后保存。',
  noSection: '当前没有可展示的配置节点。',
  appScope: '应用',
  bundleScope: 'Bundle',
  state: '状态',
  version: '版本',
  dirty: '未保存',
  clean: '已同步',
  restartHint: '应用级配置保存后，部分设置需要重启服务才会完全生效。',
  saveSuccess: '配置已保存。',
  saveFailed: '保存配置失败。',
  loading: '正在加载配置...',
  documentTitle: 'MyIoT 全局配置中心',
  totalNodes: '节点数',
  totalKeys: '配置项',
  activeBundles: '活动 Bundle',
  appRoot: '应用配置',
  appRootHint: '应用级全局配置和日志配置统一在这里管理。',
  appAll: '全部应用配置',
  loggingConfig: '日志配置',
  loggingConfigHint: '这里只显示并编辑 logging.* 相关配置。',
  bundlesRoot: 'Bundle 配置',
  bundlesRootHint: '这里汇总每个 Bundle 的偏好项、状态和运行控制。',
  selectNode: '请先从左侧树中选择一个节点。',
  groupSelected: '当前选中的是分组节点',
  groupSelectedHint: '继续点击子节点后，右侧会自动生成属性页。',
  childNodes: '子节点',
  selectedNode: '当前节点',
  generatedEditor: '自动生成属性页',
  emptyGroup: '这个分组下暂时没有子节点。',
  runtimeActions: '运行管理',
  resolve: '解析',
  start: '启动',
  stop: '停止',
  vendor: '厂商',
  runLevel: '运行级别',
  extensionBundle: '扩展包',
  manageable: '可管理',
  blockedByDependents: '运行依赖',
  runtimeHint: '这里已经接管原系统包列表的运行管理能力，可直接在当前节点执行解析、启动和停止。',
  runtimeLocked: '当前 Bundle 属于系统核心链路，这里只允许查看，不允许执行启停。',
  noDependents: '无活动依赖',
  yes: '是',
  no: '否'
}

const en = {
  connecting: 'Synchronizing global configuration...',
  synced: 'Global configuration is synchronized.',
  failed: 'Failed to load global configuration. Please retry shortly.',
  signOut: 'Sign Out',
  language: '中文',
  title: 'Global Configuration Center',
  eyebrow: 'MYIOT Config',
  copy: 'The left side uses a tree. Selecting a node with properties automatically generates the editor on the right.',
  openLogWindow: 'Floating Logs',
  treeTitle: 'Configuration Tree',
  appConfigPath: 'Application Config File',
  reload: 'Reload',
  save: 'Save Current Node',
  addEntry: 'Add Entry',
  search: 'Filter Entries',
  searchPlaceholder: 'Search by key or value',
  sourcePath: 'Source',
  helper: 'Notes',
  editable: 'Editable',
  readonly: 'Read-only',
  key: 'Key',
  value: 'Value',
  remove: 'Remove',
  noEntries: 'This node does not contain any entries yet. Add a new row and save it.',
  noSection: 'No configuration nodes are available right now.',
  appScope: 'Application',
  bundleScope: 'Bundle',
  state: 'State',
  version: 'Version',
  dirty: 'Unsaved',
  clean: 'Synced',
  restartHint: 'Application-level settings may need a service restart before every change becomes effective.',
  saveSuccess: 'Configuration saved.',
  saveFailed: 'Failed to save the configuration.',
  loading: 'Loading configuration...',
  documentTitle: 'MyIoT Global Configuration Center',
  totalNodes: 'Nodes',
  totalKeys: 'Keys',
  activeBundles: 'Active Bundles',
  appRoot: 'Application Settings',
  appRootHint: 'Application-wide settings and logging settings are managed here.',
  appAll: 'All Application Settings',
  loggingConfig: 'Logging Settings',
  loggingConfigHint: 'This node only shows and edits logging.* properties.',
  bundlesRoot: 'Bundle Settings',
  bundlesRootHint: 'This branch lists every bundle preference section and runtime state.',
  selectNode: 'Select a node from the tree on the left.',
  groupSelected: 'A group node is selected',
  groupSelectedHint: 'Choose one of the child nodes below and the matching property editor will appear on the right.',
  childNodes: 'Child Nodes',
  selectedNode: 'Current Node',
  generatedEditor: 'Generated Property Editor',
  emptyGroup: 'This group does not contain any child nodes yet.',
  runtimeActions: 'Runtime Actions',
  resolve: 'Resolve',
  start: 'Start',
  stop: 'Stop',
  vendor: 'Vendor',
  runLevel: 'Run Level',
  extensionBundle: 'Extension Bundle',
  manageable: 'Manageable',
  blockedByDependents: 'Running Dependents',
  runtimeHint: 'This panel now absorbs the former bundle list runtime controls, so bundle resolve/start/stop actions happen directly here.',
  runtimeLocked: 'This bundle belongs to the core system path, so runtime actions stay read-only here.',
  noDependents: 'No active dependents',
  yes: 'Yes',
  no: 'No'
}

const text = computed(() => (isZh.value ? zh : en))

watchEffect(() => {
  if (typeof document !== 'undefined') document.title = text.value.documentTitle
})

const featurePackages = computed(() =>
  rawFeaturePackages.map((featurePackage) => localizeFeaturePackage(featurePackage, locale.value))
)
const launchablePackages = computed(() =>
  featurePackages.value.filter((featurePackage) => featurePackage.entryPath)
)

const banner = ref({ type: 'info', text: text.value.connecting })
const loading = ref(false)
const sections = ref([])
const summary = ref({ sectionCount: 0, bundleSectionCount: 0, totalKeys: 0, applicationConfigPath: '--' })
const selectedNodeId = ref('application')
const entrySearch = ref('')
const savingSectionId = ref('')
const managingActionId = ref('')
const drafts = reactive({})
const originalDrafts = reactive({})
const openTreeState = reactive({ 'root:application': true, 'root:bundles': true })

function cloneEntries(entries = []) {
  return entries.map((entry) => ({ key: entry?.key ?? '', value: entry?.value ?? '' }))
}

function entriesFor(sectionId) {
  if (!drafts[sectionId]) drafts[sectionId] = []
  return drafts[sectionId]
}

function normalizedEntries(entries) {
  return entries.map((entry) => ({ key: String(entry?.key ?? '').trim(), value: String(entry?.value ?? '') })).filter((entry) => entry.key)
}

function entrySignature(entries) {
  return JSON.stringify(normalizedEntries(entries))
}

function findSectionById(sectionId) {
  return sections.value.find((section) => section.id === sectionId) ?? null
}

function filterEntriesByPrefix(entries, prefix = '') {
  if (!prefix) return entries
  return entries.filter((entry) => String(entry?.key ?? '').startsWith(prefix))
}

function nodeEntriesSnapshot(node, source = drafts) {
  if (!node) return []
  if (node.kind === 'section') return source[node.sectionId] ?? []
  if (node.kind === 'virtual') return filterEntriesByPrefix(source[node.sourceSectionId] ?? [], node.filterPrefix)
  return []
}

function isDirtyForNode(node) {
  if (!node || node.kind === 'group') return false
  return entrySignature(nodeEntriesSnapshot(node, drafts)) !== entrySignature(nodeEntriesSnapshot(node, originalDrafts))
}

function createTreeNodes(sectionList) {
  const applicationSection = sectionList.find((section) => section.scope === 'application') ?? null
  const bundleSections = sectionList.filter((section) => section.scope === 'bundle')
  const applicationChildren = []

  if (applicationSection) {
    applicationChildren.push({
      id: applicationSection.id,
      kind: 'section',
      title: text.value.appAll,
      icon: 'mdi-file-cog-outline',
      sectionId: applicationSection.id,
      description: applicationSection.helper
    })
    applicationChildren.push({
      id: 'virtual:logging',
      kind: 'virtual',
      title: text.value.loggingConfig,
      icon: 'mdi-text-box-search-outline',
      sourceSectionId: applicationSection.id,
      filterPrefix: 'logging.',
      description: text.value.loggingConfigHint
    })
  }

  return [
    {
      id: 'root:application',
      kind: 'group',
      title: text.value.appRoot,
      icon: 'mdi-application-cog-outline',
      description: text.value.appRootHint,
      children: applicationChildren
    },
    {
      id: 'root:bundles',
      kind: 'group',
      title: text.value.bundlesRoot,
      icon: 'mdi-package-variant-closed',
      description: text.value.bundlesRootHint,
      children: bundleSections.map((section) => ({
        id: section.id,
        kind: 'section',
        title: section.title,
        icon: section.icon || 'mdi-package-variant',
        sectionId: section.id,
        description: section.helper,
        state: section.state
      }))
    }
  ]
}

function collectNodeMap(nodes, map = {}) {
  nodes.forEach((node) => {
    map[node.id] = node
    if (node.children?.length) collectNodeMap(node.children, map)
  })
  return map
}

function firstEditableNodeId(nodes) {
  for (const node of nodes) {
    if (node.kind !== 'group') return node.id
    if (node.children?.length) {
      const childId = firstEditableNodeId(node.children)
      if (childId) return childId
    }
  }
  return ''
}

function applyPayload(payload, preferredNodeId = '') {
  sections.value = payload.sections ?? []
  summary.value = payload.summary ?? { sectionCount: 0, bundleSectionCount: 0, totalKeys: 0, applicationConfigPath: '--' }

  Object.keys(drafts).forEach((key) => delete drafts[key])
  Object.keys(originalDrafts).forEach((key) => delete originalDrafts[key])
  sections.value.forEach((section) => {
    drafts[section.id] = cloneEntries(section.entries)
    originalDrafts[section.id] = cloneEntries(section.entries)
  })

  const nodes = createTreeNodes(sections.value)
  const nodeMap = collectNodeMap(nodes)
  const nextNodeId = preferredNodeId || selectedNodeId.value
  selectedNodeId.value = nodeMap[nextNodeId] ? nextNodeId : firstEditableNodeId(nodes)
}

async function requestJson(path, options = {}) {
  const response = await fetch(path, {
    credentials: 'same-origin',
    headers: createUiLocaleHeaders({ Accept: 'application/json', ...(options.headers || {}) }),
    ...options
  })
  if (response.status === 401) {
    window.location.replace('/myiot/login/index.html')
    throw new Error(text.value.failed)
  }
  const payload = await response.json()
  if (!response.ok) throw new Error(payload.message || `${response.status}`)
  return payload
}

async function loadConfigCenter(preferredNodeId = '') {
  loading.value = true
  try {
    const payload = await requestJson(`/myiot/config/data.json?_=${Date.now()}`)
    applyPayload(payload, preferredNodeId)
    banner.value = { type: 'success', text: payload.message || text.value.synced }
  } catch (error) {
    banner.value = { type: 'error', text: error.message || text.value.failed }
  } finally {
    loading.value = false
  }
}

const treeNodes = computed(() => createTreeNodes(sections.value))
const treeNodeMap = computed(() => collectNodeMap(treeNodes.value))
const selectedTreeNode = computed(() => treeNodeMap.value[selectedNodeId.value] ?? null)
const selectedEditorSection = computed(() => {
  const node = selectedTreeNode.value
  if (!node) return null
  if (node.kind === 'section') return findSectionById(node.sectionId)
  if (node.kind === 'virtual') return findSectionById(node.sourceSectionId)
  return null
})
const selectedBundleDependents = computed(() => {
  const dependents = selectedEditorSection.value?.activeDependents
  return Array.isArray(dependents) ? dependents : []
})
const activeBundleCount = computed(() =>
  sections.value.filter((section) => section.scope === 'bundle' && String(section.state || '').toLowerCase().includes('active')).length
)
const summaryCards = computed(() => ([
  { label: text.value.totalNodes, value: summary.value.sectionCount },
  { label: text.value.totalKeys, value: summary.value.totalKeys },
  { label: text.value.activeBundles, value: activeBundleCount.value }
]))
const visibleEntries = computed(() => {
  const node = selectedTreeNode.value
  if (!node || node.kind === 'group') return []
  const query = entrySearch.value.trim().toLowerCase()
  const sourceSectionId = node.kind === 'virtual' ? node.sourceSectionId : node.sectionId
  return (entriesFor(sourceSectionId) ?? [])
    .map((entry, index) => ({ entry, index }))
    .filter(({ entry }) => {
      if (node.kind === 'virtual' && !String(entry.key || '').startsWith(node.filterPrefix)) return false
      if (!query) return true
      return String(entry.key).toLowerCase().includes(query) || String(entry.value).toLowerCase().includes(query)
    })
})

function toggleTreeGroup(nodeId) {
  openTreeState[nodeId] = !openTreeState[nodeId]
}

function isTreeGroupOpen(nodeId) {
  return openTreeState[nodeId] !== false
}

function selectNode(node) {
  selectedNodeId.value = node.id
}

function bundleStateTone(state) {
  const normalized = String(state || '').toLowerCase()
  if (normalized.includes('active')) return 'success'
  if (normalized.includes('resolved')) return 'info'
  if (normalized.includes('installed')) return 'warning'
  return 'secondary'
}

function bundleActionKey(sectionId, action) {
  return `${sectionId}:${action}`
}

async function runBundleAction(action) {
  const section = selectedEditorSection.value
  if (!section || section.scope !== 'bundle' || !section.target) return

  managingActionId.value = bundleActionKey(section.id, action)
  try {
    const body = new URLSearchParams()
    body.set('sectionId', section.id)
    body.set('target', section.target)
    body.set('action', action)
    const payload = await requestJson('/myiot/config/manage.json', { method: 'POST', body })
    banner.value = { type: 'success', text: payload.message || text.value.synced }
    await loadConfigCenter(section.id)
  } catch (error) {
    banner.value = { type: 'error', text: error.message || text.value.failed }
  } finally {
    managingActionId.value = ''
  }
}

function addEntry() {
  const node = selectedTreeNode.value
  const section = selectedEditorSection.value
  if (!node || !section || !section.editable || node.kind === 'group') return
  entriesFor(section.id).push({ key: node.kind === 'virtual' ? node.filterPrefix : '', value: '' })
}

function removeEntry(index) {
  const section = selectedEditorSection.value
  if (!section || !section.editable) return
  entriesFor(section.id).splice(index, 1)
}

async function saveSelectedNode() {
  const node = selectedTreeNode.value
  const section = selectedEditorSection.value
  if (!node || !section || !section.editable || node.kind === 'group') return

  savingSectionId.value = section.id
  try {
    const body = new URLSearchParams()
    body.set('sectionId', section.id)
    body.set('scope', section.scope)
    body.set('target', section.target || '')
    body.set('entries', JSON.stringify(normalizedEntries(entriesFor(section.id))))
    const payload = await requestJson('/myiot/config/save.json', { method: 'POST', body })
    banner.value = { type: 'success', text: payload.message || text.value.saveSuccess }
    await loadConfigCenter(node.id)
  } catch (error) {
    banner.value = { type: 'error', text: error.message || text.value.saveFailed }
  } finally {
    savingSectionId.value = ''
  }
}

function openPopupWindow(url, name, width = 1180, height = 820) {
  const left = Math.max(Math.round((window.screen.width - width) / 2), 32)
  const top = Math.max(Math.round((window.screen.height - height) / 2), 32)
  const features = ['popup=yes', 'resizable=yes', 'scrollbars=yes', 'toolbar=no', 'menubar=no', 'location=no', 'status=no', `width=${width}`, `height=${height}`, `left=${left}`, `top=${top}`].join(',')
  return window.open(url, name, features)
}

function openLogWindow() {
  const popup = openPopupWindow('/myiot/logs/index.html?popup=1', 'myiot-log-viewer')
  if (!popup) window.open('/myiot/logs/index.html', '_blank', 'noopener,noreferrer')
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
  banner.value = { type: 'success', text: text.value.synced }
  await loadConfigCenter('application')
})

watch(selectedNodeId, () => {
  entrySearch.value = ''
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
              <p class="eyebrow">{{ text.eyebrow }}</p>
              <h1>{{ text.title }}</h1>
              <p class="brand-copy">{{ text.copy }}</p>
            </div>
          </div>

          <div class="header-pills">
            <v-btn variant="outlined" color="primary" size="small" @click="toggleLocale">
              {{ text.language }}
            </v-btn>
            <v-btn variant="outlined" color="info" size="small" @click="openLogWindow">
              {{ text.openLogWindow }}
            </v-btn>
            <div class="meta-pill">
              <v-icon icon="mdi-account-circle-outline" size="18"></v-icon>
              <span>{{ sessionState.username }}</span>
            </div>
            <v-btn variant="tonal" color="secondary" size="small" class="logout-button" @click="handleSignOut">
              {{ text.signOut }}
            </v-btn>
          </div>
        </header>

        <div class="package-strip">
          <a
            v-for="featurePackage in launchablePackages"
            :key="featurePackage.id"
            :href="featurePackage.entryPath"
            class="meta-pill meta-pill-link"
            :class="{ 'meta-pill-active': featurePackage.id === 'myiot.global-config' }"
          >
            <v-icon :icon="featurePackage.icon" size="18"></v-icon>
            <span>{{ featurePackage.name }}</span>
          </a>
        </div>

        <v-alert :type="banner.type" variant="tonal" border="start" class="mt-5">
          {{ banner.text }}
        </v-alert>

        <div class="layout-grid">
          <aside class="side-column">
            <section class="feature-panel">
              <div class="feature-frame"></div>

              <div class="panel-head">
                <div>
                  <p class="section-kicker">{{ text.treeTitle }}</p>
                  <h2>{{ text.treeTitle }}</h2>
                </div>
                <v-btn variant="text" color="primary" :loading="loading" @click="loadConfigCenter(selectedNodeId)">
                  {{ text.reload }}
                </v-btn>
              </div>

              <div class="status-card-list summary-grid">
                <div v-for="card in summaryCards" :key="card.label" class="status-card">
                  <span>{{ card.label }}</span>
                  <strong>{{ card.value }}</strong>
                </div>
              </div>

              <div class="status-card">
                <span>{{ text.appConfigPath }}</span>
                <strong class="path-value">{{ summary.applicationConfigPath || '--' }}</strong>
              </div>

              <div v-if="treeNodes.length" class="tree-shell">
                <section v-for="group in treeNodes" :key="group.id" class="tree-group">
                  <button
                    type="button"
                    class="tree-row tree-row-group"
                    :class="{ 'tree-row-active': selectedNodeId === group.id }"
                    @click="selectNode(group)"
                  >
                    <div class="tree-row-main">
                      <span class="tree-toggle" @click.stop="toggleTreeGroup(group.id)">
                        <v-icon :icon="isTreeGroupOpen(group.id) ? 'mdi-chevron-down' : 'mdi-chevron-right'" size="18"></v-icon>
                      </span>
                      <v-icon :icon="group.icon" size="18"></v-icon>
                      <span>{{ group.title }}</span>
                    </div>
                    <v-chip size="x-small" variant="tonal" color="info">
                      {{ group.children?.length ?? 0 }}
                    </v-chip>
                  </button>

                  <div v-if="isTreeGroupOpen(group.id)" class="tree-children">
                    <button
                      v-for="child in group.children"
                      :key="child.id"
                      type="button"
                      class="tree-row tree-row-leaf"
                      :class="{ 'tree-row-active': selectedNodeId === child.id }"
                      @click="selectNode(child)"
                    >
                      <div class="tree-row-main">
                        <span class="tree-branch-line" aria-hidden="true"></span>
                        <v-icon :icon="child.icon" size="18"></v-icon>
                        <span>{{ child.title }}</span>
                      </div>
                      <v-chip
                        size="x-small"
                        variant="tonal"
                        :color="isDirtyForNode(child) ? 'warning' : 'success'"
                      >
                        {{ isDirtyForNode(child) ? text.dirty : text.clean }}
                      </v-chip>
                    </button>
                  </div>
                </section>
              </div>

              <div v-else class="empty-state compact-empty">
                {{ loading ? text.loading : text.noSection }}
              </div>
            </section>
          </aside>

          <section class="feature-panel">
            <div class="feature-frame"></div>

            <div v-if="selectedTreeNode" class="editor-shell">
              <div v-if="selectedTreeNode.kind === 'group'" class="editor-shell">
                <div class="panel-head">
                  <div>
                    <p class="section-kicker">{{ text.selectedNode }}</p>
                    <h2>{{ selectedTreeNode.title }}</h2>
                    <p class="panel-copy">{{ selectedTreeNode.description }}</p>
                  </div>
                  <div class="meta-pill">
                    <v-icon icon="mdi-file-tree-outline" size="18"></v-icon>
                    <span>{{ selectedTreeNode.children?.length ?? 0 }}</span>
                  </div>
                </div>

                <div class="detail-grid">
                  <div class="status-card">
                    <span>{{ text.groupSelected }}</span>
                    <strong>{{ selectedTreeNode.title }}</strong>
                  </div>
                  <div class="status-card">
                    <span>{{ text.childNodes }}</span>
                    <strong>{{ selectedTreeNode.children?.length ?? 0 }}</strong>
                  </div>
                </div>

                <v-alert type="info" variant="tonal" border="start">
                  {{ text.groupSelectedHint }}
                </v-alert>

                <div v-if="selectedTreeNode.children?.length" class="child-node-list">
                  <button
                    v-for="child in selectedTreeNode.children"
                    :key="child.id"
                    type="button"
                    class="child-node-card"
                    @click="selectNode(child)"
                  >
                    <div class="section-title-group">
                      <v-icon :icon="child.icon" size="18"></v-icon>
                      <strong>{{ child.title }}</strong>
                    </div>
                    <p>{{ child.description }}</p>
                  </button>
                </div>

                <div v-else class="empty-state compact-empty">
                  {{ text.emptyGroup }}
                </div>
              </div>

              <div v-else class="editor-shell">
                <div class="panel-head">
                  <div>
                    <p class="section-kicker">{{ text.generatedEditor }}</p>
                    <h2>{{ selectedTreeNode.title }}</h2>
                    <p class="panel-copy">{{ selectedTreeNode.description }}</p>
                  </div>
                  <div class="header-pills">
                    <div class="meta-pill">
                      <v-icon icon="mdi-file-cog-outline" size="18"></v-icon>
                      <span>{{ visibleEntries.length }}</span>
                    </div>
                    <div class="meta-pill" v-if="selectedEditorSection?.state">
                      <v-icon icon="mdi-lan-check" size="18"></v-icon>
                      <span>{{ selectedEditorSection.state }}</span>
                    </div>
                  </div>
                </div>

                <div class="detail-grid">
                  <div class="status-card">
                    <span>{{ text.sourcePath }}</span>
                    <strong class="path-value">{{ selectedEditorSection?.sourcePath || '--' }}</strong>
                  </div>
                  <div class="status-card">
                    <span>{{ text.helper }}</span>
                    <strong>{{ selectedEditorSection?.editable ? text.editable : text.readonly }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.state">
                    <span>{{ text.state }}</span>
                    <strong>{{ selectedEditorSection.state }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.version">
                    <span>{{ text.version }}</span>
                    <strong>{{ selectedEditorSection.version }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.scope === 'bundle'">
                    <span>{{ text.vendor }}</span>
                    <strong>{{ selectedEditorSection.vendor || '--' }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.scope === 'bundle'">
                    <span>{{ text.runLevel }}</span>
                    <strong>{{ selectedEditorSection.runLevel ?? '--' }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.scope === 'bundle'">
                    <span>{{ text.extensionBundle }}</span>
                    <strong>{{ selectedEditorSection.extensionBundle ? text.yes : text.no }}</strong>
                  </div>
                  <div class="status-card" v-if="selectedEditorSection?.scope === 'bundle'">
                    <span>{{ text.manageable }}</span>
                    <strong>{{ selectedEditorSection.manageable ? text.yes : text.no }}</strong>
                  </div>
                </div>

                <v-alert v-if="selectedEditorSection?.scope === 'application'" type="warning" variant="tonal" class="mt-4">
                  {{ text.restartHint }}
                </v-alert>

                <v-alert v-if="selectedEditorSection?.scope === 'bundle'" type="info" variant="tonal" class="mt-4">
                  {{ text.runtimeHint }}
                </v-alert>

                <v-alert
                  v-if="selectedEditorSection?.scope === 'bundle' && !selectedEditorSection?.manageable"
                  type="warning"
                  variant="tonal"
                  class="mt-4"
                >
                  {{ selectedEditorSection?.manageReason || text.runtimeLocked }}
                </v-alert>

                <v-alert
                  v-if="selectedEditorSection?.scope === 'bundle' && selectedBundleDependents.length"
                  type="error"
                  variant="tonal"
                  class="mt-4"
                >
                  {{ text.blockedByDependents }}: {{ selectedBundleDependents.join(', ') }}
                </v-alert>

                <div v-if="selectedEditorSection?.scope === 'bundle'" class="runtime-toolbar">
                  <div class="runtime-toolbar-copy">
                    <p class="section-kicker">{{ text.runtimeActions }}</p>
                    <strong>{{ selectedBundleDependents.length ? text.blockedByDependents : text.noDependents }}</strong>
                  </div>
                  <div class="runtime-actions">
                    <v-btn
                      color="info"
                      variant="outlined"
                      :loading="managingActionId === bundleActionKey(selectedEditorSection.id, 'resolve')"
                      :disabled="!selectedEditorSection?.canResolve"
                      @click="runBundleAction('resolve')"
                    >
                      {{ text.resolve }}
                    </v-btn>
                    <v-btn
                      color="success"
                      variant="outlined"
                      :loading="managingActionId === bundleActionKey(selectedEditorSection.id, 'start')"
                      :disabled="!selectedEditorSection?.canStart"
                      @click="runBundleAction('start')"
                    >
                      {{ text.start }}
                    </v-btn>
                    <v-btn
                      color="error"
                      variant="outlined"
                      :loading="managingActionId === bundleActionKey(selectedEditorSection.id, 'stop')"
                      :disabled="!selectedEditorSection?.canStop"
                      @click="runBundleAction('stop')"
                    >
                      {{ text.stop }}
                    </v-btn>
                  </div>
                </div>

                <div class="editor-toolbar">
                  <v-text-field
                    v-model="entrySearch"
                    :label="text.search"
                    :placeholder="text.searchPlaceholder"
                    prepend-inner-icon="mdi-magnify"
                    hide-details
                  />
                  <v-btn color="primary" variant="outlined" :disabled="!selectedEditorSection?.editable" @click="addEntry">
                    {{ text.addEntry }}
                  </v-btn>
                  <v-btn
                    color="secondary"
                    :loading="savingSectionId === selectedEditorSection?.id"
                    :disabled="!selectedEditorSection?.editable || !isDirtyForNode(selectedTreeNode)"
                    @click="saveSelectedNode"
                  >
                    {{ text.save }}
                  </v-btn>
                </div>

                <div v-if="visibleEntries.length" class="entry-list">
                  <article v-for="{ entry, index } in visibleEntries" :key="`${selectedNodeId}-${index}`" class="entry-card">
                    <v-text-field
                      v-model="entry.key"
                      :label="text.key"
                      :disabled="!selectedEditorSection?.editable"
                      hide-details
                    />
                    <v-textarea
                      v-model="entry.value"
                      :label="text.value"
                      :disabled="!selectedEditorSection?.editable"
                      auto-grow
                      rows="1"
                      hide-details
                    />
                    <v-btn variant="text" color="error" :disabled="!selectedEditorSection?.editable" @click="removeEntry(index)">
                      {{ text.remove }}
                    </v-btn>
                  </article>
                </div>

                <div v-else class="empty-state editor-empty">
                  {{ text.noEntries }}
                </div>
              </div>
            </div>

            <div v-else class="empty-state editor-empty">
              {{ loading ? text.loading : text.selectNode }}
            </div>
          </section>
        </div>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.package-strip {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin-top: 18px;
}

.layout-grid {
  display: grid;
  grid-template-columns: minmax(340px, 0.92fr) minmax(0, 1.48fr);
  gap: 20px;
  margin-top: 24px;
}

.side-column,
.status-card-list,
.detail-grid,
.entry-list,
.editor-shell,
.tree-shell,
.child-node-list {
  display: grid;
  gap: 16px;
}

.feature-panel {
  position: relative;
  display: grid;
  gap: 16px;
  padding: 24px;
  border-radius: 28px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background:
    linear-gradient(180deg, rgba(10, 23, 41, 0.94), rgba(6, 16, 30, 0.98)),
    radial-gradient(circle at top right, rgba(107, 210, 255, 0.08), transparent 42%);
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.22);
  overflow: hidden;
}

.feature-frame {
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid rgba(112, 240, 193, 0.08);
  pointer-events: none;
}

.panel-head,
.editor-toolbar,
.runtime-toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.panel-head {
  align-items: flex-start;
}

.panel-head h2 {
  margin-top: 4px;
}

.panel-copy {
  margin: 10px 0 0;
  color: rgba(210, 232, 255, 0.7);
}

.meta-pill-link {
  color: #d8ecff;
  text-decoration: none;
}

.meta-pill-active {
  border-color: rgba(112, 240, 193, 0.34);
  color: #70f0c1;
}

.summary-grid,
.detail-grid {
  grid-template-columns: repeat(2, minmax(0, 1fr));
}

.status-card,
.child-node-card {
  display: grid;
  gap: 8px;
  padding: 16px 18px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
}

.status-card span,
.child-node-card p {
  margin: 0;
  color: rgba(210, 232, 255, 0.62);
}

.status-card strong,
.child-node-card strong {
  color: #eef7ff;
}

.path-value {
  word-break: break-all;
}

.tree-group {
  display: grid;
  gap: 10px;
}

.tree-children {
  display: grid;
  gap: 10px;
  padding-left: 18px;
}

.tree-row,
.child-node-card {
  width: 100%;
  text-align: left;
  cursor: pointer;
  transition: border-color 160ms ease, transform 160ms ease, box-shadow 160ms ease;
}

.tree-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 14px 16px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 18px;
  background: rgba(5, 16, 29, 0.72);
  color: #e6f4ff;
}

.tree-row:hover,
.tree-row-active,
.child-node-card:hover {
  border-color: rgba(112, 240, 193, 0.32);
  box-shadow: inset 0 0 0 1px rgba(112, 240, 193, 0.2), 0 0 24px rgba(58, 216, 255, 0.08);
  transform: translateY(-1px);
}

.tree-row-main,
.section-title-group {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  min-width: 0;
}

.tree-row-main span:last-child {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.tree-branch-line {
  width: 10px;
  height: 1px;
  background: rgba(112, 240, 193, 0.35);
}

.tree-row-group {
  font-weight: 700;
}

.tree-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 22px;
  height: 22px;
  border-radius: 999px;
}

.editor-toolbar {
  flex-wrap: wrap;
  align-items: stretch;
}

.runtime-toolbar {
  flex-wrap: wrap;
  align-items: center;
  padding: 16px 18px;
  border-radius: 20px;
  border: 1px solid rgba(112, 240, 193, 0.16);
  background: linear-gradient(135deg, rgba(9, 23, 38, 0.92), rgba(9, 31, 50, 0.78));
}

.runtime-toolbar-copy {
  display: grid;
  gap: 6px;
}

.runtime-toolbar-copy strong {
  color: #eef7ff;
}

.runtime-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}

.editor-toolbar :deep(.v-input) {
  min-width: min(100%, 320px);
  flex: 1 1 260px;
}

.entry-card {
  display: grid;
  grid-template-columns: minmax(220px, 0.8fr) minmax(0, 1.4fr) auto;
  gap: 14px;
  align-items: start;
  padding: 18px;
  border-radius: 20px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  background: rgba(5, 16, 29, 0.72);
}

.empty-state {
  display: grid;
  place-items: center;
  min-height: 160px;
  padding: 24px;
  text-align: center;
  color: rgba(214, 225, 245, 0.68);
  border: 1px dashed rgba(93, 150, 221, 0.24);
  border-radius: 22px;
  background: rgba(7, 17, 31, 0.72);
}

.compact-empty {
  min-height: 120px;
}

.editor-empty {
  min-height: 320px;
}

@media (max-width: 1200px) {
  .layout-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 960px) {
  .summary-grid,
  .detail-grid,
  .entry-card {
    grid-template-columns: 1fr;
  }

  .runtime-actions {
    width: 100%;
  }
}
</style>
