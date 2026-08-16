import { useState, useEffect } from 'react';

const parseSpritesheet = (content) => {
  let texture = '';
  const frames = [];
  if (!content) return { texture, frames };
  for (const line of content.split('\n')) {
    const trimmed = line.trim();
    if (trimmed.startsWith('texture:')) {
      texture = trimmed.split(':', 2)[1].trim();
    } else if (trimmed.includes(':') && trimmed.includes('=')) {
      const parts = trimmed.split(':', 2);
      const idx = parseInt(parts[0].trim());
      const frameData = {};
      for (const p of parts[1].trim().split(/\s+/)) {
        if (p.includes('=')) {
          const [k, v] = p.split('=', 2);
          frameData[k] = parseInt(v);
        }
      }
      frames.push({ index: idx, ...frameData });
    }
  }
  return { texture, frames };
};

export default function SpriteEditor({ data, onChange, onRemove, projectId, token, ComponentSection, NumberInput, SelectInput, TextInput, LAYERS, refreshAssets, objects = [] }) {
  const [uploading, setUploading] = useState(false);
  const [spritesheet, setSpritesheet] = useState(null);
  const [showCreateObject, setShowCreateObject] = useState(false);
  const [newObjectName, setNewObjectName] = useState('');
  const [sliceW, setSliceW] = useState(32);
  const [sliceH, setSliceH] = useState(32);
  const [sliceCols, setSliceCols] = useState(1);
  const [sliceRows, setSliceRows] = useState(1);
  const [showSliceForm, setShowSliceForm] = useState(false);

  const apiHeaders = { 'Authorization': `Bearer ${token}` };
  const baseUrl = `http://localhost:3001/api/projects/${projectId}`;

  useEffect(() => {
    if (!data.objectId || data.objectId === '(none)') {
      setSpritesheet(null);
      return;
    }
    
    fetch(`${baseUrl}/objects/${data.objectId}`, { headers: apiHeaders })
      .then(res => res.ok ? res.json() : null)
      .then(objData => {
        if (objData && objData.spritesheet) {
          fetch(`${baseUrl}/spritesheets/${objData.spritesheet}`, { headers: apiHeaders })
            .then(res => res.ok ? res.json() : null)
            .then(sheetData => {
              if (sheetData && sheetData.content) {
                setSpritesheet(parseSpritesheet(sheetData.content));
              }
            });
        }
      })
      .catch(err => console.error('Failed to load spritesheet:', err));
  }, [data.objectId, baseUrl, token]);

  const handleAppendFrame = async (e) => {
    const files = e.target.files;
    if (!files || files.length === 0 || !data.objectId) return;

    setUploading(true);
    try {
      const objRes = await fetch(`${baseUrl}/objects/${data.objectId}`, { headers: apiHeaders });
      if (!objRes.ok) throw new Error("Failed to load object details");
      const objData = await objRes.json();
      const sheetName = objData.spritesheet;

      for (const file of files) {
        const formData = new FormData();
        formData.append('file', file);
        const appendRes = await fetch(`${baseUrl}/spritesheets/${sheetName}/append`, {
          method: 'POST',
          headers: { 'Authorization': `Bearer ${token}` },
          body: formData
        });
        if (appendRes.ok) {
          const appendData = await appendRes.json();
          onChange({ ...data, frameIndex: appendData.frameIndex });
        }
      }
      
      const sheetRes = await fetch(`${baseUrl}/spritesheets/${sheetName}`, { headers: apiHeaders });
      if (sheetRes.ok) {
        const sheetData = await sheetRes.json();
        setSpritesheet(parseSpritesheet(sheetData.content));
      }
    } catch (err) {
      console.error('Frame append failed:', err);
    }
    setUploading(false);
    e.target.value = '';
  };

  const handleCreateNewObjectUpload = async (e) => {
    const files = e.target.files;
    if (!files || files.length === 0) return;

    setUploading(true);
    try {
      let lastUploadedName = null;
      for (const file of files) {
        const formData = new FormData();
        formData.append('file', file);
        const res = await fetch(`${baseUrl}/assets/upload`, { 
          method: 'POST', 
          headers: { 'Authorization': `Bearer ${token}` },
          body: formData 
        });
        if (res.ok) {
          const resData = await res.json();
          lastUploadedName = resData.name;
          
          await fetch(`${baseUrl}/spritesheets`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', ...apiHeaders },
            body: JSON.stringify({ name: resData.name, texture: resData.name, width: 32, height: 32, cols: 1, rows: 1 })
          });
          
          await fetch(`${baseUrl}/objects`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', ...apiHeaders },
            body: JSON.stringify({ name: resData.name, spritesheet: resData.name, animations: resData.name })
          });
        }
      }
      
      if (refreshAssets) refreshAssets();
      
      if (lastUploadedName) {
        onChange({ ...data, objectId: lastUploadedName, frameIndex: 0 });
      }
    } catch (err) {
      console.error('New object upload failed:', err);
    }
    setUploading(false);
    e.target.value = '';
  };

  const handleCreateEmptyObject = async () => {
    if (!newObjectName.trim()) return;
    const objName = newObjectName.trim();
    try {
      await fetch(`${baseUrl}/spritesheets`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', ...apiHeaders },
        body: JSON.stringify({ name: objName, texture: objName, width: 32, height: 32, cols: 1, rows: 1 })
      });
      
      await fetch(`${baseUrl}/objects`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', ...apiHeaders },
        body: JSON.stringify({ name: objName, spritesheet: objName, animations: objName })
      });

      await fetch(`${baseUrl}/animations`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', ...apiHeaders },
        body: JSON.stringify({ name: objName, clips: [{ name: "idle", frames: [{ frame: 0, duration: 1.0 }] }] })
      });

      if (refreshAssets) refreshAssets();
      onChange({ ...data, objectId: objName, frameIndex: 0 });
      setNewObjectName('');
      setShowCreateObject(false);
    } catch (err) {
      console.error('Failed to create empty object:', err);
    }
  };

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
                    if (!data.objectId || !spritesheet) return;
                    try {
                      const objRes = await fetch(`${baseUrl}/objects/${data.objectId}`, { headers: apiHeaders });
                      if (objRes.ok) {
                        const objData = await objRes.json();
                        await fetch(`${baseUrl}/spritesheets`, {
                          method: 'POST',
                          headers: { 'Content-Type': 'application/json', ...apiHeaders },
                          body: JSON.stringify({
                            name: objData.spritesheet,
                            texture: spritesheet.texture || objData.spritesheet,
                            width: sliceW,
                            height: sliceH,
                            cols: sliceCols,
                            rows: sliceRows
                          })
                        });
                        const sheetRes = await fetch(`${baseUrl}/spritesheets/${objData.spritesheet}`, { headers: apiHeaders });
                        if (sheetRes.ok) {
                          const sheetData = await sheetRes.json();
                          setSpritesheet(parseSpritesheet(sheetData.content));
                        }
                        onChange({ ...data, frameIndex: 0 });
                      }
                    } catch (err) {
                      console.error('Slicing failed:', err);
                    }
                    setShowSliceForm(false);
                  }}
                  style={{ width: '100%', marginTop: '6px' }}
                >
                  Apply Grid Slice
                </button>
              </div>
            )}
          </div>

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
                        backgroundImage: `url(${baseUrl}/textures/${spritesheet.texture}/image?token=${token}&v=${spritesheet.frames.length})`,
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
                          const objRes = await fetch(`${baseUrl}/objects/${data.objectId}`, { headers: apiHeaders });
                          if (objRes.ok) {
                            const objData = await objRes.json();
                            const sheetName = objData.spritesheet;
                            const remaining = spritesheet.frames.filter(frame => frame.index !== f.index);
                            const newLines = [`texture:${spritesheet.texture}`];
                            remaining.forEach((r, newIdx) => {
                              newLines.push(`${newIdx}: x=${r.x} y=${r.y} w=${r.w} h=${r.h}`);
                            });
                            const newContent = newLines.join('\n') + '\n';
                            const updateRes = await fetch(`${baseUrl}/spritesheets/${sheetName}`, {
                              method: 'PUT',
                              headers: { 'Content-Type': 'application/json', ...apiHeaders },
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
                        position: 'absolute',
                        top: '-8px',
                        right: '-8px'
                      }}
                    >
                      ✕
                    </button>
                  </div>
                )
              })}
            </div>
          ) : (
            <div className="empty-state" style={{ padding: '8px 0' }}>No frames defined.</div>
          )}

          <div className="field-row" style={{ marginTop: '8px' }}>
            <label>Append Frame</label>
            <label className="btn-secondary btn-sm upload-btn" style={{ flex: 1, textAlign: 'center' }}>
              {uploading ? '⏳...' : '➕ Upload Image'}
              <input type="file" accept="image/*" onChange={handleAppendFrame} style={{ display: 'none' }} />
            </label>
          </div>
        </>
      )}
    </ComponentSection>
  );
}
