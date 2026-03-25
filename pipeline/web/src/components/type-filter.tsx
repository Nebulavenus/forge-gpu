import { Button } from "@/components/ui/button"
import { TYPE_META } from "@/lib/asset-meta"

interface TypeOption {
  label: string
  value: string
}

const ASSET_TYPES: TypeOption[] = [
  { label: "All", value: "" },
  ...Object.entries(TYPE_META).map(([, meta]) => ({
    label: meta.label,
    value: meta.filterValue,
  })),
]

interface TypeFilterProps {
  value: string
  onChange: (type: string) => void
}

export function TypeFilter({ value, onChange }: TypeFilterProps) {
  return (
    <div role="toolbar" aria-label="Filter by type" className="flex items-center gap-1">
      {ASSET_TYPES.map((type) => {
        const isActive = value === type.value
        return (
          <Button
            type="button"
            key={type.value || "all"}
            variant={isActive ? "secondary" : "ghost"}
            size="sm"
            aria-pressed={isActive}
            onClick={() => onChange(type.value)}
          >
            {type.label}
          </Button>
        )
      })}
    </div>
  )
}
