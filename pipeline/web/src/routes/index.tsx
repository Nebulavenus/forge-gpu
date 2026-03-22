import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { useQuery } from "@tanstack/react-query"
import { useEffect, useState } from "react"
import { Search } from "lucide-react"
import { fetchAssets } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
import { Input } from "@/components/ui/input"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { TypeFilter } from "@/components/type-filter"
import { AtlasPreview } from "@/components/atlas-preview"

export const Route = createFileRoute("/")({
  component: AssetBrowser,
})

function statusVariant(status: string) {
  switch (status) {
    case "processed":
      return "default" as const
    case "new":
      return "secondary" as const
    case "changed":
      return "secondary" as const
    case "missing":
      return "destructive" as const
    default:
      return "outline" as const
  }
}

function typeColor(type: string) {
  switch (type) {
    case "texture":
      return "bg-blue-500/20 text-blue-400"
    case "mesh":
      return "bg-green-500/20 text-green-400"
    case "animation":
      return "bg-purple-500/20 text-purple-400"
    case "scene":
      return "bg-amber-500/20 text-amber-400"
    default:
      return "bg-muted text-muted-foreground"
  }
}

function AssetBrowser() {
  const navigate = useNavigate()
  const [search, setSearch] = useState("")
  const [debouncedSearch, setDebouncedSearch] = useState("")
  const [typeFilter, setTypeFilter] = useState("")

  useEffect(() => {
    const timer = setTimeout(() => setDebouncedSearch(search), 300)
    return () => clearTimeout(timer)
  }, [search])

  const { data, isLoading, error } = useQuery({
    queryKey: ["assets", typeFilter, debouncedSearch],
    queryFn: () =>
      fetchAssets({
        type: typeFilter || undefined,
        search: debouncedSearch || undefined,
      }),
  })

  return (
    <div className="space-y-6">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div className="relative w-full max-w-sm">
          <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-muted-foreground" />
          <Input
            placeholder="Search assets..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            className="pl-9"
          />
        </div>
        <TypeFilter value={typeFilter} onChange={setTypeFilter} />
      </div>

      {isLoading && (
        <div className="py-12 text-center text-muted-foreground">
          Loading assets...
        </div>
      )}

      {error && (
        <div className="py-12 text-center text-destructive">
          Failed to load assets: {(error as Error).message}
        </div>
      )}

      {data && data.assets.length === 0 && (
        <div className="py-12 text-center text-muted-foreground">
          No assets found.
        </div>
      )}

      {data && data.assets.length > 0 && (
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
          {data.assets.map((asset) => (
            <Card
              key={asset.id}
              className="cursor-pointer transition-colors hover:bg-card/80"
              role="button"
              tabIndex={0}
              onClick={() =>
                navigate({ to: "/assets/$assetId", params: { assetId: asset.id } })
              }
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault()
                  navigate({ to: "/assets/$assetId", params: { assetId: asset.id } })
                }
              }}
            >
              <CardHeader className="pb-3">
                <CardTitle className="flex items-center justify-between text-sm">
                  <span className="truncate">{asset.name}</span>
                  <span
                    className={`ml-2 inline-flex shrink-0 items-center rounded-md px-2 py-0.5 text-xs font-medium ${typeColor(asset.asset_type)}`}
                  >
                    {asset.asset_type}
                  </span>
                </CardTitle>
              </CardHeader>
              <CardContent>
                <div className="flex items-center justify-between text-xs text-muted-foreground">
                  <span>{formatBytes(asset.file_size)}</span>
                  <Badge variant={statusVariant(asset.status)} className="text-xs">
                    {asset.status}
                  </Badge>
                </div>
              </CardContent>
            </Card>
          ))}
        </div>
      )}

      {data && (
        <div className="text-xs text-muted-foreground">
          {data.total} asset{data.total !== 1 ? "s" : ""}
        </div>
      )}

      <AtlasPreview />
    </div>
  )
}
