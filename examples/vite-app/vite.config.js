import { defineConfig } from "vite";

export default defineConfig({
  // Keep Vite from pre-bundling tesfix: that would move the JavaScript and break the .wasm lookup.
  optimizeDeps: { exclude: ["tesfix"] },
  build: { target: "esnext" },
});
