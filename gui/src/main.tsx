import React from 'react'
import { createRoot } from 'react-dom/client'
import App from './App'
import Settings from './Settings'
import './styles.css'

const isSettings = window.location.pathname === '/settings'
const Component = isSettings ? Settings : App

createRoot(document.getElementById('root')!).render(<Component />)
