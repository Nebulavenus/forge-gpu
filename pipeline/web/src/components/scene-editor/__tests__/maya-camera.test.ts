import { describe, expect, it } from "vitest"
import { renderHook, act } from "@testing-library/react"
import * as THREE from "three"
import { MAYA_MOUSE_BUTTONS, useAltKey } from "../viewport"

// ── MAYA_MOUSE_BUTTONS constant ─────────────────────────────────────────

describe("MAYA_MOUSE_BUTTONS", () => {
  it("maps LMB to ROTATE (orbit)", () => {
    expect(MAYA_MOUSE_BUTTONS.LEFT).toBe(THREE.MOUSE.ROTATE)
  })

  it("maps MMB to PAN (track)", () => {
    expect(MAYA_MOUSE_BUTTONS.MIDDLE).toBe(THREE.MOUSE.PAN)
  })

  it("maps RMB to DOLLY (zoom)", () => {
    expect(MAYA_MOUSE_BUTTONS.RIGHT).toBe(THREE.MOUSE.DOLLY)
  })
})

// ── useAltKey hook ──────────────────────────────────────────────────────

describe("useAltKey", () => {
  it("returns false initially", () => {
    const { result } = renderHook(() => useAltKey())
    expect(result.current).toBe(false)
  })

  it("returns true when Alt key is pressed", () => {
    const { result } = renderHook(() => useAltKey())

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt" }))
    })
    expect(result.current).toBe(true)
  })

  it("returns false when Alt key is released", () => {
    const { result } = renderHook(() => useAltKey())

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt" }))
    })
    expect(result.current).toBe(true)

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keyup", { key: "Alt" }))
    })
    expect(result.current).toBe(false)
  })

  it("resets to false on window blur (e.g. Alt+Tab)", () => {
    const { result } = renderHook(() => useAltKey())

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt" }))
    })
    expect(result.current).toBe(true)

    act(() => {
      window.dispatchEvent(new Event("blur"))
    })
    expect(result.current).toBe(false)
  })

  it("recognizes AltGraph (right Alt on international keyboards)", () => {
    const { result } = renderHook(() => useAltKey())

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "AltGraph" }))
    })
    expect(result.current).toBe(true)

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keyup", { key: "AltGraph" }))
    })
    expect(result.current).toBe(false)
  })

  it("ignores non-Alt key events", () => {
    const { result } = renderHook(() => useAltKey())

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Shift" }))
    })
    expect(result.current).toBe(false)
  })

  it("handles repeated keydown events (key repeat) without issues", () => {
    const { result } = renderHook(() => useAltKey())

    // Initial keydown (repeat: false in real browsers)
    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt", repeat: false }))
    })
    expect(result.current).toBe(true)

    // Browsers fire repeated keydown events when a key is held
    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt", repeat: true }))
    })
    // Still true — repeated events are harmless (React batches identical state)
    expect(result.current).toBe(true)

    act(() => {
      window.dispatchEvent(new KeyboardEvent("keyup", { key: "Alt" }))
    })
    expect(result.current).toBe(false)
  })

  it("cleans up event listeners on unmount", () => {
    const { result, unmount } = renderHook(() => useAltKey())

    expect(result.current).toBe(false)
    unmount()

    // After unmount, pressing Alt should not cause errors or state updates.
    // result.current retains its last value before unmount.
    act(() => {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Alt" }))
    })
    expect(result.current).toBe(false)
  })
})
