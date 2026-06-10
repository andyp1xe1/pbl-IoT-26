import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { VitePWA } from "vite-plugin-pwa";

export default defineConfig({
  plugins: [
    react(),
    VitePWA({
      registerType: "autoUpdate",
      injectRegister: "auto",
      includeAssets: ["favicon.svg", "icon.svg", "icon-maskable.svg"],
      workbox: {
        // The dist bundles the board webp images, so precache them too.
        globPatterns: ["**/*.{js,css,html,svg,webp,woff2}"],
        cleanupOutdatedCaches: true,
      },
      manifest: {
        name: "Air Glove",
        short_name: "Air Glove",
        description:
          "Web Bluetooth companion for the Air Glove — pair, tune, calibrate.",
        theme_color: "#ecedf6",
        background_color: "#ecedf6",
        display: "standalone",
        orientation: "any",
        scope: "/",
        start_url: "/",
        icons: [
          {
            src: "icon.svg",
            sizes: "any",
            type: "image/svg+xml",
            purpose: "any",
          },
          {
            src: "icon-maskable.svg",
            sizes: "any",
            type: "image/svg+xml",
            purpose: "maskable",
          },
        ],
      },
    }),
  ],
  server: {
    host: "localhost",
    port: 5173,
  },
});
