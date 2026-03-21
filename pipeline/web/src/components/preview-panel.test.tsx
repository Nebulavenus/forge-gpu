import { describe, it, expect, vi, afterEach } from "vitest"
import { render, screen, cleanup } from "@testing-library/react"
import { PreviewPanel } from "./preview-panel"
import type { AssetInfo } from "@/lib/api"

afterEach(cleanup)

// Mock the heavy preview components — we only test routing logic here.
vi.mock("@/components/texture-preview", () => ({
  TexturePreview: ({ url, label }: { url: string; label?: string }) => (
    <div data-testid="texture-preview" data-url={url} data-label={label} />
  ),
}))

vi.mock("@/components/mesh-preview", () => ({
  MeshPreview: ({ url, assetId }: { url: string; assetId: string }) => (
    <div data-testid="mesh-preview" data-url={url} data-asset-id={assetId} />
  ),
}))

function makeAsset(overrides: Partial<AssetInfo> = {}): AssetInfo {
  return {
    id: "test-asset",
    name: "test.png",
    relative_path: "test.png",
    asset_type: "texture",
    source_path: "/source/test.png",
    output_path: null,
    fingerprint: "abc123",
    file_size: 100,
    output_size: null,
    status: "new",
    ...overrides,
  }
}

describe("PreviewPanel", () => {
  it("renders TexturePreview for texture assets", () => {
    render(<PreviewPanel asset={makeAsset({ asset_type: "texture", name: "brick.png" })} />)
    expect(screen.getByTestId("texture-preview")).toBeInTheDocument()
  })

  it("renders side-by-side previews when processed output exists", () => {
    render(
      <PreviewPanel
        asset={makeAsset({
          asset_type: "texture",
          name: "brick.png",
          output_path: "/output/brick.png",
        })}
      />,
    )
    const previews = screen.getAllByTestId("texture-preview")
    expect(previews).toHaveLength(2)
    expect(previews[0]).toHaveAttribute("data-label", "Source")
    expect(previews[1]).toHaveAttribute("data-label", "Processed")
  })

  it("renders MeshPreview for .gltf files", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "mesh", name: "hero.gltf", id: "hero" })}
      />,
    )
    expect(screen.getByTestId("mesh-preview")).toBeInTheDocument()
  })

  it("renders MeshPreview for .glb files", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "mesh", name: "hero.glb", id: "hero" })}
      />,
    )
    expect(screen.getByTestId("mesh-preview")).toBeInTheDocument()
  })

  it("does NOT render MeshPreview for .obj files", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "mesh", name: "cube.obj", id: "cube" })}
      />,
    )
    expect(screen.queryByTestId("mesh-preview")).not.toBeInTheDocument()
    expect(screen.getByText(/glTF\/GLB format/)).toBeInTheDocument()
    expect(screen.getByText(/\.obj/)).toBeInTheDocument()
  })

  it("shows format message for non-glTF mesh extensions", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "mesh", name: "model.fbx", id: "model" })}
      />,
    )
    expect(screen.queryByTestId("mesh-preview")).not.toBeInTheDocument()
    expect(screen.getByText(/glTF\/GLB format/)).toBeInTheDocument()
  })

  it("shows placeholder for animation assets", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "animation", name: "walk.fanim" })}
      />,
    )
    expect(screen.getByText(/Preview not available/)).toBeInTheDocument()
  })

  it("shows placeholder for scene assets", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "scene", name: "level.fscene" })}
      />,
    )
    expect(screen.getByText(/Preview not available/)).toBeInTheDocument()
  })

  it("shows placeholder for unknown asset types", () => {
    render(
      <PreviewPanel
        asset={makeAsset({ asset_type: "unknown", name: "data.bin" })}
      />,
    )
    expect(screen.getByText(/No preview available/)).toBeInTheDocument()
  })
})
