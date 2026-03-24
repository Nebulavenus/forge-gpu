import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { useQuery } from "@tanstack/react-query"
import { useEffect, useState } from "react"
import { Search, X } from "lucide-react"
import { fetchAssets } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
import { STATUS_META, TYPE_META, statusBadgeVariant, typeBgColor, validateAssetSearch, type AssetSearchParams } from "@/lib/asset-meta"
import { Input } from "@/components/ui/input"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import { TypeFilter } from "@/components/type-filter"
import { AtlasPreview } from "@/components/atlas-preview"

export const Route = createFileRoute("/assets/")({
  component: AssetBrowser,
  validateSearch: validateAssetSearch,
})

function AssetBrowser() {
  const navigate = useNavigate()
  const { type: searchType, status: statusFilter, search: searchQuery } = Route.useSearch()
  const typeFilter = searchType ?? ""
  const [localSearch, setLocalSearch] = useState(searchQuery ?? "")

  /* Sync local input when the URL search param changes externally */
  useEffect(() => {
    setLocalSearch(searchQuery ?? "")
  }, [searchQuery])

  /* Debounce typing → URL update */
  useEffect(() => {
    const timer = setTimeout(() => {
      const trimmed = localSearch.trim()
      if (trimmed !== (searchQuery ?? "")) {
        navigate({
          to: "/assets",
          search: {
            ...(searchType ? { type: searchType } : {}),
            ...(statusFilter ? { status: statusFilter } : {}),
            ...(trimmed ? { search: trimmed } : {}),
          },
          replace: true,
        })
      }
    }, 300)
    return () => clearTimeout(timer)
  }, [localSearch, navigate, searchType, statusFilter, searchQuery])

  const { data, isLoading, error } = useQuery({
    queryKey: ["assets", typeFilter, statusFilter, searchQuery],
    queryFn: () =>
      fetchAssets({
        type: typeFilter || undefined,
        status: statusFilter || undefined,
        search: searchQuery || undefined,
      }),
  })

  /** Build search params for detail navigation. Round-trips the committed
   *  filter state so pressing Back returns to the same list. */
  function detailSearch(): AssetSearchParams {
    return {
      ...(searchType ? { type: searchType } : {}),
      ...(statusFilter ? { status: statusFilter } : {}),
      ...(searchQuery ? { search: searchQuery } : {}),
    }
  }

  return (
    <div className="space-y-6">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div className="flex items-center gap-2">
          <div className="relative w-full max-w-sm">
            <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-muted-foreground" aria-hidden="true" />
            <Input
              aria-label="Search assets"
              placeholder="Search assets..."
              value={localSearch}
              onChange={(e) => setLocalSearch(e.target.value)}
              className="pl-9"
            />
          </div>
          {statusFilter && (
            <Badge variant={statusBadgeVariant(statusFilter)} className="flex items-center gap-1 whitespace-nowrap">
              {STATUS_META[statusFilter]?.label ?? statusFilter}
              <Button
                variant="ghost"
                size="icon"
                className="h-4 w-4 p-0 hover:bg-transparent"
                aria-label="Clear status filter"
                onClick={() =>
                  navigate({
                    to: "/assets",
                    search: {
                      ...(searchType ? { type: searchType } : {}),
                      ...(localSearch.trim() ? { search: localSearch.trim() } : {}),
                    },
                  })
                }
              >
                <X className="h-3 w-3" />
              </Button>
            </Badge>
          )}
          {searchType && !TYPE_META[searchType] && (
            <Badge variant="outline" className="flex items-center gap-1 whitespace-nowrap">
              {searchType}
              <Button
                variant="ghost"
                size="icon"
                className="h-4 w-4 p-0 hover:bg-transparent"
                aria-label="Clear type filter"
                onClick={() =>
                  navigate({
                    to: "/assets",
                    search: {
                      ...(statusFilter ? { status: statusFilter } : {}),
                      ...(localSearch.trim() ? { search: localSearch.trim() } : {}),
                    },
                  })
                }
              >
                <X className="h-3 w-3" />
              </Button>
            </Badge>
          )}
        </div>
        <TypeFilter
          value={typeFilter}
          onChange={(newType) =>
            navigate({
              to: "/assets",
              search: {
                ...(newType ? { type: newType } : {}),
                ...(statusFilter ? { status: statusFilter } : {}),
                ...(localSearch.trim() ? { search: localSearch.trim() } : {}),
              },
            })
          }
        />
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
                navigate({
                  to: "/assets/$assetId",
                  params: { assetId: asset.id },
                  search: detailSearch(),
                })
              }
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault()
                  navigate({
                    to: "/assets/$assetId",
                    params: { assetId: asset.id },
                    search: detailSearch(),
                  })
                }
              }}
            >
              <CardHeader className="pb-3">
                <CardTitle className="flex items-center justify-between text-sm">
                  <span className="truncate">{asset.name}</span>
                  <span
                    className={`ml-2 inline-flex shrink-0 items-center rounded-md px-2 py-0.5 text-xs font-medium ${typeBgColor(asset.asset_type)}`}
                  >
                    {asset.asset_type}
                  </span>
                </CardTitle>
              </CardHeader>
              <CardContent>
                <div className="flex items-center justify-between text-xs text-muted-foreground">
                  <span>{formatBytes(asset.file_size)}</span>
                  <Badge variant={statusBadgeVariant(asset.status)} className="text-xs">
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
