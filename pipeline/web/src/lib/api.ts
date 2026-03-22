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

export async function apiFetch<T>(url: string): Promise<T> {
  const response = await fetch(url)
  if (!response.ok) {
    throw new ApiError(response.status, response.statusText)
  }
  return response.json() as Promise<T>
}

export function fetchAssets(params?: {
  type?: string
  search?: string
}): Promise<AssetsResponse> {
  const searchParams = new URLSearchParams()
  if (params?.type) searchParams.set("type", params.type)
  if (params?.search) searchParams.set("search", params.search)
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
    throw new ApiError(response.status, response.statusText)
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
