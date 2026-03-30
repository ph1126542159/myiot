const featurePackageCatalog = [
  {
    id: 'myiot.launcher',
    name: '登录',
    title: '登录入口',
    version: '0.1.0',
    category: 'launcher',
    icon: 'mdi-shield-key-outline',
    order: 10,
    status: 'active',
    entryPath: '/myiot/login/index.html',
    description: 'MyIoT 的轻量登录入口，负责身份验证和主页面跳转.',
  },
  {
    id: 'myiot.home',
    name: '主页面',
    title: '控制台主页',
    version: '0.1.0',
    category: 'home',
    icon: 'mdi-view-dashboard-outline',
    order: 20,
    status: 'active',
    entryPath: '/myiot/home/index.html',
    description: 'MyIoT 的主控制台页面，展示功能包、状态总览和后续扩展入口.',
  },
  {
    id: 'myiot.bundle-list',
    name: '包列表',
    title: '系统包列表',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-package-variant-closed',
    order: 30,
    status: 'active',
    entryPath: '/myiot/packages/index.html',
    description: '展示当前系统已加载的所有 OSP 功能包、状态、版本和运行级别.',
  },
  {
    id: 'myiot.process-console-ui',
    name: '终端',
    title: '终端交互',
    version: '0.1.0',
    category: 'diagnostics',
    order: 35,
    status: 'active',
    icon: 'mdi-console-network-outline',
    entryPath: '/myiot/console/index.html',
    description: '独立打开当前进程终端交互页面，执行调试命令并查看即时输出.',
  },
  {
    id: 'myiot.system-monitor',
    name: '监控',
    title: '系统监控',
    version: '0.1.0',
    category: 'diagnostics',
    icon: 'mdi-chart-timeline-variant',
    order: 40,
    status: 'active',
    entryPath: '/myiot/monitor/index.html',
    description: '实时展示系统磁盘、内存、线程、进程、IO、网络和 CPU 走势.',
  },
  {
    id: 'myiot.jndm123',
    name: 'JNDM123',
    title: 'JNDM123 采集',
    version: '0.1.0',
    category: 'hardware',
    icon: 'mdi-chip',
    order: 45,
    status: 'active',
    entryPath: '/myiot/jndm123/index.html',
    description: '对 JNDM123 板上的 CDCE937 分频器和 6 片 AD7606 做网页化控制与实时曲线显示.',
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
  if (status === 'active') return '已启用'
  if (status === 'planned') return '规划中'
  return '待接入'
}
