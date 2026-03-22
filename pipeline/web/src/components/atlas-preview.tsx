import { useEffect, useRef, useState, useCallback } from "react"

interface AtlasEntry {
  x: number
  y: number
  width: number
  height: number
  u_offset: number
  v_offset: number
  u_scale: number
  v_scale: number
}

interface AtlasMetadata {
  version: number
  width: number
  height: number
  padding: number
  utilization: number
  entries: Record<string, AtlasEntry>
}

const RECT_COLORS = [
  "#ef4444",
  "#f97316",
  "#eab308",
  "#22c55e",
  "#06b6d4",
  "#3b82f6",
  "#8b5cf6",
  "#ec4899",
]

export function AtlasPreview() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const [metadata, setMetadata] = useState<AtlasMetadata | null>(null)
  const [hoveredEntry, setHoveredEntry] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [zoom, setZoom] = useState(1)
  const [offset, setOffset] = useState({ x: 0, y: 0 })
  const [dragging, setDragging] = useState(false)
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 })
  const imgRef = useRef<HTMLImageElement | null>(null)

  // Load atlas metadata
  useEffect(() => {
    let cancelled = false
    fetch("/api/atlas")
      .then((res) => {
        if (!res.ok) throw new Error(`${res.status} ${res.statusText}`)
        return res.json() as Promise<AtlasMetadata>
      })
      .then((data) => {
        if (!cancelled) setMetadata(data)
      })
      .catch((err) => {
        if (!cancelled) setError(String(err))
      })
    return () => {
      cancelled = true
    }
  }, [])

  // Load atlas image
  useEffect(() => {
    if (!metadata) return
    let cancelled = false
    const img = new Image()
    img.crossOrigin = "anonymous"
    img.onload = () => {
      if (!cancelled) {
        imgRef.current = img
        drawAtlas(img, metadata, null)
      }
    }
    img.onerror = () => {
      if (!cancelled) setError("Failed to load atlas image")
    }
    img.src = "/api/atlas/image"
    return () => {
      cancelled = true
      img.onload = null
      img.onerror = null
    }
  }, [metadata])

  const drawAtlas = useCallback(
    (
      img: HTMLImageElement,
      meta: AtlasMetadata,
      hovered: string | null
    ) => {
      const canvas = canvasRef.current
      if (!canvas) return
      const ctx = canvas.getContext("2d")
      if (!ctx) return

      canvas.width = meta.width
      canvas.height = meta.height

      // Draw the atlas image as background
      ctx.drawImage(img, 0, 0)

      // Draw labeled rectangles for each entry
      const entries = Object.entries(meta.entries)
      entries.forEach(([name, entry], i) => {
        const color = RECT_COLORS[i % RECT_COLORS.length]!
        const isHovered = name === hovered

        ctx.strokeStyle = color
        ctx.lineWidth = isHovered ? 3 : 1.5
        ctx.strokeRect(entry.x, entry.y, entry.width, entry.height)

        if (isHovered) {
          ctx.fillStyle = color + "33" // 20% opacity
          ctx.fillRect(entry.x, entry.y, entry.width, entry.height)
        }

        // Label
        const fontSize = Math.max(10, Math.min(14, entry.width / 8))
        ctx.font = `${fontSize}px monospace`
        ctx.fillStyle = color
        const labelY = entry.y + fontSize + 4
        const labelX = entry.x + 4
        // Background for readability
        const textMetrics = ctx.measureText(name)
        ctx.fillStyle = "rgba(0, 0, 0, 0.7)"
        ctx.fillRect(
          labelX - 2,
          labelY - fontSize,
          textMetrics.width + 4,
          fontSize + 4
        )
        ctx.fillStyle = color
        ctx.fillText(name, labelX, labelY)
      })
    },
    []
  )

  // Redraw when hover changes
  useEffect(() => {
    if (imgRef.current && metadata) {
      drawAtlas(imgRef.current, metadata, hoveredEntry)
    }
  }, [hoveredEntry, metadata, drawAtlas])

  // Mouse tracking for hover detection
  const handleMouseMove = useCallback(
    (e: React.MouseEvent) => {
      if (!metadata || !canvasRef.current || !containerRef.current) return

      if (dragging) {
        setOffset({
          x: e.clientX - dragStart.x,
          y: e.clientY - dragStart.y,
        })
        return
      }

      const rect = containerRef.current.getBoundingClientRect()
      const canvasW = metadata.width * zoom
      const canvasH = metadata.height * zoom
      const mx = (e.clientX - rect.left - offset.x) / canvasW * metadata.width
      const my = (e.clientY - rect.top - offset.y) / canvasH * metadata.height

      let found: string | null = null
      for (const [name, entry] of Object.entries(metadata.entries)) {
        if (
          mx >= entry.x &&
          mx <= entry.x + entry.width &&
          my >= entry.y &&
          my <= entry.y + entry.height
        ) {
          found = name
          break
        }
      }
      setHoveredEntry(found)
    },
    [metadata, zoom, offset, dragging, dragStart]
  )

  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      if (e.button !== 0) return
      setDragging(true)
      setDragStart({ x: e.clientX - offset.x, y: e.clientY - offset.y })
    },
    [offset]
  )

  const handleMouseUp = useCallback(() => {
    setDragging(false)
  }, [])

  // Wheel zoom
  useEffect(() => {
    const el = containerRef.current
    if (!el) return

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault()
      const factor = e.deltaY < 0 ? 1.1 : 1 / 1.1
      setZoom((prev) => Math.min(10, Math.max(0.1, prev * factor)))
    }

    el.addEventListener("wheel", handleWheel, { passive: false })
    return () => el.removeEventListener("wheel", handleWheel)
  }, [])

  if (error) {
    return (
      <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
        No atlas available.
      </div>
    )
  }

  if (!metadata) {
    return (
      <div className="rounded-lg border border-border bg-card p-6 text-center text-sm text-muted-foreground">
        Loading atlas...
      </div>
    )
  }

  const hoveredMeta = hoveredEntry ? metadata.entries[hoveredEntry] : null

  return (
    <div className="space-y-2">
      <div className="flex items-center justify-between">
        <p className="text-xs font-medium text-muted-foreground">
          Atlas — {metadata.width}x{metadata.height} —{" "}
          {Math.round(metadata.utilization * 100)}% utilization —{" "}
          {Object.keys(metadata.entries).length} materials
        </p>
        <button
          type="button"
          onClick={() => {
            setZoom(1)
            setOffset({ x: 0, y: 0 })
          }}
          className="rounded-md border border-border bg-card px-2 py-1 text-xs text-muted-foreground hover:bg-accent"
        >
          Reset
        </button>
      </div>
      <div
        ref={containerRef}
        className="relative h-[400px] w-full cursor-grab overflow-hidden rounded-lg border border-border bg-[#1a1a1a] active:cursor-grabbing"
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
      >
        <canvas
          ref={canvasRef}
          style={{
            position: "absolute",
            left: offset.x,
            top: offset.y,
            width: metadata.width * zoom,
            height: metadata.height * zoom,
            imageRendering: zoom > 2 ? "pixelated" : "auto",
          }}
        />
      </div>
      {hoveredMeta && hoveredEntry && (
        <div className="rounded-md border border-border bg-card px-3 py-2 text-xs text-muted-foreground">
          <span className="font-medium text-foreground">{hoveredEntry}</span>
          {" — "}
          {hoveredMeta.width}x{hoveredMeta.height}px
          {" — offset=("}
          {hoveredMeta.u_offset.toFixed(3)}, {hoveredMeta.v_offset.toFixed(3)})
          {" scale=("}
          {hoveredMeta.u_scale.toFixed(3)}, {hoveredMeta.v_scale.toFixed(3)})
        </div>
      )}
    </div>
  )
}
