import { useMemo } from "react"
import * as THREE from "three"

/** Known API endpoint names that should not be rewritten to /companions. */
const KNOWN_ENDPOINTS = new Set(["file", "companions", "thumbnail", "settings", "dependencies"])

/** Asset API path prefix. */
const ASSET_API_PREFIX = "/api/assets/"

/**
 * Create a LoadingManager that routes companion file requests (e.g. .bin,
 * textures referenced by a .gltf) through the /companions API endpoint.
 *
 * IMPORTANT: drei's useGLTF shares a single GLTFLoader instance across all
 * models. Setting loader.manager only affects the shared loader, so the
 * last model to render "wins". To handle this, resolveURL extracts the
 * asset ID from the URL itself rather than relying on a closure-captured
 * asset ID. This makes the manager work correctly regardless of which
 * model set it last.
 */
export function useCompanionManager(assetId: string) {
  return useMemo(() => {
    const manager = new THREE.LoadingManager()
    const encodedId = encodeURIComponent(assetId)

    manager.resolveURL = (url: string) => {
      // Data URIs — pass through
      if (url.startsWith("data:")) {
        return url
      }

      // Normalize same-origin absolute URLs to relative paths so companion
      // rewriting still applies when a glTF contains full URLs.
      if (url.startsWith(window.location.origin)) {
        url = url.slice(window.location.origin.length)
      }

      // Absolute HTTP(S) pointing to a different origin — pass through
      if (url.startsWith("http://") || url.startsWith("https://")) {
        return url
      }

      // URLs under /api/assets/ — Three.js constructed these by resolving
      // relative URIs against the loaded file's base URL.
      if (url.startsWith(ASSET_API_PREFIX)) {
        // Parse: /api/assets/{assetId}/{rest}
        const afterPrefix = url.slice(ASSET_API_PREFIX.length)
        const slashIdx = afterPrefix.indexOf("/")
        if (slashIdx === -1) return url // Malformed — pass through

        const urlAssetId = afterPrefix.slice(0, slashIdx)
        const rest = afterPrefix.slice(slashIdx + 1)
        const endpoint = rest.split("?")[0] ?? ""

        // Known endpoint (file, companions, etc.) — pass through
        if (KNOWN_ENDPOINTS.has(endpoint)) {
          return url
        }

        // Unknown segment = filename that Three.js appended.
        // Rewrite to the companions endpoint using the asset ID from
        // the URL (not the closure), so it works even when the shared
        // loader is set by a different model.
        return `${ASSET_API_PREFIX}${urlAssetId}/companions?path=${encodeURIComponent(endpoint)}`
      }

      // Relative path (no /api/ prefix) — route through this asset's companions
      return `${ASSET_API_PREFIX}${encodedId}/companions?path=${encodeURIComponent(url)}`
    }
    return manager
  }, [assetId])
}
