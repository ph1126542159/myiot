import 'vuetify/styles'
import '@mdi/font/css/materialdesignicons.css'

import { createVuetify } from 'vuetify'

export default createVuetify({
  theme: {
    defaultTheme: 'jndm123Theme',
    themes: {
      jndm123Theme: {
        dark: true,
        colors: {
          background: '#071119',
          surface: '#0a1725',
          'surface-bright': '#122437',
          'surface-light': '#173049',
          primary: '#38d6ff',
          secondary: '#8cf2b2',
          accent: '#f7a94a',
          error: '#ff6e88',
          info: '#7ab8ff',
          success: '#7cf0a7',
          warning: '#ffd166'
        }
      }
    }
  },
  defaults: {
    VBtn: {
      rounded: 'xl',
      height: 46
    },
    VCard: {
      rounded: 'xl'
    },
    VTextField: {
      variant: 'outlined',
      density: 'comfortable',
      hideDetails: 'auto'
    },
    VSelect: {
      variant: 'outlined',
      density: 'comfortable',
      hideDetails: 'auto'
    }
  }
})
