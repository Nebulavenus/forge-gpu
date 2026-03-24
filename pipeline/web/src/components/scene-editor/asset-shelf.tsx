/**
 * Collapsible asset shelf for dragging mesh assets into the 3D viewport.
 *
 * Lists mesh assets fetched from the existing `/api/assets?type=mesh`
 * endpoint. Each item is draggable via HTML5 drag-and-drop — the asset
 * ID is stored in the drag data transfer so the viewport can read it on
 * drop and create a new scene object at the drop position.
 */

import { type SyntheticEvent, useState } from "react"
import { Box, ChevronRight, Package, Search, X } from "lucide-react"
import { Input } from "@/components/ui/input"
import { Button } from "@/components/ui/button"
import { cn } from "@/lib/utils"
import type { AssetInfo } from "@/lib/api"

/** MIME type used to identify asset shelf drag data. */
export const ASSET_DRAG_MIME = "application/x-forge-asset"

interface AssetShelfProps {
  /** Mesh assets to display (pre-filtered by the parent). */
  meshAssets: AssetInfo[]
  isLoading?: boolean
  /** Keyboard alternative: add asset at the origin via Enter/Space. */
  onAddAsset?: (assetId: string) => void
}

/** Thumbnail with lazy loading and Box icon fallback. */
function AssetThumb({ assetId, name }: { assetId: string; name: string }) {
  const [failed, setFailed] = useState(false)

  if (failed) {
    return <Box className="h-3.5 w-3.5 shrink-0 text-green-400" />
  }

  return (
    <img
      src={`/api/assets/${encodeURIComponent(assetId)}/thumbnail`}
      alt={name}
      loading="lazy"
      className="h-3.5 w-3.5 shrink-0 rounded-sm object-cover"
      onError={(e: SyntheticEvent<HTMLImageElement>) => {
        e.currentTarget.style.display = "none"
        setFailed(true)
      }}
    />
  )
}

export function AssetShelf({ meshAssets, isLoading, onAddAsset }: AssetShelfProps) {
  const [collapsed, setCollapsed] = useState(false)
  const [search, setSearch] = useState("")

  const filtered = meshAssets.filter((a) =>
    a.name.toLowerCase().includes(search.toLowerCase()),
  )

  return (
    <div className="flex flex-col border-t border-border h-full overflow-hidden">
      {/* Header — always visible, toggles collapse */}
      <button
        type="button"
        className="flex items-center gap-1 px-3 py-2 text-xs font-medium text-muted-foreground hover:bg-accent"
        onClick={() => setCollapsed(!collapsed)}
        aria-expanded={!collapsed}
        aria-controls="asset-shelf-content"
        aria-label="Toggle asset shelf"
      >
        <ChevronRight
          className={cn(
            "h-3 w-3 shrink-0 transition-transform",
            !collapsed && "rotate-90",
          )}
        />
        <Package className="h-3 w-3 shrink-0" />
        Asset Shelf
      </button>

      {!collapsed && (
        <div id="asset-shelf-content" className="flex flex-col overflow-hidden">
          {/* Search */}
          <div className="flex items-center gap-1 border-t border-border px-2 py-1">
            <Search className="h-3 w-3 text-muted-foreground shrink-0" />
            <Input
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              placeholder="Filter..."
              aria-label="Filter mesh assets"
              className="border-0 shadow-none focus-visible:ring-0 h-6 px-1 text-xs"
            />
            {search && (
              <Button
                variant="ghost"
                size="icon"
                className="h-5 w-5 shrink-0"
                aria-label="Clear search"
                onClick={() => setSearch("")}
              >
                <X className="h-3 w-3" />
              </Button>
            )}
          </div>

          {/* Asset list */}
          <div className="flex-1 overflow-y-auto min-h-0">
            {isLoading && (
              <p className="px-3 py-4 text-center text-xs text-muted-foreground">
                Loading...
              </p>
            )}

            {!isLoading && filtered.length === 0 && (
              <p className="px-3 py-4 text-center text-xs text-muted-foreground">
                {search ? "No matches" : "No mesh assets"}
              </p>
            )}

            {!isLoading && filtered.length > 0 && (
              <div role="list" aria-label="Mesh assets">
                {filtered.map((asset) => (
                  <div
                    key={asset.id}
                    role="listitem"
                    draggable
                    onDragStart={(e) => {
                      e.dataTransfer.setData(ASSET_DRAG_MIME, asset.id)
                      e.dataTransfer.setData("text/plain", asset.name)
                      e.dataTransfer.effectAllowed = "copy"
                    }}
                    className={cn(
                      "flex items-center gap-2 px-2 py-1.5 text-xs cursor-grab",
                      "hover:bg-accent active:cursor-grabbing",
                    )}
                    title={asset.relative_path}
                  >
                    <AssetThumb assetId={asset.id} name={asset.name} />
                    <span className="truncate flex-1">{asset.name}</span>
                    {onAddAsset && (
                      <button
                        type="button"
                        className={cn(
                          "shrink-0 rounded px-1 text-[10px] text-muted-foreground",
                          "hover:bg-primary/10 hover:text-primary",
                          "focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-primary",
                        )}
                        aria-label={`Add ${asset.name} to scene`}
                        onClick={() => onAddAsset(asset.id)}
                      >
                        Add
                      </button>
                    )}
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
