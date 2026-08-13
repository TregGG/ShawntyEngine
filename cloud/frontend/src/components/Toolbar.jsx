import { useState } from 'react'
import { Link } from 'react-router-dom'

export default function Toolbar({ 
  projectName, onSave, onAddEntity, saveStatus, connected, uiMode, setUiMode,
  onOpenProjectSettings, scenes = ['main'], currentScene = 'main', onSceneChange, onCreateScene, onDeleteScene
}) {
  const [showNewScene, setShowNewScene] = useState(false)
  const [newSceneName, setNewSceneName] = useState('')

  const handleCreate = () => {
    if (newSceneName.trim()) {
      onCreateScene(newSceneName.trim())
      setNewSceneName('')
      setShowNewScene(false)
    }
  }

  return (
    <div className="toolbar">
      <div className="toolbar-left">
        <Link to="/dashboard" className="btn-secondary btn-sm" style={{ textDecoration: 'none', marginRight: '8px' }}>← Dashboard</Link>
        <span className="toolbar-brand">🎮 ShawntyEngine Cloud</span>
        <span style={{ fontSize: '13px', color: 'var(--text-secondary)' }}>{projectName || 'Untitled Project'}</span>
        
        <div style={{ width: '1px', height: '24px', background: '#333', margin: '0 8px' }} />
        
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <select
            className="scene-selector"
            value={currentScene || ''}
            onChange={(e) => onSceneChange(e.target.value)}
            style={{ background: 'var(--bg-input)', color: 'var(--text-primary)', border: '1px solid var(--border)', borderRadius: '4px', padding: '2px 4px' }}
          >
            {scenes.map(s => <option key={s} value={s}>{s}</option>)}
          </select>
          <button 
            className="btn-secondary btn-sm" 
            onClick={onDeleteScene}
            title="Delete current scene"
            disabled={scenes.length <= 1}
            style={{ padding: '0 6px', color: '#ff5555' }}
          >🗑️</button>
        </div>

        {showNewScene ? (
          <div style={{ display: 'flex', gap: '4px', marginLeft: '4px' }}>
            <input
              type="text"
              placeholder="scene_name"
              value={newSceneName}
              onChange={e => setNewSceneName(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleCreate()}
              autoFocus
              style={{ background: 'var(--bg-input)', color: 'var(--text-primary)', border: '1px solid var(--border)', borderRadius: '4px', padding: '2px 4px', width: '100px' }}
            />
            <button className="btn-primary btn-sm" onClick={handleCreate}>Create</button>
            <button className="btn-secondary btn-sm" onClick={() => setShowNewScene(false)}>✕</button>
          </div>
        ) : (
          <button className="btn-secondary btn-sm" onClick={() => setShowNewScene(true)} style={{ marginLeft: '4px' }}>
            + Scene
          </button>
        )}

        <button className="btn-secondary btn-sm" onClick={onOpenProjectSettings} style={{ marginLeft: '8px' }}>
          ⚙️ Settings
        </button>

        <div style={{ width: '1px', height: '24px', background: '#333', margin: '0 8px' }} />

        <button className={`btn-sm ${uiMode ? 'btn-primary' : 'btn-secondary'}`} onClick={() => setUiMode(!uiMode)}>
          {uiMode ? '📱 UI Mode: ON' : '📱 UI Mode: OFF'}
        </button>
      </div>
      <div className="toolbar-center">
        <button className="btn-primary" onClick={onAddEntity}>+ Entity</button>
      </div>
      <div className="toolbar-right">
        <button className={`btn-primary ${saveStatus === 'saving' ? 'btn-loading' : ''}`} onClick={onSave}>
          {saveStatus === 'saving' ? '⏳ Saving...' : saveStatus === 'saved' ? '✅ Saved!' : saveStatus === 'error' ? '❌ Error' : '💾 Save'}
        </button>
        <span className="toolbar-shortcut">Ctrl+S</span>
        <div className={`status-dot ${connected ? 'connected' : ''}`} title={connected ? 'Live sync connected' : 'Disconnected'} />
      </div>
    </div>
  )
}
