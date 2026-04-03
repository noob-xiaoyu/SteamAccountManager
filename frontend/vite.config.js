import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { viteSingleFile } from 'vite-plugin-singlefile'

export default defineConfig({
  plugins: [vue(), viteSingleFile()],
  base: '/',
  build: {
    assetsInlineLimit: 100000000, // 确保图片也会被转为 Base64
  }
})
