import { createRootRoute, Link, Outlet, useRouterState } from "@tanstack/react-router"
import { useWebSocket } from "@/lib/ws"
import { StatusBar } from "@/components/status-bar"
import { cn } from "@/lib/utils"

function NavLink({ to, children }: { to: string; children: React.ReactNode }) {
  const { location } = useRouterState()
  const isActive =
    to === "/"
      ? location.pathname === "/"
      : location.pathname === to || location.pathname.startsWith(to + "/")

  return (
    <Link
      to={to}
      className={cn(
        "text-xs transition-colors hover:text-foreground",
        isActive ? "text-foreground font-medium" : "text-muted-foreground",
      )}
    >
      {children}
    </Link>
  )
}

function RootLayout() {
  const { lastMessage, isConnected } = useWebSocket("/ws/status")

  return (
    <div className="flex h-screen flex-col">
      <header className="flex items-center gap-6 border-b border-border bg-card px-4 py-3">
        <h1 className="text-sm font-semibold tracking-tight">
          forge pipeline
        </h1>
        <nav aria-label="Primary" className="flex items-center gap-4">
          <NavLink to="/">Dashboard</NavLink>
          <NavLink to="/assets">Assets</NavLink>
          <NavLink to="/scenes">Scenes</NavLink>
        </nav>
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
