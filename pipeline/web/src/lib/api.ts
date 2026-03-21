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

async function apiFetch<T>(url: string): Promise<T> {
  const response = await fetch(url)
  if (!response.ok) {
    throw new Error(`API error: ${response.status} ${response.statusText}`)
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
  return apiFetch<AssetInfo>(`/api/assets/${id}`)
}

export function fetchStatus(): Promise<PipelineStatus> {
  return apiFetch<PipelineStatus>("/api/status")
}
