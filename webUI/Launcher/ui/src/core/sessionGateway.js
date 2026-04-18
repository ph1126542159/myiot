import { reactive } from 'vue'
import { createUiLocaleHeaders, getUiLocale } from './requestLocale.js'

export const sessionState = reactive({
  authenticated: false,
  username: '',
  message: 'Session gateway is ready. Please authenticate first.',
  lastError: '',
  deviceIp: '',
  devicePort: '',
  serverAddress: '',
  accessProtocol: '',
  accessUrl: '',
  clientIp: ''
})

const sessionEndpoint = '/myiot/auth/session.json'
const loginEndpoint = '/myiot/auth/login'
const logoutEndpoint = '/myiot/auth/logout'

function applySession(payload) {
  sessionState.authenticated = Boolean(payload.authenticated)
  sessionState.username = payload.username ?? ''
  sessionState.message = payload.message ?? sessionState.message
  sessionState.lastError = payload.lastError ?? ''
  sessionState.deviceIp = payload.deviceIp == null ? '' : String(payload.deviceIp)
  sessionState.devicePort = payload.devicePort == null ? '' : String(payload.devicePort)
  sessionState.serverAddress = payload.serverAddress ?? ''
  sessionState.accessProtocol = payload.accessProtocol ?? ''
  sessionState.accessUrl = payload.accessUrl ?? ''
  sessionState.clientIp = payload.clientIp ?? ''
}

async function fallbackAuthenticate() {
  const isZh = getUiLocale() === 'zh'

  applySession({
    authenticated: false,
    username: '',
    message: isZh ? '认证服务暂时不可用，请稍后重试。' : 'The authentication service is temporarily unavailable. Please try again.',
    lastError: isZh ? '认证服务暂时不可用，请稍后重试。' : 'The authentication service is temporarily unavailable. Please try again.'
  })
  return { ok: false, message: sessionState.lastError }
}

async function readJsonPayload(response, fallbackMessage) {
  try {
    const payload = await response.json()
    if (payload && typeof payload === 'object') return payload
  } catch {
  }

  return {
    authenticated: false,
    username: '',
    message: fallbackMessage,
    lastError: fallbackMessage
  }
}

export async function refreshSession() {
  try {
    const response = await fetch(sessionEndpoint, {
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({ Accept: 'application/json' })
    })
    if (!response.ok) throw new Error(`session status ${response.status}`)
    const payload = await response.json()
    applySession(payload)
    return payload
  } catch {
    return { ...sessionState }
  }
}

export async function authenticate(credentials) {
  const unavailableMessage = getUiLocale() === 'zh'
    ? '认证服务暂时不可用，请稍后重试。'
    : 'The authentication service is temporarily unavailable. Please try again.'

  try {
    const body = new URLSearchParams()
    body.set('username', credentials.username)
    body.set('password', credentials.password)
    const response = await fetch(loginEndpoint, {
      method: 'POST',
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({
        'Content-Type': 'application/x-www-form-urlencoded',
        Accept: 'application/json'
      }),
      body
    })
    const payload = await readJsonPayload(response, unavailableMessage)
    applySession(payload)

    if (!response.ok || !payload.authenticated) {
      return { ok: false, message: payload.lastError ?? payload.message ?? unavailableMessage }
    }

    return { ok: true, message: payload.message ?? sessionState.message }
  } catch {
    return fallbackAuthenticate()
  }
}

export async function signOut() {
  try {
    const response = await fetch(logoutEndpoint, {
      method: 'POST',
      credentials: 'same-origin',
      headers: createUiLocaleHeaders({ Accept: 'application/json' })
    })
    if (!response.ok) throw new Error(`logout status ${response.status}`)
    const payload = await response.json()
    applySession(payload)
    return payload
  } catch {
    applySession({
      authenticated: false,
      username: '',
      message: getUiLocale() === 'zh' ? '已退出登录，请重新验证身份。' : 'Signed out. Please authenticate again.',
      lastError: ''
    })
    return { ...sessionState }
  }
}
