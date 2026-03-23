import {
  Component,
  type ErrorInfo,
  type ReactNode,
} from "react"
import { createFileRoute, useNavigate } from "@tanstack/react-router"
import { PreviewPanel } from "@/components/preview-panel"
import { useQuery } from "@tanstack/react-query"
import { ArrowLeft } from "lucide-react"
import { fetchAsset } from "@/lib/api"
import { formatBytes } from "@/lib/utils"
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

function AssetDetail() {
  const { assetId } = Route.useParams()
  const navigate = useNavigate()

  const { data: asset, isLoading, error } = useQuery({
    queryKey: ["asset", assetId],
    queryFn: () => fetchAsset(assetId),
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
        onClick={() => navigate({ to: "/" })}
        className="gap-1"
      >
        <ArrowLeft className="h-4 w-4" />
        Back
      </Button>

      <div className="space-y-1">
        <h2 className="text-lg font-semibold">{asset.name}</h2>
        <div className="flex items-center gap-2">
          <Badge variant="secondary">{asset.asset_type}</Badge>
          <Badge variant={asset.status === "missing" ? "destructive" : asset.status === "processed" ? "default" : "secondary"}>
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

      <PreviewErrorFence key={asset.id}>
        <PreviewPanel asset={asset} />
      </PreviewErrorFence>
    </div>
  )
}
