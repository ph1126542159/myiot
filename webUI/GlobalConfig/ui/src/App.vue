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

function countEntriesByPrefix(entries, prefix = '') {
  return filterEntriesByPrefix(entries, prefix).length
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

function formatTreeLabel(segment) {
  const normalized = String(segment ?? '').trim()
  if (!normalized) return 'root'

  const knownLabels = {
    io: 'IO',
    osp: 'OSP',
    js: 'JS',
    net: 'Net',
    appinf: 'AppInf',
    myiot: 'MyIoT',
    webui: 'WebUI',
    webevent: 'WebEvent',
    bundleadmin: 'BundleAdmin',
    processconsole: 'Process Console',
    bundlewatcher: 'Bundle Watcher',
    globalconfig: 'Global Config',
    launcher: 'Launcher',
    logviewer: 'Log Viewer',
    monitor: 'Monitor',
  }

  if (knownLabels[normalized]) return knownLabels[normalized]
  if (normalized === normalized.toUpperCase()) return normalized

  return normalized
    .replace(/[-_]+/g, ' ')
    .replace(/\b\w/g, (character) => character.toUpperCase())
}

function createGroupNode(id, title, icon, description, children = []) {
  return { id, kind: 'group', title, icon, description, children }
}

function createVirtualNode(id, title, icon, sourceSectionId, filterPrefix, description, entryCount = 0) {
  return {
    id,
    kind: 'virtual',
    title,
    icon,
    sourceSectionId,
    filterPrefix,
    description,
    entryCount,
  }
}

function createSectionTreeNode(section, title = section.title) {
  return {
    id: section.id,
    kind: 'section',
    title,
    icon: section.icon || 'mdi-file-cog-outline',
    sectionId: section.id,
    description: section.helper,
    state: section.state,
    entryCount: section.entryCount ?? 0,
  }
}

function buildApplicationNodes(applicationSection) {
  if (!applicationSection) return []

  const entries = applicationSection.entries ?? []
  const topLevelMap = new Map()
  for (const entry of entries) {
    const parts = String(entry?.key ?? '').split('.').filter(Boolean)
    if (!parts.length) continue

    const topLevel = parts[0]
    if (!topLevelMap.has(topLevel)) topLevelMap.set(topLevel, new Set())
    if (parts[1]) topLevelMap.get(topLevel).add(parts[1])
  }

  const children = [
    createSectionTreeNode(applicationSection, text.value.appAll),
  ]

  Array.from(topLevelMap.keys())
    .sort((left, right) => left.localeCompare(right))
    .forEach((topLevel) => {
      const topPrefix = `${topLevel}.`
      const secondLevel = Array.from(topLevelMap.get(topLevel) ?? []).sort((left, right) => left.localeCompare(right))

      if (secondLevel.length > 1) {
        const nestedChildren = [
          createVirtualNode(
            `virtual:${topLevel}`,
            formatTreeLabel(topLevel),
            topLevel === 'logging' ? 'mdi-text-box-search-outline' : 'mdi-folder-outline',
            applicationSection.id,
            topPrefix,
            applicationSection.helper,
            countEntriesByPrefix(entries, topPrefix),
          ),
        ]

        secondLevel.forEach((childKey) => {
          const childPrefix = `${topLevel}.${childKey}.`
          nestedChildren.push(
            createVirtualNode(
              `virtual:${topLevel}:${childKey}`,
              formatTreeLabel(childKey),
              topLevel === 'logging' ? 'mdi-tune-variant' : 'mdi-file-tree-outline',
              applicationSection.id,
              childPrefix,
              applicationSection.helper,
              countEntriesByPrefix(entries, childPrefix),
            ),
          )
        })

        children.push(
          createGroupNode(
            `group:application:${topLevel}`,
            formatTreeLabel(topLevel),
            topLevel === 'logging' ? 'mdi-text-box-search-outline' : 'mdi-folder-multiple-outline',
            applicationSection.helper,
            nestedChildren,
          ),
        )
      } else {
        children.push(
          createVirtualNode(
            `virtual:${topLevel}`,
            formatTreeLabel(topLevel),
            topLevel === 'logging' ? 'mdi-text-box-search-outline' : 'mdi-folder-outline',
            applicationSection.id,
            topPrefix,
            applicationSection.helper,
            countEntriesByPrefix(entries, topPrefix),
          ),
        )
      }
    })

  return children
}

function buildBundleNamespaceNodes(bundleSections) {
  const root = []
  const groupMap = new Map()

  function ensureGroup(pathSegments) {
    if (!pathSegments.length) return root

    const groupId = `group:bundle:${pathSegments.join('.')}`
    if (groupMap.has(groupId)) return groupMap.get(groupId).children

    const parentSegments = pathSegments.slice(0, -1)
    const parentChildren = ensureGroup(parentSegments)
    const groupNode = createGroupNode(
      groupId,
      formatTreeLabel(pathSegments[pathSegments.length - 1]),
      'mdi-folder-multiple-outline',
      text.value.bundlesRootHint,
      [],
    )
    groupMap.set(groupId, groupNode)
    parentChildren.push(groupNode)
    return groupNode.children
  }

  bundleSections.forEach((section) => {
    const segments = String(section.symbolicName || section.target || '')
      .split('.')
      .filter(Boolean)

    if (!segments.length) {
      root.push(createSectionTreeNode(section))
      return
    }

    const parentChildren = ensureGroup(segments.slice(0, -1))
    parentChildren.push(createSectionTreeNode(section, formatTreeLabel(segments[segments.length - 1])))
  })

  const sortNodes = (nodes) => {
    nodes.sort((left, right) => {
      if (left.kind === 'group' && right.kind !== 'group') return -1
      if (left.kind !== 'group' && right.kind === 'group') return 1
      return String(left.title).localeCompare(String(right.title))
    })
    nodes.forEach((node) => {
      if (node.children?.length) sortNodes(node.children)
    })
    return nodes
  }

  return sortNodes(root)
}

function createTreeNodes(sectionList) {
  const applicationSection = sectionList.find((section) => section.scope === 'application') ?? null
  const bundleSections = sectionList.filter((section) => section.scope === 'bundle')
  const applicationChildren = buildApplicationNodes(applicationSection)
  const bundleChildren = buildBundleNamespaceNodes(bundleSections)

  return [
    createGroupNode('root:application', text.value.appRoot, 'mdi-application-cog-outline', text.value.appRootHint, applicationChildren),
    createGroupNode('root:bundles', text.value.bundlesRoot, 'mdi-package-variant-closed', text.value.bundlesRootHint, bundleChildren),
  ]
}

function collectTreeMetadata(nodes, nodeMap = {}, parentMap = {}, parentId = '') {
  nodes.forEach((node) => {
    nodeMap[node.id] = node
    parentMap[node.id] = parentId
    if (node.children?.length) collectTreeMetadata(node.children, nodeMap, parentMap, node.id)
  })
  return { nodeMap, parentMap }
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

function countLeafNodes(node) {
  if (!node) return 0
  if (node.kind !== 'group') return 1
  return (node.children ?? []).reduce((total, child) => total + countLeafNodes(child), 0)
}

function flattenVisibleTreeRows(nodes, depth = 0, rows = []) {
  nodes.forEach((node) => {
    const hasChildren = Boolean(node.children?.length)
    rows.push({
      node,
      depth,
      hasChildren,
      isOpen: hasChildren ? isTreeGroupOpen(node.id) : false,
      leafCount: hasChildren ? countLeafNodes(node) : 0,
    })

    if (hasChildren && isTreeGroupOpen(node.id)) {
      flattenVisibleTreeRows(node.children, depth + 1, rows)
    }
  })
  return rows
}

function expandNodeAncestors(nodeId, parentMap) {
  let cursor = nodeId
  while (cursor) {
    const parentId = parentMap[cursor]
    if (parentId) openTreeState[parentId] = true
    cursor = parentId
  }
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
  const { nodeMap, parentMap } = collectTreeMetadata(nodes)
  const nextNodeId = preferredNodeId || selectedNodeId.value
  selectedNodeId.value = nodeMap[nextNodeId] ? nextNodeId : firstEditableNodeId(nodes)
  expandNodeAncestors(selectedNodeId.value, parentMap)
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
const treeMetadata = computed(() => collectTreeMetadata(treeNodes.value))
const treeNodeMap = computed(() => treeMetadata.value.nodeMap)
const treeRows = computed(() => flattenVisibleTreeRows(treeNodes.value))
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
  expandNodeAncestors(node.id, treeMetadata.value.parentMap)
}

function bundleStateTone(state) {
  const normalized = String(state || '').toLowerCase()
  if (normalized.includes('active')) return 'success'
  if (normalized.includes('resolved')) return 'info'
  if (normalized.includes('installed')) return 'warning'
  return 'secondary'
}

function treeRowSubtitle(row) {
  if (row.node.kind === 'group') return `${row.leafCount} ${text.value.totalNodes}`
  if (row.node.kind === 'virtual' || row.node.id === 'application') {
    return `${row.node.entryCount ?? 0} ${text.value.totalKeys}`
  }
  if (row.node.kind === 'section' && row.node.state) return row.node.state
  return row.node.description || ''
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

function withCacheBust(url) {
  const separator = url.includes('?') ? '&' : '?'
  return `${url}${separator}_=${Date.now()}`
}

function openLogWindow() {
  const popup = openPopupWindow(withCacheBust('/myiot/logs/index.html?popup=1'), 'myiot-log-viewer')
  if (!popup) window.open(withCacheBust('/myiot/logs/index.html'), '_blank', 'noopener,noreferrer')
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

              <div v-if="treeRows.length" class="tree-shell">
                <button
                  v-for="row in treeRows"
                  :key="row.node.id"
                  type="button"
                  class="tree-row"
                  :class="{
                    'tree-row-group': row.node.kind === 'group',
                    'tree-row-leaf': row.node.kind !== 'group',
                    'tree-row-active': selectedNodeId === row.node.id
                  }"
                  :style="{ '--tree-depth': row.depth, '--tree-indent': `${18 + row.depth * 18}px` }"
                  @click="selectNode(row.node)"
                >
                  <div class="tree-row-main">
                    <span
                      class="tree-toggle"
                      :class="{ 'tree-toggle-placeholder': !row.hasChildren }"
                      @click.stop="row.hasChildren && toggleTreeGroup(row.node.id)"
                    >
                      <v-icon
                        v-if="row.hasChildren"
                        :icon="row.isOpen ? 'mdi-chevron-down' : 'mdi-chevron-right'"
                        size="18"
                      ></v-icon>
                    </span>
                    <span class="tree-node-icon">
                      <v-icon :icon="row.node.icon" size="18"></v-icon>
                    </span>
                    <span class="tree-row-copy">
                      <strong>{{ row.node.title }}</strong>
                    </span>
                  </div>
                  <div class="tree-row-meta">
                    <span class="tree-row-count">
                      {{ row.node.kind === 'group' ? row.leafCount : (row.node.entryCount ?? 0) }}
                    </span>
                    <span
                      v-if="row.node.kind !== 'group'"
                      class="tree-row-status"
                      :class="{ 'tree-row-status-dirty': isDirtyForNode(row.node) }"
                    >
                      {{ isDirtyForNode(row.node) ? text.dirty : text.clean }}
                    </span>
                  </div>
                </button>
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

                <div class="detail-grid detail-grid-dense">
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
                      size="small"
                      color="info"
                      variant="outlined"
                      :loading="managingActionId === bundleActionKey(selectedEditorSection.id, 'resolve')"
                      :disabled="!selectedEditorSection?.canResolve"
                      @click="runBundleAction('resolve')"
                    >
                      {{ text.resolve }}
                    </v-btn>
                    <v-btn
                      size="small"
                      color="success"
                      variant="outlined"
                      :loading="managingActionId === bundleActionKey(selectedEditorSection.id, 'start')"
                      :disabled="!selectedEditorSection?.canStart"
                      @click="runBundleAction('start')"
                    >
                      {{ text.start }}
                    </v-btn>
                    <v-btn
                      size="small"
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

                <div class="editor-toolbar editor-toolbar-dense">
                  <v-text-field
                    v-model="entrySearch"
                    density="compact"
                    variant="outlined"
                    :label="text.search"
                    :placeholder="text.searchPlaceholder"
                    prepend-inner-icon="mdi-magnify"
                    hide-details
                  />
                  <v-btn size="small" color="primary" variant="outlined" :disabled="!selectedEditorSection?.editable" @click="addEntry">
                    {{ text.addEntry }}
                  </v-btn>
                  <v-btn
                    size="small"
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
                      density="compact"
                      variant="outlined"
                      :label="text.key"
                      :disabled="!selectedEditorSection?.editable"
                      hide-details
                    />
                    <v-textarea
                      v-model="entry.value"
                      density="compact"
                      variant="outlined"
                      :label="text.value"
                      :disabled="!selectedEditorSection?.editable"
                      auto-grow
                      rows="1"
                      max-rows="6"
                      hide-details
                    />
                    <v-btn size="small" variant="text" color="error" :disabled="!selectedEditorSection?.editable" @click="removeEntry(index)">
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
  gap: 12px;
  padding: 18px;
  border-radius: 20px;
  border: 1px solid rgba(78, 188, 255, 0.16);
  background:
    linear-gradient(180deg, rgba(10, 23, 41, 0.94), rgba(6, 16, 30, 0.98)),
    radial-gradient(circle at top right, rgba(107, 210, 255, 0.06), transparent 40%);
  box-shadow: 0 12px 28px rgba(0, 0, 0, 0.18);
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
  gap: 6px;
  padding: 12px 14px;
  border: 1px solid rgba(78, 188, 255, 0.12);
  border-radius: 12px;
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

.tree-row,
.child-node-card {
  width: 100%;
  text-align: left;
  cursor: pointer;
  transition: border-color 160ms ease, transform 160ms ease, box-shadow 160ms ease;
}

.tree-row {
  position: relative;
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  align-items: center;
  min-height: 34px;
  gap: 10px;
  padding: 4px 8px 4px var(--tree-indent, 18px);
  border: none;
  border-radius: 8px;
  background: transparent;
  color: #e6f4ff;
}

.tree-shell {
  position: relative;
  gap: 2px;
  padding: 6px 0 6px 4px;
  border-radius: 12px;
  border: 1px solid rgba(78, 188, 255, 0.08);
  background: rgba(3, 12, 22, 0.35);
}

.tree-shell::before {
  content: '';
  position: absolute;
  top: 8px;
  bottom: 8px;
  left: 21px;
  width: 1px;
  background: linear-gradient(180deg, rgba(112, 240, 193, 0.18), rgba(78, 188, 255, 0.04));
  pointer-events: none;
}

.tree-row::before {
  content: '';
  position: absolute;
  left: calc(var(--tree-indent, 18px) - 8px);
  top: 50%;
  width: 8px;
  height: 1px;
  background: rgba(112, 240, 193, 0.2);
  transform: translateY(-50%);
  pointer-events: none;
}

.tree-row:hover,
.tree-row-active,
.child-node-card:hover {
  background: rgba(76, 148, 210, 0.12);
  box-shadow: inset 2px 0 0 rgba(112, 240, 193, 0.7);
  transform: none;
}

.tree-row-group {
  font-weight: 600;
}

.tree-row-main,
.section-title-group {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
}

.tree-row-copy {
  min-width: 0;
}

.tree-row-copy strong {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 0.9rem;
  font-weight: 500;
}

.tree-row-meta {
  display: inline-flex;
  align-items: center;
  justify-content: flex-end;
  gap: 8px;
  min-width: max-content;
}

.tree-row-count,
.tree-row-status {
  font-size: 0.75rem;
  line-height: 1;
  color: rgba(198, 223, 246, 0.6);
}

.tree-row-count {
  min-width: 18px;
  text-align: right;
}

.tree-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 16px;
  height: 16px;
  border-radius: 4px;
  color: rgba(225, 241, 255, 0.72);
  flex: 0 0 16px;
}

.tree-toggle-placeholder {
  opacity: 0;
  pointer-events: none;
}

.tree-node-icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 16px;
  height: 16px;
  color: rgba(159, 220, 255, 0.9);
}

.tree-row-status-dirty {
  color: #ffcc7a;
}

.detail-grid-dense {
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 10px;
}

.editor-toolbar {
  flex-wrap: wrap;
  align-items: stretch;
  gap: 10px;
}

.editor-toolbar-dense {
  padding: 10px 12px;
  border-radius: 12px;
  border: 1px solid rgba(78, 188, 255, 0.1);
  background: rgba(5, 16, 29, 0.52);
}

.runtime-toolbar {
  flex-wrap: wrap;
  align-items: center;
  padding: 12px 14px;
  border-radius: 14px;
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
  gap: 8px;
}

.editor-toolbar :deep(.v-input) {
  min-width: min(100%, 260px);
  flex: 1 1 260px;
}

.entry-card {
  display: grid;
  grid-template-columns: minmax(220px, 0.8fr) minmax(0, 1.4fr) auto;
  gap: 10px;
  align-items: start;
  padding: 12px;
  border-radius: 12px;
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

  .tree-row {
    grid-template-columns: 1fr;
    align-items: flex-start;
  }

  .tree-row-meta {
    justify-content: flex-start;
    padding-left: calc(var(--tree-indent, 18px) + 22px);
  }

  .runtime-actions {
    width: 100%;
  }
}
</style>
