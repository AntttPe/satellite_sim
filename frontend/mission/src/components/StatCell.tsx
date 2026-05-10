import { cn } from '@/lib/utils'

interface StatCellProps {
  label: string
  value: string
  unit?: string
  accent?: 'cyan' | 'green' | 'amber' | 'violet' | 'red' | 'default'
  align?: 'left' | 'center' | 'right'
}

const ACCENT: Record<NonNullable<StatCellProps['accent']>, string> = {
  default: 'text-hud-fg',
  cyan: 'text-hud-cyan',
  green: 'text-hud-green',
  amber: 'text-hud-amber',
  violet: 'text-hud-violet',
  red: 'text-hud-red',
}

export function StatCell({ label, value, unit, accent = 'default', align = 'left' }: StatCellProps) {
  return (
    <div
      className={cn(
        'flex flex-col gap-0.5 rounded-sm border border-hud-border/60 bg-hud-panel-2/40 px-3 py-2',
        align === 'center' && 'items-center text-center',
        align === 'right' && 'items-end text-right',
      )}
    >
      <span className="text-[10px] uppercase tracking-[0.25em] text-hud-fg-dim">{label}</span>
      <span className="flex items-baseline gap-1.5">
        <span className={cn('text-[18px] font-semibold tabular-nums leading-none', ACCENT[accent])}>
          {value}
        </span>
        {unit && <span className="text-[10px] uppercase tracking-widest text-hud-fg-dim">{unit}</span>}
      </span>
    </div>
  )
}
