import path from 'node:path'
import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vuetify from 'vite-plugin-vuetify'

const currentDir = path.dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  base: '/myiot/login/',
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
    port: 5175,
  },
})
