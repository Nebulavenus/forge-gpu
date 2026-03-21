import { Button } from "@/components/ui/button"

const ASSET_TYPES = [
  { label: "All", value: "" },
  { label: "Textures", value: "texture" },
  { label: "Meshes", value: "mesh" },
  { label: "Animations", value: "animation" },
  { label: "Scenes", value: "scene" },
] as const

interface TypeFilterProps {
  value: string
  onChange: (type: string) => void
}

export function TypeFilter({ value, onChange }: TypeFilterProps) {
  return (
    <div className="flex items-center gap-1">
      {ASSET_TYPES.map((type) => (
        <Button
          key={type.value}
          variant={value === type.value ? "secondary" : "ghost"}
          size="sm"
          onClick={() => onChange(type.value)}
        >
          {type.label}
        </Button>
      ))}
    </div>
  )
}
