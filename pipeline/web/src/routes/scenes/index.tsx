import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query"
import { Plus, Trash2, X } from "lucide-react"
import { useCallback, useEffect, useRef, useState } from "react"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { createScene, deleteScene, fetchScenes } from "@/lib/scene-api"

export const Route = createFileRoute("/scenes/")({
  component: ScenesIndex,
})

/* ── New Scene modal ─────────────────────────────────────────────── */

function NewSceneModal({
  open,
  onClose,
  onSubmit,
  onReset,
  isPending,
  error,
}: {
  open: boolean
  onClose: () => void
  onSubmit: (name: string) => void
  onReset: () => void
  isPending: boolean
  error: Error | null
}) {
  const [name, setName] = useState("")
  const inputRef = useRef<HTMLInputElement>(null)

  // Keep a stable ref to onReset so the effect only fires when `open` changes
  const resetRef = useRef(onReset)
  resetRef.current = onReset

  // Clear stale state when modal opens
  useEffect(() => {
    if (open) {
      setName("")
      resetRef.current()
      // Small delay to ensure the element is mounted
      requestAnimationFrame(() => inputRef.current?.focus())
    }
  }, [open])

  // Guard close while create is in-flight to prevent orphaned navigation
  const requestClose = useCallback(() => {
    if (!isPending) onClose()
  }, [isPending, onClose])

  const handleSubmit = useCallback(
    (e: React.FormEvent) => {
      e.preventDefault()
      if (isPending) return
      const trimmed = name.trim()
      if (trimmed) {
        onSubmit(trimmed)
      }
    },
    [isPending, name, onSubmit],
  )

  // Close on Escape
  useEffect(() => {
    if (!open) return
    const handleKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") requestClose()
    }
    window.addEventListener("keydown", handleKey)
    return () => window.removeEventListener("keydown", handleKey)
  }, [open, requestClose])

  if (!open) return null

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm"
      onClick={requestClose}
      role="dialog"
      aria-modal="true"
      aria-label="Create new scene"
    >
      <div
        className="w-full max-w-sm rounded-lg border border-border bg-card p-6 shadow-lg"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="mb-4 flex items-center justify-between">
          <h3 className="text-base font-semibold">New Scene</h3>
          <Button
            variant="ghost"
            size="sm"
            className="h-6 w-6 p-0"
            onClick={requestClose}
            disabled={isPending}
            aria-label="Close"
          >
            <X className="h-4 w-4" />
          </Button>
        </div>
        <form onSubmit={handleSubmit}>
          <Input
            ref={inputRef}
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="Scene name"
            aria-label="Scene name"
            disabled={isPending}
          />
          {error && (
            <p className="mt-2 text-sm text-destructive">
              {error.message || "Failed to create scene"}
            </p>
          )}
          <div className="mt-4 flex justify-end gap-2">
            <Button
              type="button"
              variant="ghost"
              size="sm"
              onClick={requestClose}
              disabled={isPending}
            >
              Cancel
            </Button>
            <Button
              type="submit"
              size="sm"
              disabled={isPending || !name.trim()}
            >
              {isPending ? "Creating..." : "Create"}
            </Button>
          </div>
        </form>
      </div>
    </div>
  )
}

/* ── Scenes page ─────────────────────────────────────────────────── */

function ScenesIndex() {
  const navigate = useNavigate()
  const queryClient = useQueryClient()
  const [showNewModal, setShowNewModal] = useState(false)

  const { data, isLoading, error } = useQuery({
    queryKey: ["scenes"],
    queryFn: fetchScenes,
  })

  const createMutation = useMutation({
    mutationFn: (name: string) => createScene(name),
    onSuccess: (scene) => {
      queryClient.invalidateQueries({ queryKey: ["scenes"] })
      setShowNewModal(false)
      navigate({ to: "/scenes/$sceneId", params: { sceneId: scene.id } })
    },
  })

  const deleteMutation = useMutation({
    mutationFn: (id: string) => deleteScene(id),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ["scenes"] })
    },
  })

  const handleNew = useCallback(() => {
    setShowNewModal(true)
  }, [])

  const handleDelete = (id: string, name: string) => {
    if (window.confirm(`Delete scene "${name}"?`)) {
      deleteMutation.mutate(id)
    }
  }

  if (isLoading) {
    return (
      <div className="py-12 text-center text-muted-foreground">
        Loading scenes...
      </div>
    )
  }

  if (error) {
    return (
      <div className="py-12 text-center text-destructive">
        Failed to load scenes: {(error as Error).message}
      </div>
    )
  }

  const scenes = data?.scenes ?? []

  return (
    <div className="mx-auto max-w-4xl space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold">Scenes</h2>
        <Button size="sm" onClick={handleNew}>
          <Plus className="h-4 w-4 mr-1" />
          New Scene
        </Button>
      </div>

      {scenes.length === 0 ? (
        <p className="py-12 text-center text-muted-foreground">
          No scenes yet. Create one to get started.
        </p>
      ) : (
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {scenes.map((scene) => (
            <Card
              key={scene.id}
              className="relative transition-colors hover:bg-accent/50"
            >
              <Link
                to="/scenes/$sceneId"
                params={{ sceneId: scene.id }}
                className="block"
              >
                <CardHeader className="pb-2">
                  <CardTitle className="text-sm">{scene.name}</CardTitle>
                </CardHeader>
                <CardContent>
                  <span className="text-xs text-muted-foreground">
                    {scene.object_count}{" "}
                    {scene.object_count === 1 ? "object" : "objects"}
                  </span>
                </CardContent>
              </Link>
              <Button
                size="sm"
                variant="ghost"
                className="absolute bottom-2 right-2 h-6 w-6 p-0"
                aria-label={`Delete ${scene.name}`}
                onClick={() => handleDelete(scene.id, scene.name)}
              >
                <Trash2 className="h-3 w-3" />
              </Button>
            </Card>
          ))}
        </div>
      )}

      <NewSceneModal
        open={showNewModal}
        onClose={() => setShowNewModal(false)}
        onSubmit={(name) => createMutation.mutate(name)}
        onReset={() => createMutation.reset()}
        isPending={createMutation.isPending}
        error={createMutation.error ?? null}
      />
    </div>
  )
}
