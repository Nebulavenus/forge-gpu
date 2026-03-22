/** API functions for scene CRUD operations. */

import { apiFetch, apiWrite } from "@/lib/api"
import type { SceneData, SceneListItem } from "@/components/scene-editor/types"

// ── Response types ──────────────────────────────────────────────────────

export interface SceneListResponse {
  scenes: SceneListItem[]
  total: number
}

export interface SceneResponse extends SceneData {
  id: string
}

// ── API functions ───────────────────────────────────────────────────────

export function fetchScenes(): Promise<SceneListResponse> {
  return apiFetch<SceneListResponse>("/api/scenes")
}

export function fetchScene(sceneId: string): Promise<SceneResponse> {
  return apiFetch<SceneResponse>(
    `/api/scenes/${encodeURIComponent(sceneId)}`,
  )
}

export function createScene(name: string): Promise<SceneResponse> {
  return apiWrite<SceneResponse>("/api/scenes", "POST", { name })
}

export function saveScene(
  sceneId: string,
  data: SceneData,
): Promise<SceneResponse> {
  return apiWrite<SceneResponse>(
    `/api/scenes/${encodeURIComponent(sceneId)}`,
    "PUT",
    data,
  )
}

export function deleteScene(sceneId: string): Promise<{ message: string }> {
  return apiWrite<{ message: string }>(
    `/api/scenes/${encodeURIComponent(sceneId)}`,
    "DELETE",
  )
}
