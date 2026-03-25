import { Button } from "@/components/ui/button"
import { STATUS_META } from "@/lib/asset-meta"

interface StatusOption {
  label: string
  value: string
  bg?: string
}

const STATUS_OPTIONS: StatusOption[] = [
  { label: "All", value: "" },
  ...Object.entries(STATUS_META).map(([value, meta]) => ({
    label: meta.label,
    value,
    bg: meta.bg,
  })),
]

interface StatusFilterProps {
  value: string
  onChange: (status: string) => void
}

export function StatusFilter({ value, onChange }: StatusFilterProps) {
  return (
    <div role="toolbar" aria-label="Filter by status" className="flex items-center gap-1">
      {STATUS_OPTIONS.map((option) => {
        const isActive = value === option.value
        return (
          <Button
            type="button"
            key={option.value || "all"}
            variant={isActive ? "secondary" : "ghost"}
            size="sm"
            aria-pressed={isActive}
            onClick={() => onChange(option.value)}
          >
            {option.bg != null && (
              <span
                className={`h-2 w-2 rounded-full ${option.bg}`}
                aria-hidden="true"
              />
            )}
            {option.label}
          </Button>
        )
      })}
    </div>
  )
}
