import { useEffect, useRef, useState } from "react"
import { Search, X, Box, FileQuestion } from "lucide-react"
import { Input } from "@/components/ui/input"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { cn, formatBytes } from "@/lib/utils"
import type { AssetInfo } from "@/lib/api"

interface AssetPickerProps {
  assets: AssetInfo[]
  isLoading?: boolean
  isError?: boolean
  error?: Error | null
  onSelect: (asset: AssetInfo | null) => void
  onCancel: () => void
}

/**
 * A searchable dropdown overlay for selecting mesh assets.
 * Replaces the previous window.prompt() picker with a filterable,
 * click-based selection UI.
 */
export function AssetPicker({
  assets,
  isLoading,
  isError,
  error,
  onSelect,
  onCancel,
}: AssetPickerProps) {
  const [search, setSearch] = useState("")
  const inputRef = useRef<HTMLInputElement>(null)
  const panelRef = useRef<HTMLDivElement>(null)
  const listRef = useRef<HTMLDivElement>(null)
  const [focusIndex, setFocusIndex] = useState(-1)

  // Filter assets by search query
  const filtered = assets.filter((a) =>
    a.name.toLowerCase().includes(search.toLowerCase()),
  )

  // Focus the search input on mount and trap focus within the panel
  useEffect(() => {
    const prev = document.activeElement as HTMLElement | null
    inputRef.current?.focus()
    return () => {
      prev?.focus()
    }
  }, [])

  // Trap Tab/Shift+Tab within the dialog panel
  useEffect(() => {
    const panel = panelRef.current
    if (!panel) return

    const handleTab = (e: KeyboardEvent) => {
      if (e.key !== "Tab") return
      const focusable = panel.querySelectorAll<HTMLElement>(
        'input, button, [tabindex]:not([tabindex="-1"])',
      )
      if (focusable.length === 0) return

      const first = focusable[0]!
      const last = focusable[focusable.length - 1]!

      if (e.shiftKey && document.activeElement === first) {
        e.preventDefault()
        last.focus()
      } else if (!e.shiftKey && document.activeElement === last) {
        e.preventDefault()
        first.focus()
      }
    }

    panel.addEventListener("keydown", handleTab)
    return () => panel.removeEventListener("keydown", handleTab)
  }, [])

  // Reset focus index when search changes
  useEffect(() => {
    setFocusIndex(-1)
  }, [search])

  // Scroll focused item into view
  useEffect(() => {
    if (focusIndex < 0 || !listRef.current) return
    const items = listRef.current.querySelectorAll("[data-asset-item]")
    items[focusIndex]?.scrollIntoView({ block: "nearest" })
  }, [focusIndex])

  const handleKeyDown = (e: React.KeyboardEvent) => {
    // Include the placeholder item (+1) in navigation count
    const itemCount = filtered.length + 1

    switch (e.key) {
      case "ArrowDown":
        e.preventDefault()
        setFocusIndex((i) => (i === -1 ? 0 : (i + 1) % itemCount))
        break
      case "ArrowUp":
        e.preventDefault()
        setFocusIndex((i) =>
          i === -1 ? itemCount - 1 : (i - 1 + itemCount) % itemCount,
        )
        break
      case "Enter": {
        // Let native activation proceed for focused interactive elements
        // (e.g. the clear button) so their onClick fires normally
        const tag = (e.target as HTMLElement).tagName
        if (tag === "BUTTON" || tag === "A" || tag === "SELECT") break

        e.preventDefault()
        if (focusIndex >= 0 && focusIndex < filtered.length) {
          onSelect(filtered[focusIndex]!)
        } else if (focusIndex === filtered.length) {
          // Placeholder item
          onSelect(null)
        } else if (focusIndex < 0 && filtered.length === 1) {
          // Auto-select when only one match remains
          onSelect(filtered[0]!)
        }
        break
      }
      case "Escape":
        e.preventDefault()
        onCancel()
        break
    }
  }

  return (
    // Backdrop
    <div
      className="fixed inset-0 z-50 flex items-start justify-center pt-[15vh] bg-black/50"
      onClick={onCancel}
    >
      {/* Panel */}
      <div
        ref={panelRef}
        role="dialog"
        aria-modal="true"
        aria-label="Select a mesh asset"
        className="w-full max-w-md rounded-lg border border-border bg-card shadow-lg"
        onClick={(e) => e.stopPropagation()}
        onKeyDown={handleKeyDown}
      >
        {/* Header */}
        <div className="flex items-center gap-2 border-b border-border px-3 py-2">
          <Search className="h-4 w-4 text-muted-foreground shrink-0" />
          <Input
            ref={inputRef}
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Search mesh assets..."
            className="border-0 shadow-none focus-visible:ring-0 h-7 px-0"
          />
          {search && (
            <Button
              variant="ghost"
              size="icon"
              aria-label="Clear search"
              className="h-6 w-6 shrink-0"
              onClick={() => setSearch("")}
            >
              <X className="h-3 w-3" />
            </Button>
          )}
        </div>

        {/* Asset list */}
        <div ref={listRef} className="max-h-72 overflow-y-auto p-1">
          {isLoading && (
            <div className="px-3 py-6 text-center text-sm text-muted-foreground">
              Loading mesh assets...
            </div>
          )}

          {isError && (
            <div className="px-3 py-6 text-center text-sm text-destructive">
              Failed to load assets{error?.message ? `: ${error.message}` : ""}
            </div>
          )}

          {!isLoading && !isError && filtered.length === 0 && (
            <div className="px-3 py-6 text-center text-sm text-muted-foreground">
              {search
                ? <>No assets match &quot;{search}&quot;</>
                : "No mesh assets available"}
            </div>
          )}

          {filtered.map((asset, i) => (
            <button
              key={asset.id}
              data-asset-item
              type="button"
              className={cn(
                "flex w-full items-center gap-3 rounded-md px-3 py-2 text-left text-sm transition-colors",
                "hover:bg-accent hover:text-accent-foreground",
                focusIndex === i && "bg-accent text-accent-foreground",
              )}
              onClick={() => onSelect(asset)}
              onMouseEnter={() => setFocusIndex(i)}
            >
              <Box className="h-5 w-5 shrink-0 text-muted-foreground" />
              <div className="flex-1 min-w-0">
                <div className="truncate font-medium">{asset.name}</div>
                <div className="flex items-center gap-2 text-xs text-muted-foreground">
                  <span className="truncate">{asset.relative_path}</span>
                  {asset.file_size > 0 && (
                    <span className="shrink-0">
                      {formatBytes(asset.file_size)}
                    </span>
                  )}
                </div>
              </div>
              <Badge variant="secondary" className="shrink-0 text-[10px]">
                {asset.asset_type}
              </Badge>
            </button>
          ))}

          {/* Empty placeholder option — always visible */}
          <button
            data-asset-item
            type="button"
            className={cn(
              "flex w-full items-center gap-3 rounded-md px-3 py-2 text-left text-sm transition-colors",
              "hover:bg-accent hover:text-accent-foreground",
              focusIndex === filtered.length && "bg-accent text-accent-foreground",
            )}
            onClick={() => onSelect(null)}
            onMouseEnter={() => setFocusIndex(filtered.length)}
          >
            <FileQuestion className="h-5 w-5 shrink-0 text-muted-foreground" />
            <div className="flex-1 min-w-0">
              <div className="font-medium text-muted-foreground">
                Empty Object
              </div>
              <div className="text-xs text-muted-foreground">
                No mesh — placeholder transform node
              </div>
            </div>
          </button>
        </div>

        {/* Footer hint */}
        <div className="border-t border-border px-3 py-1.5 text-[11px] text-muted-foreground">
          <kbd className="rounded border border-border bg-muted px-1">↑↓</kbd>{" "}
          navigate{" "}
          <kbd className="rounded border border-border bg-muted px-1">Enter</kbd>{" "}
          select{" "}
          <kbd className="rounded border border-border bg-muted px-1">Esc</kbd>{" "}
          cancel
        </div>
      </div>
    </div>
  )
}
