import type { AssetSearchParams } from "@/lib/asset-meta"

export interface AssetInfo {
  id: string
  name: string
  relative_path: string
  asset_type: string
  source_path: string
  output_path: string | null
  fingerprint: string
  file_size: number
  output_size: number | null
  status: string
  output_mtime: string | null
}

export interface AssetsResponse {
  assets: AssetInfo[]
  total: number
}

export interface PipelineStatus {
  total: number
  by_type: Record<string, number>
  by_status: Record<string, number>
  source_dir: string
  output_dir: string
}

export class ApiError extends Error {
  constructor(
    public readonly status: number,
    statusText: string,
  ) {
    super(`API error: ${status} ${statusText}`)
    this.name = "ApiError"
  }
}

async function parseErrorMessage(response: Response): Promise<string> {
  try {
    const body = await response.json()
    if (typeof body.detail === "string") return body.detail
    if (typeof body.message === "string") return body.message
    if (typeof body.error === "string") return body.error
  } catch {
    // JSON parsing failed — fall back to statusText
  }
  return response.statusText
}

export async function apiFetch<T>(url: string): Promise<T> {
  const response = await fetch(url)
  if (!response.ok) {
    const message = await parseErrorMessage(response)
    throw new ApiError(response.status, message)
  }
  return response.json() as Promise<T>
}

export function fetchAssets(params?: AssetSearchParams): Promise<AssetsResponse> {
  const searchParams = new URLSearchParams()
  if (params?.type) searchParams.set("type", params.type)
  if (params?.status) searchParams.set("status", params.status)
  if (params?.search) searchParams.set("search", params.search)
  if (params?.sort) searchParams.set("sort", params.sort)
  if (params?.order) searchParams.set("order", params.order)
  const query = searchParams.toString()
  const url = query ? `/api/assets?${query}` : "/api/assets"
  return apiFetch<AssetsResponse>(url)
}

export function fetchAsset(id: string): Promise<AssetInfo> {
  return apiFetch<AssetInfo>(`/api/assets/${encodeURIComponent(id)}`)
}

export function fetchStatus(): Promise<PipelineStatus> {
  return apiFetch<PipelineStatus>("/api/status")
}

export function fetchRecentAssets(limit = 8): Promise<AssetsResponse> {
  return apiFetch<AssetsResponse>(`/api/assets?sort=recent&limit=${limit}`)
}

// ── Import settings ────────────────────────────────────────────────

export interface SettingsSchemaField {
  type: string
  label: string
  description: string
  default: unknown
  min?: number
  max?: number
  options?: string[]
  group?: string
}

export interface ImportSettingsResponse {
  effective: Record<string, unknown>
  per_asset: Record<string, unknown>
  global_settings: Record<string, unknown>
  schema_fields: Record<string, SettingsSchemaField>
  has_overrides: boolean
}

export interface ProcessResponse {
  message: string
}

export function fetchImportSettings(
  assetId: string,
): Promise<ImportSettingsResponse> {
  return apiFetch<ImportSettingsResponse>(`/api/assets/${encodeURIComponent(assetId)}/settings`)
}

export async function apiWrite<T>(
  url: string,
  method: "GET" | "POST" | "PUT" | "PATCH" | "DELETE",
  body?: unknown,
): Promise<T> {
  const response = await fetch(url, {
    method,
    headers: body ? { "Content-Type": "application/json" } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  })
  if (!response.ok) {
    const message = await parseErrorMessage(response)
    throw new ApiError(response.status, message)
  }
  return response.json() as Promise<T>
}

export function saveImportSettings(
  assetId: string,
  overrides: Record<string, unknown>,
): Promise<ImportSettingsResponse> {
  return apiWrite<ImportSettingsResponse>(
    `/api/assets/${encodeURIComponent(assetId)}/settings`,
    "PUT",
    overrides,
  )
}

export function deleteImportSettings(
  assetId: string,
): Promise<ImportSettingsResponse> {
  return apiWrite<ImportSettingsResponse>(
    `/api/assets/${encodeURIComponent(assetId)}/settings`,
    "DELETE",
  )
}

export function processAsset(assetId: string): Promise<ProcessResponse> {
  return apiWrite<ProcessResponse>(
    `/api/assets/${encodeURIComponent(assetId)}/process`,
    "POST",
  )
}

// ── Batch processing ───────────────────────────────────────────────

export interface BatchProcessItemResult {
  asset_id: string
  status: "succeeded" | "failed" | "skipped"
  message: string
}

export interface BatchProcessResponse {
  batch_id: string
  succeeded: number
  failed: number
  skipped: number
  results: BatchProcessItemResult[]
}

export function processBatch(assetIds: string[]): Promise<BatchProcessResponse> {
  return apiWrite<BatchProcessResponse>(
    "/api/process/batch",
    "POST",
    { asset_ids: assetIds },
  )
}
