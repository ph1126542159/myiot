const DEFAULT_LOCALE = 'zh'
const LOCALE_KEY = 'myiot.ui.locale.v2'
const LEGACY_LOCALE_KEY = 'myiot.ui.locale'

function normalizeLocale(value) {
  return value === 'en' ? 'en' : 'zh'
}

export function getUiLocale() {
  if (typeof window === 'undefined') return DEFAULT_LOCALE

  try {
    const stored = window.localStorage.getItem(LOCALE_KEY)
    if (stored) return normalizeLocale(stored)

    const legacyStored = window.localStorage.getItem(LEGACY_LOCALE_KEY)
    if (legacyStored) return normalizeLocale(legacyStored)
  } catch {
  }

  return DEFAULT_LOCALE
}

export function createUiLocaleHeaders(headers = {}) {
  const normalizedHeaders = new Headers(headers)
  normalizedHeaders.set('X-MyIoT-Locale', getUiLocale())
  return normalizedHeaders
}
