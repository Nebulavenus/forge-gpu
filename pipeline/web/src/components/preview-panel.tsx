import type { AssetInfo } from "@/lib/api"
import { TexturePreview } from "@/components/texture-preview"
import { MeshPreview } from "@/components/mesh-preview"
import { MaterialPreview } from "@/components/material-preview"

interface PreviewPanelProps {
  asset: AssetInfo
}

function fileUrl(assetId: string, variant?: "processed"): string {
  const base = `/api/assets/${encodeURIComponent(assetId)}/file`
  return variant ? `${base}?variant=processed` : base
}

export function PreviewPanel({ asset }: PreviewPanelProps) {
  switch (asset.asset_type) {
    case "texture":
      if (asset.output_path) {
        return (
          <div className="flex gap-4">
            <div className="flex-1">
              <TexturePreview
                url={fileUrl(asset.id)}
                label="Source"
              />
            </div>
            <div className="flex-1">
              <TexturePreview
                url={fileUrl(asset.id, "processed")}
                label="Processed"
              />
            </div>
          </div>
        )
      }
      return <TexturePreview url={fileUrl(asset.id)} />

    case "mesh": {
      const name = asset.name.toLowerCase()
      const isGltf = name.endsWith(".gltf") || name.endsWith(".glb")
      if (isGltf) {
        return (
          <div className="space-y-4">
            {asset.output_path && (asset.output_path.endsWith(".gltf") || asset.output_path.endsWith(".glb")) ? (
              <div className="flex gap-4">
                <div className="flex-1">
                  <p className="text-xs font-medium text-muted-foreground mb-2">Source</p>
                  <MeshPreview url={fileUrl(asset.id)} assetId={asset.id} />
                </div>
                <div className="flex-1">
                  <p className="text-xs font-medium text-muted-foreground mb-2">Processed</p>
                  <MeshPreview url={fileUrl(asset.id, "processed")} assetId={asset.id} />
                </div>
              </div>
            ) : (
              <MeshPreview url={fileUrl(asset.id)} assetId={asset.id} />
            )}
            {/* Material textures from the source glTF — processed outputs use
                binary formats (.fmesh) that don't carry embedded textures. */}
            <MaterialPreview url={fileUrl(asset.id)} assetId={asset.id} />
          </div>
        )
      }
      const dot = asset.name.lastIndexOf(".")
      const ext = dot >= 0 ? asset.name.slice(dot) : asset.name
      return (
        <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
          Mesh preview supports glTF/GLB format. Unsupported extension: {ext}
        </div>
      )
    }

    case "animation":
    case "scene":
      return (
        <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
          Preview not available for {asset.asset_type} assets.
        </div>
      )

    default:
      return (
        <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
          No preview available.
        </div>
      )
  }
}
