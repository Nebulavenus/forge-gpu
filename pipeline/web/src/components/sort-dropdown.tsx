import { Select } from "@/components/ui/select"

/** Each option encodes both the field and direction so a single `<select>`
 *  is enough — no separate "order" toggle needed. */
const SORT_OPTIONS = [
  { label: "Default", value: "" },
  { label: "Name (A–Z)", value: "name:asc" },
  { label: "Name (Z–A)", value: "name:desc" },
  { label: "Size (largest)", value: "size:desc" },
  { label: "Size (smallest)", value: "size:asc" },
  { label: "Status (actionable)", value: "status:asc" },
  { label: "Type (A–Z)", value: "type:asc" },
  { label: "Type (Z–A)", value: "type:desc" },
] as const

interface SortDropdownProps {
  sort: string | undefined
  order: string | undefined
  onChange: (sort: string | undefined, order: string | undefined) => void
}

export function SortDropdown({ sort, order, onChange }: SortDropdownProps) {
  const current = sort ? `${sort}:${order ?? (sort === "size" ? "desc" : "asc")}` : ""

  return (
    <Select
      aria-label="Sort assets"
      value={current}
      onChange={(e) => {
        const val = e.target.value
        if (!val) {
          onChange(undefined, undefined)
        } else {
          const [field, dir] = val.split(":")
          onChange(field, dir)
        }
      }}
      className="w-44"
    >
      {SORT_OPTIONS.map((opt) => (
        <option key={opt.value} value={opt.value}>
          {opt.label}
        </option>
      ))}
    </Select>
  )
}
