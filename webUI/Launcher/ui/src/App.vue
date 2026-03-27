<script setup>
import { computed, onMounted, ref, watch } from 'vue'
import { authenticate, refreshSession, sessionState } from './core/sessionGateway'

const REMEMBERED_CREDENTIALS_KEY = 'myiot.launcher.rememberedCredentials'

const username = ref('')
const password = ref('')
const rememberPassword = ref(false)
const loading = ref(false)
const banner = ref({
  type: 'info',
  text: '请输入账号和密码。'
})
const autoFilledFromStorage = ref(false)

function readRememberedCredentials() {
  try {
    const raw = window.localStorage.getItem(REMEMBERED_CREDENTIALS_KEY)
    if (!raw) return null

    const parsed = JSON.parse(raw)
    if (!parsed || typeof parsed !== 'object') return null

    return {
      username: typeof parsed.username === 'string' ? parsed.username : '',
      password: typeof parsed.password === 'string' ? parsed.password : ''
    }
  } catch {
    return null
  }
}

function writeRememberedCredentials() {
  try {
    window.localStorage.setItem(REMEMBERED_CREDENTIALS_KEY, JSON.stringify({
      username: username.value,
      password: password.value
    }))
  } catch {
  }
}

function clearRememberedCredentials() {
  try {
    window.localStorage.removeItem(REMEMBERED_CREDENTIALS_KEY)
  } catch {
  }
}

const hasRememberedCredentials = computed(() =>
  Boolean(readRememberedCredentials()?.username || readRememberedCredentials()?.password)
)

onMounted(async () => {
  const payload = await refreshSession()
  if (payload.authenticated) {
    window.location.replace('/myiot/home/index.html')
    return
  }

  const rememberedCredentials = readRememberedCredentials()
  if (rememberedCredentials) {
    username.value = rememberedCredentials.username
    password.value = rememberedCredentials.password
    rememberPassword.value = Boolean(rememberedCredentials.username || rememberedCredentials.password)
    autoFilledFromStorage.value = rememberPassword.value
  }

  banner.value = {
    type: 'info',
    text: autoFilledFromStorage.value ? '已自动填入上次保存的账号密码，请确认后登录。' : '请输入账号和密码。'
  }
})

watch(rememberPassword, (enabled) => {
  if (!enabled) {
    clearRememberedCredentials()
    autoFilledFromStorage.value = false
  }
})

watch([username, password], () => {
  autoFilledFromStorage.value = false
})

function clearSavedCredentials() {
  username.value = ''
  password.value = ''
  rememberPassword.value = false
  autoFilledFromStorage.value = false
  clearRememberedCredentials()
  banner.value = {
    type: 'info',
    text: '已清除当前浏览器保存的账号密码，请重新输入。'
  }
}

async function submitLogin() {
  loading.value = true
  banner.value = {
    type: 'info',
    text: '正在验证身份...'
  }

  const result = await authenticate({
    username: username.value,
    password: password.value
  })

  banner.value = {
    type: result.ok ? 'success' : 'error',
    text: result.message
  }

  if (result.ok) {
    if (rememberPassword.value) {
      writeRememberedCredentials()
    } else {
      clearRememberedCredentials()
      password.value = ''
    }
    window.setTimeout(() => {
      window.location.replace('/myiot/home/index.html')
    }, 260)
  }

  loading.value = false
}
</script>

<template>
  <v-app>
    <div class="shell-scene">
      <div class="ambient-grid" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-a" aria-hidden="true"></div>
      <div class="ambient-glow ambient-glow-b" aria-hidden="true"></div>

      <v-container fluid class="shell-container">
        <section class="login-shell">
          <div class="login-card">
            <div class="brand-mark login-mark">
              <span></span>
              <span></span>
              <span></span>
            </div>

            <p class="eyebrow text-center">MYIOT</p>
            <h1 class="login-title">登录系统</h1>
            <p class="login-copy">登录成功后进入主页面。</p>

            <v-alert
              :type="banner.type"
              variant="tonal"
              border="start"
              class="mt-6"
            >
              {{ banner.text }}
            </v-alert>

            <div v-if="hasRememberedCredentials" class="saved-credentials-row">
              <span>当前浏览器存在已保存的账号密码</span>
              <v-btn
                variant="text"
                color="secondary"
                size="small"
                @click="clearSavedCredentials"
              >
                清除已保存密码
              </v-btn>
            </div>

            <v-form class="login-form-simple" @submit.prevent="submitLogin">
              <label class="field-label">账号</label>
              <v-text-field
                v-model="username"
                prepend-inner-icon="mdi-account-circle-outline"
                placeholder="请输入账号"
                autocomplete="username"
              ></v-text-field>

              <label class="field-label mt-4">密码</label>
              <v-text-field
                v-model="password"
                type="password"
                prepend-inner-icon="mdi-lock-outline"
                placeholder="请输入密码"
                autocomplete="current-password"
              ></v-text-field>

              <div class="login-options-row">
                <v-checkbox
                  v-model="rememberPassword"
                  label="记住密码"
                  color="primary"
                  density="comfortable"
                  hide-details
                ></v-checkbox>
                <span class="login-option-hint">仅保存在当前浏览器</span>
              </div>

              <v-btn
                color="primary"
                size="large"
                type="submit"
                class="login-button-simple mt-6"
                block
                :loading="loading"
                :disabled="loading"
              >
                登录
              </v-btn>
            </v-form>
          </div>
        </section>
      </v-container>
    </div>
  </v-app>
</template>

<style scoped>
.login-shell {
  min-height: calc(100vh - 60px);
  display: grid;
  place-items: center;
}

.login-card {
  width: min(100%, 460px);
  padding: 40px 34px 34px;
  border: 1px solid rgba(78, 188, 255, 0.18);
  border-radius: 28px;
  background: linear-gradient(180deg, rgba(9, 21, 38, 0.94), rgba(7, 18, 33, 0.86));
  box-shadow: 0 28px 80px rgba(0, 0, 0, 0.42);
  backdrop-filter: blur(22px);
}

.login-mark {
  margin: 0 auto 18px;
}

.login-title {
  margin: 0;
  text-align: center;
  font-size: clamp(1.8rem, 2vw, 2.3rem);
}

.login-copy {
  margin: 12px 0 0;
  text-align: center;
  color: rgba(210, 232, 255, 0.68);
}

.text-center {
  text-align: center;
}

.login-form-simple {
  margin-top: 22px;
}

.saved-credentials-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-top: 16px;
  color: rgba(210, 232, 255, 0.72);
  font-size: 0.9rem;
}

.login-options-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-top: 10px;
}

.login-option-hint {
  color: rgba(210, 232, 255, 0.58);
  font-size: 0.84rem;
  white-space: nowrap;
}

.login-button-simple {
  min-width: 100%;
  box-shadow: 0 18px 34px rgba(58, 216, 255, 0.2);
}

@media (max-width: 520px) {
  .saved-credentials-row,
  .login-options-row {
    align-items: flex-start;
    flex-direction: column;
  }

  .login-option-hint {
    margin-top: -6px;
  }
}
</style>
