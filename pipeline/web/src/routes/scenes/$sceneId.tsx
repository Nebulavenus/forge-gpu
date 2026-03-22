import { useEffect, useState } from "react"
import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { useMutation, useQuery } from "@tanstack/react-query"
import { ArrowLeft } from "lucide-react"
import { Button } from "@/components/ui/button"
import { fetchAssets } from "@/lib/api"
import { fetchScene, saveScene } from "@/lib/scene-api"
import { useSceneStore } from "@/components/scene-editor/use-scene-store"
import { Toolbar } from "@/components/scene-editor/toolbar"
import { HierarchyPanel } from "@/components/scene-editor/hierarchy-panel"
import { Viewport } from "@/components/scene-editor/viewport"
import { InspectorPanel } from "@/components/scene-editor/inspector-panel"
import type { SceneObject } from "@/components/scene-editor/types"

export const Route = createFileRoute("/scenes/$sceneId")({
  component: SceneEditor,
})

function SceneEditor() {
  const { sceneId } = Route.useParams()
  const navigate = useNavigate()
  const { state, dispatch, selectedObject } = useSceneStore()

  // Fetch scene data
  const { data, isLoading, error } = useQuery({
    queryKey: ["scene", sceneId],
    queryFn: () => fetchScene(sceneId),
  })

  // Initialize store when data arrives
  const [initializedFor, setInitializedFor] = useState<string | null>(null)
  useEffect(() => {
    if (data && initializedFor !== sceneId) {
      dispatch({ type: "LOAD_SCENE", scene: data })
      setInitializedFor(sceneId)
    }
  }, [data, initializedFor, sceneId, dispatch])

  // Fetch mesh assets for the "Add" picker
  const { data: assetsData } = useQuery({
    queryKey: ["assets", "mesh"],
    queryFn: () => fetchAssets({ type: "mesh" }),
  })

  // Save mutation
  const saveMutation = useMutation({
    mutationFn: () => {
      if (!state.scene) throw new Error("No scene to save")
      return saveScene(sceneId, state.scene)
    },
    onSuccess: (saved) => {
      dispatch({ type: "LOAD_SCENE", scene: saved })
      setInitializedFor(sceneId)
    },
  })

  const handleSave = () => saveMutation.mutate()

  const handleAdd = () => {
    const meshAssets = assetsData?.assets ?? []
    if (meshAssets.length === 0) {
      // No mesh assets — add a placeholder object
      const obj: SceneObject = {
        id: crypto.randomUUID().slice(0, 12),
        name: "Empty Object",
        asset_id: null,
        position: [0, 0, 0],
        rotation: [0, 0, 0, 1],
        scale: [1, 1, 1],
        parent_id: null,
        visible: true,
      }
      dispatch({ type: "ADD_OBJECT", object: obj })
      return
    }

    // Simple prompt-based picker — list asset names
    const names = meshAssets.map((a, i) => `${i + 1}. ${a.name}`)
    const choice = window.prompt(
      `Select a mesh asset:\n${names.join("\n")}\n\nEnter number (or empty for placeholder):`,
    )

    if (choice === null) return // cancelled

    const index = parseInt(choice, 10) - 1
    const asset = meshAssets[index]

    const obj: SceneObject = {
      id: crypto.randomUUID().slice(0, 12),
      name: asset?.name ?? "Empty Object",
      asset_id: asset?.id ?? null,
      position: [0, 0, 0],
      rotation: [0, 0, 0, 1],
      scale: [1, 1, 1],
      parent_id: null,
      visible: true,
    }
    dispatch({ type: "ADD_OBJECT", object: obj })
  }

  if (isLoading) {
    return (
      <div className="py-12 text-center text-muted-foreground">
        Loading scene...
      </div>
    )
  }

  if (error) {
    return (
      <div className="py-12 text-center text-destructive">
        Failed to load scene: {(error as Error).message}
      </div>
    )
  }

  if (!state.scene) return null

  return (
    <div className="flex h-full flex-col -m-6">
      {/* Header */}
      <div className="flex items-center gap-2 border-b border-border bg-card px-4 py-2">
        <Button
          variant="ghost"
          size="sm"
          onClick={() => navigate({ to: "/scenes" })}
        >
          <ArrowLeft className="h-4 w-4 mr-1" />
          Scenes
        </Button>
        <span className="text-sm font-medium">{state.scene.name}</span>
        {state.dirty && (
          <span className="text-xs text-muted-foreground">(unsaved)</span>
        )}
      </div>

      {/* Toolbar */}
      <Toolbar
        state={state}
        dispatch={dispatch}
        onSave={handleSave}
        onAdd={handleAdd}
      />

      {/* Main area */}
      <div className="flex flex-1 min-h-0">
        <HierarchyPanel
          objects={state.scene.objects}
          selectedId={state.selectedId}
          dispatch={dispatch}
        />
        <Viewport
          objects={state.scene.objects}
          selectedId={state.selectedId}
          gizmoMode={state.gizmoMode}
          dispatch={dispatch}
        />
        <InspectorPanel
          object={selectedObject}
          allObjects={state.scene.objects}
          dispatch={dispatch}
        />
      </div>
    </div>
  )
}
