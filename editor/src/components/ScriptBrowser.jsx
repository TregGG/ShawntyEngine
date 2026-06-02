import { useState } from 'react'
import { useScripts, useScript, createScript } from '../hooks/useApi'

export default function ScriptBrowser() {
  const { scripts, loading, refresh } = useScripts()
  const [selectedScript, setSelectedScript] = useState(null)
  const [showNewScript, setShowNewScript] = useState(false)
  const [newScriptName, setNewScriptName] = useState('')
  const { script: scriptData } = useScript(selectedScript)

  const handleCreate = async () => {
    if (!newScriptName.trim()) return
    try {
      await createScript(newScriptName.trim())
      refresh()
      setNewScriptName('')
      setShowNewScript(false)
    } catch (err) {
      console.error('Failed to create script:', err)
    }
  }

  return (
    <div className="script-browser">
      <div className="script-list">
        <div className="script-list-header">
          <span>Scripts</span>
          {showNewScript ? (
            <div className="toolbar-inline-form compact">
              <input
                type="text"
                placeholder="script_name.py"
                value={newScriptName}
                onChange={e => setNewScriptName(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && handleCreate()}
                autoFocus
              />
              <button className="btn-primary btn-sm" onClick={handleCreate}>+</button>
              <button className="btn-secondary btn-sm" onClick={() => setShowNewScript(false)}>✕</button>
            </div>
          ) : (
            <button className="btn-icon" onClick={() => setShowNewScript(true)} title="New Script">+</button>
          )}
        </div>
        {loading ? (
          <div className="empty-state">Loading...</div>
        ) : scripts?.length === 0 ? (
          <div className="empty-state">No scripts found</div>
        ) : (
          scripts?.map(name => (
            <div
              key={name}
              className={`script-item ${selectedScript === name ? 'selected' : ''}`}
              onClick={() => setSelectedScript(name)}
            >
              🐍 {name}
            </div>
          ))
        )}
      </div>
      <div className="script-preview">
        {selectedScript && scriptData ? (
          <>
            <div className="script-preview-header">{selectedScript}</div>
            <pre className="script-viewer">
              <code>{scriptData.content}</code>
            </pre>
          </>
        ) : (
          <div className="empty-state">Select a script to preview</div>
        )}
      </div>
    </div>
  )
}
