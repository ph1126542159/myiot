const featurePackageCatalog = [
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
    description: 'Lightweight login entry for authentication and initial routing.',
  },
  {
    id: 'myiot.home',
    name: 'Home',
    title: 'Control Console Home',
    version: '0.1.0',
    category: 'home',
    icon: 'mdi-view-dashboard-outline',
    order: 20,
    status: 'active',
    entryPath: '/myiot/home/index.html',
    description: 'Central workspace for package navigation, overview, and diagnostics entry points.',
  },
  {
    id: 'myiot.bundle-list',
    name: 'Bundles',
    title: 'System Bundle List',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-package-variant-closed',
    order: 30,
    status: 'active',
    entryPath: '/myiot/packages/index.html',
    description: 'Review installed OSP bundles, status, versions, and runtime level.',
  },
  {
    id: 'myiot.process-console-ui',
    name: 'Console',
    title: 'Process Console',
    version: '0.1.0',
    category: 'diagnostics',
    order: 35,
    status: 'active',
    icon: 'mdi-console-network-outline',
    entryPath: '/myiot/console/index.html',
    description: 'Standalone process diagnostics console for command-driven inspection.',
  },
  {
    id: 'myiot.log-viewer',
    name: 'Logs',
    title: 'Realtime Logs',
    version: '0.1.0',
    category: 'diagnostics',
    order: 36,
    status: 'active',
    icon: 'mdi-file-document-refresh-outline',
    entryPath: '/myiot/logs/index.html',
    description: 'Dedicated realtime log preview window that can stay open beside other packages.',
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
    description: 'Realtime view of disk, memory, threads, processes, IO, networking, and CPU trends.',
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
    description: 'Web control and preview page for the CDCE937 divider and 6x AD7606 capture chain.',
  },
]

function sortPackages(left, right) {
  return (left.order ?? 100) - (right.order ?? 100)
}

export const featurePackages = [...featurePackageCatalog].sort(sortPackages)

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
