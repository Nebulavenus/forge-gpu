import { useEffect, useRef, useState, useCallback } from "react"

interface TexturePreviewProps {
  url: string
  label?: string
}

type Channel = "rgb" | "r" | "g" | "b" | "a"

const ZOOM_MIN = 0.1
const ZOOM_MAX = 10
const ZOOM_FACTOR = 1.1

export function TexturePreview({ url, label }: TexturePreviewProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const imgRef = useRef<HTMLImageElement | null>(null)
  const channelRef = useRef<Channel>("rgb")
  const [channel, setChannel] = useState<Channel>("rgb")
  const [zoom, setZoom] = useState(1)
  const [offset, setOffset] = useState({ x: 0, y: 0 })
  const [dragging, setDragging] = useState(false)
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 })
  const [dimensions, setDimensions] = useState({ w: 0, h: 0 })

  const drawImage = useCallback(
    (img: HTMLImageElement, ch: Channel) => {
      const canvas = canvasRef.current
      if (!canvas) return
      const ctx = canvas.getContext("2d")
      if (!ctx) return

      canvas.width = img.width
      canvas.height = img.height
      ctx.drawImage(img, 0, 0)

      if (ch !== "rgb") {
        const imageData = ctx.getImageData(0, 0, img.width, img.height)
        const data = imageData.data
        for (let i = 0; i < data.length; i += 4) {
          const r = data[i]!
          const g = data[i + 1]!
          const b = data[i + 2]!
          const a = data[i + 3]!
          switch (ch) {
            case "r":
              data[i] = r
              data[i + 1] = 0
              data[i + 2] = 0
              data[i + 3] = 255
              break
            case "g":
              data[i] = 0
              data[i + 1] = g
              data[i + 2] = 0
              data[i + 3] = 255
              break
            case "b":
              data[i] = 0
              data[i + 1] = 0
              data[i + 2] = b
              data[i + 3] = 255
              break
            case "a":
              data[i] = a
              data[i + 1] = a
              data[i + 2] = a
              data[i + 3] = 255
              break
          }
        }
        ctx.putImageData(imageData, 0, 0)
      }
    },
    [],
  )

  useEffect(() => {
    channelRef.current = channel
  }, [channel])

  useEffect(() => {
    let cancelled = false
    const img = new Image()
    img.crossOrigin = "anonymous"
    img.onload = () => {
      if (cancelled) return
      imgRef.current = img
      setDimensions({ w: img.width, h: img.height })
      drawImage(img, channelRef.current)
    }
    img.onerror = () => {
      if (cancelled) return
      imgRef.current = null
      setDimensions({ w: 0, h: 0 })
    }
    img.src = url
    return () => {
      cancelled = true
      img.onload = null
      img.onerror = null
    }
  }, [url, drawImage])

  useEffect(() => {
    if (imgRef.current) {
      drawImage(imgRef.current, channel)
    }
  }, [channel, drawImage])

  // Attach wheel handler imperatively with { passive: false } so
  // preventDefault() actually suppresses page scroll during zoom.
  // React's onWheel registers passive listeners where preventDefault
  // is silently ignored.
  useEffect(() => {
    const el = containerRef.current
    if (!el) return

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault()

      const rect = el.getBoundingClientRect()
      const mx = e.clientX - rect.left
      const my = e.clientY - rect.top

      const factor = e.deltaY < 0 ? ZOOM_FACTOR : 1 / ZOOM_FACTOR
      setZoom((prevZoom) => {
        const newZoom = Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, prevZoom * factor))
        const scale = newZoom / prevZoom
        setOffset((prevOffset) => ({
          x: mx - scale * (mx - prevOffset.x),
          y: my - scale * (my - prevOffset.y),
        }))
        return newZoom
      })
    }

    el.addEventListener("wheel", handleWheel, { passive: false })
    return () => el.removeEventListener("wheel", handleWheel)
  }, [])

  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      if (e.button !== 0) return
      setDragging(true)
      setDragStart({ x: e.clientX - offset.x, y: e.clientY - offset.y })
    },
    [offset],
  )

  const handleMouseMove = useCallback(
    (e: React.MouseEvent) => {
      if (!dragging) return
      setOffset({
        x: e.clientX - dragStart.x,
        y: e.clientY - dragStart.y,
      })
    },
    [dragging, dragStart],
  )

  const handleMouseUp = useCallback(() => {
    setDragging(false)
  }, [])

  const resetView = useCallback(() => {
    setZoom(1)
    setOffset({ x: 0, y: 0 })
  }, [])

  const channels: { key: Channel; label: string }[] = [
    { key: "rgb", label: "RGB" },
    { key: "r", label: "R" },
    { key: "g", label: "G" },
    { key: "b", label: "B" },
    { key: "a", label: "A" },
  ]

  return (
    <div className="space-y-2">
      {label && (
        <p className="text-xs font-medium text-muted-foreground">{label}</p>
      )}
      <div className="flex items-center gap-1">
        {channels.map((ch) => (
          <button
            type="button"
            key={ch.key}
            aria-pressed={channel === ch.key}
            onClick={() => setChannel(ch.key)}
            className={
              "rounded-md border px-2 py-1 text-xs font-medium transition-colors " +
              (channel === ch.key
                ? "border-primary bg-primary text-primary-foreground"
                : "border-border bg-card text-muted-foreground hover:bg-accent")
            }
          >
            {ch.label}
          </button>
        ))}
        <button
          type="button"
          onClick={resetView}
          className="ml-auto rounded-md border border-border bg-card px-2 py-1 text-xs text-muted-foreground hover:bg-accent"
        >
          Reset
        </button>
      </div>
      <div
        ref={containerRef}
        className="relative h-[300px] w-full cursor-grab overflow-hidden rounded-lg border border-border bg-[#1a1a1a] active:cursor-grabbing"
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
            width: dimensions.w * zoom,
            height: dimensions.h * zoom,
            imageRendering: zoom > 2 ? "pixelated" : "auto",
          }}
        />
      </div>
      <p className="text-xs text-muted-foreground">
        {dimensions.w} x {dimensions.h}
        {zoom !== 1 && ` — ${Math.round(zoom * 100)}%`}
      </p>
    </div>
  )
}
