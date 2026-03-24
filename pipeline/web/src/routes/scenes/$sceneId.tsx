import { useCallback, useEffect, useState } from "react"
import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { useMutation, useQuery } from "@tanstack/react-query"
import { ArrowLeft } from "lucide-react"
import { Button } from "@/components/ui/button"
import { fetchAssets } from "@/lib/api"
import type { AssetInfo } from "@/lib/api"
import { fetchScene, saveScene } from "@/lib/scene-api"
import { useSceneStore } from "@/components/scene-editor/use-scene-store"
import { Toolbar } from "@/components/scene-editor/toolbar"
import { HierarchyPanel } from "@/components/scene-editor/hierarchy-panel"
import { Viewport } from "@/components/scene-editor/viewport"
import { InspectorPanel } from "@/components/scene-editor/inspector-panel"
import { AssetPicker } from "@/components/scene-editor/asset-picker"
import { AssetShelf } from "@/components/scene-editor/asset-shelf"
import type { SceneObject } from "@/components/scene-editor/types"

export const Route = createFileRoute("/scenes/$sceneId")({
  component: SceneEditor,
})

/** Create a SceneObject with sensible defaults. */
function createSceneObject(
  assetId: string | null,
  name: string,
  position: [number, number, number] = [0, 0, 0],
): SceneObject {
  return {
    // 12-char prefix (~48 bits) matches the convention in use-scene-store.ts;
    // collision risk is negligible for typical scene sizes (< 10k objects).
    id: crypto.randomUUID().slice(0, 12),
    name,
    asset_id: assetId,
    position,
    rotation: [0, 0, 0, 1],
    scale: [1, 1, 1],
    parent_id: null,
    visible: true,
  }
}

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
  const {
    data: assetsData,
    isLoading: assetsLoading,
    isError: assetsError,
    error: assetsErrorObj,
  } = useQuery({
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

  // Asset picker state
  const [showPicker, setShowPicker] = useState(false)

  const handleAdd = () => setShowPicker(true)

  const handleAssetSelected = (asset: AssetInfo | null) => {
    setShowPicker(false)
    const obj = createSceneObject(asset?.id ?? null, asset?.name ?? "Empty Object")
    dispatch({ type: "ADD_OBJECT", object: obj })
  }

  // Handle drag-and-drop from the asset shelf onto the viewport
  const meshAssets = assetsData?.assets
  const handleAssetDrop = useCallback(
    (assetId: string, position: [number, number, number]) => {
      const asset = meshAssets?.find((a) => a.id === assetId)
      const obj = createSceneObject(assetId, asset?.name ?? "Object", position)
      dispatch({ type: "ADD_OBJECT", object: obj })
    },
    [meshAssets, dispatch],
  )

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
        {/* Left column: hierarchy + asset shelf */}
        <div className="flex w-60 shrink-0 flex-col border-r border-border bg-card">
          <HierarchyPanel
            objects={state.scene.objects}
            selectedId={state.selectedId}
            dispatch={dispatch}
          />
          <div className="shrink-0 max-h-[40%]">
            <AssetShelf
              meshAssets={assetsData?.assets ?? []}
              isLoading={assetsLoading}
              onAddAsset={(assetId) => handleAssetDrop(assetId, [0, 0, 0])}
            />
          </div>
        </div>
        <Viewport
          objects={state.scene.objects}
          selectedId={state.selectedId}
          gizmoMode={state.gizmoMode}
          snapEnabled={state.snapEnabled}
          snapSize={state.snapSize}
          dispatch={dispatch}
          onAssetDrop={handleAssetDrop}
        />
        <InspectorPanel
          object={selectedObject}
          allObjects={state.scene.objects}
          snapEnabled={state.snapEnabled}
          snapSize={state.snapSize}
          dispatch={dispatch}
        />
      </div>

      {/* Asset picker overlay */}
      {showPicker && (
        <AssetPicker
          assets={assetsData?.assets ?? []}
          isLoading={assetsLoading}
          isError={assetsError}
          error={assetsErrorObj as Error | null}
          onSelect={handleAssetSelected}
          onCancel={() => setShowPicker(false)}
        />
      )}
    </div>
  )
}
