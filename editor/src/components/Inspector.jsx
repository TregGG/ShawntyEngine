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
function SpriteEditor({ data, onChange, onRemove, assets }) {
  const objects = assets?.objects || []
  const [uploading, setUploading] = useState(false)

  const handleUpload = async (e) => {
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
      
      // Auto-assign the last uploaded image to this entity's sprite
      if (lastUploadedName) {
        onChange({ ...data, objectId: lastUploadedName })
      }
    } catch (err) {
      console.error('Upload failed:', err)
    }
    setUploading(false)
    e.target.value = ''
  }

  return (
    <ComponentSection title="Sprite" icon="🖼️" removable onRemove={onRemove}>
      <SelectInput
        label="Object"
        value={data.objectId || ''}
        options={['(none)', ...objects]}
        onChange={v => onChange({ ...data, objectId: v === '(none)' ? undefined : v })}
      />
      <NumberInput label="Frame" value={data.frameIndex || 0} onChange={v => onChange({ ...data, frameIndex: Math.max(0, Math.floor(v)) })} step={1} />
      <SelectInput label="Layer" value={data.layer || 'Foreground'} options={LAYERS} onChange={v => onChange({ ...data, layer: v })} />

      <div className="field-row">
        <label>Upload</label>
        <label className="btn-secondary btn-sm upload-btn">
          {uploading ? '⏳...' : '📁 Upload Image'}
          <input
            type="file"
            accept="image/*,.tga"
            multiple
            onChange={handleUpload}
            style={{ display: 'none' }}
          />
        </label>
      </div>
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
// Main Inspector
// ---------------------------------------------------------------------------
export default function Inspector({ entity, entityIndex, onUpdate, assets, scripts }) {
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

      {/* Add Component dropdown */}
      <AddComponentDropdown
        existingComponents={existingComponents}
        onAdd={addComponent}
        onAddScript={(defaults) => addComponent('script', defaults)}
      />
    </div>
  )
}
