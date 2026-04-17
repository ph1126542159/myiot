function sortPackages(left, right) {
  return (left.order ?? 100) - (right.order ?? 100)
}

// Release builds on the target packaging flow cannot reliably glob manifests
// outside the Home UI root, so keep the visible package catalog explicit.
const staticFeaturePackages = [
  {
    id: 'myiot.launcher',
    name: 'Login',
    title: 'Login Entry',
    version: '0.1.0',
    category: 'launcher',
    icon: 'mdi-shield-key-outline',
    order: 10,
    status: 'active',
    entryPath: '/myiot/login/index.html',
    description: 'Authentication and entry page for the MyIoT portal.',
  },
  {
    id: 'myiot.home',
    name: 'Home',
    title: 'Control Center',
    version: '0.1.0',
    category: 'home',
    icon: 'mdi-view-dashboard-outline',
    order: 20,
    status: 'active',
    entryPath: '/myiot/home/index.html',
    description: 'Portal home page with package navigation and status overview.',
  },
  {
    id: 'myiot.global-config',
    name: 'Config',
    title: 'Global Configuration',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-cog-box',
    order: 31,
    status: 'active',
    entryPath: '/myiot/config/index.html',
    description: 'Edit application-wide properties and per-bundle preferences.',
  },
  {
    id: 'myiot.process-console-ui',
    name: 'Console',
    title: 'Process Console',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-console-network-outline',
    order: 35,
    status: 'active',
    entryPath: '/myiot/console/index.html',
    description: 'Open the interactive process console for command execution.',
  },
  {
    id: 'myiot.log-viewer',
    name: 'Logs',
    title: 'Realtime Logs',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-file-document-refresh-outline',
    order: 36,
    status: 'active',
    entryPath: '/myiot/logs/index.html',
    description: 'Dedicated realtime log viewer for troubleshooting and review.',
  },
  {
    id: 'myiot.system-monitor',
    name: 'Monitor',
    title: 'System Monitor',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-chart-timeline-variant',
    order: 40,
    status: 'active',
    entryPath: '/myiot/monitor/index.html',
    description: 'CPU, memory, disk, network and process monitoring dashboard.',
  },
  {
    id: 'myiot.jndm123',
    name: 'JNDM123',
    title: 'JNDM123 Acquisition',
    version: '0.1.0',
    category: 'hardware',
    icon: 'mdi-chip',
    order: 45,
    status: 'active',
    entryPath: '/myiot/jndm123/index.html',
    description: 'Board acquisition control and AD7606 visualization entry.',
  },
]

export const featurePackages = staticFeaturePackages.slice().sort(sortPackages)

export function getPackageStatusTone(status) {
  if (status === 'active') return 'success'
  if (status === 'planned') return 'warning'
  return 'info'
}

export function formatPackageStatus(status) {
  if (status === 'active') return 'Active'
  if (status === 'planned') return 'Planned'
  return 'Pending'
}
