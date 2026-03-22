import { type Dispatch, useMemo, useState } from "react"
import { ChevronRight, EyeOff } from "lucide-react"
import { cn } from "@/lib/utils"
import type { SceneAction, SceneObject } from "./types"

interface HierarchyPanelProps {
  objects: SceneObject[]
  selectedId: string | null
  dispatch: Dispatch<SceneAction>
}

interface TreeNodeProps {
  object: SceneObject
  childrenMap: Map<string | null, SceneObject[]>
  selectedId: string | null
  depth: number
  dispatch: Dispatch<SceneAction>
}

function TreeNode({
  object,
  childrenMap,
  selectedId,
  depth,
  dispatch,
}: TreeNodeProps) {
  const children = childrenMap.get(object.id) ?? []
  const [expanded, setExpanded] = useState(true)
  const isSelected = object.id === selectedId

  return (
    <div>
      <button
        type="button"
        className={cn(
          "flex w-full items-center gap-1 px-2 py-1 text-xs hover:bg-accent",
          isSelected && "bg-primary/10 text-primary",
        )}
        style={{ paddingLeft: `${depth * 16 + 8}px` }}
        aria-expanded={children.length > 0 ? expanded : undefined}
        onClick={() => dispatch({ type: "SELECT", objectId: object.id })}
        onKeyDown={(e) => {
          if (children.length === 0) return
          if (e.key === "ArrowRight" && !expanded) {
            e.preventDefault()
            setExpanded(true)
          } else if (e.key === "ArrowLeft" && expanded) {
            e.preventDefault()
            setExpanded(false)
          }
        }}
      >
        {children.length > 0 ? (
          <ChevronRight
            className={cn(
              "h-3 w-3 shrink-0 transition-transform",
              expanded && "rotate-90",
            )}
            onClick={(e) => {
              e.stopPropagation()
              setExpanded(!expanded)
            }}
          />
        ) : (
          <span className="w-3 shrink-0" />
        )}
        {!object.visible && (
          <EyeOff className="h-3 w-3 shrink-0 text-muted-foreground" />
        )}
        <span className="truncate">{object.name}</span>
      </button>
      {expanded &&
        children.map((child) => (
          <TreeNode
            key={child.id}
            object={child}
            childrenMap={childrenMap}
            selectedId={selectedId}
            depth={depth + 1}
            dispatch={dispatch}
          />
        ))}
    </div>
  )
}

export function HierarchyPanel({
  objects,
  selectedId,
  dispatch,
}: HierarchyPanelProps) {
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

  return (
    <div className="w-60 shrink-0 overflow-y-auto border-r border-border bg-card">
      <div className="border-b border-border px-3 py-2 text-xs font-medium text-muted-foreground">
        Hierarchy
      </div>
      <div className="py-1">
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
              selectedId={selectedId}
              depth={0}
              dispatch={dispatch}
            />
          ))
        )}
      </div>
    </div>
  )
}
