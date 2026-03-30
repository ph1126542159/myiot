import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vuetify from 'vite-plugin-vuetify'
import { fileURLToPath, URL } from 'node:url'

export default defineConfig({
  base: '/myiot/monitor/',
  plugins: [
    vue(),
    vuetify({ autoImport: true }),
  ],
  resolve: {
    alias: {
      '@myiot-shared': fileURLToPath(new URL('../../shared', import.meta.url)),
    },
  },
  server: {
    host: '0.0.0.0',
    port: 5178,
  },
})
