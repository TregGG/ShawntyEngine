import { useState } from 'react'

const LAYERS = ['Background', 'Foreground', 'Player', 'UI']
const BODY_TYPES = ['Static', 'Dynamic', 'Kinematic']

function ComponentSection({ title, defaultExpanded = true, onRemove, children, icon = '▶' }) {
  const [expanded, setExpanded] = useState(defaultExpanded)
  return (
    <div className={`component-section ${!expanded ? 'collapsed' : ''}`}>
      <div className="component-header" onClick={() => setExpanded(!expanded)}>
        <span className="component-arrow">{expanded ? '▼' : '▶'}</span>
        <span className="component-icon">{icon}</span>
        <span className="component-title">{title}</span>
        <span className="component-header-spacer" />
        {onRemove && (
          <button className="btn-remove-component" onClick={(e) => { e.stopPropagation(); onRemove() }} title="Remove Component">✕</button>
        )}
      </div>
      {expanded && <div className="component-body">{children}</div>}
    </div>
  )
}

function NumberInput({ label, value, onChange, step = 1 }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <input type="number" step={step} value={value ?? 0} onChange={e => onChange(parseFloat(e.target.value) || 0)} />
    </div>
  )
}

function TextInput({ label, value, onChange, placeholder = "" }) {
  return (
    <div className="field-row">
      <label>{label}</label>
      <input type="text" value={value || ''} onChange={e => onChange(e.target.value)} placeholder={placeholder} />
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
      <input type="checkbox" checked={checked || false} onChange={e => onChange(e.target.checked)} />
    </div>
  )
}

export default function Inspector({ entity, onUpdate }) {
  if (!entity) return <div className="inspector"><div className="empty-state">No entity selected</div></div>

  const updateEntity = (updates) => onUpdate({ ...entity, ...updates })
  const updateComponent = (compName, data) => {
    onUpdate({ ...entity, components: { ...entity.components, [compName]: { ...entity.components[compName], ...data } } })
  }
  const removeComponent = (compName) => {
    const newComps = { ...entity.components }
    delete newComps[compName]
    onUpdate({ ...entity, components: newComps })
  }
  const addComponent = (e) => {
    const comp = e.target.value
    if (!comp) return
    const defaults = {
      sprite: { objectId: '', frameIndex: 0, layer: 0 },
      collider: { isTrigger: false, autoBounds: true, localOffset: [0, 0], localSize: [1, 1] },
      rigidbody: { bodyType: 'Dynamic', mass: 1, drag: 0, gravityScale: 1, useGravity: true, elasticity: 0 },
      animator: { objectId: '', defaultClip: '', speed: 1.0, loop: true, _clips: [] },
      script: { path: '', class: '', properties: {} }
    }
    onUpdate({ ...entity, components: { ...entity.components, [comp]: defaults[comp] || {} } })
    e.target.value = ""
  }

  const { components = {} } = entity

  return (
    <div className="inspector">
      <div className="panel-header">Inspector</div>
      <div className="inspector-content">
        <div className="entity-header">
          <TextInput label="Name" value={entity.name} onChange={name => updateEntity({ name })} />
          <SelectInput label="Category" value={entity.category || 'Environment'} options={['Environment', 'Player', 'Enemy', 'Projectile', 'UI']} onChange={c => updateEntity({ category: c })} />
        </div>

        {components.transform && (() => {
          const pos = components.transform.position || [0, 0]
          const size = components.transform.size || [1, 1]
          const localPos = components.transform.localPosition || [0, 0]
          return (
            <ComponentSection title="Transform" icon="📐" defaultExpanded={true}>
              <NumberInput label="Pos X" value={pos[0]} onChange={v => updateComponent('transform', { position: [v, pos[1]] })} />
              <NumberInput label="Pos Y" value={pos[1]} onChange={v => updateComponent('transform', { position: [pos[0], v] })} />
              <NumberInput label="Size X" value={size[0]} step={0.5} onChange={v => updateComponent('transform', { size: [v, size[1]] })} />
              <NumberInput label="Size Y" value={size[1]} step={0.5} onChange={v => updateComponent('transform', { size: [size[0], v] })} />
              <NumberInput label="Rotation" value={components.transform.rotation} step={0.1} onChange={v => updateComponent('transform', { rotation: v })} />
              <NumberInput label="Local X" value={localPos[0]} onChange={v => updateComponent('transform', { localPosition: [v, localPos[1]] })} />
              <NumberInput label="Local Y" value={localPos[1]} onChange={v => updateComponent('transform', { localPosition: [localPos[0], v] })} />
            </ComponentSection>
          )
        })()}

        {components.sprite && (
          <ComponentSection title="Sprite" icon="🖼️" removable onRemove={() => removeComponent('sprite')}>
            <TextInput label="Object ID" value={components.sprite.objectId} onChange={objectId => updateComponent('sprite', { objectId })} />
            <NumberInput label="Frame" value={components.sprite.frameIndex} onChange={frameIndex => updateComponent('sprite', { frameIndex })} />
            <SelectInput label="Layer" value={components.sprite.layer || 'Foreground'} options={LAYERS} onChange={layer => updateComponent('sprite', { layer })} />
            
            {/* Mock Upload UI to match original */}
            <div className="field-row" style={{ marginTop: '8px' }}>
              <label>Append Frame</label>
              <label className="btn-secondary btn-sm upload-btn" style={{ flex: 1, textAlign: 'center', cursor: 'pointer' }} onClick={() => alert("Cloud asset uploading is coming soon in the next backend update!")}>
                ➕ Upload & Append Image
              </label>
            </div>
          </ComponentSection>
        )}

        {components.collider && (() => {
          const offset = components.collider.localOffset || [0, 0]
          const size = components.collider.localSize || [1, 1]
          return (
            <ComponentSection title="Collider" icon="📦" removable onRemove={() => removeComponent('collider')}>
              <CheckboxInput label="Is Trigger" checked={!!components.collider.isTrigger} onChange={v => updateComponent('collider', { isTrigger: v })} />
              <CheckboxInput label="Auto Bounds" checked={!!components.collider.autoBounds} onChange={v => updateComponent('collider', { autoBounds: v })} />
              {!components.collider.autoBounds && (
                <>
                  <NumberInput label="Offset X" value={offset[0]} onChange={v => updateComponent('collider', { localOffset: [v, offset[1]] })} />
                  <NumberInput label="Offset Y" value={offset[1]} onChange={v => updateComponent('collider', { localOffset: [offset[0], v] })} />
                  <NumberInput label="Size X" value={size[0]} onChange={v => updateComponent('collider', { localSize: [v, size[1]] })} />
                  <NumberInput label="Size Y" value={size[1]} onChange={v => updateComponent('collider', { localSize: [size[0], v] })} />
                </>
              )}
            </ComponentSection>
          )
        })()}

        {components.rigidbody && (
          <ComponentSection title="RigidBody" icon="⚙️" removable onRemove={() => removeComponent('rigidbody')}>
            <SelectInput label="Body Type" value={components.rigidbody.bodyType || 'Dynamic'} options={BODY_TYPES} onChange={v => updateComponent('rigidbody', { bodyType: v })} />
            <NumberInput label="Mass" value={components.rigidbody.mass} onChange={mass => updateComponent('rigidbody', { mass })} step={0.1} />
            <NumberInput label="Drag" value={components.rigidbody.drag} onChange={drag => updateComponent('rigidbody', { drag })} step={0.1} />
            <NumberInput label="Gravity Scale" value={components.rigidbody.gravityScale} onChange={gravityScale => updateComponent('rigidbody', { gravityScale })} step={0.1} />
            <CheckboxInput label="Use Gravity" checked={!!components.rigidbody.useGravity} onChange={v => updateComponent('rigidbody', { useGravity: v })} />
            <NumberInput label="Elasticity" value={components.rigidbody.elasticity} onChange={elasticity => updateComponent('rigidbody', { elasticity })} step={0.1} />
          </ComponentSection>
        )}

        {components.animator && (
          <ComponentSection title="Animator" icon="🎬" removable onRemove={() => removeComponent('animator')}>
            <TextInput label="Object ID" value={components.animator.objectId} onChange={objectId => updateComponent('animator', { objectId })} />
            <TextInput label="Default Clip" value={components.animator.defaultClip} onChange={defaultClip => updateComponent('animator', { defaultClip })} />
            <NumberInput label="Speed" value={components.animator.speed ?? 1.0} onChange={speed => updateComponent('animator', { speed })} step={0.1} />
            <CheckboxInput label="Loop" checked={components.animator.loop ?? true} onChange={loop => updateComponent('animator', { loop })} />
          </ComponentSection>
        )}

        {components.script && (
          <ComponentSection title="Script" icon="🐍" removable onRemove={() => removeComponent('script')}>
            <TextInput label="Path" value={components.script.path} onChange={path => updateComponent('script', { path })} />
            <TextInput label="Class" value={components.script.class} onChange={c => updateComponent('script', { class: c })} />
          </ComponentSection>
        )}

        <div className="add-component-container" style={{ padding: '16px' }}>
          <select onChange={addComponent} value="" style={{ width: '100%', background: 'var(--bg-input)', color: 'var(--text-primary)', border: '1px solid var(--border)', borderRadius: '4px', padding: '4px 8px' }}>
            <option value="" disabled>+ Add Component</option>
            {!components.sprite && <option value="sprite">Sprite</option>}
            {!components.collider && <option value="collider">Collider</option>}
            {!components.rigidbody && <option value="rigidbody">RigidBody</option>}
            {!components.animator && <option value="animator">Animator</option>}
            {!components.script && <option value="script">Script</option>}
          </select>
        </div>
      </div>
    </div>
  )
}
