import { useRef, useEffect, useCallback, useState } from 'react'

const CATEGORY_COLORS = {
  Environment: '#6b7280',
  Player: '#3b82f6',
  Enemy: '#ef4444',
  Projectile: '#f59e0b',
  UI: '#8b5cf6',
}

export default function Viewport({ entities, selectedIndex, onSelect, onEntityMove, camera, onCameraChange }) {
  const canvasRef = useRef(null)
  const [isDragging, setIsDragging] = useState(false)
  const [isPanning, setIsPanning] = useState(false)
  const dragStart = useRef({ x: 0, y: 0 })
  const panStart = useRef({ x: 0, y: 0, camX: 0, camY: 0 })

  // Convert world coords to screen coords
  const worldToScreen = useCallback((wx, wy, canvas) => {
    const cx = canvas.width / 2
    const cy = canvas.height / 2
    const zoom = camera.zoom
    return {
      x: cx + (wx - camera.x) * zoom,
      y: cy - (wy - camera.y) * zoom, // Y is flipped (up = positive in game, down on screen)
    }
  }, [camera])

  // Convert screen coords to world coords
  const screenToWorld = useCallback((sx, sy, canvas) => {
    const cx = canvas.width / 2
    const cy = canvas.height / 2
    const zoom = camera.zoom
    return {
      x: camera.x + (sx - cx) / zoom,
      y: camera.y - (sy - cy) / zoom,
    }
  }, [camera])

  // Find entity at screen position
  const findEntityAt = useCallback((sx, sy, canvas) => {
    for (let i = entities.length - 1; i >= 0; i--) {
      const entity = entities[i]
      const t = entity.components?.transform
      if (!t) continue

      const pos = t.position || [0, 0]
      const size = t.size || [1, 1]

      // Entity bounds in world space (centered)
      const halfW = size[0] / 2
      const halfH = size[1] / 2

      const topLeft = worldToScreen(pos[0] - halfW, pos[1] + halfH, canvas)
      const bottomRight = worldToScreen(pos[0] + halfW, pos[1] - halfH, canvas)

      if (sx >= topLeft.x && sx <= bottomRight.x && sy >= topLeft.y && sy <= bottomRight.y) {
        return i
      }
    }
    return -1
  }, [entities, worldToScreen])

  // --- Render ---
  const render = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    const w = canvas.width
    const h = canvas.height
    const zoom = camera.zoom

    // Clear
    ctx.fillStyle = '#12121f'
    ctx.fillRect(0, 0, w, h)

    // Grid
    const gridSize = 1
    const gridPixels = gridSize * zoom
    if (gridPixels > 8) {
      const startWorld = screenToWorld(0, h, canvas)
      const endWorld = screenToWorld(w, 0, canvas)

      ctx.strokeStyle = '#1e1e30'
      ctx.lineWidth = 1
      ctx.beginPath()

      // Vertical lines
      const startX = Math.floor(startWorld.x / gridSize) * gridSize
      for (let gx = startX; gx <= endWorld.x; gx += gridSize) {
        const screen = worldToScreen(gx, 0, canvas)
        ctx.moveTo(Math.round(screen.x) + 0.5, 0)
        ctx.lineTo(Math.round(screen.x) + 0.5, h)
      }

      // Horizontal lines
      const startY = Math.floor(startWorld.y / gridSize) * gridSize
      for (let gy = startY; gy <= endWorld.y; gy += gridSize) {
        const screen = worldToScreen(0, gy, canvas)
        ctx.moveTo(0, Math.round(screen.y) + 0.5)
        ctx.lineTo(w, Math.round(screen.y) + 0.5)
      }
      ctx.stroke()
    }

    // Origin axes
    const origin = worldToScreen(0, 0, canvas)
    ctx.strokeStyle = '#ef444480'
    ctx.lineWidth = 2
    ctx.beginPath()
    ctx.moveTo(0, Math.round(origin.y) + 0.5)
    ctx.lineTo(w, Math.round(origin.y) + 0.5)
    ctx.stroke()

    ctx.strokeStyle = '#10b98180'
    ctx.beginPath()
    ctx.moveTo(Math.round(origin.x) + 0.5, 0)
    ctx.lineTo(Math.round(origin.x) + 0.5, h)
    ctx.stroke()

    // Draw entities
    entities.forEach((entity, index) => {
      const t = entity.components?.transform
      if (!t) return

      const pos = t.position || [0, 0]
      const size = t.size || [1, 1]
      const color = CATEGORY_COLORS[entity.category] || '#6b7280'
      const isSelected = index === selectedIndex

      const halfW = size[0] / 2
      const halfH = size[1] / 2
      const topLeft = worldToScreen(pos[0] - halfW, pos[1] + halfH, canvas)
      const bottomRight = worldToScreen(pos[0] + halfW, pos[1] - halfH, canvas)
      const rectW = bottomRight.x - topLeft.x
      const rectH = bottomRight.y - topLeft.y

      // Fill
      ctx.globalAlpha = 0.3
      ctx.fillStyle = color
      ctx.fillRect(topLeft.x, topLeft.y, rectW, rectH)
      ctx.globalAlpha = 1.0

      // Border
      ctx.strokeStyle = isSelected ? '#a78bfa' : color
      ctx.lineWidth = isSelected ? 2.5 : 1.5
      ctx.strokeRect(topLeft.x, topLeft.y, rectW, rectH)

      // Trigger indicator (dashed border)
      if (entity.components?.collider?.isTrigger) {
        ctx.setLineDash([4, 4])
        ctx.strokeStyle = '#f59e0b80'
        ctx.lineWidth = 1
        ctx.strokeRect(topLeft.x - 2, topLeft.y - 2, rectW + 4, rectH + 4)
        ctx.setLineDash([])
      }

      // Label
      const center = worldToScreen(pos[0], pos[1] + halfH, canvas)
      ctx.fillStyle = isSelected ? '#f0f0f5' : '#a0a0b8'
      ctx.font = `${Math.max(10, Math.min(13, zoom * 0.35))}px Inter, sans-serif`
      ctx.textAlign = 'center'
      ctx.fillText(entity.name || 'Unnamed', center.x, topLeft.y - 6)

      // Selection handles
      if (isSelected) {
        const handleSize = 6
        const handles = [
          [topLeft.x, topLeft.y],
          [topLeft.x + rectW, topLeft.y],
          [topLeft.x, topLeft.y + rectH],
          [topLeft.x + rectW, topLeft.y + rectH],
        ]
        ctx.fillStyle = '#7c3aed'
        handles.forEach(([hx, hy]) => {
          ctx.fillRect(hx - handleSize / 2, hy - handleSize / 2, handleSize, handleSize)
        })
      }
    })

    // Coordinates under cursor (bottom-left HUD)
    ctx.fillStyle = '#6b6b80'
    ctx.font = '11px JetBrains Mono, monospace'
    ctx.textAlign = 'left'
    ctx.fillText(`Zoom: ${(zoom / 40).toFixed(1)}x  |  Camera: (${camera.x.toFixed(1)}, ${camera.y.toFixed(1)})`, 10, h - 10)
  }, [entities, selectedIndex, camera, worldToScreen, screenToWorld])

  // Resize observer
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return

    const observer = new ResizeObserver(() => {
      canvas.width = canvas.offsetWidth * window.devicePixelRatio
      canvas.height = canvas.offsetHeight * window.devicePixelRatio
      const ctx = canvas.getContext('2d')
      ctx.scale(window.devicePixelRatio, window.devicePixelRatio)
      canvas.width = canvas.offsetWidth
      canvas.height = canvas.offsetHeight
      render()
    })
    observer.observe(canvas)
    return () => observer.disconnect()
  }, [render])

  // Re-render on state changes
  useEffect(() => {
    render()
  }, [render])

  // --- Mouse handlers ---
  const handleMouseDown = useCallback((e) => {
    const canvas = canvasRef.current
    if (!canvas) return
    const rect = canvas.getBoundingClientRect()
    const sx = e.clientX - rect.left
    const sy = e.clientY - rect.top

    // Middle mouse button = pan
    if (e.button === 1) {
      e.preventDefault()
      setIsPanning(true)
      panStart.current = { x: e.clientX, y: e.clientY, camX: camera.x, camY: camera.y }
      return
    }

    // Left click
    if (e.button === 0) {
      const hitIndex = findEntityAt(sx, sy, canvas)
      onSelect(hitIndex >= 0 ? hitIndex : null)

      if (hitIndex >= 0) {
        setIsDragging(true)
        const entity = entities[hitIndex]
        const pos = entity.components?.transform?.position || [0, 0]
        dragStart.current = { sx, sy, wx: pos[0], wy: pos[1] }
      }
    }
  }, [camera, findEntityAt, onSelect, entities])

  const handleMouseMove = useCallback((e) => {
    const canvas = canvasRef.current
    if (!canvas) return

    if (isPanning) {
      const dx = e.clientX - panStart.current.x
      const dy = e.clientY - panStart.current.y
      onCameraChange(prev => ({
        ...prev,
        x: panStart.current.camX - dx / prev.zoom,
        y: panStart.current.camY + dy / prev.zoom,
      }))
      return
    }

    if (isDragging && selectedIndex !== null) {
      const rect = canvas.getBoundingClientRect()
      const sx = e.clientX - rect.left
      const sy = e.clientY - rect.top
      const dx = (sx - dragStart.current.sx) / camera.zoom
      const dy = -(sy - dragStart.current.sy) / camera.zoom
      const newX = Math.round((dragStart.current.wx + dx) * 10) / 10
      const newY = Math.round((dragStart.current.wy + dy) * 10) / 10
      onEntityMove(selectedIndex, { x: newX, y: newY })
    }
  }, [isPanning, isDragging, selectedIndex, camera.zoom, onEntityMove, onCameraChange])

  const handleMouseUp = useCallback(() => {
    setIsDragging(false)
    setIsPanning(false)
  }, [])

  const handleWheel = useCallback((e) => {
    e.preventDefault()
    const delta = e.deltaY > 0 ? 0.9 : 1.1
    onCameraChange(prev => ({
      ...prev,
      zoom: Math.max(5, Math.min(200, prev.zoom * delta)),
    }))
  }, [onCameraChange])

  return (
    <canvas
      ref={canvasRef}
      className="viewport-canvas"
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
      onWheel={handleWheel}
      onContextMenu={e => e.preventDefault()}
    />
  )
}
