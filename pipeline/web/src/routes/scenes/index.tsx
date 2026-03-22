import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query"
import { Plus, Trash2 } from "lucide-react"
import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { createScene, deleteScene, fetchScenes } from "@/lib/scene-api"

export const Route = createFileRoute("/scenes/")({
  component: ScenesIndex,
})

function ScenesIndex() {
  const navigate = useNavigate()
  const queryClient = useQueryClient()

  const { data, isLoading, error } = useQuery({
    queryKey: ["scenes"],
    queryFn: fetchScenes,
  })

  const createMutation = useMutation({
    mutationFn: (name: string) => createScene(name),
    onSuccess: (scene) => {
      queryClient.invalidateQueries({ queryKey: ["scenes"] })
      navigate({ to: "/scenes/$sceneId", params: { sceneId: scene.id } })
    },
  })

  const deleteMutation = useMutation({
    mutationFn: (id: string) => deleteScene(id),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ["scenes"] })
    },
  })

  const handleNew = () => {
    const name = window.prompt("Scene name:")
    if (name?.trim()) {
      createMutation.mutate(name.trim())
    }
  }

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
    </div>
  )
}
