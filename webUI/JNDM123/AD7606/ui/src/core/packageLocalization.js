const packageTranslations = {
  'myiot.launcher': {
    zh: {
      name: '登录',
      title: '登录入口',
      description: 'MyIoT 的轻量登录入口，负责身份验证和主页面跳转。',
    },
    en: {
      name: 'Login',
      title: 'Login Entry',
      description: 'Lightweight login entry for authentication and initial routing.',
    },
  },
  'myiot.home': {
    zh: {
      name: '主页面',
      title: '控制台主页',
      description: 'MyIoT 的主控制台页面，展示功能包、状态总览和后续扩展入口。',
    },
    en: {
      name: 'Home',
      title: 'Control Console Home',
      description: 'Central workspace for package navigation, overview, and diagnostics entry points.',
    },
  },
  'myiot.process-console-ui': {
    zh: {
      name: '终端',
      title: '终端交互',
      description: '独立打开当前进程终端交互页面，执行调试命令并查看即时输出。',
    },
    en: {
      name: 'Console',
      title: 'Process Console',
      description: 'Standalone process diagnostics console for command-driven inspection.',
    },
  },
  'myiot.log-viewer': {
    zh: {
      name: '日志',
      title: '实时日志',
      description: '独立打开实时日志预览窗口，操作其他功能包时也能持续观察输出。',
    },
    en: {
      name: 'Logs',
      title: 'Realtime Logs',
      description: 'Open a dedicated realtime log preview window so logs can stay visible while operating other packages.',
    },
  },
  'myiot.system-monitor': {
    zh: {
      name: '监控',
      title: '系统监控',
      description: '实时展示系统磁盘、内存、线程、进程、IO、网络和 CPU 走势。',
    },
    en: {
      name: 'Monitor',
      title: 'System Monitor',
      description: 'Realtime view of disk, memory, threads, processes, IO, networking, and CPU trends.',
    },
  },
  'myiot.jndm123': {
    zh: {
      name: 'JNDM123',
      title: 'JNDM123 采集',
      description: '对 JNDM123 板上的 CDCE937 分频器和 6 片 AD7606 采集通道进行网页化控制与实时可视化。',
    },
    en: {
      name: 'JNDM123',
      title: 'JNDM123 Acquisition',
      description: 'Web control and preview page for the CDCE937 divider and 6x AD7606 capture chain.',
    },
  },
}

function normalizeLocale(locale) {
  return locale === 'en' ? 'en' : 'zh'
}

export function localizeFeaturePackage(featurePackage, locale) {
  const normalizedLocale = normalizeLocale(locale)
  const translations = packageTranslations[featurePackage.id]?.[normalizedLocale]
  if (!translations) return featurePackage

  return {
    ...featurePackage,
    ...translations,
  }
}

export function formatPackageStatus(status, locale) {
  const normalizedLocale = normalizeLocale(locale)
  const normalizedStatus = String(status ?? '').toLowerCase()

  if (normalizedStatus === 'active') {
    return normalizedLocale === 'en' ? 'Active' : '已启用'
  }

  if (normalizedStatus === 'planned') {
    return normalizedLocale === 'en' ? 'Planned' : '规划中'
  }

  return normalizedLocale === 'en' ? 'Pending' : '待接入'
}
