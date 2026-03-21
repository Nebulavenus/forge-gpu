import { useState, useCallback, useMemo, useRef, useEffect } from "react"
import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query"
import {
  Settings,
  RotateCcw,
  Save,
  Play,
  Loader2,
  AlertCircle,
  Check,
} from "lucide-react"
import {
  ApiError,
  fetchImportSettings,
  saveImportSettings,
  deleteImportSettings,
  processAsset,
} from "@/lib/api"
import type {
  ImportSettingsResponse,
  ProcessResponse,
  SettingsSchemaField,
} from "@/lib/api"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Select } from "@/components/ui/select"
import { Switch } from "@/components/ui/switch"
import { Separator } from "@/components/ui/separator"
import { cn } from "@/lib/utils"

interface ImportSettingsProps {
  assetId: string
}

export function ImportSettings({ assetId }: ImportSettingsProps) {
  const queryClient = useQueryClient()

  const {
    data: settings,
    isLoading,
    error,
  } = useQuery({
    queryKey: ["settings", assetId],
    queryFn: () => fetchImportSettings(assetId),
  })

  // Local edits — tracks pending changes before save
  const [edits, setEdits] = useState<Record<string, unknown>>({})
  const [saveMessage, setSaveMessage] = useState<string | null>(null)
  const [saveStatus, setSaveStatus] = useState<"success" | "error">("success")
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null)

  // Reset edits and clear transient messages when navigating to a different asset
  useEffect(() => {
    setEdits({})
    setSaveMessage(null)
    if (timerRef.current) {
      clearTimeout(timerRef.current)
      timerRef.current = null
    }
  }, [assetId])

  // Clean up pending timer on unmount
  useEffect(() => {
    return () => {
      if (timerRef.current) clearTimeout(timerRef.current)
    }
  }, [])

  const showMessage = useCallback(
    (msg: string, status: "success" | "error", ms = 2000) => {
      if (timerRef.current) clearTimeout(timerRef.current)
      setSaveMessage(msg)
      setSaveStatus(status)
      timerRef.current = setTimeout(() => setSaveMessage(null), ms)
    },
    [],
  )

  const saveMutation = useMutation<
    ImportSettingsResponse,
    Error,
    Record<string, unknown>
  >({
    mutationFn: (overrides) => saveImportSettings(assetId, overrides),
    onSuccess: () => {
      setEdits({})
      showMessage("Settings saved", "success")
      queryClient.invalidateQueries({ queryKey: ["settings", assetId] })
    },
    onError: (err: Error) => {
      showMessage(`Save failed: ${err.message}`, "error", 5000)
    },
  })

  const resetMutation = useMutation<ImportSettingsResponse, Error, void>({
    mutationFn: () => deleteImportSettings(assetId),
    onSuccess: () => {
      setEdits({})
      showMessage("Reverted to global defaults", "success")
      queryClient.invalidateQueries({ queryKey: ["settings", assetId] })
    },
    onError: (err: Error) => {
      showMessage(`Reset failed: ${err.message}`, "error", 5000)
    },
  })

  const processMutation = useMutation<ProcessResponse, Error, void>({
    mutationFn: () => processAsset(assetId),
    onSuccess: (result) => {
      showMessage(result.message, "success", 3000)
      // Invalidate both the asset detail and asset list queries so the
      // status badge and preview update after re-processing.
      queryClient.invalidateQueries({ queryKey: ["asset", assetId] })
      queryClient.invalidateQueries({ queryKey: ["assets"] })
    },
    onError: (err: Error) => {
      showMessage(`Processing failed: ${err.message}`, "error", 5000)
    },
  })

  // Compute effective values: schema default < global < per_asset < local edits
  const effectiveValues = useMemo(() => {
    if (!settings) return {}
    return { ...settings.effective, ...edits }
  }, [settings, edits])

  const handleChange = useCallback(
    (key: string, value: unknown) => {
      setEdits((prev) => ({ ...prev, [key]: value }))
    },
    [],
  )

  const handleSave = useCallback(() => {
    if (!settings) return
    // Merge existing per-asset overrides with new edits
    const merged = { ...settings.per_asset, ...edits }
    saveMutation.mutate(merged)
  }, [settings, edits, saveMutation])

  const handleReset = useCallback(() => {
    resetMutation.mutate()
  }, [resetMutation])

  const handleProcess = useCallback(() => {
    processMutation.mutate()
  }, [processMutation])

  const hasEdits = Object.keys(edits).length > 0
  const isBusy =
    saveMutation.isPending ||
    resetMutation.isPending ||
    processMutation.isPending

  if (isLoading) {
    return (
      <div className="rounded-lg border border-border bg-card p-6">
        <div className="flex items-center gap-2 text-sm text-muted-foreground">
          <Loader2 className="h-4 w-4 animate-spin" />
          Loading import settings...
        </div>
      </div>
    )
  }

  if (error) {
    // 400 means no schema for this asset type — not an error, just unsupported
    if (error instanceof ApiError && error.status === 400) return null
    return (
      <div className="rounded-lg border border-destructive/50 bg-card p-6">
        <div className="flex items-center gap-2 text-sm text-destructive">
          <AlertCircle className="h-4 w-4" />
          {(error as Error).message}
        </div>
      </div>
    )
  }

  if (!settings) return null

  // Group fields by their group property
  const groups = groupFields(settings.schema_fields)

  return (
    <div className="rounded-lg border border-border bg-card">
      {/* Header */}
      <div className="flex items-center justify-between border-b border-border px-4 py-3">
        <div className="flex items-center gap-2">
          <Settings className="h-4 w-4 text-muted-foreground" />
          <span className="text-sm font-medium">Import Settings</span>
          {settings.has_overrides && (
            <span className="rounded bg-primary/10 px-1.5 py-0.5 text-xs text-primary">
              Customized
            </span>
          )}
        </div>
        <div className="flex items-center gap-2">
          {settings.has_overrides && (
            <Button
              variant="ghost"
              size="sm"
              onClick={handleReset}
              disabled={isBusy}
              title="Revert to global defaults"
            >
              <RotateCcw className="h-3.5 w-3.5" />
              Reset
            </Button>
          )}
          <Button
            variant="ghost"
            size="sm"
            onClick={handleSave}
            disabled={!hasEdits || isBusy}
            title="Save per-asset overrides"
          >
            {saveMutation.isPending ? (
              <Loader2 className="h-3.5 w-3.5 animate-spin" />
            ) : (
              <Save className="h-3.5 w-3.5" />
            )}
            Save
          </Button>
          <Button
            variant="secondary"
            size="sm"
            onClick={handleProcess}
            disabled={isBusy}
            title="Re-process this asset with current settings"
          >
            {processMutation.isPending ? (
              <Loader2 className="h-3.5 w-3.5 animate-spin" />
            ) : (
              <Play className="h-3.5 w-3.5" />
            )}
            Process
          </Button>
        </div>
      </div>

      {/* Status message */}
      {saveMessage && (
        <div
          className={cn(
            "flex items-center gap-2 border-b border-border px-4 py-2 text-xs",
            saveStatus === "error"
              ? "bg-destructive/10 text-destructive"
              : "bg-primary/5 text-primary",
          )}
        >
          {saveStatus === "error" ? (
            <AlertCircle className="h-3.5 w-3.5" />
          ) : (
            <Check className="h-3.5 w-3.5" />
          )}
          {saveMessage}
        </div>
      )}

      {/* Settings form */}
      <div className="space-y-4 p-4">
        {groups.map(({ name, fields }, groupIndex) => (
          <div key={name}>
            {groupIndex > 0 && <Separator className="mb-4" />}
            {name && (
              <h4 className="mb-3 text-xs font-medium uppercase tracking-wider text-muted-foreground">
                {name}
              </h4>
            )}
            <div className="space-y-3">
              {fields.map(([key, field]) => (
                <SettingsField
                  key={key}
                  fieldKey={key}
                  field={field}
                  value={effectiveValues[key]}
                  isOverridden={
                    key in edits || key in settings.per_asset
                  }
                  globalValue={settings.global_settings[key]}
                  onChange={handleChange}
                />
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------
// Field renderer
// ---------------------------------------------------------------------------

interface SettingsFieldProps {
  fieldKey: string
  field: SettingsSchemaField
  value: unknown
  isOverridden: boolean
  globalValue: unknown
  onChange: (key: string, value: unknown) => void
}

function SettingsField({
  fieldKey,
  field,
  value,
  isOverridden,
  globalValue,
  onChange,
}: SettingsFieldProps) {
  const id = `setting-${fieldKey}`

  return (
    <div className="grid grid-cols-[1fr_auto] items-center gap-x-4 gap-y-1">
      <div className="space-y-0.5">
        <Label
          htmlFor={id}
          className={cn(
            "text-sm",
            isOverridden && "text-primary",
          )}
        >
          {field.label}
          {isOverridden && (
            <span className="ml-1.5 text-xs text-primary/70">
              (overridden)
            </span>
          )}
        </Label>
        <p className="text-xs text-muted-foreground">{field.description}</p>
        {globalValue !== undefined && globalValue !== null && (
          <p className="text-xs text-muted-foreground/60">
            Global: {formatValue(globalValue)}
          </p>
        )}
      </div>

      <div className="w-48">
        {field.type === "bool" && (
          <Switch
            id={id}
            checked={Boolean(value)}
            onCheckedChange={(v) => onChange(fieldKey, v)}
          />
        )}

        {field.type === "str" && field.options && (
          <Select
            id={id}
            value={String(value ?? field.default)}
            onChange={(e) => onChange(fieldKey, e.target.value)}
          >
            {field.options.map((opt: string) => (
              <option key={opt} value={opt}>
                {opt}
              </option>
            ))}
          </Select>
        )}

        {field.type === "str" && !field.options && (
          <Input
            id={id}
            type="text"
            value={String(value ?? field.default ?? "")}
            onChange={(e) => onChange(fieldKey, e.target.value)}
          />
        )}

        {field.type === "int" && (
          <Input
            id={id}
            type="number"
            inputMode="numeric"
            min={field.min}
            max={field.max}
            step={1}
            value={value != null ? String(value) : ""}
            onChange={(e) => {
              const v = e.target.value
              const parsed = parseInt(v, 10)
              onChange(fieldKey, v === "" || isNaN(parsed) ? field.default : parsed)
            }}
            onBlur={(e) => {
              // Round to integer on blur if user typed a decimal
              const v = e.target.value
              if (v !== "" && v.includes(".")) {
                onChange(fieldKey, Math.round(parseFloat(v)))
              }
            }}
          />
        )}

        {field.type === "float" && (
          <Input
            id={id}
            type="number"
            min={field.min}
            max={field.max}
            step={0.01}
            value={value != null ? String(value) : ""}
            onChange={(e) => {
              const v = e.target.value
              const parsed = parseFloat(v)
              onChange(fieldKey, v === "" || isNaN(parsed) ? field.default : parsed)
            }}
          />
        )}

        {field.type === "list[float]" && (
          <Input
            id={id}
            type="text"
            placeholder="1.0, 0.5, 0.25"
            value={
              Array.isArray(value)
                ? (value as number[]).join(", ")
                : String(value ?? "")
            }
            onChange={(e) => {
              const parts = e.target.value
                .split(",")
                .map((s) => s.trim())
                .filter((s) => s !== "")
                .map(Number)
                .filter((n) => !isNaN(n))
              onChange(fieldKey, parts)
            }}
          />
        )}
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

interface FieldGroup {
  name: string
  fields: [string, SettingsSchemaField][]
}

function groupFields(
  schema: Record<string, SettingsSchemaField>,
): FieldGroup[] {
  const groups: Map<string, [string, SettingsSchemaField][]> = new Map()

  for (const [key, field] of Object.entries(schema)) {
    const group = field.group ?? ""
    if (!groups.has(group)) {
      groups.set(group, [])
    }
    groups.get(group)!.push([key, field])
  }

  return Array.from(groups.entries()).map(([name, fields]) => ({
    name,
    fields,
  }))
}

function formatValue(value: unknown): string {
  if (typeof value === "boolean") return value ? "true" : "false"
  if (Array.isArray(value)) return `[${value.join(", ")}]`
  return String(value)
}
