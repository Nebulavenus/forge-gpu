import { useMemo } from "react"
import * as THREE from "three"

/**
 * Create a LoadingManager that routes companion file requests (e.g. .bin,
 * textures referenced by a .gltf) through the /companions API endpoint.
 * three.js resolves relative URLs against the base URL of the loaded file,
 * which would produce invalid paths like /api/assets/models--hero/hero.bin.
 * This manager intercepts those and rewrites them to the companions endpoint.
 */
export function useCompanionManager(assetId: string) {
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
