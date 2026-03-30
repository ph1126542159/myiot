import { computed, onBeforeUnmount, ref } from 'vue'

const LOCALE_KEY = 'myiot.ui.locale'

function normalizeLocale(value) {
  return value === 'zh' ? 'zh' : 'en'
}

function readLocale() {
  try {
    return normalizeLocale(window.localStorage.getItem(LOCALE_KEY))
  } catch {
    return 'zh'
  }
}

export function useUiLocale() {
  const locale = ref(typeof window === 'undefined' ? 'zh' : readLocale())

  const syncLocale = () => {
    locale.value = readLocale()
  }

  const setLocale = (nextLocale) => {
    const normalized = normalizeLocale(nextLocale)
    locale.value = normalized
    try {
      window.localStorage.setItem(LOCALE_KEY, normalized)
    } catch {
    }
  }

  const toggleLocale = () => {
    setLocale(locale.value === 'zh' ? 'en' : 'zh')
  }

  const handleStorage = (event) => {
    if (event.key === LOCALE_KEY) {
      syncLocale()
    }
  }

  if (typeof window !== 'undefined') {
    window.addEventListener('storage', handleStorage)
    onBeforeUnmount(() => {
      window.removeEventListener('storage', handleStorage)
    })
  }

  return {
    locale,
    isZh: computed(() => locale.value === 'zh'),
    setLocale,
    toggleLocale,
  }
}
