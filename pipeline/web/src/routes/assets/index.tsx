import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query"
import { useCallback, useEffect, useMemo, useState } from "react"
import { Search, X, CheckSquare, Loader2, AlertCircle, Check } from "lucide-react"
import { fetchAssets, processBatch, type AssetInfo, type BatchProcessResponse } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
import { STATUS_META, TYPE_META, statusBadgeVariant, typeBgColor, validateAssetSearch, type AssetSearchParams, type AssetViewMode } from "@/lib/asset-meta"
import { Input } from "@/components/ui/input"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import { Checkbox } from "@/components/ui/checkbox"
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

/* ── Floating batch action bar ──────────────────────────────────── */

interface BatchActionBarProps {
  selectedCount: number
  isProcessing: boolean
  batchResult: BatchProcessResponse | null
  batchError: Error | null
  onProcess: () => void
  onClear: () => void
}

function BatchActionBar({
  selectedCount,
  isProcessing,
  batchResult,
  batchError,
  onProcess,
  onClear,
}: BatchActionBarProps) {
  if (selectedCount === 0) return null

  return (
    <div
      role="toolbar"
      aria-label="Batch actions"
      className="fixed bottom-6 left-1/2 z-50 flex -translate-x-1/2 items-center gap-3 rounded-lg border border-border bg-card px-4 py-3 shadow-lg"
    >
      {/* Result summary */}
      {batchResult && (
        <div className="flex items-center gap-2 text-xs" role="status">
          <Check className="h-3.5 w-3.5 text-emerald-500" aria-hidden />
          <span>
            {batchResult.succeeded} succeeded
            {batchResult.failed > 0 && (
              <span className="text-destructive">, {batchResult.failed} failed</span>
            )}
            {batchResult.skipped > 0 && (
              <span className="text-muted-foreground">, {batchResult.skipped} skipped</span>
            )}
          </span>
        </div>
      )}

      {/* Error message */}
      {batchError && !batchResult && (
        <div className="flex items-center gap-2 text-xs text-destructive" role="alert">
          <AlertCircle className="h-3.5 w-3.5" aria-hidden />
          <span className="max-w-48 truncate">{batchError.message}</span>
        </div>
      )}

      {/* Selection count and actions */}
      {!batchResult && !batchError && (
        <span className="text-sm text-muted-foreground">
          {selectedCount} selected
        </span>
      )}

      <Button
        size="sm"
        onClick={onProcess}
        disabled={isProcessing}
        aria-label={`Process ${selectedCount} selected asset${selectedCount !== 1 ? "s" : ""}`}
      >
        {isProcessing ? (
          <Loader2 className="h-3.5 w-3.5 animate-spin" aria-hidden />
        ) : (
          <CheckSquare className="h-3.5 w-3.5" aria-hidden />
        )}
        {isProcessing ? "Processing..." : `Process Selected (${selectedCount})`}
      </Button>

      <Button
        variant="ghost"
        size="sm"
        onClick={onClear}
        disabled={isProcessing}
        aria-label="Clear selection"
      >
        <X className="h-3.5 w-3.5" aria-hidden />
        Clear
      </Button>
    </div>
  )
}

/* ── Main asset browser ─────────────────────────────────────────── */

function AssetBrowser() {
  const navigate = useNavigate()
  const queryClient = useQueryClient()
  const { type: searchType, status: statusFilter, search: searchQuery, sort: sortField, order: sortOrder, view: viewMode } = Route.useSearch()
  const typeFilter = searchType ?? ""
  const currentView: AssetViewMode = viewMode ?? "grid"
  const [localSearch, setLocalSearch] = useState(searchQuery ?? "")

  /* ── Batch processing mutation ─────────────────────────────────── */
  const batchMutation = useMutation<BatchProcessResponse, Error, string[]>({
    mutationFn: (ids) => processBatch(ids),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ["assets"] })
    },
  })

  /* ── Select mode state ─────────────────────────────────────────── */
  const [selectMode, setSelectMode] = useState(false)
  const [selectedIds, setSelectedIds] = useState<Set<string>>(new Set())

  const isBatchPending = batchMutation.isPending

  const toggleSelectMode = useCallback(() => {
    if (isBatchPending) return
    batchMutation.reset()
    setSelectMode((prev) => {
      if (prev) {
        // Exiting select mode — clear selection
        setSelectedIds(new Set())
      }
      return !prev
    })
  }, [isBatchPending, batchMutation])

  const toggleSelection = useCallback((id: string) => {
    if (isBatchPending) return
    batchMutation.reset()
    setSelectedIds((prev) => {
      const next = new Set(prev)
      if (next.has(id)) {
        next.delete(id)
      } else {
        next.add(id)
      }
      return next
    })
  }, [isBatchPending, batchMutation])

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

  const handleBatchProcess = useCallback(() => {
    const ids = Array.from(selectedIds)
    if (ids.length === 0) return
    batchMutation.mutate(ids)
  }, [selectedIds, batchMutation])

  const handleBatchClear = useCallback(() => {
    batchMutation.reset()
    // Bypass the isBatchPending guard — reset above clears pending state
    // but the closure still sees the old value, so set state directly.
    setSelectedIds(new Set())
    setSelectMode(false)
  }, [batchMutation])

  /* ── Select-all logic ──────────────────────────────────────────── */
  const visibleIds = useMemo(
    () => data?.assets.map((a) => a.id) ?? [],
    [data],
  )

  const allSelected = visibleIds.length > 0 && visibleIds.every((id) => selectedIds.has(id))
  const someSelected = visibleIds.some((id) => selectedIds.has(id))

  const toggleSelectAll = useCallback(() => {
    if (isBatchPending) return
    batchMutation.reset()
    if (allSelected) {
      // Deselect all visible
      setSelectedIds((prev) => {
        const next = new Set(prev)
        for (const id of visibleIds) next.delete(id)
        return next
      })
    } else {
      // Select all visible
      setSelectedIds((prev) => {
        const next = new Set(prev)
        for (const id of visibleIds) next.add(id)
        return next
      })
    }
  }, [isBatchPending, allSelected, visibleIds, batchMutation])

  /** Build search params for detail navigation. Uses the committed
   *  searchQuery (not live input) so the URL reflects the actual query. */
  function detailSearch(): AssetSearchParams {
    return currentSearch({ search: searchQuery?.trim() || undefined })
  }

  /** Handle card/row click — toggle selection in select mode, navigate otherwise */
  function handleAssetClick(asset: AssetInfo) {
    if (selectMode) {
      toggleSelection(asset.id)
    } else {
      navigate({
        to: "/assets/$assetId",
        params: { assetId: asset.id },
        search: detailSearch(),
      })
    }
  }

  function handleAssetKeyDown(e: React.KeyboardEvent, asset: AssetInfo) {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault()
      handleAssetClick(asset)
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
          <Button
            variant={selectMode ? "secondary" : "ghost"}
            size="sm"
            onClick={toggleSelectMode}
            disabled={isBatchPending}
            aria-label={selectMode ? "Exit select mode" : "Enter select mode"}
            aria-pressed={selectMode}
            title={selectMode ? "Exit select mode" : "Select multiple assets"}
          >
            <CheckSquare className="h-4 w-4" aria-hidden />
            Select
          </Button>
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

      {/* Select-all bar — visible only in select mode with loaded assets */}
      {selectMode && data && data.assets.length > 0 && (
        <div
          className="flex items-center gap-3 rounded-md border border-border bg-card px-3 py-2"
          role="toolbar"
          aria-label="Selection controls"
        >
          <Checkbox
            checked={allSelected}
            indeterminate={someSelected && !allSelected}
            onCheckedChange={toggleSelectAll}
            disabled={isBatchPending}
            aria-label={allSelected ? "Deselect all assets" : "Select all assets"}
          />
          <span className="text-xs text-muted-foreground">
            {allSelected ? "Deselect all" : "Select all"}
            {selectedIds.size > 0 && ` (${selectedIds.size} selected)`}
          </span>
        </div>
      )}

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
        <div
          className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4"
          role={selectMode ? "listbox" : undefined}
          aria-label={selectMode ? "Selectable assets" : undefined}
          aria-multiselectable={selectMode ? true : undefined}
        >
          {data.assets.map((asset) => (
            <Card
              key={asset.id}
              className={`cursor-pointer overflow-hidden transition-colors hover:bg-card/80 ${
                selectMode && selectedIds.has(asset.id) ? "ring-2 ring-primary" : ""
              } ${isBatchPending ? "pointer-events-none opacity-60" : ""}`}
              role={selectMode ? "option" : "button"}
              tabIndex={isBatchPending ? -1 : 0}
              aria-selected={selectMode ? selectedIds.has(asset.id) : undefined}
              aria-disabled={isBatchPending || undefined}
              onClick={() => handleAssetClick(asset)}
              onKeyDown={(e) => handleAssetKeyDown(e, asset)}
            >
              {/* Checkbox overlay in select mode */}
              {selectMode && (
                <div className="relative">
                  <AssetThumbnail asset={asset} />
                  <div className="absolute left-2 top-2">
                    <Checkbox
                      checked={selectedIds.has(asset.id)}
                      onCheckedChange={() => toggleSelection(asset.id)}
                      disabled={isBatchPending}
                      aria-label={`Select ${asset.name}`}
                    />
                  </div>
                </div>
              )}
              {!selectMode && <AssetThumbnail asset={asset} />}
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
              {selectMode && (
                <TableHead className="w-8">
                  <Checkbox
                    checked={allSelected}
                    indeterminate={someSelected && !allSelected}
                    onCheckedChange={toggleSelectAll}
                    disabled={isBatchPending}
                    aria-label={allSelected ? "Deselect all assets" : "Select all assets"}
                  />
                </TableHead>
              )}
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
                  className={`cursor-pointer ${
                    selectMode && selectedIds.has(asset.id) ? "bg-primary/5" : ""
                  } ${isBatchPending ? "pointer-events-none opacity-60" : ""}`}
                  aria-selected={selectMode ? selectedIds.has(asset.id) : undefined}
                  aria-disabled={isBatchPending || undefined}
                  onClick={() => handleAssetClick(asset)}
                >
                  {selectMode && (
                    <TableCell className="w-8 p-1">
                      <Checkbox
                        checked={selectedIds.has(asset.id)}
                        onCheckedChange={() => toggleSelection(asset.id)}
                        disabled={isBatchPending}
                        aria-label={`Select ${asset.name}`}
                      />
                    </TableCell>
                  )}
                  <TableCell className="w-10 p-1">
                    <ListThumbnail asset={asset} />
                  </TableCell>
                  <TableCell className="font-medium">
                    {selectMode ? (
                      <span>{asset.name}</span>
                    ) : (
                      <Link
                        to="/assets/$assetId"
                        params={{ assetId: asset.id }}
                        search={detailSearch()}
                        className="hover:underline focus:outline-none focus-visible:underline"
                        onClick={(e) => e.stopPropagation()}
                      >
                        {asset.name}
                      </Link>
                    )}
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

      {/* Floating batch action bar */}
      <BatchActionBar
        selectedCount={selectedIds.size}
        isProcessing={batchMutation.isPending}
        batchResult={batchMutation.data ?? null}
        batchError={batchMutation.error ?? null}
        onProcess={handleBatchProcess}
        onClear={handleBatchClear}
      />
    </div>
  )
}
