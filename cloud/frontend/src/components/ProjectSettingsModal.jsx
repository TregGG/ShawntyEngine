import { useState, useEffect } from 'react'

export default function ProjectSettingsModal({ show, onClose, prefabs }) {
  const [settings, setSettings] = useState({ defaultPlayerPrefab: 'player' })
  const [saving, setSaving] = useState(false)

  const handleChange = async (field, value) => {
    const newSettings = { ...settings, [field]: value }
    setSettings(newSettings)
    
    // Auto-save mock
    setSaving(true)
    setTimeout(() => setSaving(false), 500)
  }

  if (!show) return null

  return (
    <div className="modal-overlay" style={{
      position: 'fixed', top: 0, left: 0, width: '100vw', height: '100vh',
      backgroundColor: 'rgba(0,0,0,0.6)', zIndex: 1000,
      display: 'flex', alignItems: 'center', justifyContent: 'center'
    }}>
      <div className="modal-content" style={{
        background: 'var(--bg-panel)', border: '1px solid var(--border)', borderRadius: '8px',
        width: '100%', maxWidth: '500px',
        boxShadow: '0 10px 30px rgba(0,0,0,0.5)'
      }}>
        <div className="modal-header" style={{
          padding: '12px 16px', borderBottom: '1px solid var(--border)',
          display: 'flex', justifyContent: 'space-between', alignItems: 'center'
        }}>
          <h3 style={{ margin: 0, fontSize: '16px' }}>⚙️ Project Settings</h3>
          <button className="btn-secondary btn-sm" onClick={onClose}>✕</button>
        </div>
        
        <div className="modal-body" style={{ padding: '16px' }}>
          
          <div className="component-section">
            <div className="component-header">
              <span className="component-icon">🎮</span>
              <span className="component-title">Global Player Spawn</span>
              <span className="component-header-spacer" />
              {saving && <span style={{ fontSize: '12px', color: 'var(--text-secondary)' }}>Saving...</span>}
            </div>
            <div className="component-body">
              <div className="field-row">
                <label>Default Prefab</label>
                <select 
                  value={settings.defaultPlayerPrefab || ''} 
                  onChange={e => handleChange('defaultPlayerPrefab', e.target.value)}
                  style={{ background: 'var(--bg-input)', color: 'var(--text-primary)', border: '1px solid var(--border)', borderRadius: '4px', padding: '4px 8px' }}
                >
                  <option value="">(None)</option>
                  {(prefabs || ['player', 'enemy']).map(p => (
                    <option key={p} value={p}>{p}</option>
                  ))}
                </select>
              </div>
              <div style={{ fontSize: '11px', color: 'var(--text-secondary)', padding: '4px 0' }}>
                The prefab to spawn when a client connects, unless overridden by the scene.
              </div>
            </div>
          </div>

        </div>
      </div>
    </div>
  )
}
