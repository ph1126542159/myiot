<script setup>
import { onMounted, ref } from 'vue'
import { authenticate, refreshSession, sessionState } from './core/sessionGateway'

const username = ref('')
const password = ref('')
const loading = ref(false)
const banner = ref({
  type: 'info',
  text: '请输入账号和密码。'
})

onMounted(async () => {
  const payload = await refreshSession()
  if (payload.authenticated) {
    window.location.replace('/myiot/home/index.html')
    return
  }

  banner.value = {
    type: 'info',
    text: '请输入账号和密码。'
  }
})

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
    password.value = ''
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

.login-button-simple {
  min-width: 100%;
  box-shadow: 0 18px 34px rgba(58, 216, 255, 0.2);
}
</style>
