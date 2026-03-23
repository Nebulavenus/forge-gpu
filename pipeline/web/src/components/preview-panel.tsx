import { Component, type ErrorInfo, type ReactNode } from "react"
import type { AssetInfo } from "@/lib/api"
import { TexturePreview } from "@/components/texture-preview"
import { MeshPreview } from "@/components/mesh-preview"
import { MaterialPreview } from "@/components/material-preview"
import { PipelineMeshPreview } from "@/components/pipeline-mesh-preview"

interface PreviewPanelProps {
  asset: AssetInfo
}

/** Error boundary that catches render errors in preview components. */
class PreviewErrorBoundary extends Component<
  { children: ReactNode; label?: string },
  { error: Error | null }
> {
  constructor(props: { children: ReactNode; label?: string }) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error: Error) {
    return { error }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.warn("Preview failed:", error, info)
  }

  render() {
    if (this.state.error) {
      return (
        <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
          {this.props.label ? `${this.props.label}: ` : ""}
          Preview failed to load.
        </div>
      )
    }
    return this.props.children
  }
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
      const hasFmeshOutput = asset.output_path?.endsWith(".fmesh") ?? false

      if (isGltf) {
        return (
          <div className="space-y-4">
            {hasFmeshOutput ? (
              /* Processed .fmesh preview — the pipeline binary viewer with
                 LOD selector, wireframe toggle, and vertex/triangle stats.
                 Source glTF is not shown side-by-side because the companion
                 file resolver cannot reliably load .bin and texture files
                 for models in subdirectories. */
              <div>
                <p className="text-xs font-medium text-muted-foreground mb-2">Processed (.fmesh)</p>
                <PreviewErrorBoundary key={asset.id} label="Processed">
                  <PipelineMeshPreview assetId={asset.id} />
                </PreviewErrorBoundary>
              </div>
            ) : asset.output_path && (asset.output_path.endsWith(".gltf") || asset.output_path.endsWith(".glb")) ? (
              <div className="flex gap-4">
                <div className="flex-1">
                  <p className="text-xs font-medium text-muted-foreground mb-2">Source</p>
                  <PreviewErrorBoundary key={`source-${asset.id}`} label="Source">
                    <MeshPreview url={fileUrl(asset.id)} assetId={asset.id} fallbackLabel="Source preview unavailable" />
                  </PreviewErrorBoundary>
                </div>
                <div className="flex-1">
                  <p className="text-xs font-medium text-muted-foreground mb-2">Processed</p>
                  <PreviewErrorBoundary key={`processed-${asset.id}`} label="Processed">
                    <MeshPreview url={fileUrl(asset.id, "processed")} assetId={asset.id} fallbackLabel="Processed preview unavailable" />
                  </PreviewErrorBoundary>
                </div>
              </div>
            ) : (
              <PreviewErrorBoundary key={asset.id}>
                <MeshPreview url={fileUrl(asset.id)} assetId={asset.id} />
              </PreviewErrorBoundary>
            )}
            {/* Material textures from the source glTF — skip when processed
                output is .fmesh (materials are in the .fmat sidecar). */}
            {!hasFmeshOutput && (
              <PreviewErrorBoundary key={`material-${asset.id}`} label="Material">
                <MaterialPreview url={fileUrl(asset.id)} assetId={asset.id} />
              </PreviewErrorBoundary>
            )}
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
