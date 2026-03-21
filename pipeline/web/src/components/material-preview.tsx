import { useEffect, useRef, useState } from "react"
import { useGLTF } from "@react-three/drei"
import * as THREE from "three"
import { useCompanionManager } from "@/lib/companion-manager"

interface MaterialPreviewProps {
  url: string
  assetId: string
}

interface TextureTile {
  key: string
  label: string
  materialName: string
  texture: THREE.Texture
}

type TextureSlot = {
  property: keyof THREE.MeshStandardMaterial
  label: string
}

const TEXTURE_SLOTS: TextureSlot[] = [
  { property: "map", label: "Base Color" },
  { property: "normalMap", label: "Normal" },
  { property: "metalnessMap", label: "Metallic-Roughness" },
  { property: "emissiveMap", label: "Emissive" },
  { property: "aoMap", label: "Occlusion" },
]

function TextureTileCanvas({ texture }: { texture: THREE.Texture }) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const [imageReady, setImageReady] = useState(!!texture.image)

  // Poll for the texture image if it hasn't loaded yet. Three.js textures
  // loaded via useGLTF may have their .image populated asynchronously.
  // Texture.version increments when the image is uploaded to the GPU.
  useEffect(() => {
    if (texture.image) {
      setImageReady(true)
      return
    }
    const id = setInterval(() => {
      if (texture.image) {
        setImageReady(true)
        clearInterval(id)
      }
    }, 100)
    return () => clearInterval(id)
  }, [texture])

  useEffect(() => {
    if (!imageReady) return
    const canvas = canvasRef.current
    if (!canvas) return

    const image = texture.image as HTMLImageElement | HTMLCanvasElement | ImageBitmap | undefined
    if (!image) return

    const size = 128
    canvas.width = size
    canvas.height = size
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    ctx.drawImage(image as CanvasImageSource, 0, 0, size, size)
  }, [texture, imageReady])

  return (
    <canvas
      ref={canvasRef}
      width={128}
      height={128}
      className="w-full h-full object-cover"
    />
  )
}

export function MaterialPreview({ url, assetId }: MaterialPreviewProps) {
  const manager = useCompanionManager(assetId)
  const [tiles, setTiles] = useState<TextureTile[]>([])
  const [error, setError] = useState<string | null>(null)

  // useGLTF cannot be called conditionally, so we load the scene and
  // extract textures in an effect.
  const { scene } = useGLTF(url, undefined, undefined, (loader) => {
    loader.manager = manager
  })

  useEffect(() => {
    const found: TextureTile[] = []
    const seen = new Set<string>()

    scene.traverse((child) => {
      if (!(child instanceof THREE.Mesh) || !child.material) return

      const materials = Array.isArray(child.material)
        ? child.material
        : [child.material]

      for (const mat of materials) {
        if (!(mat instanceof THREE.MeshStandardMaterial)) continue
        const matName = mat.name || "Unnamed"

        for (const slot of TEXTURE_SLOTS) {
          const tex = mat[slot.property] as THREE.Texture | null
          if (!tex) continue

          const key = `${matName}:${slot.label}:${tex.uuid}`
          if (seen.has(key)) continue
          seen.add(key)

          found.push({
            key,
            label: slot.label,
            materialName: matName,
            texture: tex,
          })
        }
      }
    })

    if (found.length === 0) {
      setError("No material textures found")
    } else {
      setError(null)
    }
    setTiles(found)
  }, [scene])

  if (error) {
    return (
      <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
        {error}
      </div>
    )
  }

  if (tiles.length === 0) {
    return null
  }

  return (
    <div className="space-y-2">
      <p className="text-xs font-medium text-muted-foreground">Materials</p>
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-3">
        {tiles.map((tile) => (
          <div
            key={tile.key}
            className="rounded-lg border border-border bg-card overflow-hidden"
          >
            <div className="aspect-square bg-[#1a1a1a]">
              <TextureTileCanvas texture={tile.texture} />
            </div>
            <div className="p-2 space-y-0.5">
              <p className="text-xs font-medium text-foreground truncate">
                {tile.label}
              </p>
              <p className="text-xs text-muted-foreground truncate">
                {tile.materialName}
              </p>
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}
