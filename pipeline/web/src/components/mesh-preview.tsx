import { Suspense, useState, useEffect, useMemo } from "react"
import { Canvas } from "@react-three/fiber"
import { OrbitControls, useGLTF, Center, Bounds, useBounds } from "@react-three/drei"
import * as THREE from "three"

interface MeshPreviewProps {
  url: string
  assetId: string
}

/**
 * Create a LoadingManager that routes companion file requests (e.g. .bin,
 * textures referenced by a .gltf) through the /companions API endpoint.
 * three.js resolves relative URLs against the base URL of the loaded file,
 * which would produce invalid paths like /api/assets/models--hero/hero.bin.
 * This manager intercepts those and rewrites them to the companions endpoint.
 */
function useCompanionManager(assetId: string) {
  return useMemo(() => {
    const manager = new THREE.LoadingManager()
    manager.resolveURL = (url: string) => {
      if (url.startsWith("http") || url.startsWith("/api/") || url.startsWith("data:")) {
        return url
      }
      return `/api/assets/${encodeURIComponent(assetId)}/companions?path=${encodeURIComponent(url)}`
    }
    return manager
  }, [assetId])
}

function Model({ url, wireframe, assetId }: { url: string; wireframe: boolean; assetId: string }) {
  const manager = useCompanionManager(assetId)
  const { scene } = useGLTF(url, undefined, undefined, (loader) => {
    loader.manager = manager
  })
  const bounds = useBounds()

  useEffect(() => {
    bounds.refresh(scene).fit()
  }, [scene, bounds])

  useEffect(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh && child.material) {
        const materials = Array.isArray(child.material)
          ? child.material
          : [child.material]
        for (const mat of materials) {
          mat.wireframe = wireframe
        }
      }
    })
  }, [scene, wireframe])

  return <primitive object={scene} />
}

function LoadingFallback() {
  return (
    <mesh>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="#444" wireframe />
    </mesh>
  )
}

export function MeshPreview({ url, assetId }: MeshPreviewProps) {
  const [wireframe, setWireframe] = useState(false)

  return (
    <div className="relative h-[400px] w-full rounded-lg border border-border bg-card overflow-hidden">
      <Canvas
        style={{ background: "#1a1a1a" }}
        camera={{ position: [3, 3, 3], fov: 50 }}
      >
        <ambientLight intensity={0.4} />
        <directionalLight position={[5, 5, 5]} intensity={1.5} />
        <Suspense fallback={<LoadingFallback />}>
          <Bounds clip observe margin={1.2}>
            <Center>
              <Model url={url} wireframe={wireframe} assetId={assetId} />
            </Center>
          </Bounds>
        </Suspense>
        <OrbitControls makeDefault />
      </Canvas>
      <button
        type="button"
        aria-pressed={wireframe}
        onClick={() => setWireframe((v) => !v)}
        className={
          "absolute bottom-3 right-3 rounded-md border px-3 py-1.5 text-xs font-medium transition-colors " +
          (wireframe
            ? "border-primary bg-primary text-primary-foreground"
            : "border-border bg-card text-muted-foreground hover:bg-accent")
        }
      >
        Wireframe
      </button>
    </div>
  )
}
