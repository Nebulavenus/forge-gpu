import * as React from "react"
import { cn } from "@/lib/utils"
import { Check, Minus } from "lucide-react"

export interface CheckboxProps
  extends Omit<React.ButtonHTMLAttributes<HTMLButtonElement>, "onChange"> {
  checked?: boolean
  indeterminate?: boolean
  onCheckedChange?: (checked: boolean) => void
}

const Checkbox = React.forwardRef<HTMLButtonElement, CheckboxProps>(
  ({ className, checked = false, indeterminate = false, onCheckedChange, disabled, ...props }, ref) => {
    const isChecked = indeterminate ? false : checked

    return (
      <button
        {...props}
        ref={ref}
        type="button"
        role="checkbox"
        disabled={disabled}
        aria-checked={indeterminate ? "mixed" : isChecked}
        className={cn(
          "inline-flex h-4 w-4 shrink-0 items-center justify-center rounded-sm border border-input transition-colors",
          "focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring",
          "disabled:cursor-not-allowed disabled:opacity-50",
          (isChecked || indeterminate) && "border-primary bg-primary text-primary-foreground",
          className,
        )}
        onClick={(e) => {
          e.stopPropagation()
          if (disabled) return
          onCheckedChange?.(!checked)
        }}
        onKeyDown={(e) => {
          if (e.key === " ") {
            e.preventDefault()
            e.stopPropagation()
            if (disabled) return
            onCheckedChange?.(!checked)
          }
        }}
      >
        {indeterminate ? (
          <Minus className="h-3 w-3" />
        ) : isChecked ? (
          <Check className="h-3 w-3" />
        ) : null}
      </button>
    )
  },
)
Checkbox.displayName = "Checkbox"

export { Checkbox }
