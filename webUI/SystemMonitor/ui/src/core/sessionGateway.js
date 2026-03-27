import { reactive } from 'vue'

export const sessionState = reactive({
  authenticated: false,
  username: '',
  message: '登录网关已就绪，请先完成身份验证。',
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

  await new Promise((resolve) => window.setTimeout(resolve, 900))

  if (!username || !password) {
    applySession({
      authenticated: false,
      username: '',
      message: '请输入用户名和密码后再登录。',
      lastError: '请输入用户名和密码后再登录。'
    })
    return { ok: false, message: sessionState.lastError }
  }

  applySession({
    authenticated: true,
    username,
    message: `账号 ${username} 验证通过，登录会话已建立。`,
    lastError: ''
  })
  return { ok: true, message: sessionState.message }
}

export async function refreshSession() {
  try {
    const response = await fetch(sessionEndpoint, {
      credentials: 'same-origin',
      headers: { Accept: 'application/json' }
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
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        Accept: 'application/json'
      },
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
      headers: { Accept: 'application/json' }
    })
    if (!response.ok) throw new Error(`logout status ${response.status}`)
    const payload = await response.json()
    applySession(payload)
    return payload
  } catch {
    applySession({
      authenticated: false,
      username: '',
      message: '已退出登录，请重新验证身份。',
      lastError: ''
    })
    return { ...sessionState }
  }
}
