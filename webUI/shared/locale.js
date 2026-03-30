import { computed, onBeforeUnmount, ref } from 'vue'

const DEFAULT_LOCALE = 'zh'
const LOCALE_KEY = 'myiot.ui.locale.v2'
const LEGACY_LOCALE_KEY = 'myiot.ui.locale'
const LOCALE_EVENT = 'myiot:locale-change'

function normalizeLocale(value) {
  return value === 'zh' ? 'zh' : 'en'
}

function readLocale() {
  if (typeof window === 'undefined') return DEFAULT_LOCALE

  try {
    const stored = window.localStorage.getItem(LOCALE_KEY)
    if (stored) return normalizeLocale(stored)

    const legacyStored = window.localStorage.getItem(LEGACY_LOCALE_KEY)
    if (legacyStored) return normalizeLocale(legacyStored)

    return DEFAULT_LOCALE
  } catch {
    return DEFAULT_LOCALE
  }
}

function persistLocale(locale) {
  if (typeof window === 'undefined') return

  try {
    window.localStorage.setItem(LOCALE_KEY, locale)
    window.localStorage.setItem(LEGACY_LOCALE_KEY, locale)
  } catch {
  }
}

function dispatchLocaleChange(locale) {
  if (typeof window === 'undefined') return

  try {
    window.dispatchEvent(new CustomEvent(LOCALE_EVENT, { detail: locale }))
  } catch {
  }
}

export function useUiLocale() {
  const locale = ref(readLocale())

  const syncLocale = () => {
    locale.value = readLocale()
  }

  const setLocale = (nextLocale) => {
    const normalized = normalizeLocale(nextLocale)
    locale.value = normalized
    persistLocale(normalized)
    dispatchLocaleChange(normalized)
  }

  const toggleLocale = () => {
    setLocale(locale.value === 'zh' ? 'en' : 'zh')
  }

  const handleStorage = (event) => {
    if (!event.key || event.key === LOCALE_KEY || event.key === LEGACY_LOCALE_KEY) {
      syncLocale()
    }
  }

  const handleLocaleEvent = () => {
    syncLocale()
  }

  const handleVisibilityChange = () => {
    if (typeof document === 'undefined' || document.visibilityState === 'visible') {
      syncLocale()
    }
  }

  if (typeof window !== 'undefined') {
    window.addEventListener('storage', handleStorage)
    window.addEventListener(LOCALE_EVENT, handleLocaleEvent)
    window.addEventListener('focus', syncLocale)
    window.addEventListener('pageshow', syncLocale)

    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', handleVisibilityChange)
    }

    onBeforeUnmount(() => {
      window.removeEventListener('storage', handleStorage)
      window.removeEventListener(LOCALE_EVENT, handleLocaleEvent)
      window.removeEventListener('focus', syncLocale)
      window.removeEventListener('pageshow', syncLocale)

      if (typeof document !== 'undefined') {
        document.removeEventListener('visibilitychange', handleVisibilityChange)
      }
    })
  }

  return {
    locale,
    isZh: computed(() => locale.value === 'zh'),
    setLocale,
    toggleLocale,
  }
}

