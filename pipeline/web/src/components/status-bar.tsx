import { cn } from "@/lib/utils"

interface StatusBarProps {
  isConnected: boolean
  lastMessage: string | null
}

function formatMessage(raw: string): string {
  try {
    const msg = JSON.parse(raw) as { type?: string; timestamp?: string }
    if (msg.type === "heartbeat" && msg.timestamp) {
      const time = new Date(msg.timestamp)
      if (isNaN(time.getTime())) return raw
      return `Last heartbeat: ${time.toLocaleTimeString()}`
    }
    return msg.type ?? raw
  } catch {
    return raw
  }
}

export function StatusBar({ isConnected, lastMessage }: StatusBarProps) {
  return (
    <div className="flex items-center gap-3 border-t border-border bg-card px-4 py-2 text-xs text-muted-foreground">
      <div className="flex items-center gap-1.5">
        <div
          className={cn(
            "h-2 w-2 rounded-full",
            isConnected ? "bg-green-500" : "bg-red-500",
          )}
        />
        <span>{isConnected ? "Connected" : "Disconnected"}</span>
      </div>
      {lastMessage && (
        <>
          <span className="text-border">|</span>
          <span className="truncate">{formatMessage(lastMessage)}</span>
        </>
      )}
    </div>
  )
}
