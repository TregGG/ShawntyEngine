import { useState, useCallback } from 'react'
import { usePrefabs, usePrefab, createPrefab } from '../hooks/useApi'

export default function PrefabBrowser({ onInstantiate, onEditPrefab }) {
  const { prefabs, loading, refresh } = usePrefabs()
  const [selectedPrefab, setSelectedPrefab] = useState(null)
  const [showNewPrefab, setShowNewPrefab] = useState(false)
  const [newPrefabName, setNewPrefabName] = useState('')
  const { prefab: prefabData } = usePrefab(selectedPrefab)

  const handleCreate = async () => {
    if (!newPrefabName.trim()) return
    try {
      await createPrefab(newPrefabName.trim())
      refresh()
      setNewPrefabName('')
      setShowNewPrefab(false)
    } catch (err) {
      console.error('Failed to create prefab:', err)
    }
  }

  const handleSavePrefab = useCallback(async (name, updatedPrefab) => {
    try {
      await fetch(`/api/prefabs/${name}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updatedPrefab),
      })
      refresh()
    } catch (err) {
      console.error('Failed to save prefab:', err)
    }
  }, [refresh])

  return (
    <div className="prefab-browser">
      <div className="prefab-list">
        <div className="script-list-header">
          <span>Prefabs</span>
          {showNewPrefab ? (
            <div className="toolbar-inline-form compact">
              <input
                type="text"
                placeholder="prefab_name"
                value={newPrefabName}
                onChange={e => setNewPrefabName(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && handleCreate()}
                autoFocus
              />
              <button className="btn-primary btn-sm" onClick={handleCreate}>+</button>
              <button className="btn-secondary btn-sm" onClick={() => setShowNewPrefab(false)}>✕</button>
            </div>
          ) : (
            <button className="btn-icon" onClick={() => setShowNewPrefab(true)} title="New Prefab">+</button>
          )}
        </div>
        {loading ? (
          <div className="empty-state">Loading...</div>
        ) : prefabs?.length === 0 ? (
          <div className="empty-state">No prefabs found</div>
        ) : (
          prefabs?.map(name => (
            <div
              key={name}
              className={`prefab-item ${selectedPrefab === name ? 'selected' : ''}`}
              onClick={() => setSelectedPrefab(name)}
            >
              <span className="prefab-item-name">🧩 {name}</span>
              {selectedPrefab === name && prefabData && (
                <div className="prefab-item-actions">
                  <button
                    className="btn-primary btn-sm"
                    onClick={(e) => {
                      e.stopPropagation()
                      onInstantiate(prefabData)
                    }}
                    title="Add this prefab as a new entity in the scene"
                  >
                    + Add to Scene
                  </button>
                  <button
                    className="btn-secondary btn-sm"
                    onClick={(e) => {
                      e.stopPropagation()
                      onEditPrefab(name, prefabData)
                    }}
                    title="Open prefab editor"
                  >
                    ✏️ Edit
                  </button>
                </div>
              )}
            </div>
          ))
        )}
      </div>
      <div className="prefab-preview">
        {selectedPrefab && prefabData ? (
          <>
            <div className="script-preview-header">{selectedPrefab}.prefab</div>
            <pre className="script-viewer">
              <code>{JSON.stringify(prefabData, null, 2)}</code>
            </pre>
          </>
        ) : (
          <div className="empty-state">Select a prefab to preview</div>
        )}
      </div>
    </div>
  )
}
