import { LayoutGrid, List } from "lucide-react"
import { Button } from "@/components/ui/button"
import type { AssetViewMode } from "@/lib/asset-meta"

interface ViewToggleProps {
  value: AssetViewMode
  onChange: (mode: AssetViewMode) => void
}

export function ViewToggle({ value, onChange }: ViewToggleProps) {
  return (
    <div className="flex items-center rounded-md border" role="group" aria-label="View mode">
      <Button
        variant={value === "grid" ? "secondary" : "ghost"}
        size="icon"
        className="h-8 w-8 rounded-r-none border-0"
        aria-label="Grid view"
        aria-pressed={value === "grid"}
        onClick={() => onChange("grid")}
      >
        <LayoutGrid className="h-4 w-4" />
      </Button>
      <Button
        variant={value === "list" ? "secondary" : "ghost"}
        size="icon"
        className="h-8 w-8 rounded-l-none border-0"
        aria-label="List view"
        aria-pressed={value === "list"}
        onClick={() => onChange("list")}
      >
        <List className="h-4 w-4" />
      </Button>
    </div>
  )
}
