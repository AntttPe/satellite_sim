import { clsx, type ClassValue } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

export function formatBer(value: number): string {
  if (!Number.isFinite(value) || value <= 0) return '0'
  return value.toExponential(2)
}

export function formatNumber(value: number, digits = 3): string {
  if (!Number.isFinite(value)) return '—'
  return value.toFixed(digits)
}

export function formatUptime(ms: number): string {
  const total = Math.floor(ms / 1000)
  const h = Math.floor(total / 3600)
  const m = Math.floor((total % 3600) / 60)
  const s = total % 60
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
}
