import 'vuetify/styles'
import '@mdi/font/css/materialdesignicons.css'

import { createVuetify } from 'vuetify'

export default createVuetify({
  theme: {
    defaultTheme: 'myiotDark',
    themes: {
      myiotDark: {
        dark: true,
        colors: {
          background: '#06111f',
          surface: '#08182b',
          'surface-bright': '#0d223d',
          'surface-light': '#112949',
          primary: '#3ad8ff',
          secondary: '#70f0c1',
          accent: '#10b4ff',
          error: '#ff597c',
          info: '#4cc9ff',
          success: '#65f9b4',
          warning: '#ffd166'
        }
      }
    }
  },
  defaults: {
    VTextField: {
      variant: 'outlined',
      color: 'primary',
      density: 'comfortable',
      hideDetails: 'auto'
    },
    VBtn: {
      rounded: 'xl',
      height: 50
    },
    VCard: {
      rounded: 'xl'
    }
  }
})
