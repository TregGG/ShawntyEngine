import { useState } from 'react'

const CATEGORY_ICONS = {
  Environment: '🌳', Player: '🎮', Enemy: '👾', Projectile: '💥', UI: '📱',
}

const COMPONENT_BADGES = {
  transform: '📐', sprite: '🖼️', collider: '📦', rigidbody: '⚙️', animator: '🎬', script: '🐍',
}

export default function Hierarchy({ entities, relationships = [], selectedIndex, onSelect, onDelete, onDuplicate, onAdd, onAddChild, onParent, uiMode }) {
  const [contextMenu, setContextMenu] = useState(null)
  const [expanded, setExpanded] = useState({})
  const [draggedIndex, setDraggedIndex] = useState(null)
  const [dropTargetIndex, setDropTargetIndex] = useState(null)

  const isDescendant = (childIdx, ancestorIdx) => {
    if (childIdx === ancestorIdx) return true
    let currentIdx = childIdx
    while (currentIdx !== null && currentIdx !== -1) {
      if (currentIdx === ancestorIdx) return true
      const entity = entities[currentIdx]
      let parentId = null
      relationships.forEach(rel => {
        if (rel.children.includes(entity?.editorId)) parentId = rel.parent
      })
      if (!parentId) return false
      currentIdx = entities.findIndex(e => e.editorId === parentId)
    }
    return false
  }

  const handleDragStart = (e, index) => {
    e.dataTransfer.setData('text/plain', index)
    setDraggedIndex(index)
  }
  const handleDragOver = (e, index) => {
    e.preventDefault(); e.stopPropagation()
    if (draggedIndex !== null && draggedIndex !== index && index !== null) {
      if (!isDescendant(index, draggedIndex)) setDropTargetIndex(index)
    }
  }
  const handleDragLeave = (e) => { e.preventDefault(); setDropTargetIndex(null) }
  const handleDrop = (e, targetIndex) => {
    e.preventDefault(); e.stopPropagation(); setDropTargetIndex(null)
    if (draggedIndex !== null && draggedIndex !== targetIndex) {
      if (targetIndex === null || !isDescendant(targetIndex, draggedIndex)) {
        if (onParent) onParent(draggedIndex, targetIndex)
      }
    }
    setDraggedIndex(null)
  }
  const handleContextMenu = (e, index) => { e.preventDefault(); e.stopPropagation(); setContextMenu({ x: e.clientX, y: e.clientY, index }) }
  const closeContextMenu = () => setContextMenu(null)
  const toggleExpand = (e, editorId) => { e.stopPropagation(); setExpanded(prev => ({ ...prev, [editorId]: !prev[editorId] })) }

  const rootIndices = []
  const childrenMap = {}
  relationships.forEach(rel => {
    childrenMap[rel.parent] = []
    rel.children.forEach(childId => {
      const idx = entities.findIndex(e => e.editorId === childId)
      if (idx !== -1) childrenMap[rel.parent].push(idx)
    })
  })
  const allChildIndices = new Set(Object.values(childrenMap).flat())
  entities.forEach((e, idx) => { if (!allChildIndices.has(idx)) rootIndices.push(idx) })

  const renderEntity = (index, depth = 0) => {
    const entity = entities[index]
    const isUIEntity = entity.category === 'UI'
    if (uiMode && !isUIEntity) return null
    if (!uiMode && isUIEntity) return null
    const icon = CATEGORY_ICONS[entity.category] || '📦'
    const components = entity.components ? Object.keys(entity.components) : []
    const isSelected = index === selectedIndex
    const childIndices = childrenMap[entity.editorId] || []
    const hasChildren = childIndices.length > 0
    const isExpanded = expanded[entity.editorId] !== false
    return (
      <div key={entity.editorId || index}>
        <div className={`entity-item ${isSelected ? 'selected' : ''}`}
          style={{ paddingLeft: `${14 + depth * 14}px`, borderBottom: dropTargetIndex === index ? '2px solid var(--accent)' : 'none', background: dropTargetIndex === index ? 'var(--bg-hover)' : '', opacity: draggedIndex === index ? 0.5 : 1 }}
          onClick={() => onSelect(index)} onContextMenu={(e) => handleContextMenu(e, index)}
          draggable onDragStart={(e) => handleDragStart(e, index)} onDragOver={(e) => handleDragOver(e, index)} onDragLeave={handleDragLeave} onDrop={(e) => handleDrop(e, index)}>
          {hasChildren ? (<span className={`component-arrow ${isExpanded ? 'open' : ''}`} onClick={(e) => toggleExpand(e, entity.editorId)}>▶</span>) : (<span className="component-arrow" style={{ opacity: 0 }}>▶</span>)}
          <span className="entity-icon">{icon}</span>
          <span className="entity-name">{entity.name || 'Unnamed'}</span>
          <span className="entity-badges">{components.filter(c => c !== 'transform').map(c => (<span key={c} className="badge" title={c}>{COMPONENT_BADGES[c] || '•'}</span>))}</span>
        </div>
        {hasChildren && isExpanded && (<div>{childIndices.map(childIdx => renderEntity(childIdx, depth + 1))}</div>)}
      </div>
    )
  }

  return (
    <div className="hierarchy" onClick={closeContextMenu} onDragOver={(e) => handleDragOver(e, null)} onDrop={(e) => handleDrop(e, null)}>
      <div className="panel-header"><span>Hierarchy</span><button className="btn-icon" onClick={onAdd} title="Add Root Entity">+</button></div>
      {entities.length === 0 ? (<div className="empty-state">No entities in scene</div>) : (<div className="entity-list">{rootIndices.map(idx => renderEntity(idx, 0))}</div>)}
      {contextMenu && (
        <div className="context-menu" style={{ top: contextMenu.y, left: contextMenu.x }}>
          {onAddChild && (<div className="context-menu-item" onClick={() => { onAddChild(contextMenu.index); closeContextMenu() }}>👶 Add Child</div>)}
          <div className="context-menu-item" onClick={() => { onDuplicate(contextMenu.index); closeContextMenu() }}>📋 Duplicate</div>
          <div className="context-menu-separator" />
          <div className="context-menu-item danger" onClick={() => { onDelete(contextMenu.index); closeContextMenu() }}>🗑️ Delete</div>
        </div>
      )}
    </div>
  )
}
