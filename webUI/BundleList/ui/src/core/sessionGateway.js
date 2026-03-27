import { reactive } from 'vue'

export const sessionState = reactive({
  authenticated: false,
  username: '',
  message: '登录网关已就绪，请先完成身份验证。',
  lastError: ''
})

const sessionEndpoint = '/myiot/auth/session.json'
const logoutEndpoint = '/myiot/auth/logout'

function applySession(payload) {
  sessionState.authenticated = Boolean(payload.authenticated)
  sessionState.username = payload.username ?? ''
  sessionState.message = payload.message ?? sessionState.message
  sessionState.lastError = payload.lastError ?? ''
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
