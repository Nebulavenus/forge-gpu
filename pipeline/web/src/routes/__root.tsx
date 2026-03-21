import { createRootRoute, Outlet } from "@tanstack/react-router"
import { useWebSocket } from "@/lib/ws"
import { StatusBar } from "@/components/status-bar"

function RootLayout() {
  const { lastMessage, isConnected } = useWebSocket("/ws/status")

  return (
    <div className="flex h-screen flex-col">
      <header className="flex items-center justify-between border-b border-border bg-card px-4 py-3">
        <h1 className="text-sm font-semibold tracking-tight">
          forge pipeline
        </h1>
      </header>

      <main className="flex-1 overflow-auto p-6">
        <Outlet />
      </main>

      <StatusBar isConnected={isConnected} lastMessage={lastMessage} />
    </div>
  )
}

export const Route = createRootRoute({
  component: RootLayout,
})
