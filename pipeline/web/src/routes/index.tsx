import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { useQuery } from "@tanstack/react-query"
import { Folder, FolderOutput, Image } from "lucide-react"
import { fetchStatus, type PipelineStatus } from "@/lib/api"
import { Card, CardContent } from "@/components/ui/card"
import { cn } from "@/lib/utils"
import { STATUS_META, TYPE_META } from "@/lib/asset-meta"

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

/* ── Dashboard ─────────────────────────────────────────────────── */

function Dashboard() {
  const navigate = useNavigate()
  const { data, isLoading, error } = useQuery<PipelineStatus>({
    queryKey: ["status"],
    queryFn: fetchStatus,
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

      {/* Directory paths */}
      <DirectoryPaths sourceDir={data.source_dir} outputDir={data.output_dir} />
    </div>
  )
}
