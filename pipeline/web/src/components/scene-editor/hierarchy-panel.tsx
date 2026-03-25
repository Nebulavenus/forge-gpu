import {
  type Dispatch,
  type DragEvent,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react"
import { ChevronRight, EyeOff, Search } from "lucide-react"
import { cn } from "@/lib/utils"
import type { SceneAction, SceneObject } from "./types"

// ── Constants ────────────────────────────────────────────────────────────

/** MIME type used for hierarchy drag-and-drop. */
const HIERARCHY_DRAG_MIME = "application/x-forge-hierarchy"

/** Drop zone: on the node (reparent as child) vs. between nodes (reorder). */
type DropPosition = "before" | "on" | "after"

// ── Context menu ─────────────────────────────────────────────────────────

interface ContextMenuState {
  objectId: string
  x: number
  y: number
}

interface ContextMenuProps {
  menu: ContextMenuState
  object: SceneObject
  dispatch: Dispatch<SceneAction>
  onClose: () => void
  onStartRename: (objectId: string) => void
}

function HierarchyContextMenu({
  menu,
  object,
  dispatch,
  onClose,
  onStartRename,
}: ContextMenuProps) {
  const ref = useRef<HTMLDivElement>(null)

  // Clamp menu position so it stays within the viewport, and auto-focus
  // the first menu button for keyboard accessibility.
  const [pos, setPos] = useState({ x: menu.x, y: menu.y })
  useEffect(() => {
    const el = ref.current
    if (!el) return
    const margin = 4
    const x = Math.min(menu.x, window.innerWidth - el.offsetWidth - margin)
    const y = Math.min(menu.y, window.innerHeight - el.offsetHeight - margin)
    setPos({ x: Math.max(margin, x), y: Math.max(margin, y) })
    // Focus the first menu item so keyboard users can navigate immediately
    const firstButton = el.querySelector("button")
    if (firstButton) firstButton.focus()
  }, [menu.x, menu.y])

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) {
        onClose()
      }
    }
    document.addEventListener("mousedown", handler, { capture: true })
    return () => {
      document.removeEventListener("mousedown", handler, { capture: true })
    }
  }, [onClose])

  // Close on Escape
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose()
    }
    document.addEventListener("keydown", handler)
    return () => document.removeEventListener("keydown", handler)
  }, [onClose])

  const items: { label: string; action: () => void }[] = [
    {
      label: "Rename",
      action: () => {
        onStartRename(menu.objectId)
        onClose()
      },
    },
    {
      label: "Duplicate",
      action: () => {
        dispatch({ type: "DUPLICATE_OBJECT", objectId: menu.objectId })
        onClose()
      },
    },
    {
      label: "Add Child",
      action: () => {
        const child: SceneObject = {
          id: crypto.randomUUID().slice(0, 12),
          name: "New Object",
          asset_id: null,
          position: [0, 0, 0],
          rotation: [0, 0, 0, 1],
          scale: [1, 1, 1],
          parent_id: menu.objectId,
          visible: true,
        }
        dispatch({ type: "ADD_OBJECT", object: child })
        onClose()
      },
    },
    {
      label: object.visible ? "Hide" : "Show",
      action: () => {
        dispatch({
          type: "SET_VISIBILITY",
          objectId: menu.objectId,
          visible: !object.visible,
        })
        onClose()
      },
    },
    {
      label: "Delete",
      action: () => {
        dispatch({ type: "REMOVE_OBJECT", objectId: menu.objectId })
        onClose()
      },
    },
  ]

  return (
    <div
      ref={ref}
      role="menu"
      aria-label="Object actions"
      className="fixed z-50 min-w-[140px] rounded-md border border-border bg-popover py-1 shadow-md"
      style={{ left: pos.x, top: pos.y }}
    >
      {items.map((item) => (
        <button
          key={item.label}
          type="button"
          role="menuitem"
          className={cn(
            "flex w-full px-3 py-1.5 text-xs hover:bg-accent",
            item.label === "Delete" && "text-destructive",
          )}
          onClick={item.action}
        >
          {item.label}
        </button>
      ))}
    </div>
  )
}

// ── Tree node ────────────────────────────────────────────────────────────

interface TreeNodeProps {
  object: SceneObject
  childrenMap: Map<string | null, SceneObject[]>
  selectedIds: Set<string>
  depth: number
  dispatch: Dispatch<SceneAction>
  renamingId: string | null
  onStartRename: (objectId: string) => void
  onFinishRename: () => void
  onContextMenu: (objectId: string, x: number, y: number) => void
  /** IDs of objects matching the search filter. null = no filter active. */
  matchingIds: Set<string> | null
}

function TreeNode({
  object,
  childrenMap,
  selectedIds,
  depth,
  dispatch,
  renamingId,
  onStartRename,
  onFinishRename,
  onContextMenu,
  matchingIds,
}: TreeNodeProps) {
  const children = childrenMap.get(object.id) ?? []
  const [expanded, setExpanded] = useState(true)
  const isSelected = selectedIds.has(object.id)
  const isRenaming = renamingId === object.id
  const renameRef = useRef<HTMLInputElement>(null)

  // When a search filter is active, force-expand nodes with matching descendants
  const hasFilteredChildren = matchingIds !== null && children.length > 0
    && hasDescendantMatch(object.id, childrenMap, matchingIds)
  const expandedForRender = children.length > 0 && (expanded || hasFilteredChildren)

  // Drag-and-drop state
  const [dropPosition, setDropPosition] = useState<DropPosition | null>(null)

  // Focus rename input when entering rename mode
  useEffect(() => {
    if (isRenaming && renameRef.current) {
      renameRef.current.focus()
      renameRef.current.select()
    }
  }, [isRenaming])

  // If a filter is active and this node (and none of its descendants) match,
  // hide the entire subtree.
  if (matchingIds !== null && !matchingIds.has(object.id)) {
    // Check if any descendant matches — if so, we still render to show the path
    const hasMatchingDescendant = children.some(
      (c) => matchingIds.has(c.id) || hasDescendantMatch(c.id, childrenMap, matchingIds),
    )
    if (!hasMatchingDescendant) return null
  }

  // ── Click handler with modifier support ──

  const handleClick = (e: React.MouseEvent) => {
    if (e.shiftKey) {
      dispatch({ type: "SELECT", objectId: object.id, mode: "add" })
    } else if (e.ctrlKey || e.metaKey) {
      dispatch({ type: "SELECT", objectId: object.id, mode: "toggle" })
    } else {
      dispatch({ type: "SELECT", objectId: object.id })
    }
  }

  // ── Drag handlers ──

  const handleDragStart = (e: DragEvent) => {
    e.dataTransfer.setData(HIERARCHY_DRAG_MIME, object.id)
    e.dataTransfer.effectAllowed = "move"
  }

  const getDropPosition = (e: DragEvent): DropPosition => {
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect()
    const y = e.clientY - rect.top
    const fraction = y / rect.height
    if (fraction < 0.25) return "before"
    if (fraction > 0.75) return "after"
    return "on"
  }

  const handleDragOver = (e: DragEvent) => {
    if (!e.dataTransfer.types.includes(HIERARCHY_DRAG_MIME)) return
    e.preventDefault()
    e.dataTransfer.dropEffect = "move"
    setDropPosition(getDropPosition(e))
  }

  const handleDragLeave = () => {
    setDropPosition(null)
  }

  const handleDrop = (e: DragEvent) => {
    e.preventDefault()
    setDropPosition(null)
    const draggedId = e.dataTransfer.getData(HIERARCHY_DRAG_MIME)
    if (!draggedId || draggedId === object.id) return

    const pos = getDropPosition(e)
    if (pos === "on") {
      // Drop on the node — reparent as child
      dispatch({
        type: "REPARENT_OBJECT",
        objectId: draggedId,
        newParentId: object.id,
      })
      setExpanded(true)
    } else {
      // Drop before/after — reorder within parent's children
      const siblings = childrenMap.get(object.parent_id) ?? []
      const targetIdx = siblings.findIndex((s) => s.id === object.id)
      // beforeId: the sibling that should come after the dropped object
      let beforeId: string | null = null
      if (pos === "before") {
        beforeId = object.id
      } else {
        // "after" — insert before the next sibling, skipping the dragged
        // node itself (it will be removed from its current position)
        let nextBeforeId: string | null = null
        for (let i = targetIdx + 1; i < siblings.length; i++) {
          const sib = siblings[i]
          if (sib && sib.id !== draggedId) {
            nextBeforeId = sib.id
            break
          }
        }
        beforeId = nextBeforeId
      }
      dispatch({
        type: "REORDER_OBJECT",
        objectId: draggedId,
        newParentId: object.parent_id,
        beforeId,
      })
    }
  }

  // ── Rename handlers ──

  const commitRename = () => {
    if (renameRef.current) {
      const newName = renameRef.current.value.trim()
      if (newName && newName !== object.name) {
        dispatch({ type: "RENAME_OBJECT", objectId: object.id, name: newName })
      }
    }
    onFinishRename()
  }

  // ── Drop indicator styling ──

  const dropIndicatorClass =
    dropPosition === "before"
      ? "border-t-2 border-primary"
      : dropPosition === "after"
        ? "border-b-2 border-primary"
        : dropPosition === "on"
          ? "bg-primary/20"
          : ""

  return (
    <div
      role="treeitem"
      aria-label={object.name}
      aria-selected={isSelected}
      aria-expanded={children.length > 0 ? expandedForRender : undefined}
      tabIndex={0}
      onKeyDown={(e) => {
        // Only handle events targeting this treeitem directly, not bubbled
        // from child treeitems or nested interactive elements
        if (e.target !== e.currentTarget) return
        if (e.key === "ContextMenu" || (e.shiftKey && e.key === "F10")) {
          e.preventDefault()
          const rect = (e.currentTarget as HTMLElement).getBoundingClientRect()
          onContextMenu(object.id, rect.left + rect.width / 2, rect.top + rect.height / 2)
        } else if (e.key === "Enter") {
          e.preventDefault()
          dispatch({ type: "SELECT", objectId: object.id })
        } else if (e.key === " ") {
          // Space toggles selection (ARIA treeitem pattern)
          e.preventDefault()
          dispatch({ type: "SELECT", objectId: object.id, mode: "toggle" })
        } else if (children.length > 0 && e.key === "ArrowRight" && !expandedForRender) {
          e.preventDefault()
          setExpanded(true)
        } else if (children.length > 0 && e.key === "ArrowLeft" && expandedForRender) {
          e.preventDefault()
          setExpanded(false)
        }
      }}
    >
      <div
        className={cn(
          "flex w-full items-center gap-1 px-2 py-1 text-xs hover:bg-accent",
          isSelected && "bg-primary/10 text-primary",
          dropIndicatorClass,
        )}
        style={{ paddingLeft: `${depth * 16 + 8}px` }}
        draggable
        onDragStart={handleDragStart}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        onClick={handleClick}
        onDoubleClick={() => onStartRename(object.id)}
        onContextMenu={(e) => {
          e.preventDefault()
          onContextMenu(object.id, e.clientX, e.clientY)
        }}
      >
        {children.length > 0 ? (
          <ChevronRight
            aria-hidden="true"
            className={cn(
              "h-3 w-3 shrink-0 transition-transform",
              expandedForRender && "rotate-90",
            )}
            onClick={(e) => {
              e.stopPropagation()
              setExpanded(!expanded)
            }}
          />
        ) : (
          <span className="w-3 shrink-0" aria-hidden="true" />
        )}
        {!object.visible && (
          <EyeOff className="h-3 w-3 shrink-0 text-muted-foreground" aria-label="Hidden" />
        )}
        {isRenaming ? (
          <input
            ref={renameRef}
            aria-label="Rename object"
            className="flex-1 bg-transparent text-xs outline-none border-b border-primary px-0.5"
            defaultValue={object.name}
            onBlur={commitRename}
            onKeyDown={(e) => {
              if (e.key === "Enter") commitRename()
              if (e.key === "Escape") onFinishRename()
            }}
          />
        ) : (
          <span className="truncate">{object.name}</span>
        )}
      </div>
      {expandedForRender && (
        <div role="group" aria-label={`Children of ${object.name}`}>
          {children.map((child) => (
            <TreeNode
              key={child.id}
              object={child}
              childrenMap={childrenMap}
              selectedIds={selectedIds}
              depth={depth + 1}
              dispatch={dispatch}
              renamingId={renamingId}
              onStartRename={onStartRename}
              onFinishRename={onFinishRename}
              onContextMenu={onContextMenu}
              matchingIds={matchingIds}
            />
          ))}
        </div>
      )}
    </div>
  )
}

// ── Helpers ──────────────────────────────────────────────────────────────

/** Recursively check if any descendant of `parentId` is in `matchingIds`. */
function hasDescendantMatch(
  parentId: string,
  childrenMap: Map<string | null, SceneObject[]>,
  matchingIds: Set<string>,
  visited: Set<string> = new Set(),
): boolean {
  if (visited.has(parentId)) return false
  visited.add(parentId)
  const children = childrenMap.get(parentId) ?? []
  for (const child of children) {
    if (matchingIds.has(child.id)) return true
    if (hasDescendantMatch(child.id, childrenMap, matchingIds, visited)) return true
  }
  return false
}

// ── Root drop zone ──────────────────────────────────────────────────────

interface RootDropZoneProps {
  dispatch: Dispatch<SceneAction>
}

/** Invisible drop zone at the bottom of the tree for dropping to root level. */
function RootDropZone({ dispatch }: RootDropZoneProps) {
  const [active, setActive] = useState(false)

  return (
    <div
      className={cn(
        "h-4 transition-colors",
        active && "bg-primary/20",
      )}
      aria-label="Drop here to make root object"
      onDragOver={(e) => {
        if (!e.dataTransfer.types.includes(HIERARCHY_DRAG_MIME)) return
        e.preventDefault()
        e.dataTransfer.dropEffect = "move"
        setActive(true)
      }}
      onDragLeave={() => setActive(false)}
      onDrop={(e) => {
        e.preventDefault()
        setActive(false)
        const draggedId = e.dataTransfer.getData(HIERARCHY_DRAG_MIME)
        if (!draggedId) return
        dispatch({
          type: "REORDER_OBJECT",
          objectId: draggedId,
          newParentId: null,
          beforeId: null,
        })
      }}
    />
  )
}

// ── Hierarchy panel ─────────────────────────────────────────────────────

interface HierarchyPanelProps {
  objects: SceneObject[]
  selectedIds: Set<string>
  dispatch: Dispatch<SceneAction>
}

export function HierarchyPanel({
  objects,
  selectedIds,
  dispatch,
}: HierarchyPanelProps) {
  const [search, setSearch] = useState("")
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null)
  const [renamingId, setRenamingId] = useState<string | null>(null)

  const childrenMap = useMemo(() => {
    const map = new Map<string | null, SceneObject[]>()
    for (const obj of objects) {
      const children = map.get(obj.parent_id) ?? []
      children.push(obj)
      map.set(obj.parent_id, children)
    }
    return map
  }, [objects])

  const roots = childrenMap.get(null) ?? []

  // Search filter: collect IDs of objects whose name matches
  const matchingIds = useMemo(() => {
    const query = search.trim().toLowerCase()
    if (!query) return null
    // Pre-compute a lookup map so ancestor traversal is O(1) per step
    const objectById = new Map(objects.map((o) => [o.id, o]))
    const ids = new Set<string>()
    for (const obj of objects) {
      if (obj.name.toLowerCase().includes(query)) {
        ids.add(obj.id)
        // Also include all ancestors so the path is visible
        const visited = new Set<string>()
        let parentId = obj.parent_id
        while (parentId !== null && !visited.has(parentId)) {
          visited.add(parentId)
          ids.add(parentId)
          const parent = objectById.get(parentId)
          parentId = parent?.parent_id ?? null
        }
      }
    }
    return ids
  }, [search, objects])

  const handleContextMenu = useCallback(
    (objectId: string, x: number, y: number) => {
      dispatch({ type: "SELECT", objectId })
      setContextMenu({ objectId, x, y })
    },
    [dispatch],
  )

  const handleStartRename = useCallback((objectId: string) => {
    setRenamingId(objectId)
    setContextMenu(null)
  }, [])

  const handleFinishRename = useCallback(() => {
    setRenamingId(null)
  }, [])

  const closeContextMenu = useCallback(() => {
    setContextMenu(null)
  }, [])

  const contextObject = contextMenu
    ? objects.find((o) => o.id === contextMenu.objectId)
    : null

  return (
    <div className="flex-1 overflow-y-auto min-h-0" aria-label="Scene hierarchy">
      <div className="border-b border-border px-3 py-2 text-xs font-medium text-muted-foreground">
        Hierarchy
        {selectedIds.size > 1 && (
          <span className="ml-2 text-primary" aria-live="polite">
            ({selectedIds.size} selected)
          </span>
        )}
      </div>

      {/* Search input */}
      <div className="border-b border-border px-2 py-1.5">
        <div className="flex items-center gap-1.5 rounded-md bg-muted px-2 py-1">
          <Search className="h-3 w-3 shrink-0 text-muted-foreground" aria-hidden="true" />
          <input
            type="text"
            aria-label="Filter objects by name"
            className="flex-1 bg-transparent text-xs outline-none placeholder:text-muted-foreground"
            placeholder="Filter..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
          {search && (
            <button
              type="button"
              aria-label="Clear filter"
              className="text-xs text-muted-foreground hover:text-foreground"
              onClick={() => setSearch("")}
            >
              &times;
            </button>
          )}
        </div>
      </div>

      {/* Tree */}
      <div
        className="py-1"
        role="tree"
        aria-label="Object hierarchy"
        aria-multiselectable="true"
      >
        {roots.length === 0 ? (
          <p className="px-3 py-4 text-center text-xs text-muted-foreground">
            No objects
          </p>
        ) : (
          roots.map((obj) => (
            <TreeNode
              key={obj.id}
              object={obj}
              childrenMap={childrenMap}
              selectedIds={selectedIds}
              depth={0}
              dispatch={dispatch}
              renamingId={renamingId}
              onStartRename={handleStartRename}
              onFinishRename={handleFinishRename}
              onContextMenu={handleContextMenu}
              matchingIds={matchingIds}
            />
          ))
        )}
        <RootDropZone dispatch={dispatch} />
      </div>

      {/* Context menu */}
      {contextMenu && contextObject && (
        <HierarchyContextMenu
          menu={contextMenu}
          object={contextObject}
          dispatch={dispatch}
          onClose={closeContextMenu}
          onStartRename={handleStartRename}
        />
      )}
    </div>
  )
}
