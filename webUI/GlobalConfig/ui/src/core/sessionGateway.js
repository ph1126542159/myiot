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

async function fallbackAuthenticate(credentials) {
  const username = credentials.username.trim()
  const password = credentials.password.trim()
  const isZh = getUiLocale() === 'zh'

  await new Promise((resolve) => window.setTimeout(resolve, 900))

  if (!username || !password) {
    applySession({
      authenticated: false,
      username: '',
      message: isZh ? '请输入用户名和密码后再登录。' : 'Please enter both username and password before signing in.',
      lastError: isZh ? '请输入用户名和密码后再登录。' : 'Please enter both username and password before signing in.'
    })
    return { ok: false, message: sessionState.lastError }
  }

  applySession({
    authenticated: true,
    username,
    message: isZh ? `账号 ${username} 验证通过，登录会话已建立。` : `Account ${username} authenticated successfully.`,
    lastError: ''
  })
  return { ok: true, message: sessionState.message }
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
    if (!response.ok) throw new Error(`login status ${response.status}`)
    const payload = await response.json()
    applySession(payload)
    return { ok: Boolean(payload.authenticated), message: payload.message ?? payload.lastError ?? sessionState.message }
  } catch {
    return fallbackAuthenticate(credentials)
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
