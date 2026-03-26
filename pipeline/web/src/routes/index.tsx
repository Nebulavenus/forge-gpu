import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { useQuery, useQueryClient, useMutation } from "@tanstack/react-query"
import {
  ArrowRight, Box, Check, Clock, Folder, FolderOutput, Image, Loader2, Map,
  Play, RefreshCw, AlertCircle,
} from "lucide-react"
import {
  fetchRecentAssets, fetchStatus, processAll, rescanAssets,
  type AssetInfo, type BatchProcessResponse, type PipelineStatus, type RescanResponse,
} from "@/lib/api"
import { fetchScenes, type SceneListResponse } from "@/lib/scene-api"
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import { Card, CardContent } from "@/components/ui/card"
import { cn } from "@/lib/utils"
import { STATUS_META, TYPE_META, typeBgColor } from "@/lib/asset-meta"

export const Route = createFileRoute("/")({
  component: Dashboard,
})

/* ── Stat card ─────────────────────────────────────────────────── */

function StatCard({
  label,
  count,
  icon: Icon,
  iconColor,
  onClick,
}: {
  label: string
  count: number
  icon: typeof Image
  iconColor?: string
  onClick?: () => void
}) {
  return (
    <Card
      className={cn(
        "transition-colors",
        onClick && "cursor-pointer hover:bg-card/80",
      )}
      role={onClick ? "button" : undefined}
      tabIndex={onClick ? 0 : undefined}
      onClick={onClick}
      onKeyDown={
        onClick
          ? (e) => {
              if (e.key === "Enter" || e.key === " ") {
                e.preventDefault()
                onClick()
              }
            }
          : undefined
      }
    >
      <CardContent className="flex items-center gap-3 p-4">
        <Icon className={cn("h-5 w-5 shrink-0", iconColor ?? "text-muted-foreground")} />
        <div className="min-w-0">
          <p className="text-2xl font-semibold leading-none">{count}</p>
          <p className="mt-1 text-xs text-muted-foreground">{label}</p>
        </div>
      </CardContent>
    </Card>
  )
}

/* ── Status breakdown bar ──────────────────────────────────────── */

function StatusBreakdown({
  byStatus,
  total,
  onStatusClick,
}: {
  byStatus: Record<string, number>
  total: number
  onStatusClick: (status: string) => void
}) {
  /* Iterate STATUS_META keys first for stable ordering, then append any
     unknown statuses from the data. The legend shows all statuses (including
     zero-count); the bar only renders non-zero segments. */
  const knownKeys = Object.keys(STATUS_META)
  const extraKeys = Object.keys(byStatus).filter((k) => !STATUS_META[k])
  const allKeys = [...knownKeys, ...extraKeys]
  const legendEntries = allKeys.map((k) => [k, byStatus[k] ?? 0] as const)
  const barEntries = legendEntries.filter(([, count]) => count > 0)

  return (
    <div className="space-y-2">
      <h2 className="text-sm font-medium">Pipeline status</h2>

      {/* Stacked bar — only render when there are assets */}
      {total > 0 && <div className="flex h-3 overflow-hidden rounded-full bg-muted">
        {barEntries.map(([status, count]) => {
          const meta = STATUS_META[status]
          const pct = (count / total) * 100
          return (
            <div
              key={status}
              role="button"
              tabIndex={0}
              aria-label={`${meta?.label ?? status}: ${count}`}
              className={cn(
                "h-full cursor-pointer transition-all hover:opacity-80",
                meta?.bg ?? "bg-muted-foreground",
              )}
              style={{ width: `${pct}%` }}
              title={`${meta?.label ?? status}: ${count} (${pct.toFixed(0)}%)`}
              onClick={() => onStatusClick(status)}
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault()
                  onStatusClick(status)
                }
              }}
            />
          )
        })}
      </div>}

      {/* Legend */}
      <div className="flex flex-wrap gap-x-4 gap-y-1">
        {legendEntries.map(([status, count]) => {
          const meta = STATUS_META[status]
          return (
            <div
              key={status}
              role="button"
              tabIndex={0}
              aria-label={`${meta?.label ?? status}: ${count}`}
              className="flex cursor-pointer items-center gap-1.5 text-xs text-muted-foreground hover:text-foreground"
              onClick={() => onStatusClick(status)}
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault()
                  onStatusClick(status)
                }
              }}
            >
              <div className={cn("h-2.5 w-2.5 rounded-sm", meta?.bg ?? "bg-muted-foreground")} />
              <span>
                {meta?.label ?? status}: {count}
              </span>
            </div>
          )
        })}
      </div>
    </div>
  )
}

/* ── Directory footer ──────────────────────────────────────────── */

function DirectoryPaths({ sourceDir, outputDir }: { sourceDir: string; outputDir: string }) {
  return (
    <div className="flex flex-wrap gap-x-6 gap-y-1 text-xs text-muted-foreground">
      <span className="flex items-center gap-1.5">
        <Folder className="h-3.5 w-3.5" />
        Source: <code className="rounded bg-muted px-1 py-0.5">{sourceDir}</code>
      </span>
      <span className="flex items-center gap-1.5">
        <FolderOutput className="h-3.5 w-3.5" />
        Output: <code className="rounded bg-muted px-1 py-0.5">{outputDir}</code>
      </span>
    </div>
  )
}

/* ── Relative time ─────────────────────────────────────────────── */

function relativeTime(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime()
  const seconds = Math.floor(diff / 1000)
  if (seconds < 60) return "just now"
  const minutes = Math.floor(seconds / 60)
  if (minutes < 60) return `${minutes} min ago`
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return `${hours}h ago`
  const days = Math.floor(hours / 24)
  return `${days}d ago`
}

/* ── Recent activity ───────────────────────────────────────────── */

function RecentActivity({ assets }: { assets: AssetInfo[] }) {
  /* Only show assets that have an output (i.e. have been processed). */
  const processed = assets.filter((a) => a.output_mtime)
  if (processed.length === 0) return null

  return (
    <div className="space-y-2">
      <h2 className="text-sm font-medium">Recent</h2>
      <div className="grid grid-cols-1 gap-2 sm:grid-cols-2 lg:grid-cols-4">
        {processed.map((asset) => {
          const meta = TYPE_META[asset.asset_type]
          const Icon = meta?.icon ?? Folder
          return (
            <Link
              key={asset.id}
              to="/assets/$assetId"
              params={{ assetId: asset.id }}
              className="group"
            >
              <Card className="transition-colors group-hover:bg-card/80">
                <CardContent className="flex items-center gap-3 p-3">
                  <Icon
                    className={cn(
                      "h-4 w-4 shrink-0",
                      meta?.color ?? "text-muted-foreground",
                    )}
                  />
                  <div className="min-w-0 flex-1">
                    <p className="truncate text-sm font-medium leading-tight">
                      {asset.name}
                    </p>
                    <div className="mt-1 flex items-center gap-2">
                      <Badge
                        variant="outline"
                        className={cn("text-[10px] leading-none", typeBgColor(asset.asset_type))}
                      >
                        {asset.asset_type}
                      </Badge>
                      <span className="flex items-center gap-1 text-[10px] text-muted-foreground">
                        <Clock className="h-2.5 w-2.5" />
                        {relativeTime(asset.output_mtime!)}
                      </span>
                    </div>
                  </div>
                </CardContent>
              </Card>
            </Link>
          )
        })}
      </div>
    </div>
  )
}

/* ── Navigation cards ──────────────────────────────────────────── */

function NavigationCards({
  assetCount,
  sceneCount,
  scenesError,
}: {
  assetCount: number
  sceneCount: number | undefined
  scenesError: boolean
}) {
  const assetsLabel = `Browse assets — ${assetCount} total`
  const scenesLabel = sceneCount !== undefined
    ? `Browse scenes — ${sceneCount} total`
    : "Browse scenes"

  return (
    <nav aria-label="Quick navigation">
      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2">
        <Link to="/assets" aria-label={assetsLabel}>
          <Card className="group h-full transition-colors hover:bg-card/80">
            <CardContent className="flex items-center gap-4 p-6">
              <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-lg bg-blue-500/20">
                <Box className="h-5 w-5 text-blue-400" aria-hidden="true" />
              </div>
              <div className="min-w-0 flex-1">
                <p className="text-base font-semibold">Assets</p>
                <p className="mt-0.5 text-sm text-muted-foreground">
                  {assetCount} {assetCount === 1 ? "asset" : "assets"}
                </p>
              </div>
              <ArrowRight
                className="h-4 w-4 shrink-0 text-muted-foreground transition-transform group-hover:translate-x-0.5"
                aria-hidden="true"
              />
            </CardContent>
          </Card>
        </Link>
        <Link to="/scenes" aria-label={scenesLabel}>
          <Card className="group h-full transition-colors hover:bg-card/80">
            <CardContent className="flex items-center gap-4 p-6">
              <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-lg bg-amber-500/20">
                <Map className="h-5 w-5 text-amber-400" aria-hidden="true" />
              </div>
              <div className="min-w-0 flex-1">
                <p className="text-base font-semibold">Scenes</p>
                <p className="mt-0.5 text-sm text-muted-foreground">
                  {sceneCount !== undefined
                    ? `${sceneCount} ${sceneCount === 1 ? "scene" : "scenes"}`
                    : scenesError
                      ? "Browse scenes"
                      : "Loading..."}
                </p>
              </div>
              <ArrowRight
                className="h-4 w-4 shrink-0 text-muted-foreground transition-transform group-hover:translate-x-0.5"
                aria-hidden="true"
              />
            </CardContent>
          </Card>
        </Link>
      </div>
    </nav>
  )
}

/* ── Quick actions ─────────────────────────────────────────────── */

function QuickActions({ actionableCount }: { actionableCount: number }) {
  const queryClient = useQueryClient()

  const invalidateDashboard = () => {
    queryClient.invalidateQueries({ queryKey: ["status"] })
    queryClient.invalidateQueries({ queryKey: ["assets"] })
    queryClient.invalidateQueries({ queryKey: ["recent-assets"] })
  }

  const processAllMutation = useMutation<BatchProcessResponse, Error>({
    mutationFn: processAll,
    onSuccess: invalidateDashboard,
  })

  const rescanMutation = useMutation<RescanResponse, Error>({
    mutationFn: rescanAssets,
    onSuccess: invalidateDashboard,
  })

  const handleProcessAll = () => {
    rescanMutation.reset()
    processAllMutation.mutate()
  }

  const handleRescan = () => {
    processAllMutation.reset()
    rescanMutation.mutate()
  }

  const isProcessing = processAllMutation.isPending
  const isRescanning = rescanMutation.isPending
  const processResult = processAllMutation.data
  const processError = processAllMutation.error
  const rescanError = rescanMutation.error

  return (
    <section aria-label="Quick actions">
      <h2 className="mb-2 text-sm font-medium">Quick actions</h2>
      <div className="flex flex-wrap items-center gap-3">
        <Button
          variant="default"
          size="sm"
          disabled={isProcessing || isRescanning || actionableCount === 0}
          aria-disabled={isProcessing || isRescanning || actionableCount === 0}
          aria-label={
            isProcessing
              ? "Processing all assets…"
              : isRescanning
                ? "Process All unavailable while rescanning…"
                : actionableCount === 0
                  ? "Process All — no processable assets"
                  : `Process All — ${actionableCount} processable ${actionableCount === 1 ? "asset" : "assets"}`
          }
          onClick={handleProcessAll}
        >
          {isProcessing ? (
            <Loader2 className="h-4 w-4 animate-spin" aria-hidden="true" />
          ) : (
            <Play className="h-4 w-4" aria-hidden="true" />
          )}
          {isProcessing ? "Processing…" : "Process All"}
        </Button>

        <Button
          variant="outline"
          size="sm"
          disabled={isRescanning || isProcessing}
          aria-disabled={isRescanning || isProcessing}
          aria-label={
            isRescanning
              ? "Rescanning source directory…"
              : isProcessing
                ? "Rescan unavailable while processing assets…"
                : "Rescan source directory for new or changed files"
          }
          onClick={handleRescan}
        >
          {isRescanning ? (
            <Loader2 className="h-4 w-4 animate-spin" aria-hidden="true" />
          ) : (
            <RefreshCw className="h-4 w-4" aria-hidden="true" />
          )}
          {isRescanning ? "Rescanning…" : "Rescan"}
        </Button>

        {/* Result announcement — visible to screen readers and sighted users */}
        {processResult && (
          <p
            className="flex items-center gap-1.5 text-xs text-muted-foreground"
            role="status"
            aria-live="polite"
          >
            <Check className="h-3.5 w-3.5 text-emerald-500" aria-hidden="true" />
            {processResult.succeeded} succeeded
            {processResult.failed > 0 && (
              <span className="text-destructive">, {processResult.failed} failed</span>
            )}
            {processResult.skipped > 0 && <span>, {processResult.skipped} skipped</span>}
          </p>
        )}
        {processError && (
          <p
            className="flex items-center gap-1.5 text-xs text-destructive"
            role="alert"
          >
            <AlertCircle className="h-3.5 w-3.5" aria-hidden="true" />
            {processError.message}
          </p>
        )}
        {rescanError && (
          <p
            className="flex items-center gap-1.5 text-xs text-destructive"
            role="alert"
          >
            <AlertCircle className="h-3.5 w-3.5" aria-hidden="true" />
            {rescanError.message}
          </p>
        )}
        {rescanMutation.isSuccess && (
          <p
            className="flex items-center gap-1.5 text-xs text-muted-foreground"
            role="status"
            aria-live="polite"
          >
            <Check className="h-3.5 w-3.5 text-emerald-500" aria-hidden="true" />
            Rescan complete — {rescanMutation.data.total} assets found
          </p>
        )}
      </div>
    </section>
  )
}

/* ── Dashboard ─────────────────────────────────────────────────── */

function Dashboard() {
  const navigate = useNavigate()
  const { data, isLoading, error } = useQuery<PipelineStatus>({
    queryKey: ["status"],
    queryFn: fetchStatus,
  })
  const { data: recentData } = useQuery({
    queryKey: ["recent-assets"],
    queryFn: () => fetchRecentAssets(8),
  })
  const { data: scenesData, isError: scenesError } = useQuery<SceneListResponse>({
    queryKey: ["scenes"],
    queryFn: fetchScenes,
  })

  if (isLoading) {
    return (
      <div className="py-12 text-center text-muted-foreground">
        Loading pipeline status...
      </div>
    )
  }

  if (error) {
    return (
      <div className="py-12 text-center text-destructive">
        Failed to load status: {(error as Error).message}
      </div>
    )
  }

  if (!data) return null

  /* Iterate TYPE_META keys first for stable ordering, then append unknowns. */
  const knownTypeKeys = Object.keys(TYPE_META)
  const extraTypeKeys = Object.keys(data.by_type).filter((k) => !TYPE_META[k])
  const typeEntries: [string, number][] = [...knownTypeKeys, ...extraTypeKeys].map(
    (k) => [k, data.by_type[k] ?? 0],
  )

  return (
    <div className="space-y-6">
      {/* Stat cards */}
      <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 lg:grid-cols-5">
        <StatCard
          label="Total assets"
          count={data.total}
          icon={Folder}
          onClick={() => navigate({ to: "/assets" })}
        />
        {typeEntries.map(([type, count]) => {
          const meta = TYPE_META[type]
          return (
            <StatCard
              key={type}
              label={meta?.label ?? type}
              count={count}
              icon={meta?.icon ?? Folder}
              iconColor={meta?.color}
              onClick={() =>
                navigate({
                  to: "/assets",
                  search: { type: meta?.filterValue ?? type },
                })
              }
            />
          )
        })}
      </div>

      {/* Status breakdown */}
      <StatusBreakdown
        byStatus={data.by_status}
        total={data.total}
        onStatusClick={(status) =>
          navigate({ to: "/assets", search: { status } })
        }
      />

      {/* Quick actions */}
      <QuickActions
        actionableCount={data.actionable_count}
      />

      {/* Recent activity */}
      {recentData && <RecentActivity assets={recentData.assets} />}

      {/* Navigation cards */}
      <NavigationCards
        assetCount={data.total}
        sceneCount={scenesData?.total}
        scenesError={scenesError}
      />

      {/* Directory paths */}
      <DirectoryPaths sourceDir={data.source_dir} outputDir={data.output_dir} />
    </div>
  )
}
