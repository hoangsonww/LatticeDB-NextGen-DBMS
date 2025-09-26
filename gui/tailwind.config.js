/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        ink: "#0f1226",
        mist: "#f6f7fb",
        primary: {
          DEFAULT: "#6b9cff",
          50: "#eef4ff",
          500: "#6b9cff",
          600: "#4f81f5",
        },
        accent: { DEFAULT: "#22c55e" },
      },
      boxShadow: {
        soft: "0 8px 30px rgba(0,0,0,0.06)",
      },
    },
  },
  plugins: [],
};
