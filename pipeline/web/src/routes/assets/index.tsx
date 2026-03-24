import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { useQuery } from "@tanstack/react-query"
import { useCallback, useEffect, useState } from "react"
import { Search, X } from "lucide-react"
import { fetchAssets, type AssetInfo } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
import { STATUS_META, TYPE_META, statusBadgeVariant, typeBgColor, validateAssetSearch, type AssetSearchParams, type AssetViewMode } from "@/lib/asset-meta"
import { Input } from "@/components/ui/input"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import { TypeFilter } from "@/components/type-filter"
import { SortDropdown } from "@/components/sort-dropdown"
import { ViewToggle } from "@/components/view-toggle"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import { AtlasPreview } from "@/components/atlas-preview"

export const Route = createFileRoute("/assets/")({
  component: AssetBrowser,
  validateSearch: validateAssetSearch,
})

/* ── Thumbnail types that the backend can generate ──────────────── */
const THUMBNAIL_TYPES = new Set(["texture"])

function AssetThumbnail({ asset }: { asset: AssetInfo }) {
  const [failed, setFailed] = useState(false)
  const hasThumbnail = THUMBNAIL_TYPES.has(asset.asset_type) && !failed

  const onError = useCallback(() => setFailed(true), [])

  if (!hasThumbnail) {
    /* Colored icon fallback for non-texture types */
    const meta = TYPE_META[asset.asset_type]
    const Icon = meta?.icon
    return (
      <div role="img" aria-label={`${asset.name} thumbnail`} className={`flex aspect-[4/3] items-center justify-center rounded-t-lg ${meta?.bgColor ?? "bg-muted"}`}>
        {Icon && <Icon aria-hidden className="h-10 w-10 opacity-60" />}
      </div>
    )
  }

  return (
    <div className="flex aspect-[4/3] items-center justify-center overflow-hidden rounded-t-lg bg-muted">
      <img
        src={`/api/assets/${encodeURIComponent(asset.id)}/thumbnail`}
        alt={`${asset.name} thumbnail`}
        width={128}
        height={128}
        loading="lazy"
        onError={onError}
        className="h-full w-full object-cover"
      />
    </div>
  )
}

function ListThumbnail({ asset }: { asset: AssetInfo }) {
  const [failed, setFailed] = useState(false)
  const onError = useCallback(() => setFailed(true), [])
  const meta = TYPE_META[asset.asset_type]
  const Icon = meta?.icon

  if (THUMBNAIL_TYPES.has(asset.asset_type) && !failed) {
    return (
      <img
        src={`/api/assets/${encodeURIComponent(asset.id)}/thumbnail`}
        alt=""
        width={32}
        height={32}
        loading="lazy"
        onError={onError}
        className="h-8 w-8 rounded object-cover"
      />
    )
  }

  if (Icon) {
    return (
      <div className={`flex h-8 w-8 items-center justify-center rounded ${meta?.bgColor ?? "bg-muted"}`}>
        <Icon aria-hidden className="h-4 w-4 opacity-60" />
      </div>
    )
  }

  return <div className="h-8 w-8 rounded bg-muted" />
}

function AssetBrowser() {
  const navigate = useNavigate()
  const { type: searchType, status: statusFilter, search: searchQuery, sort: sortField, order: sortOrder, view: viewMode } = Route.useSearch()
  const typeFilter = searchType ?? ""
  const currentView: AssetViewMode = viewMode ?? "grid"
  const [localSearch, setLocalSearch] = useState(searchQuery ?? "")

  /* Sync local input when the URL search param changes externally */
  useEffect(() => {
    setLocalSearch(searchQuery ?? "")
  }, [searchQuery])

  /** Build search params preserving all current filters and sort state.
   *  Uses the live localSearch input so control-driven navigations
   *  (sort, type filter) keep the user's in-progress text. */
  function currentSearch(overrides?: Partial<AssetSearchParams>): AssetSearchParams {
    const merged = {
      type: searchType,
      status: statusFilter,
      search: localSearch.trim() || undefined,
      sort: sortField,
      order: sortOrder,
      view: viewMode,
      ...overrides,
    }
    // Strip undefined/empty values so the URL stays clean
    return Object.fromEntries(
      Object.entries(merged).filter(([, v]) => v != null && v !== ""),
    ) as AssetSearchParams
  }

  /* Debounce typing → URL update */
  useEffect(() => {
    const timer = setTimeout(() => {
      const trimmed = localSearch.trim()
      if (trimmed !== (searchQuery ?? "")) {
        navigate({
          to: "/assets",
          search: currentSearch({ search: trimmed || undefined }),
          replace: true,
        })
      }
    }, 300)
    return () => clearTimeout(timer)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [localSearch, navigate, searchType, statusFilter, searchQuery, sortField, sortOrder, viewMode])

  const { data, isLoading, error } = useQuery({
    queryKey: ["assets", typeFilter, statusFilter, searchQuery, sortField, sortOrder],
    queryFn: () =>
      fetchAssets({
        type: typeFilter || undefined,
        status: statusFilter || undefined,
        search: searchQuery || undefined,
        sort: sortField || undefined,
        order: sortOrder || undefined,
      }),
  })

  /** Build search params for detail navigation. Uses the committed
   *  searchQuery (not live input) so the URL reflects the actual query. */
  function detailSearch(): AssetSearchParams {
    return currentSearch({ search: searchQuery?.trim() || undefined })
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
                    search: currentSearch({ status: undefined }),
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
                    search: currentSearch({ type: undefined }),
                  })
                }
              >
                <X className="h-3 w-3" />
              </Button>
            </Badge>
          )}
        </div>
        <div className="flex items-center gap-2">
          <SortDropdown
            sort={sortField}
            order={sortOrder}
            onChange={(newSort, newOrder) =>
              navigate({
                to: "/assets",
                search: currentSearch({ sort: newSort, order: newOrder }),
              })
            }
          />
          <TypeFilter
            value={typeFilter}
            onChange={(newType) =>
              navigate({
                to: "/assets",
                search: currentSearch({ type: newType || undefined }),
              })
            }
          />
          <ViewToggle
            value={currentView}
            onChange={(mode) =>
              navigate({
                to: "/assets",
                search: currentSearch({ view: mode === "grid" ? undefined : mode }),
              })
            }
          />
        </div>
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

      {data && data.assets.length > 0 && currentView === "grid" && (
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
          {data.assets.map((asset) => (
            <Card
              key={asset.id}
              className="cursor-pointer overflow-hidden transition-colors hover:bg-card/80"
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
              <AssetThumbnail asset={asset} />
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

      {data && data.assets.length > 0 && currentView === "list" && (
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead className="w-10"><span className="sr-only">Thumbnail</span></TableHead>
              <TableHead>Name</TableHead>
              <TableHead>Type</TableHead>
              <TableHead>Size</TableHead>
              <TableHead>Output Size</TableHead>
              <TableHead>Status</TableHead>
              <TableHead>Fingerprint</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {data.assets.map((asset) => (
                <TableRow
                  key={asset.id}
                  className="cursor-pointer"
                  onClick={() =>
                    navigate({
                      to: "/assets/$assetId",
                      params: { assetId: asset.id },
                      search: detailSearch(),
                    })
                  }
                >
                  <TableCell className="w-10 p-1">
                    <ListThumbnail asset={asset} />
                  </TableCell>
                  <TableCell className="font-medium">
                    <Link
                      to="/assets/$assetId"
                      params={{ assetId: asset.id }}
                      search={detailSearch()}
                      className="hover:underline focus:outline-none focus-visible:underline"
                      onClick={(e) => e.stopPropagation()}
                    >
                      {asset.name}
                    </Link>
                  </TableCell>
                  <TableCell>
                    <span className={`inline-flex items-center rounded-md px-2 py-0.5 text-xs font-medium ${typeBgColor(asset.asset_type)}`}>
                      {asset.asset_type}
                    </span>
                  </TableCell>
                  <TableCell className="text-muted-foreground">{formatBytes(asset.file_size)}</TableCell>
                  <TableCell className="text-muted-foreground">
                    {asset.output_size != null ? formatBytes(asset.output_size) : "—"}
                  </TableCell>
                  <TableCell>
                    <Badge variant={statusBadgeVariant(asset.status)} className="text-xs">
                      {asset.status}
                    </Badge>
                  </TableCell>
                  <TableCell className="font-mono text-xs text-muted-foreground">
                    {asset.fingerprint.slice(0, 12)}
                  </TableCell>
                </TableRow>
            ))}
          </TableBody>
        </Table>
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
