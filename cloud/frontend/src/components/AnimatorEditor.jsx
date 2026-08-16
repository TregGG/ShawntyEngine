import { useState, useEffect, useCallback } from 'react';

export default function AnimatorEditor({ data, onChange, onRemove, projectId, token, ComponentSection, NumberInput, SelectInput, TextInput, CheckboxInput, objects = [] }) {
  const [clips, setClips] = useState(data._clips || []);
  const [newClipName, setNewClipName] = useState('');
  const [expandedClip, setExpandedClip] = useState(null);

  const apiHeaders = { 'Authorization': `Bearer ${token}` };
  const baseUrl = `http://localhost:3001/api/projects/${projectId}`;

  useEffect(() => {
    if (data.objectId) {
      fetch(`${baseUrl}/animations/${data.objectId}`, { headers: apiHeaders })
        .then(r => r.ok ? r.json() : null)
        .then(d => {
          if (d && d.clips) {
            setClips(d.clips);
          }
        })
        .catch(() => {});
    }
  }, [data.objectId, baseUrl, token]);

  const saveClips = useCallback((updatedClips) => {
    setClips(updatedClips);
    if (data.objectId) {
      fetch(`${baseUrl}/animations/${data.objectId}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', ...apiHeaders },
        body: JSON.stringify({ clips: updatedClips }),
      }).catch(err => console.error('Failed to save animation:', err));
    }
    onChange({ ...data, _clips: updatedClips });
  }, [data, onChange, baseUrl, token]);

  const addClip = () => {
    if (!newClipName.trim()) return;
    const updated = [...clips, { name: newClipName.trim(), frames: [{ frame: 0, duration: 0.1 }] }];
    saveClips(updated);
    setNewClipName('');
    setExpandedClip(updated.length - 1);
  };

  const removeClip = (idx) => {
    const updated = clips.filter((_, i) => i !== idx);
    saveClips(updated);
    if (expandedClip === idx) setExpandedClip(null);
  };

  const updateFrame = (clipIdx, frameIdx, field, value) => {
    const updated = JSON.parse(JSON.stringify(clips));
    updated[clipIdx].frames[frameIdx][field] = value;
    saveClips(updated);
  };

  const addFrame = (clipIdx) => {
    const updated = JSON.parse(JSON.stringify(clips));
    const lastFrame = updated[clipIdx].frames[updated[clipIdx].frames.length - 1];
    updated[clipIdx].frames.push({
      frame: (lastFrame?.frame ?? 0) + 1,
      duration: lastFrame?.duration ?? 0.1
    });
    saveClips(updated);
  };

  const removeFrame = (clipIdx, frameIdx) => {
    const updated = JSON.parse(JSON.stringify(clips));
    updated[clipIdx].frames.splice(frameIdx, 1);
    saveClips(updated);
  };

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
  );
}
