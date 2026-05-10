import { cn } from '@/lib/utils'
import type { ReactNode } from 'react'

interface PanelProps {
  title: string
  subtitle?: string
  className?: string
  children: ReactNode
  accent?: 'cyan' | 'green' | 'amber' | 'violet' | 'red'
}

const ACCENT_MAP: Record<NonNullable<PanelProps['accent']>, string> = {
  cyan: 'text-hud-cyan',
  green: 'text-hud-green',
  amber: 'text-hud-amber',
  violet: 'text-hud-violet',
  red: 'text-hud-red',
}

export function Panel({ title, subtitle, className, children, accent = 'cyan' }: PanelProps) {
  return (
    <div
      className={cn(
        'relative flex flex-col overflow-hidden rounded-sm border border-hud-border bg-hud-panel/80',
        'shadow-[inset_0_0_0_1px_rgba(34,211,238,0.04),0_0_40px_-20px_rgba(34,211,238,0.4)]',
        className,
      )}
    >
      {/* corner accents */}
      <span className="pointer-events-none absolute left-0 top-0 h-2 w-2 border-l border-t border-hud-cyan/70" />
      <span className="pointer-events-none absolute right-0 top-0 h-2 w-2 border-r border-t border-hud-cyan/70" />
      <span className="pointer-events-none absolute bottom-0 left-0 h-2 w-2 border-b border-l border-hud-cyan/70" />
      <span className="pointer-events-none absolute bottom-0 right-0 h-2 w-2 border-b border-r border-hud-cyan/70" />

      <header className="flex items-baseline justify-between border-b border-hud-border/60 bg-hud-panel-2/60 px-3 py-1.5">
        <h2 className={cn('text-[11px] font-semibold uppercase tracking-[0.25em]', ACCENT_MAP[accent])}>
          {title}
        </h2>
        {subtitle && (
          <span className="text-[10px] uppercase tracking-widest text-hud-fg-dim">{subtitle}</span>
        )}
      </header>
      <div className="flex-1 p-3">{children}</div>
    </div>
  )
}
