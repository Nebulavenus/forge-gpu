import {
  Component,
  type ErrorInfo,
  type ReactNode,
} from "react"
import { createFileRoute, Link, useNavigate } from "@tanstack/react-router"
import { PreviewPanel } from "@/components/preview-panel"
import { useQuery } from "@tanstack/react-query"
import { ArrowLeft, ArrowDownRight, ArrowUpRight } from "lucide-react"
import { fetchAsset, fetchAssetDependencies, type AssetInfo } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
import { statusBadgeVariant, validateAssetSearch } from "@/lib/asset-meta"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import {
  Table,
  TableBody,
  TableCell,
  TableRow,
} from "@/components/ui/table"

export const Route = createFileRoute("/assets/$assetId")({
  component: AssetDetail,
  validateSearch: validateAssetSearch,
})

/** Catch errors from 3D preview components so they don't crash the route. */
class PreviewErrorFence extends Component<
  { children: ReactNode },
  { error: Error | null }
> {
  constructor(props: { children: ReactNode }) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error: Error) {
    return { error }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.warn("Preview render error caught:", error, info)
  }

  render() {
    if (this.state.error) {
      return (
        <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
          Preview failed: {this.state.error.message}
        </div>
      )
    }
    return this.props.children
  }
}

/** Clickable list of related assets with an icon and heading. */
function DependencyList({
  assets,
  icon,
  heading,
  emptyLabel,
}: {
  assets: AssetInfo[]
  icon: ReactNode
  heading: string
  emptyLabel: string
}) {
  return (
    <div>
      <h4 className="mb-2 flex items-center gap-1.5 text-sm font-medium text-muted-foreground">
        {icon}
        {heading}
      </h4>
      {assets.length === 0 ? (
        <p className="text-sm text-muted-foreground">{emptyLabel}</p>
      ) : (
        <ul role="list" aria-label={heading} className="space-y-1">
          {assets.map((dep) => {
            const isScene =
              dep.asset_type === "scene" && dep.id.startsWith("scene--")
            const sceneId = isScene
              ? dep.id.replace(/^scene--/, "")
              : undefined

            return (
              <li key={dep.id}>
                {isScene ? (
                  <Link
                    to="/scenes/$sceneId"
                    params={{ sceneId: sceneId! }}
                    className="inline-flex items-center gap-2 rounded-md px-2 py-1 text-sm hover:bg-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring"
                    aria-label={`Scene: ${dep.name}`}
                  >
                    <Badge variant="secondary">{dep.asset_type}</Badge>
                    <span>{dep.name}</span>
                  </Link>
                ) : (
                  <Link
                    to="/assets/$assetId"
                    params={{ assetId: dep.id }}
                    className="inline-flex items-center gap-2 rounded-md px-2 py-1 text-sm hover:bg-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring"
                    aria-label={`${dep.asset_type} asset: ${dep.name}`}
                  >
                    <Badge variant="secondary">{dep.asset_type}</Badge>
                    <span>{dep.name}</span>
                    <Badge variant={statusBadgeVariant(dep.status)}>
                      {dep.status}
                    </Badge>
                  </Link>
                )}
              </li>
            )
          })}
        </ul>
      )}
    </div>
  )
}

function AssetDetail() {
  const { assetId } = Route.useParams()
  const { type: searchType, status: statusFilter, search: searchQuery } = Route.useSearch()
  const navigate = useNavigate()

  const { data: asset, isLoading, error } = useQuery({
    queryKey: ["asset", assetId],
    queryFn: () => fetchAsset(assetId),
  })

  const { data: deps } = useQuery({
    queryKey: ["asset-dependencies", assetId],
    queryFn: () => fetchAssetDependencies(assetId),
    enabled: !!asset,
  })

  if (isLoading) {
    return (
      <div className="py-12 text-center text-muted-foreground">
        Loading asset...
      </div>
    )
  }

  if (error) {
    return (
      <div className="py-12 text-center text-destructive">
        Failed to load asset: {(error as Error).message}
      </div>
    )
  }

  if (!asset) return null

  return (
    <div className="mx-auto max-w-5xl space-y-6">
      <Button
        variant="ghost"
        size="sm"
        onClick={() =>
          navigate({
            to: "/assets",
            search: {
              ...(searchType ? { type: searchType } : {}),
              ...(statusFilter ? { status: statusFilter } : {}),
              ...(searchQuery ? { search: searchQuery } : {}),
            },
          })
        }
        className="gap-1"
      >
        <ArrowLeft className="h-4 w-4" />
        Back
      </Button>

      <div className="space-y-1">
        <h2 className="text-lg font-semibold">{asset.name}</h2>
        <div className="flex items-center gap-2">
          <Badge variant="secondary">{asset.asset_type}</Badge>
          <Badge variant={statusBadgeVariant(asset.status)}>
            {asset.status}
          </Badge>
        </div>
      </div>

      <Table>
        <TableBody>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Source path
            </TableCell>
            <TableCell className="font-mono text-xs">
              {asset.source_path}
            </TableCell>
          </TableRow>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Relative path
            </TableCell>
            <TableCell className="font-mono text-xs">
              {asset.relative_path}
            </TableCell>
          </TableRow>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Output path
            </TableCell>
            <TableCell className="font-mono text-xs">
              {asset.output_path ?? "N/A"}
            </TableCell>
          </TableRow>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Fingerprint
            </TableCell>
            <TableCell className="font-mono text-xs">
              {asset.fingerprint}
            </TableCell>
          </TableRow>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Source size
            </TableCell>
            <TableCell>{formatBytes(asset.file_size)}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell className="font-medium text-muted-foreground">
              Output size
            </TableCell>
            <TableCell>
              {asset.output_size != null
                ? formatBytes(asset.output_size)
                : "N/A"}
            </TableCell>
          </TableRow>
        </TableBody>
      </Table>

      {deps && (
        <section aria-labelledby="deps-heading" className="space-y-4">
          <h3 id="deps-heading" className="text-base font-semibold">
            Dependencies
          </h3>
          <div className="grid gap-6 sm:grid-cols-2">
            <DependencyList
              assets={deps.depends_on}
              icon={
                <ArrowDownRight
                  className="h-4 w-4"
                  aria-hidden="true"
                />
              }
              heading="Depends on"
              emptyLabel="No dependencies"
            />
            <DependencyList
              assets={deps.depended_by}
              icon={
                <ArrowUpRight
                  className="h-4 w-4"
                  aria-hidden="true"
                />
              }
              heading="Depended by"
              emptyLabel="No dependents"
            />
          </div>
        </section>
      )}

      <PreviewErrorFence key={asset.id}>
        <PreviewPanel asset={asset} />
      </PreviewErrorFence>
    </div>
  )
}
