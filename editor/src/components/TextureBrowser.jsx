import { useState } from 'react'

export default function TextureBrowser({ assets, refreshAssets }) {
  const [selectedTexture, setSelectedTexture] = useState(null)
  const [uploading, setUploading] = useState(false)

  const textures = assets?.textures || []

  const getStem = (filename) => {
    return filename.substring(0, filename.lastIndexOf('.')) || filename
  }

  const handleDelete = async (filename, e) => {
    e.stopPropagation()
    const stem = getStem(filename)
    if (!window.confirm(`Are you sure you want to delete texture "${stem}"?`)) {
      return
    }

    try {
      const res = await fetch(`/api/textures/${encodeURIComponent(stem)}`, {
        method: 'DELETE'
      })
      if (res.ok) {
        if (selectedTexture === filename) {
          setSelectedTexture(null)
        }
        if (refreshAssets) refreshAssets()
      } else {
        const errData = await res.json()
        alert(`Failed to delete texture: ${errData.detail || res.statusText}`)
      }
    } catch (err) {
      console.error('Failed to delete texture:', err)
    }
  }

  const handleUpload = async (e) => {
    const files = e.target.files
    if (!files || files.length === 0) return

    setUploading(true)
    try {
      for (const file of files) {
        const formData = new FormData()
        formData.append('file', file)
        const res = await fetch('/api/assets/upload', {
          method: 'POST',
          body: formData
        })
        if (res.ok) {
          const data = await res.json()
          if (refreshAssets) refreshAssets()
          setSelectedTexture(data.filename) // Auto select the new texture filename
        }
      }
    } catch (err) {
      console.error('Upload failed:', err)
    } finally {
      setUploading(false)
      e.target.value = ''
    }
  }

  return (
    <div className="script-browser">
      <div className="script-list">
        <div className="script-list-header">
          <span>Textures</span>
          <label className="btn-icon" style={{ margin: 0, cursor: 'pointer' }} title="Upload Texture">
            {uploading ? '⏳' : '+'}
            <input
              type="file"
              accept="image/*"
              onChange={handleUpload}
              style={{ display: 'none' }}
              disabled={uploading}
            />
          </label>
        </div>
        {textures.length === 0 ? (
          <div className="empty-state">No textures found</div>
        ) : (
          textures.map(filename => {
            const isSelected = selectedTexture === filename
            return (
              <div
                key={filename}
                className={`script-item ${isSelected ? 'selected' : ''}`}
                onClick={() => setSelectedTexture(filename)}
                style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', width: '100%' }}
              >
                <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', flex: 1 }}>
                  🖼️ {filename}
                </span>
                <button
                  onClick={(e) => handleDelete(filename, e)}
                  style={{
                    background: '#ef4444',
                    border: 'none',
                    color: '#ffffff',
                    cursor: 'pointer',
                    fontSize: '12px',
                    width: '20px',
                    height: '20px',
                    borderRadius: '50%',
                    marginLeft: '8px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontWeight: 'bold',
                    boxShadow: '0 2px 4px rgba(0,0,0,0.3)'
                  }}
                  title={`Delete ${filename}`}
                >
                  ✕
                </button>
              </div>
            )
          })
        )}
      </div>
      <div className="script-preview" style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
        {selectedTexture ? (
          <div style={{ display: 'flex', flexDirection: 'column', flex: 1, padding: '16px', overflowY: 'auto' }}>
            <div className="script-preview-header" style={{ marginBottom: '12px', borderBottom: '1px solid var(--border)' }}>
              {selectedTexture}
            </div>
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              background: '#09090e',
              border: '1px dashed #2a2a3e',
              borderRadius: '8px',
              padding: '16px',
              minHeight: '200px',
              position: 'relative'
            }}>
              <img
                src={`/api/textures/${encodeURIComponent(getStem(selectedTexture))}/image`}
                alt={selectedTexture}
                style={{
                  maxWidth: '100%',
                  maxHeight: '300px',
                  objectFit: 'contain',
                  imageRendering: 'pixelated',
                  boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
                }}
              />
            </div>
            <div style={{ marginTop: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                Format: TGA (source) / PNG (preview)<br/>
                Render Mode: Pixelated
              </div>
              <button
                className="btn-secondary"
                style={{ background: '#7f1d1d', color: '#fca5a5', border: '1px solid #991b1b' }}
                onClick={(e) => handleDelete(selectedTexture, e)}
              >
                🗑️ Delete Texture Image
              </button>
            </div>
          </div>
        ) : (
          <div className="empty-state" style={{ display: 'flex', flex: 1, alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            Select a texture to preview
          </div>
        )}
      </div>
    </div>
  )
}
