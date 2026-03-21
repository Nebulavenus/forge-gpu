import type { AssetInfo } from "@/lib/api"
import { TexturePreview } from "@/components/texture-preview"
import { MeshPreview } from "@/components/mesh-preview"

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

    case "mesh":
      return <MeshPreview url={fileUrl(asset.id)} assetId={asset.id} />

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
