import { Component, Suspense, useState, useEffect, type ReactNode, type ErrorInfo } from "react"
import { Canvas } from "@react-three/fiber"
import { OrbitControls, useGLTF, Center, Bounds, useBounds } from "@react-three/drei"
import * as THREE from "three"
import { useCompanionManager } from "@/lib/companion-manager"

interface MeshPreviewProps {
  url: string
  assetId: string
  /** Label shown when the preview fails to load (default: "Preview unavailable"). */
  fallbackLabel?: string
}

/** Error boundary that catches useGLTF failures inside the Canvas fiber. */
class CanvasErrorBoundary extends Component<
  { children: ReactNode; onError: (err: Error) => void },
  { hasError: boolean }
> {
  constructor(props: { children: ReactNode; onError: (err: Error) => void }) {
    super(props)
    this.state = { hasError: false }
  }

  static getDerivedStateFromError() {
    return { hasError: true }
  }

  componentDidCatch(error: Error, _info: ErrorInfo) {
    this.props.onError(error)
  }

  render() {
    if (this.state.hasError) return null
    return this.props.children
  }
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

export function MeshPreview({ url, assetId, fallbackLabel = "Preview unavailable" }: MeshPreviewProps) {
  const [wireframe, setWireframe] = useState(false)
  const [error, setError] = useState<Error | null>(null)

  // Reset error state when URL changes so a new asset gets a fresh attempt
  useEffect(() => {
    setError(null)
  }, [url])

  if (error) {
    return (
      <div className="flex h-[400px] w-full items-center justify-center rounded-lg border border-border bg-card text-sm text-muted-foreground">
        {fallbackLabel}
      </div>
    )
  }

  return (
    <div className="relative h-[400px] w-full rounded-lg border border-border bg-card overflow-hidden">
      <Canvas
        style={{ background: "#1a1a1a" }}
        camera={{ position: [3, 3, 3], fov: 50 }}
      >
        <ambientLight intensity={0.4} />
        <directionalLight position={[5, 5, 5]} intensity={1.5} />
        <CanvasErrorBoundary key={url} onError={setError}>
          <Suspense fallback={<LoadingFallback />}>
            <Bounds clip observe margin={1.2}>
              <Center>
                <Model url={url} wireframe={wireframe} assetId={assetId} />
              </Center>
            </Bounds>
          </Suspense>
        </CanvasErrorBoundary>
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
