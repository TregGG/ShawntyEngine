import { useState, useEffect } from 'react'

export default function ProjectSettingsModal({ show, onClose, prefabs }) {
  const [settings, setSettings] = useState({ defaultPlayerPrefab: 'player' })
  const [saving, setSaving] = useState(false)

  useEffect(() => {
    if (show) {
      // Fetch current settings
      fetch('/api/project/settings')
        .then(res => res.json())
        .then(data => setSettings(data))
        .catch(err => console.error('Failed to load project settings:', err))
    }
  }, [show])

  const handleChange = async (field, value) => {
    const newSettings = { ...settings, [field]: value }
    setSettings(newSettings)
    
    // Auto-save
    setSaving(true)
    try {
      await fetch('/api/project/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(newSettings)
      })
    } catch (err) {
      console.error('Failed to save project settings:', err)
    } finally {
      setSaving(false)
    }
  }

  if (!show) return null

  return (
    <div className="modal-overlay" style={{
      position: 'fixed', top: 0, left: 0, width: '100vw', height: '100vh',
      backgroundColor: 'rgba(0,0,0,0.6)', zIndex: 1000,
      display: 'flex', alignItems: 'center', justifyContent: 'center'
    }}>
      <div className="modal-content" style={{
        background: '#1e1e2e', border: '1px solid #333', borderRadius: '8px',
        width: '100%', maxWidth: '500px',
        boxShadow: '0 10px 30px rgba(0,0,0,0.5)'
      }}>
        <div className="modal-header" style={{
          padding: '12px 16px', borderBottom: '1px solid #333',
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
              {saving && <span style={{ fontSize: '12px', color: '#888' }}>Saving...</span>}
            </div>
            <div className="component-body">
              <div className="field-row">
                <label>Default Prefab</label>
                <select 
                  value={settings.defaultPlayerPrefab || ''} 
                  onChange={e => handleChange('defaultPlayerPrefab', e.target.value)}
                >
                  <option value="">(None)</option>
                  {(prefabs || []).map(p => (
                    <option key={p} value={p}>{p}</option>
                  ))}
                </select>
              </div>
              <div style={{ fontSize: '11px', color: '#666', padding: '4px 0' }}>
                The prefab to spawn when a client connects, unless overridden by the scene.
              </div>
            </div>
          </div>

        </div>
      </div>
    </div>
  )
}
