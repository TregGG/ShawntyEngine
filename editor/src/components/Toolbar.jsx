import { useState } from 'react'

export default function Toolbar({
  scenes, currentScene, onSceneChange, onSave, onAddEntity,
  saveStatus, connected, refreshScenes, uiMode, setUiMode,
  onOpenProjectSettings
}) {
  const [showNewScene, setShowNewScene] = useState(false)
  const [newSceneName, setNewSceneName] = useState('')

  const handleCreateScene = async () => {
    if (!newSceneName.trim()) return
    try {
      const res = await fetch('/api/scenes', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: newSceneName.trim() })
      })
      if (res.ok) {
        refreshScenes()
        onSceneChange(newSceneName.trim())
        setNewSceneName('')
        setShowNewScene(false)
      }
    } catch (err) {
      console.error('Failed to create scene:', err)
    }
  }

  const handleDeleteScene = async () => {
    if (!currentScene) return
    if (!window.confirm(`Are you sure you want to delete the scene '${currentScene}'? This cannot be undone.`)) return
    
    try {
      const res = await fetch(`/api/scenes/${currentScene}`, { method: 'DELETE' })
      if (res.ok) {
        refreshScenes()
        // Selection of a new scene will be handled by the App.jsx hook when scenes change
      } else {
        alert('Failed to delete scene')
      }
    } catch (err) {
      console.error('Failed to delete scene:', err)
      alert('Failed to delete scene')
    }
  }

  return (
    <div className="toolbar">
      <div className="toolbar-left">
        <span className="toolbar-brand">🎮 ShawntyEngine</span>

        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <select
            className="scene-selector"
            value={currentScene || ''}
            onChange={(e) => onSceneChange(e.target.value)}
          >
            {scenes.map(s => (
              <option key={s} value={s}>{s}</option>
            ))}
          </select>
          <button 
            className="btn-secondary btn-sm" 
            onClick={handleDeleteScene}
            title="Delete current scene"
            disabled={!currentScene}
            style={{ padding: '0 6px', color: '#ff5555' }}
          >
            🗑️
          </button>
        </div>

        <button className="btn-secondary btn-sm" onClick={onOpenProjectSettings} style={{ marginLeft: '8px' }}>
          ⚙️ Settings
        </button>

        {showNewScene ? (
          <div className="toolbar-inline-form">
            <input
              type="text"
              placeholder="scene_name"
              value={newSceneName}
              onChange={e => setNewSceneName(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleCreateScene()}
              autoFocus
            />
            <button className="btn-primary btn-sm" onClick={handleCreateScene}>Create</button>
            <button className="btn-secondary btn-sm" onClick={() => setShowNewScene(false)}>✕</button>
          </div>
        ) : (
          <button className="btn-secondary btn-sm" onClick={() => setShowNewScene(true)}>
            + Scene
          </button>
        )}

        <div style={{ width: '1px', height: '24px', background: '#333', margin: '0 8px' }} />

        <button 
          className={`btn-sm ${uiMode ? 'btn-primary' : 'btn-secondary'}`} 
          onClick={() => setUiMode(!uiMode)}
        >
          {uiMode ? '📱 UI Mode: ON' : '📱 UI Mode: OFF'}
        </button>
      </div>

      <div className="toolbar-center">
        <button className="btn-primary" onClick={onAddEntity}>
          + Entity
        </button>
      </div>

      <div className="toolbar-right">
        <button
          className={`btn-primary ${saveStatus === 'saving' ? 'btn-loading' : ''}`}
          onClick={onSave}
        >
          {saveStatus === 'saving' ? '⏳ Saving...' :
           saveStatus === 'saved' ? '✅ Saved!' :
           saveStatus === 'error' ? '❌ Error' :
           '💾 Save'}
        </button>
        <span className="toolbar-shortcut">Ctrl+S</span>
        <div className={`status-dot ${connected ? 'connected' : ''}`}
             title={connected ? 'Live sync connected' : 'No live connection'} />
      </div>
    </div>
  )
}
