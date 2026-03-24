import { Image, Box, Film, Map } from "lucide-react"

/* ── Status metadata ──────────────────────────────────────────── */

export interface StatusMeta {
  bg: string
  label: string
  badgeVariant: "default" | "secondary" | "destructive" | "outline"
}

export const STATUS_META: Record<string, StatusMeta> = {
  processed: { bg: "bg-emerald-500", label: "Processed", badgeVariant: "default" },
  new: { bg: "bg-blue-500", label: "New", badgeVariant: "secondary" },
  changed: { bg: "bg-amber-500", label: "Changed", badgeVariant: "secondary" },
  missing: { bg: "bg-red-500", label: "Missing", badgeVariant: "destructive" },
}

export function statusBadgeVariant(status: string) {
  return STATUS_META[status]?.badgeVariant ?? ("outline" as const)
}

/* ── Type metadata ────────────────────────────────────────────── */

export interface TypeMeta {
  icon: typeof Image
  color: string
  bgColor: string
  label: string
  filterValue: string
}

export const TYPE_META: Record<string, TypeMeta> = {
  texture: { icon: Image, color: "text-blue-400", bgColor: "bg-blue-500/20 text-blue-400", label: "Textures", filterValue: "texture" },
  mesh: { icon: Box, color: "text-green-400", bgColor: "bg-green-500/20 text-green-400", label: "Meshes", filterValue: "mesh" },
  animation: { icon: Film, color: "text-purple-400", bgColor: "bg-purple-500/20 text-purple-400", label: "Animations", filterValue: "animation" },
  scene: { icon: Map, color: "text-amber-400", bgColor: "bg-amber-500/20 text-amber-400", label: "Scenes", filterValue: "scene" },
}

export function typeBgColor(type: string): string {
  return TYPE_META[type]?.bgColor ?? "bg-muted text-muted-foreground"
}

/* ── Shared asset search params ───────────────────────────────── */

export interface AssetSearchParams {
  type?: string
  status?: string
  search?: string
  sort?: string
  order?: string
}

const VALID_SORT_FIELDS = new Set(["name", "size", "status", "type"])
const VALID_SORT_ORDERS = new Set(["asc", "desc"])

export function validateAssetSearch(search: Record<string, unknown>): AssetSearchParams {
  const type = typeof search.type === "string" ? search.type.trim() : undefined
  const status = typeof search.status === "string" ? search.status.trim() : undefined
  const s = typeof search.search === "string" ? search.search.trim() : undefined
  const rawSort = typeof search.sort === "string" ? search.sort.trim() : undefined
  const rawOrder = typeof search.order === "string" ? search.order.trim() : undefined
  const sort = rawSort && VALID_SORT_FIELDS.has(rawSort) ? rawSort : undefined
  const order = sort && rawOrder && VALID_SORT_ORDERS.has(rawOrder) ? rawOrder : undefined
  return {
    type: type || undefined,
    status: status || undefined,
    search: s || undefined,
    sort: sort || undefined,
    order: order || undefined,
  }
}
