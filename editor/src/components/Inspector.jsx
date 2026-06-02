import { useState, useCallback, useEffect } from 'react'

const CATEGORIES = ['Environment', 'Player', 'Enemy', 'Projectile', 'UI']
const LAYERS = ['Background', 'Foreground', 'Player', 'UI']
const BODY_TYPES = ['Static', 'Dynamic', 'Kinematic']

const COMPONENT_META = {
  transform: { label: 'Transform', icon: '📐' },
  sprite: { label: 'Sprite', icon: '🖼️' },
  collider: { label: 'Collider', icon: '📦' },
  rigidbody: { label: 'RigidBody', icon: '⚙️' },
  animator: { label: 'Animator', icon: '🎬' },
  script: { label: 'Script', icon: '🐍' },
  ui: { label: 'UI Element', icon: '📱' },
}

// ---------------------------------------------------------------------------
// Reusable field inputs
// ---------------------------------------------------------------------------
function NumberInput({ label, value, onChange, step = 0.1 }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <input
        type="number"
        value={value ?? 0}
        step={step}
        onChange={e => onChange(parseFloat(e.target.value) || 0)}
      />
    </div>
  )
}

function TextInput({ label, value, onChange, placeholder }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <input
        type="text"
        value={value || ''}
        placeholder={placeholder}
        onChange={e => onChange(e.target.value)}
      />
    </div>
  )
}

function SelectInput({ label, value, options, onChange }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <select value={value || options[0]} onChange={e => onChange(e.target.value)}>
        {options.map(o => <option key={o} value={o}>{o}</option>)}
      </select>
    </div>
  )
}

function CheckboxInput({ label, checked, onChange }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <input
        type="checkbox"
        checked={checked || false}
        onChange={e => onChange(e.target.checked)}
      />
    </div>
  )
}

// ---------------------------------------------------------------------------
// Collapsible component section with REMOVE on the right
// ---------------------------------------------------------------------------
function ComponentSection({ title, icon, removable, onRemove, children }) {
  const [collapsed, setCollapsed] = useState(false)

  return (
    <div className={`component-section ${collapsed ? 'collapsed' : ''}`}>
      <div className="component-header" onClick={() => setCollapsed(!collapsed)}>
        <span className="component-arrow">{collapsed ? '▶' : '▼'}</span>
        <span className="component-icon">{icon}</span>
        <span className="component-title">{title}</span>
        <span className="component-header-spacer" />
        {removable && (
          <button
            className="btn-remove-component"
            onClick={(e) => { e.stopPropagation(); onRemove() }}
            title={`Remove ${title}`}
          >
            ✕
          </button>
        )}
      </div>
      {!collapsed && (
        <div className="component-body">
          {children}
        </div>
      )}
    </div>
  )
}

// ---------------------------------------------------------------------------
// Transform Editor
// ---------------------------------------------------------------------------
function TransformEditor({ data, onChange }) {
  const pos = data.position || [0, 0]
  const size = data.size || [1, 1]
  const localPos = data.localPosition || [0, 0]

  const set = (field, value) => onChange({ ...data, [field]: value })

  return (
    <ComponentSection title="Transform" icon="📐">
      <NumberInput label="Pos X" value={pos[0]} onChange={v => set('position', [v, pos[1]])} />
      <NumberInput label="Pos Y" value={pos[1]} onChange={v => set('position', [pos[0], v])} />
      <NumberInput label="Size X" value={size[0]} onChange={v => set('size', [v, size[1]])} step={0.5} />
      <NumberInput label="Size Y" value={size[1]} onChange={v => set('size', [size[0], v])} step={0.5} />
      <NumberInput label="Rotation" value={data.rotation || 0} onChange={v => set('rotation', v)} step={0.1} />
      <NumberInput label="Local X" value={localPos[0]} onChange={v => set('localPosition', [v, localPos[1]])} />
      <NumberInput label="Local Y" value={localPos[1]} onChange={v => set('localPosition', [localPos[0], v])} />
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Sprite Editor — with asset upload
// ---------------------------------------------------------------------------
// Helper to parse spritesheet lines
const parseSpritesheet = (content) => {
  let texture = ''
  const frames = []
  if (!content) return { texture, frames }
  for (const line of content.split('\n')) {
    const trimmed = line.trim()
    if (trimmed.startsWith('texture:')) {
      texture = trimmed.split(':', 2)[1].trim()
    } else if (trimmed.includes(':') && trimmed.includes('=')) {
      const parts = trimmed.split(':', 2)
      const idx = parseInt(parts[0].trim())
      const frameData = {}
      for (const p of parts[1].trim().split(/\s+/)) {
        if (p.includes('=')) {
          const [k, v] = p.split('=', 2)
          frameData[k] = parseInt(v)
        }
      }
      frames.push({ index: idx, ...frameData })
    }
  }
  return { texture, frames }
}

function SpriteEditor({ data, onChange, onRemove, assets, refreshAssets }) {
  const objects = assets?.objects || []
  const [uploading, setUploading] = useState(false)
  const [spritesheet, setSpritesheet] = useState(null)
  
  // Create New Object form state
  const [showCreateObject, setShowCreateObject] = useState(false)
  const [newObjectName, setNewObjectName] = useState('')

  // Slicing state variables
  const [sliceW, setSliceW] = useState(32)
  const [sliceH, setSliceH] = useState(32)
  const [sliceCols, setSliceCols] = useState(1)
  const [sliceRows, setSliceRows] = useState(1)
  const [showSliceForm, setShowSliceForm] = useState(false)

  // Fetch and parse spritesheet on objectId change
  useEffect(() => {
    if (!data.objectId || data.objectId === '(none)') {
      setSpritesheet(null)
      return
    }
    
    // 1. Fetch Object Details
    fetch(`/api/objects/${data.objectId}`)
      .then(res => res.ok ? res.json() : null)
      .then(objData => {
        if (objData && objData.spritesheet) {
          // 2. Fetch Spritesheet Details
          fetch(`/api/spritesheets/${objData.spritesheet}`)
            .then(res => res.ok ? res.json() : null)
            .then(sheetData => {
              if (sheetData && sheetData.content) {
                setSpritesheet(parseSpritesheet(sheetData.content))
              }
            })
        }
      })
      .catch(err => console.error('Failed to load spritesheet:', err))
  }, [data.objectId])

  // Handle uploading and appending a frame to the current spritesheet
  const handleAppendFrame = async (e) => {
    const files = e.target.files
    if (!files || files.length === 0 || !data.objectId) return

    setUploading(true)
    try {
      const objRes = await fetch(`/api/objects/${data.objectId}`)
      if (!objRes.ok) throw new Error("Failed to load object details")
      const objData = await objRes.json()
      const sheetName = objData.spritesheet

      for (const file of files) {
        const formData = new FormData()
        formData.append('file', file)
        const appendRes = await fetch(`/api/spritesheets/${sheetName}/append`, {
          method: 'POST',
          body: formData
        })
        if (appendRes.ok) {
          const appendData = await appendRes.json()
          // Automatically set selected frame to the newly appended one
          onChange({ ...data, frameIndex: appendData.frameIndex })
        }
      }
      
      // Refresh the local spritesheet view
      const sheetRes = await fetch(`/api/spritesheets/${sheetName}`)
      if (sheetRes.ok) {
        const sheetData = await sheetRes.json()
        setSpritesheet(parseSpritesheet(sheetData.content))
      }
    } catch (err) {
      console.error('Frame append failed:', err)
    }
    setUploading(false)
    e.target.value = ''
  }

  // Handle uploading a file to create a brand new object
  const handleCreateNewObjectUpload = async (e) => {
    const files = e.target.files
    if (!files || files.length === 0) return

    setUploading(true)
    try {
      let lastUploadedName = null
      for (const file of files) {
        const formData = new FormData()
        formData.append('file', file)
        const res = await fetch('/api/assets/upload', { method: 'POST', body: formData })
        if (res.ok) {
          const data = await res.json()
          lastUploadedName = data.name
          
          // Auto-create a default 1x1 spritesheet
          await fetch('/api/spritesheets', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name: data.name, texture: data.name, width: 32, height: 32, cols: 1, rows: 1 })
          })
          
          // Auto-create object asset
          await fetch('/api/objects', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name: data.name, spritesheet: data.name, animations: data.name })
          })
        }
      }
      
      if (refreshAssets) refreshAssets()
      
      if (lastUploadedName) {
        onChange({ ...data, objectId: lastUploadedName, frameIndex: 0 })
      }
    } catch (err) {
      console.error('New object upload failed:', err)
    }
    setUploading(false)
    e.target.value = ''
  }

  // Handle creating a new empty object directly from text input
  const handleCreateEmptyObject = async () => {
    if (!newObjectName.trim()) return
    const objName = newObjectName.trim()
    try {
      // 1. Create a default spritesheet
      await fetch('/api/spritesheets', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: objName, texture: objName, width: 32, height: 32, cols: 1, rows: 1 })
      })
      
      // 2. Create the Object asset
      await fetch('/api/objects', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: objName, spritesheet: objName, animations: objName })
      })

      // 3. Create the empty Animation asset
      await fetch('/api/animations', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: objName, clips: [{ name: "idle", frames: [{ frame: 0, duration: 1.0 }] }] })
      })

      if (refreshAssets) refreshAssets()
      onChange({ ...data, objectId: objName, frameIndex: 0 })
      setNewObjectName('')
      setShowCreateObject(false)
    } catch (err) {
      console.error('Failed to create empty object:', err)
    }
  }

  return (
    <ComponentSection title="Sprite" icon="🖼️" removable onRemove={onRemove}>
      <div className="field-row">
        <label>Object</label>
        <div style={{ display: 'flex', gap: '4px', flex: 1 }}>
          <select 
            value={data.objectId || ''} 
            onChange={e => onChange({ ...data, objectId: e.target.value === '(none)' ? undefined : e.target.value, frameIndex: 0 })}
            style={{ flex: 1 }}
          >
            <option value="(none)">(none)</option>
            {objects.map(o => <option key={o} value={o}>{o}</option>)}
          </select>
          <button 
            type="button"
            className="btn-secondary btn-sm"
            onClick={() => setShowCreateObject(!showCreateObject)}
            title="Create new Object asset"
          >
            ➕
          </button>
        </div>
      </div>

      {showCreateObject && (
        <div className="field-group-box" style={{ background: '#1c1c28', padding: '8px', borderRadius: '4px', marginBottom: '8px' }}>
          <div className="field-row" style={{ fontSize: '11px', color: '#8f8fa9', marginBottom: '4px' }}>
            ℹ️ Object maps a SpriteSheet and Animations for the engine to render.
          </div>
          <TextInput 
            label="Name" 
            value={newObjectName} 
            onChange={setNewObjectName} 
            placeholder="e.g. player_hero"
          />
          <div style={{ display: 'flex', gap: '8px', marginTop: '6px', justifyContent: 'flex-end' }}>
            <button className="btn-secondary btn-sm" onClick={() => setShowCreateObject(false)}>Cancel</button>
            <button className="btn-primary btn-sm" onClick={handleCreateEmptyObject}>Create Empty</button>
            <label className="btn-primary btn-sm upload-btn" style={{ margin: 0 }}>
              {uploading ? '⏳...' : '📁 Upload File'}
              <input
                type="file"
                accept="image/*"
                onChange={handleCreateNewObjectUpload}
                style={{ display: 'none' }}
              />
            </label>
          </div>
        </div>
      )}

      {data.objectId && data.objectId !== '(none)' && (
        <>
          <NumberInput label="Frame" value={data.frameIndex || 0} onChange={v => onChange({ ...data, frameIndex: Math.max(0, Math.floor(v)) })} step={1} />
          <SelectInput label="Layer" value={data.layer || 'Foreground'} options={LAYERS} onChange={v => onChange({ ...data, layer: v })} />

          {/* Slicing options */}
          <div style={{ marginTop: '8px', marginBottom: '8px' }}>
            <button 
              type="button" 
              className="btn-secondary btn-sm"
              onClick={() => setShowSliceForm(!showSliceForm)}
              style={{ width: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
            >
              ✂️ Slice Texture into Grid
            </button>
            {showSliceForm && (
              <div className="field-group-box" style={{ background: '#1c1c28', padding: '8px', borderRadius: '4px', marginTop: '6px' }}>
                <NumberInput label="Width (px)" value={sliceW} onChange={setSliceW} step={8} />
                <NumberInput label="Height (px)" value={sliceH} onChange={setSliceH} step={8} />
                <NumberInput label="Columns" value={sliceCols} onChange={sliceVal => setSliceCols(Math.max(1, sliceVal))} step={1} />
                <NumberInput label="Rows" value={sliceRows} onChange={sliceVal => setSliceRows(Math.max(1, sliceVal))} step={1} />
                <button 
                  type="button"
                  className="btn-primary btn-sm"
                  onClick={async () => {
                    if (!data.objectId || !spritesheet) return
                    try {
                      const objRes = await fetch(`/api/objects/${data.objectId}`)
                      if (objRes.ok) {
                        const objData = await objRes.json()
                        await fetch('/api/spritesheets', {
                          method: 'POST',
                          headers: { 'Content-Type': 'application/json' },
                          body: JSON.stringify({
                            name: objData.spritesheet,
                            texture: spritesheet.texture || objData.spritesheet,
                            width: sliceW,
                            height: sliceH,
                            cols: sliceCols,
                            rows: sliceRows
                          })
                        })
                        // Refresh
                        const sheetRes = await fetch(`/api/spritesheets/${objData.spritesheet}`)
                        if (sheetRes.ok) {
                          const sheetData = await sheetRes.json()
                          setSpritesheet(parseSpritesheet(sheetData.content))
                        }
                        onChange({ ...data, frameIndex: 0 })
                      }
                    } catch (err) {
                      console.error('Slicing failed:', err)
                    }
                    setShowSliceForm(false)
                  }}
                  style={{ width: '100%', marginTop: '6px' }}
                >
                  Apply Grid Slice
                </button>
              </div>
            )}
          </div>

          {/* Frames List Visual Preview */}
          <div className="field-group-header">Spritesheet Frames</div>
          {spritesheet && spritesheet.frames && spritesheet.frames.length > 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', maxHeight: '180px', overflowY: 'auto', background: '#0e0e17', padding: '6px', borderRadius: '4px', border: '1px solid #1f1f2e' }}>
              {spritesheet.frames.map(f => {
                const isSelected = (data.frameIndex || 0) === f.index;
                return (
                  <div 
                    key={f.index}
                    onClick={() => onChange({ ...data, frameIndex: f.index })}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      padding: '4px',
                      borderRadius: '4px',
                      background: isSelected ? 'var(--bg-hover)' : 'transparent',
                      border: isSelected ? '1px solid var(--accent)' : '1px solid transparent',
                      cursor: 'pointer',
                      position: 'relative'
                    }}
                  >
                    <div style={{
                      width: '40px',
                      height: '40px',
                      overflow: 'hidden',
                      border: '1px solid #2f2f45',
                      borderRadius: '3px',
                      background: '#040408',
                      position: 'relative'
                    }}>
                      <div style={{
                        width: `${f.w}px`,
                        height: `${f.h}px`,
                        position: 'absolute',
                        top: '50%',
                        left: '50%',
                        backgroundImage: `url(/api/textures/${spritesheet.texture}/image?v=${spritesheet.frames.length})`,
                        backgroundPosition: `-${f.x}px -${f.y}px`,
                        backgroundRepeat: 'no-repeat',
                        transformOrigin: 'center center',
                        transform: `translate(-50%, -50%) scale(${f.w > 0 && f.h > 0 ? Math.min(1, 38 / Math.max(f.w, f.h)) : 1})`
                      }} />
                    </div>
                    <div style={{ display: 'flex', flexDirection: 'column', fontSize: '11px', flex: 1 }}>
                      <span style={{ fontWeight: 'bold', color: isSelected ? '#a78bfa' : '#d1d5db' }}>Frame {f.index}</span>
                      <span style={{ color: '#6b7280' }}>x={f.x} y={f.y} w={f.w} h={f.h}</span>
                    </div>
                    {isSelected && <span style={{ color: '#a78bfa', fontSize: '12px', marginRight: '4px' }}>✓</span>}
                    <button
                      type="button"
                      onClick={async (e) => {
                        e.stopPropagation();
                        if (!window.confirm(`Are you sure you want to remove Frame ${f.index}?`)) {
                          return;
                        }
                        try {
                          const objRes = await fetch(`/api/objects/${data.objectId}`);
                          if (objRes.ok) {
                            const objData = await objRes.json();
                            const sheetName = objData.spritesheet;
                            const remaining = spritesheet.frames.filter(frame => frame.index !== f.index);
                            const newLines = [`texture:${spritesheet.texture}`];
                            remaining.forEach((r, newIdx) => {
                              newLines.push(`${newIdx}: x=${r.x} y=${r.y} w=${r.w} h=${r.h}`);
                            });
                            const newContent = newLines.join('\n') + '\n';
                            const updateRes = await fetch(`/api/spritesheets/${sheetName}`, {
                              method: 'PUT',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ content: newContent })
                            });
                            if (updateRes.ok) {
                              setSpritesheet({
                                texture: spritesheet.texture,
                                frames: remaining.map((r, newIdx) => ({ ...r, index: newIdx }))
                              });
                              let nextFrameIndex = data.frameIndex || 0;
                              if (nextFrameIndex === f.index) {
                                nextFrameIndex = 0;
                              } else if (nextFrameIndex > f.index) {
                                nextFrameIndex = Math.max(0, nextFrameIndex - 1);
                              }
                              if (remaining.length === 0) {
                                nextFrameIndex = 0;
                              } else if (nextFrameIndex >= remaining.length) {
                                nextFrameIndex = remaining.length - 1;
                              }
                              onChange({ ...data, frameIndex: nextFrameIndex });
                            } else {
                              alert('Failed to update spritesheet file.');
                            }
                          }
                        } catch (err) {
                          console.error('Failed to remove frame:', err);
                        }
                      }}
                      style={{
                        background: '#ef4444',
                        border: '2px solid #000000',
                        color: '#ffffff',
                        cursor: 'pointer',
                        fontSize: '12px',
                        width: '24px',
                        height: '24px',
                        borderRadius: '50%',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        fontWeight: 'bold',
                        boxShadow: '0 2px 4px rgba(0,0,0,0.5)',
                        position: 'absolute',
                        top: '-8px',
                        right: '-8px'
                      }}
                      title={`Remove Frame ${f.index}`}
                    >
                      ✕
                    </button>
                  </div>
                )
              })}
            </div>
          ) : (
            <div className="empty-state" style={{ padding: '8px 0' }}>No frames defined in spritesheet.</div>
          )}

          <div className="field-row" style={{ marginTop: '8px' }}>
            <label>Append Frame</label>
            <label className="btn-secondary btn-sm upload-btn" style={{ flex: 1, textAlign: 'center' }}>
              {uploading ? '⏳...' : '➕ Upload & Append Image'}
              <input
                type="file"
                accept="image/*"
                onChange={handleAppendFrame}
                style={{ display: 'none' }}
              />
            </label>
          </div>
        </>
      )}
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Collider Editor
// ---------------------------------------------------------------------------
function ColliderEditor({ data, onChange, onRemove }) {
  return (
    <ComponentSection title="Collider" icon="📦" removable onRemove={onRemove}>
      <CheckboxInput label="Is Trigger" checked={data.isTrigger} onChange={v => onChange({ ...data, isTrigger: v })} />
      <CheckboxInput label="Auto Bounds" checked={data.autoBounds} onChange={v => onChange({ ...data, autoBounds: v })} />
      {!data.autoBounds && (
        <>
          <NumberInput label="Offset X" value={data.localOffset?.[0] || 0} onChange={v => onChange({ ...data, localOffset: [v, data.localOffset?.[1] || 0] })} />
          <NumberInput label="Offset Y" value={data.localOffset?.[1] || 0} onChange={v => onChange({ ...data, localOffset: [data.localOffset?.[0] || 0, v] })} />
          <NumberInput label="Size X" value={data.localSize?.[0] || 1} onChange={v => onChange({ ...data, localSize: [v, data.localSize?.[1] || 1] })} />
          <NumberInput label="Size Y" value={data.localSize?.[1] || 1} onChange={v => onChange({ ...data, localSize: [data.localSize?.[0] || 1, v] })} />
        </>
      )}
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// RigidBody Editor
// ---------------------------------------------------------------------------
function RigidBodyEditor({ data, onChange, onRemove }) {
  return (
    <ComponentSection title="RigidBody" icon="⚙️" removable onRemove={onRemove}>
      <SelectInput label="Body Type" value={data.bodyType || 'Static'} options={BODY_TYPES} onChange={v => onChange({ ...data, bodyType: v })} />
      <NumberInput label="Mass" value={data.mass ?? 1.0} onChange={v => onChange({ ...data, mass: v })} />
      <NumberInput label="Drag" value={data.drag ?? 0} onChange={v => onChange({ ...data, drag: v })} />
      <NumberInput label="Gravity Scale" value={data.gravityScale ?? 1.0} onChange={v => onChange({ ...data, gravityScale: v })} />
      <CheckboxInput label="Use Gravity" checked={data.useGravity} onChange={v => onChange({ ...data, useGravity: v })} />
      <NumberInput label="Elasticity" value={data.elasticity ?? 0} onChange={v => onChange({ ...data, elasticity: v })} step={0.1} />
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Animator Editor — with clip creation / frame editing
// ---------------------------------------------------------------------------
function AnimatorEditor({ data, onChange, onRemove, assets }) {
  const objects = assets?.objects || []
  const [clips, setClips] = useState(data._clips || [])
  const [newClipName, setNewClipName] = useState('')
  const [expandedClip, setExpandedClip] = useState(null)

  // Load existing animations from API when objectId changes
  useEffect(() => {
    if (data.objectId) {
      fetch(`/api/animations/${data.objectId}`)
        .then(r => r.ok ? r.json() : null)
        .then(d => {
          if (d && d.clips) {
            setClips(d.clips)
          }
        })
        .catch(() => {})
    }
  }, [data.objectId])

  const saveClips = useCallback((updatedClips) => {
    setClips(updatedClips)
    // Save to server if objectId exists
    if (data.objectId) {
      fetch(`/api/animations/${data.objectId}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ clips: updatedClips }),
      }).catch(err => console.error('Failed to save animation:', err))
    }
    onChange({ ...data, _clips: updatedClips })
  }, [data, onChange])

  const addClip = () => {
    if (!newClipName.trim()) return
    const updated = [...clips, { name: newClipName.trim(), frames: [{ frame: 0, duration: 0.1 }] }]
    saveClips(updated)
    setNewClipName('')
    setExpandedClip(updated.length - 1)
  }

  const removeClip = (idx) => {
    const updated = clips.filter((_, i) => i !== idx)
    saveClips(updated)
    if (expandedClip === idx) setExpandedClip(null)
  }

  const updateFrame = (clipIdx, frameIdx, field, value) => {
    const updated = JSON.parse(JSON.stringify(clips))
    updated[clipIdx].frames[frameIdx][field] = value
    saveClips(updated)
  }

  const addFrame = (clipIdx) => {
    const updated = JSON.parse(JSON.stringify(clips))
    const lastFrame = updated[clipIdx].frames[updated[clipIdx].frames.length - 1]
    updated[clipIdx].frames.push({
      frame: (lastFrame?.frame ?? 0) + 1,
      duration: lastFrame?.duration ?? 0.1
    })
    saveClips(updated)
  }

  const removeFrame = (clipIdx, frameIdx) => {
    const updated = JSON.parse(JSON.stringify(clips))
    updated[clipIdx].frames.splice(frameIdx, 1)
    saveClips(updated)
  }

  return (
    <ComponentSection title="Animator" icon="🎬" removable onRemove={onRemove}>
      <SelectInput
        label="Object"
        value={data.objectId || ''}
        options={['(none)', ...objects]}
        onChange={v => onChange({ ...data, objectId: v === '(none)' ? undefined : v })}
      />
      <TextInput label="Default Clip" value={data.defaultClip} onChange={v => onChange({ ...data, defaultClip: v })} />
      <NumberInput label="Speed" value={data.speed ?? 1.0} onChange={v => onChange({ ...data, speed: v })} />
      <CheckboxInput label="Loop" checked={data.loop ?? true} onChange={v => onChange({ ...data, loop: v })} />

      <div className="field-group-header">Animation Clips</div>

      {clips.map((clip, clipIdx) => (
        <div key={clipIdx} className="anim-clip">
          <div
            className="anim-clip-header"
            onClick={() => setExpandedClip(expandedClip === clipIdx ? null : clipIdx)}
          >
            <span className="component-arrow">{expandedClip === clipIdx ? '▼' : '▶'}</span>
            <span className="anim-clip-name">🎞️ {clip.name}</span>
            <span className="anim-clip-info">{clip.frames.length} frames</span>
            <button className="btn-remove-sm" onClick={e => { e.stopPropagation(); removeClip(clipIdx) }}>✕</button>
          </div>

          {expandedClip === clipIdx && (
            <div className="anim-clip-body">
              <div className="anim-frames-header">
                <span className="anim-col-idx">#</span>
                <span className="anim-col-frame">Frame</span>
                <span className="anim-col-dur">Duration</span>
                <span className="anim-col-del"></span>
              </div>

              {clip.frames.map((frame, frameIdx) => (
                <div key={frameIdx} className="anim-frame-row">
                  <span className="anim-col-idx">{frameIdx}</span>
                  <input
                    type="number"
                    className="anim-col-frame"
                    value={frame.frame ?? 0}
                    min={0}
                    step={1}
                    onChange={e => updateFrame(clipIdx, frameIdx, 'frame', parseInt(e.target.value) || 0)}
                  />
                  <input
                    type="number"
                    className="anim-col-dur"
                    value={frame.duration ?? 0.1}
                    min={0.01}
                    step={0.05}
                    onChange={e => updateFrame(clipIdx, frameIdx, 'duration', parseFloat(e.target.value) || 0.1)}
                  />
                  <button
                    className="btn-remove-sm anim-col-del"
                    onClick={() => removeFrame(clipIdx, frameIdx)}
                    title="Remove frame"
                  >✕</button>
                </div>
              ))}

              <button className="btn-secondary btn-sm anim-add-frame" onClick={() => addFrame(clipIdx)}>
                + Add Frame
              </button>
            </div>
          )}
        </div>
      ))}

      <div className="anim-new-clip">
        <input
          type="text"
          placeholder="Clip name (e.g. run)"
          value={newClipName}
          onChange={e => setNewClipName(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && addClip()}
        />
        <button className="btn-primary btn-sm" onClick={addClip}>+ Clip</button>
      </div>
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Script Editor
// ---------------------------------------------------------------------------
function ScriptEditor({ data, onChange, onRemove, scripts }) {
  const [newKey, setNewKey] = useState('')
  const [newValue, setNewValue] = useState('')

  const addProperty = () => {
    if (!newKey.trim()) return
    const props = { ...(data.properties || {}), [newKey.trim()]: newValue }
    onChange({ ...data, properties: props })
    setNewKey('')
    setNewValue('')
  }

  const removeProperty = (key) => {
    const props = { ...(data.properties || {}) }
    delete props[key]
    onChange({ ...data, properties: props })
  }

  return (
    <ComponentSection title="Script" icon="🐍" removable onRemove={onRemove}>
      <div className="field-row">
        <label>Script</label>
        <select
          value={data.path || ''}
          onChange={e => {
            const path = e.target.value
            // Auto-derive class name from filename
            const stem = path.split('/').pop()?.replace('.py', '') || ''
            const className = stem.split('_').map(w => w.charAt(0).toUpperCase() + w.slice(1)).join('')
            onChange({ ...data, path, class: className })
          }}
        >
          <option value="">(select script)</option>
          {(scripts || []).map(s => (
            <option key={s} value={`test_compiled/scripts/${s}`}>{s}</option>
          ))}
        </select>
      </div>
      <TextInput label="Class" value={data.class} onChange={v => onChange({ ...data, class: v })} />

      <div className="field-group-header">Properties</div>
      {data.properties && Object.entries(data.properties).map(([key, val]) => (
        <div key={key} className="field-row property-row">
          <span className="property-key">{key}</span>
          <input
            type="text"
            value={typeof val === 'string' ? val : JSON.stringify(val)}
            onChange={e => {
              const props = { ...(data.properties || {}) }
              props[key] = e.target.value
              onChange({ ...data, properties: props })
            }}
          />
          <button className="btn-remove-sm" onClick={() => removeProperty(key)}>✕</button>
        </div>
      ))}
      <div className="field-row add-property">
        <input type="text" placeholder="key" value={newKey} onChange={e => setNewKey(e.target.value)} />
        <input type="text" placeholder="value" value={newValue} onChange={e => setNewValue(e.target.value)} onKeyDown={e => e.key === 'Enter' && addProperty()} />
        <button className="btn-icon" onClick={addProperty}>+</button>
      </div>
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Add Component Dropdown
// ---------------------------------------------------------------------------
function AddComponentDropdown({ existingComponents, onAdd, onAddScript }) {
  const [open, setOpen] = useState(false)

  const allComponents = Object.keys(COMPONENT_META)
  const addable = allComponents.filter(c => !existingComponents.includes(c))

  if (addable.length === 0) return null

  const defaults = {
    transform: { position: [0, 0], size: [1, 1], rotation: 0 },
    sprite: { layer: 'Foreground' },
    collider: { autoBounds: true, isTrigger: false },
    rigidbody: { bodyType: 'Static', mass: 1.0, drag: 0, gravityScale: 1.0 },
    animator: { speed: 1.0, loop: true },
    script: { path: '', class: '', properties: {} },
    ui: { type: 'Panel', position: [0, 0], size: [100, 40], backgroundColor: [0.5, 0.5, 0.5, 1.0] },
  }

  return (
    <div className="add-component-area">
      <button
        className="add-component-btn"
        onClick={() => setOpen(!open)}
      >
        + Add Component
      </button>

      {open && (
        <div className="add-component-dropdown">
          {addable.map(comp => {
            const meta = COMPONENT_META[comp]
            return (
              <button
                key={comp}
                className="add-component-option"
                onClick={() => {
                  if (comp === 'script') {
                    onAddScript(defaults.script)
                  } else {
                    onAdd(comp, defaults[comp])
                  }
                  setOpen(false)
                }}
              >
                <span className="add-comp-icon">{meta.icon}</span>
                <span>{meta.label}</span>
              </button>
            )
          })}
        </div>
      )}
    </div>
  )
}

// ---------------------------------------------------------------------------
// Color Input Component
// ---------------------------------------------------------------------------
const rgbToHex = (r, g, b) => {
  const toHex = c => Math.max(0, Math.min(255, Math.round(c * 255))).toString(16).padStart(2, '0')
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`
}

const hexToRgb = (hex) => {
  const r = parseInt(hex.slice(1, 3), 16) / 255
  const g = parseInt(hex.slice(3, 5), 16) / 255
  const b = parseInt(hex.slice(5, 7), 16) / 255
  return [r, g, b]
}

function ColorInput({ label, value, onChange, hasAlpha = true }) {
  const [r=1, g=1, b=1, a=1] = value || []
  const hex = rgbToHex(r, g, b)

  return (
    <div className="field-row">
      <label>{label}</label>
      <div style={{ display: 'flex', gap: '8px', alignItems: 'center', flex: 1 }}>
        <input 
          type="color" 
          value={hex} 
          onChange={e => {
            const [nr, ng, nb] = hexToRgb(e.target.value)
            if (hasAlpha) onChange([nr, ng, nb, a])
            else onChange([nr, ng, nb])
          }}
          style={{ padding: 0, width: '40px', height: '24px', border: 'none', background: 'none', cursor: 'pointer' }}
        />
        {hasAlpha && (
          <input 
            type="number" 
            step="0.05" 
            min="0" 
            max="1" 
            value={a} 
            onChange={e => {
              const na = Math.max(0, Math.min(1, parseFloat(e.target.value) || 0))
              onChange([r, g, b, na])
            }}
            title="Alpha (Opacity)"
            style={{ width: '60px' }}
          />
        )}
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------
// UI Editor
// ---------------------------------------------------------------------------
function UIEditor({ data, onChange, onRemove }) {
  return (
    <ComponentSection title="UI Element" icon="🖥️" removable onRemove={onRemove}>
      <SelectInput label="Type" value={data.type || 'Panel'} options={['Panel', 'Text', 'Button', 'Input']} onChange={v => onChange({ ...data, type: v })} />
      
      <NumberInput label="Position X" value={data.position?.[0] || 0} onChange={v => onChange({ ...data, position: [v, data.position?.[1] || 0] })} step={10} />
      <NumberInput label="Position Y" value={data.position?.[1] || 0} onChange={v => onChange({ ...data, position: [data.position?.[0] || 0, v] })} step={10} />
      
      <NumberInput label="Size W" value={data.size?.[0] || 100} onChange={v => onChange({ ...data, size: [v, data.size?.[1] || 40] })} step={10} />
      <NumberInput label="Size H" value={data.size?.[1] || 40} onChange={v => onChange({ ...data, size: [data.size?.[0] || 100, v] })} step={10} />

      <ColorInput 
        label="Bg Color" 
        value={data.backgroundColor || [0.5, 0.5, 0.5, 1.0]} 
        onChange={v => onChange({ ...data, backgroundColor: v })} 
        hasAlpha={true} 
      />

      {(data.type === 'Text' || data.type === 'Button' || data.type === 'Input') && (
        <>
          <TextInput label="Text" value={data.text || ''} onChange={v => onChange({ ...data, text: v })} />
          <ColorInput 
            label="Text Color" 
            value={data.textColor || [1, 1, 1]} 
            onChange={v => onChange({ ...data, textColor: v })} 
            hasAlpha={false} 
          />
        </>
      )}

      {data.type === 'Button' && (
        <>
          <SelectInput label="Action" value={data.action || ''} options={['', 'Host', 'Join', 'ToggleActive']} onChange={v => onChange({ ...data, action: v })} />
          {(data.action === 'Join' || data.action === 'ToggleActive') && (
            <TextInput label="Action Target (ID)" value={data.actionTarget || ''} onChange={v => onChange({ ...data, actionTarget: v })} placeholder="e.g. ui_xyz123" />
          )}
        </>
      )}
    </ComponentSection>
  )
}

// ---------------------------------------------------------------------------
// Main Inspector
// ---------------------------------------------------------------------------
export default function Inspector({ entity, entityIndex, onUpdate, assets, refreshAssets, scripts }) {
  if (!entity) {
    return (
      <div className="inspector">
        <div className="panel-header"><span>Inspector</span></div>
        <div className="empty-state">Select an entity to inspect</div>
      </div>
    )
  }

  const updateComponent = (compName, compData) => {
    const updated = JSON.parse(JSON.stringify(entity))
    if (compData === null) {
      delete updated.components[compName]
    } else {
      updated.components[compName] = compData
    }
    onUpdate(updated)
  }

  const addComponent = (compName, defaultData) => {
    const updated = JSON.parse(JSON.stringify(entity))
    updated.components[compName] = defaultData
    onUpdate(updated)
  }

  const existingComponents = Object.keys(entity.components || {})

  return (
    <div className="inspector">
      <div className="panel-header"><span>Inspector</span></div>

      {/* Entity header */}
      <div className="inspector-entity-header">
        <TextInput
          label="Name"
          value={entity.name}
          onChange={v => onUpdate({ ...entity, name: v })}
        />
        <SelectInput
          label="Category"
          value={entity.category || 'Environment'}
          options={CATEGORIES}
          onChange={v => onUpdate({ ...entity, category: v })}
        />
        <TextInput
          label="Editor ID"
          value={entity.editorId}
          onChange={v => onUpdate({ ...entity, editorId: v })}
        />
      </div>

      {/* Component editors in order */}
      {entity.components?.transform && (
        <TransformEditor
          data={entity.components.transform}
          onChange={d => updateComponent('transform', d)}
        />
      )}
      {entity.components?.sprite && (
        <SpriteEditor
          data={entity.components.sprite}
          onChange={d => updateComponent('sprite', d)}
          onRemove={() => updateComponent('sprite', null)}
          assets={assets}
          refreshAssets={refreshAssets}
        />
      )}
      {entity.components?.collider && (
        <ColliderEditor
          data={entity.components.collider}
          onChange={d => updateComponent('collider', d)}
          onRemove={() => updateComponent('collider', null)}
        />
      )}
      {entity.components?.rigidbody && (
        <RigidBodyEditor
          data={entity.components.rigidbody}
          onChange={d => updateComponent('rigidbody', d)}
          onRemove={() => updateComponent('rigidbody', null)}
        />
      )}
      {entity.components?.animator && (
        <AnimatorEditor
          data={entity.components.animator}
          onChange={d => updateComponent('animator', d)}
          onRemove={() => updateComponent('animator', null)}
          assets={assets}
        />
      )}
      {entity.components?.script && (
        <ScriptEditor
          data={entity.components.script}
          onChange={d => updateComponent('script', d)}
          onRemove={() => updateComponent('script', null)}
          scripts={scripts}
        />
      )}
      {entity.components?.ui && (
        <UIEditor
          data={entity.components.ui}
          onChange={d => updateComponent('ui', d)}
          onRemove={() => updateComponent('ui', null)}
        />
      )}

      {/* Add Component dropdown */}
      <AddComponentDropdown
        existingComponents={existingComponents}
        onAdd={addComponent}
        onAddScript={(defaults) => addComponent('script', defaults)}
      />
    </div>
  )
}
