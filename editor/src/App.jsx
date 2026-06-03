import { useState, useCallback, useEffect } from 'react'
import Toolbar from './components/Toolbar'
import Hierarchy from './components/Hierarchy'
import Inspector from './components/Inspector'
import Viewport from './components/Viewport'
import ScriptBrowser from './components/ScriptBrowser'
import PrefabBrowser from './components/PrefabBrowser'
import TextureBrowser from './components/TextureBrowser'
import { useScenes, useScene, saveScene, useAssets, useScripts } from './hooks/useApi'
import { useWebSocket } from './hooks/useWebSocket'

function App() {
  // --- State ---
  const [currentSceneName, setCurrentSceneName] = useState(null)
  
  const [sceneCache, setSceneCache] = useState(null)
  const [selectedEntityIndex, setSelectedEntityIndex] = useState(null)
  const [bottomTab, setBottomTab] = useState('scripts')
  const [saveStatus, setSaveStatus] = useState(null)
  const [editingPrefab, setEditingPrefab] = useState(null) // { name, data }
  const [uiMode, setUiMode] = useState(false) // Toggle to show/edit only UI entities

  // Layout sizes
  const [hierarchyW, setHierarchyW] = useState(240)
  const [inspectorW, setInspectorW] = useState(300)
  const [bottomH, setBottomH] = useState(200)

  // Resizing state
  const [resizing, setResizing] = useState(null) // 'hierarchy', 'inspector', 'bottom'

  useEffect(() => {
    if (!resizing) return

    const handleMouseMove = (e) => {
      if (resizing === 'hierarchy') {
        setHierarchyW(Math.max(150, Math.min(600, e.clientX)))
      } else if (resizing === 'inspector') {
        setInspectorW(Math.max(200, Math.min(800, window.innerWidth - e.clientX)))
      } else if (resizing === 'bottom') {
        setBottomH(Math.max(100, Math.min(600, window.innerHeight - e.clientY)))
      }
    }

    const handleMouseUp = () => setResizing(null)

    window.addEventListener('mousemove', handleMouseMove)
    window.addEventListener('mouseup', handleMouseUp)

    return () => {
      window.removeEventListener('mousemove', handleMouseMove)
      window.removeEventListener('mouseup', handleMouseUp)
    }
  }, [resizing])

  // --- API data ---
  const { scenes, loading: scenesLoading, refresh: refreshScenes } = useScenes()
  const { scene, loading: sceneLoading, refresh: refreshScene } = useScene(currentSceneName)
  const { assets, refresh: refreshAssets } = useAssets()
  const { scripts: scriptsList } = useScripts()

  // Local copy of scene data for editing
  const [sceneData, setSceneData] = useState(null)
  // Helper to convert C++ recursive UI structure into flat Editor entities
  const convertUIToEntities = (uiArray, entitiesOut, relsOut, parentId = null) => {
    if (!uiArray) return
    for (const ui of uiArray) {
      const editorId = ui.editorId || `ui_${Math.random().toString(36).substr(2, 9)}`
      const entity = {
        editorId,
        name: ui.name || 'UIElement',
        category: 'UI',
        components: {
          ui: {
            type: ui.type || 'Panel',
            active: ui.active !== false,
            position: ui.position || [0, 0],
            size: ui.size || [100, 100],
            backgroundColor: ui.backgroundColor || [1, 1, 1, 1],
            text: ui.text || '',
            textColor: ui.textColor || [1, 1, 1],
            action: ui.action || '',
            actionTarget: ui.actionTarget || ''
          }
        }
      }
      entitiesOut.push(entity)

      if (parentId) {
        let rel = relsOut.find(r => r.parent === parentId)
        if (!rel) {
          rel = { parent: parentId, children: [] }
          relsOut.push(rel)
        }
        rel.children.push(editorId)
      }

      if (ui.children && ui.children.length > 0) {
        convertUIToEntities(ui.children, entitiesOut, relsOut, editorId)
      }
    }
  }

  // Sync scene data when loaded from API
  useEffect(() => {
    if (scene) {
      const parsed = JSON.parse(JSON.stringify(scene))
      
      // Convert UI into flat entities
      if (parsed.scene && parsed.scene.ui) {
        if (!parsed.scene.entities) parsed.scene.entities = []
        if (!parsed.scene.relationships) parsed.scene.relationships = []
        convertUIToEntities(parsed.scene.ui, parsed.scene.entities, parsed.scene.relationships)
        delete parsed.scene.ui // Remove it from state, we reconstruct on save
      }

      setSceneData(parsed)
      setSelectedEntityIndex(null)
    }
  }, [scene])

  // Auto-select first scene on load
  useEffect(() => {
    if (scenes && scenes.length > 0 && !currentSceneName) {
      setCurrentSceneName(scenes[0])
    }
  }, [scenes, currentSceneName])

  // --- WebSocket for live updates ---
  const { connected } = useWebSocket(useCallback((msg) => {
    if (msg.type === 'scene_updated' && msg.name === currentSceneName) {
      refreshScene()
    }
    if (msg.type === 'scene_created' || msg.type === 'scene_deleted') {
      refreshScenes()
    }
  }, [currentSceneName, refreshScene, refreshScenes]))

  // --- Scene editing helpers ---
  const entities = editingPrefab ? [] : (sceneData?.scene?.entities || [])
  const selectedEntity = editingPrefab 
    ? null // Handled directly in render
    : (selectedEntityIndex !== null ? entities[selectedEntityIndex] : null)

  const updateEntity = useCallback((index, updatedEntity) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      
      if (editingPrefab) {
        if (index === 0) {
          next.prefab.name = updatedEntity.name
          next.prefab.category = updatedEntity.category
          next.prefab.components = updatedEntity.components
        } else {
          // It's a child of the prefab. We map flat index to children array.
          next.prefab.children[index - 1] = updatedEntity
        }
      } else {
        next.scene.entities[index] = updatedEntity
      }
      return next
    })
  }, [editingPrefab])

  const addEntity = useCallback(() => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const isUI = uiMode
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: isUI ? `UI Element ${next.scene ? next.scene.entities.filter(e=>e.category==='UI').length + 1 : 1}` : `Entity ${next.scene ? next.scene.entities.length + 1 : 1}`,
        category: isUI ? 'UI' : 'Environment',
        components: isUI ? {
          ui: {
            type: 'Panel',
            position: [0, 0],
            size: [100, 40],
            backgroundColor: [0.5, 0.5, 0.5, 1.0]
          }
        } : {
          transform: {
            position: [0.0, 0.0],
            size: [1.0, 1.0],
            rotation: 0
          }
        }
      }
      
      if (editingPrefab) {
        // Can't add root to prefab, treat as child of root
        if (!next.prefab.children) next.prefab.children = []
        next.prefab.children.push(newEntity)
        setSelectedEntityIndex(next.prefab.children.length)
      } else {
        next.scene.entities.push(newEntity)
        setSelectedEntityIndex(next.scene.entities.length - 1)
      }
      return next
    })
  }, [editingPrefab])

  const deleteEntity = useCallback((index) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      
      if (editingPrefab) {
        if (index === 0) {
          console.warn("Cannot delete root prefab entity")
          return prev
        }
        next.prefab.children.splice(index - 1, 1)
      } else {
        const entityId = next.scene.entities[index].editorId
        next.scene.entities.splice(index, 1)
        
        // Remove from relationships
        if (next.scene.relationships) {
          next.scene.relationships = next.scene.relationships.map(rel => ({
            ...rel,
            children: rel.children.filter(id => id !== entityId)
          })).filter(rel => rel.parent !== entityId && rel.children.length > 0)
        }
      }
      return next
    })
    setSelectedEntityIndex(null)
  }, [editingPrefab])

  const addChild = useCallback((parentIndex) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const isUI = uiMode
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: isUI ? `Child UI Element` : `Child Entity`,
        category: isUI ? 'UI' : 'Environment',
        components: isUI ? {
          ui: {
            type: 'Panel',
            position: [0, 0],
            size: [100, 40],
            backgroundColor: [0.5, 0.5, 0.5, 1.0]
          }
        } : {
          transform: { position: [0.0, 0.0], size: [1.0, 1.0], rotation: 0 }
        }
      }

      if (editingPrefab) {
        if (!next.prefab.children) next.prefab.children = []
        next.prefab.children.push(newEntity)
        setSelectedEntityIndex(next.prefab.children.length) // root is 0, children start at 1
      } else {
        const parentEntity = next.scene.entities[parentIndex]
        if (!parentEntity.editorId) {
          parentEntity.editorId = `entity_${Date.now()}_parent`
        }
        
        next.scene.entities.push(newEntity)
        
        if (!next.scene.relationships) next.scene.relationships = []
        let rel = next.scene.relationships.find(r => r.parent === parentEntity.editorId)
        if (!rel) {
          rel = { parent: parentEntity.editorId, children: [] }
          next.scene.relationships.push(rel)
        }
        rel.children.push(newEntity.editorId)
        
        setSelectedEntityIndex(next.scene.entities.length - 1)
      }
      return next
    })
  }, [editingPrefab])

  const parentEntity = useCallback((childIndex, parentIndex) => {
    setSceneData(prev => {
      if (editingPrefab) {
        console.warn("Nesting children inside prefabs is limited to 1 level in the engine")
        return prev
      }

      const next = JSON.parse(JSON.stringify(prev))
      const childEntityId = next.scene.entities[childIndex].editorId
      
      // Remove child from any existing parent
      if (next.scene.relationships) {
        next.scene.relationships = next.scene.relationships.map(rel => ({
          ...rel,
          children: rel.children.filter(id => id !== childEntityId)
        })).filter(rel => rel.children.length > 0)
      } else {
        next.scene.relationships = []
      }

      if (parentIndex !== null && parentIndex !== childIndex) {
        const parentEntityId = next.scene.entities[parentIndex].editorId
        
        let rel = next.scene.relationships.find(r => r.parent === parentEntityId)
        if (!rel) {
          rel = { parent: parentEntityId, children: [] }
          next.scene.relationships.push(rel)
        }
        rel.children.push(childEntityId)
      }

      return next
    })
  }, [editingPrefab])

  const duplicateEntity = useCallback((index) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      
      if (editingPrefab) {
        if (index === 0) return prev // Can't duplicate root easily here
        const clone = JSON.parse(JSON.stringify(next.prefab.children[index - 1]))
        clone.editorId = `${clone.editorId}_copy_${Date.now()}`
        clone.name = `${clone.name} (Copy)`
        next.prefab.children.splice(index, 0, clone)
        setSelectedEntityIndex(index + 1)
      } else {
        const original = next.scene.entities[index]
        const clone = JSON.parse(JSON.stringify(original))
        clone.editorId = `${original.editorId}_copy_${Date.now()}`
        clone.name = `${original.name} (Copy)`
        if (clone.components?.transform?.position) {
          clone.components.transform.position[0] += 1.0
        }
        next.scene.entities.splice(index + 1, 0, clone)
        setSelectedEntityIndex(index + 1)
      }
      return next
    })
  }, [editingPrefab])

  // --- Save ---
  const handleSave = useCallback(async () => {
    let saveData = sceneData
    if (sceneData?.scene) {
      saveData = JSON.parse(JSON.stringify(sceneData))
      const rootUI = []
      const relationships = saveData.scene.relationships || []
      const uiEntities = saveData.scene.entities.filter(e => e.category === 'UI')
      saveData.scene.entities = saveData.scene.entities.filter(e => e.category !== 'UI')
      
      const buildUI = (editorId) => {
        const entity = uiEntities.find(e => e.editorId === editorId)
        if (!entity) return null
        const uiObj = { editorId: entity.editorId, name: entity.name, ...entity.components.ui }
        const rel = relationships.find(r => r.parent === editorId)
        if (rel && rel.children) {
          uiObj.children = rel.children.map(buildUI).filter(Boolean)
        }
        return uiObj
      }
      
      uiEntities.forEach(uiEnt => {
        const isChild = relationships.some(r => r.children.includes(uiEnt.editorId))
        if (!isChild) {
          rootUI.push(buildUI(uiEnt.editorId))
        }
      })
      
      saveData.scene.relationships = relationships.map(r => ({
        ...r,
        children: r.children.filter(cid => !uiEntities.some(u => u.editorId === cid))
      })).filter(r => !uiEntities.some(u => u.editorId === r.parent) && r.children.length > 0)
      
      saveData.scene.ui = rootUI
    }

    if (editingPrefab && saveData) {
      try {
        setSaveStatus('saving')
        await fetch(`/api/prefabs/${editingPrefab.name}`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(saveData),
        })
        setSaveStatus('saved')
        setTimeout(() => setSaveStatus(null), 2000)
      } catch (err) {
        setSaveStatus('error')
        console.error('Prefab save failed:', err)
        setTimeout(() => setSaveStatus(null), 3000)
      }
      return
    }

    if (!currentSceneName || !saveData) return
    try {
      setSaveStatus('saving')
      await saveScene(currentSceneName, saveData)
      setSaveStatus('saved')
      setTimeout(() => setSaveStatus(null), 2000)
    } catch (err) {
      setSaveStatus('error')
      console.error('Save failed:', err)
      setTimeout(() => setSaveStatus(null), 3000)
    }
  }, [currentSceneName, sceneData, editingPrefab])

  // Keyboard shortcuts
  useEffect(() => {
    const handler = (e) => {
      if (e.ctrlKey && e.key === 's') {
        e.preventDefault()
        handleSave()
      }
      if (e.key === 'Delete' && selectedEntityIndex !== null) {
        deleteEntity(selectedEntityIndex)
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [handleSave, selectedEntityIndex, deleteEntity])

  // --- Camera state for viewport ---
  const [camera, setCamera] = useState({ x: 0, y: 0, zoom: 40 })

  const updateCameraFromScene = useCallback(() => {
    if (sceneData?.scene?.camera) {
      const cam = sceneData.scene.camera
      setCamera(prev => ({
        x: cam.position?.[0] || 0,
        y: cam.position?.[1] || 0,
        zoom: (cam.scale || 1) * 40
      }))
    }
  }, [sceneData])

  useEffect(() => {
    updateCameraFromScene()
  }, [sceneData?.scene?.name])

  return (
    <div 
      className={`editor-layout ${resizing ? 'resizing' : ''}`}
      style={{
        '--hierarchy-w': `${hierarchyW}px`,
        '--inspector-w': `${inspectorW}px`,
        '--bottom-h': `${bottomH}px`,
        cursor: resizing === 'hierarchy' || resizing === 'inspector' ? 'col-resize' : resizing === 'bottom' ? 'row-resize' : 'auto'
      }}
    >
      <Toolbar
        scenes={scenes || []}
        currentScene={currentSceneName}
        onSceneChange={setCurrentSceneName}
        onSave={handleSave}
        onAddEntity={addEntity}
        saveStatus={saveStatus}
        connected={connected}
        refreshScenes={refreshScenes}
        uiMode={uiMode}
        setUiMode={setUiMode}
      />

      <div className="panel hierarchy-panel">
        <Hierarchy
          entities={
            editingPrefab && sceneData?.prefab
              ? [
                  {
                    name: sceneData.prefab.name,
                    category: sceneData.prefab.category,
                    editorId: editingPrefab.name,
                    components: sceneData.prefab.components
                  },
                  ...(sceneData.prefab.children || [])
                ]
              : entities
          }
          relationships={
            editingPrefab && sceneData?.prefab
              ? [{
                  parent: editingPrefab.name,
                  children: (sceneData.prefab.children || []).map(c => c.editorId)
                }]
              : sceneData?.scene?.relationships || []
          }
          selectedIndex={selectedEntityIndex}
          onSelect={setSelectedEntityIndex}
          onDelete={deleteEntity}
          onDuplicate={duplicateEntity}
          onAdd={addEntity}
          onAddChild={addChild}
          onParent={parentEntity}
          uiMode={uiMode}
        />
      </div>

      <div className="resizer-vertical" style={{ gridArea: 'hierarchy', justifySelf: 'end' }} onMouseDown={() => setResizing('hierarchy')} />

      <div className="viewport-area">
        <Viewport
          uiMode={uiMode}
          entities={
            editingPrefab && sceneData?.prefab
              ? [
                  {
                    name: sceneData.prefab.name,
                    category: sceneData.prefab.category,
                    editorId: editingPrefab.name,
                    components: sceneData.prefab.components
                  },
                  ...(sceneData.prefab.children || [])
                ]
              : entities
          }
          selectedIndex={selectedEntityIndex}
          onSelect={setSelectedEntityIndex}
          onEntityMove={(index, newPos) => {
            const entityArray = editingPrefab 
              ? [{components: sceneData.prefab.components}, ...(sceneData.prefab.children || [])]
              : entities
              
            const entity = JSON.parse(JSON.stringify(entityArray[index]))
            if (entity.components.ui) {
              entity.components.ui.position = [newPos.x, newPos.y]
            } else if (entity.components.transform) {
              entity.components.transform.position = [newPos.x, newPos.y]
            }
            updateEntity(index, entity)
          }}
          camera={camera}
          onCameraChange={setCamera}
        />
      </div>

      <div className="resizer-vertical" style={{ gridArea: 'inspector', justifySelf: 'start', marginLeft: '-5px' }} onMouseDown={() => setResizing('inspector')} />

      <div className="panel inspector-panel">
        {editingPrefab ? (
          <div className="inspector">
            <div className="panel-header">
              <span>✏️ Editing Prefab: {editingPrefab.name}</span>
              <button className="btn-sm btn-secondary" onClick={() => {
                setEditingPrefab(null)
                if (sceneCache) {
                  setSceneData(sceneCache)
                  setSceneCache(null)
                } else {
                  setSceneData(scene) // Fallback
                }
                setSelectedEntityIndex(null)
              }}>✕ Close</button>
            </div>
            <Inspector
              entity={
                selectedEntityIndex === 0 
                  ? {
                      name: sceneData.prefab.name,
                      category: sceneData.prefab.category || 'Environment',
                      editorId: editingPrefab.name,
                      components: sceneData.prefab.components || {},
                    }
                  : sceneData.prefab.children?.[selectedEntityIndex - 1]
              }
              entityIndex={selectedEntityIndex}
              onUpdate={(updated) => updateEntity(selectedEntityIndex, updated)}
              assets={assets}
              refreshAssets={refreshAssets}
              scripts={scriptsList}
              scenes={scenes || []}
            />
          </div>
        ) : (
          <Inspector
            entity={selectedEntity}
            entityIndex={selectedEntityIndex}
            onUpdate={(updated) => updateEntity(selectedEntityIndex, updated)}
            assets={assets}
            refreshAssets={refreshAssets}
            scripts={scriptsList}
            scenes={scenes || []}
          />
        )}
      </div>

      <div className="resizer-horizontal" style={{ gridArea: 'bottom', alignSelf: 'start', marginTop: '-5px' }} onMouseDown={() => setResizing('bottom')} />

      <div className="bottom-panel">
        <div className="tab-bar">
          <button
            className={bottomTab === 'scripts' ? 'active' : ''}
            onClick={() => setBottomTab('scripts')}
          >
            📜 Scripts
          </button>
          <button
            className={bottomTab === 'prefabs' ? 'active' : ''}
            onClick={() => setBottomTab('prefabs')}
          >
            🧩 Prefabs
          </button>
          <button
            className={bottomTab === 'textures' ? 'active' : ''}
            onClick={() => setBottomTab('textures')}
          >
            🖼️ Textures
          </button>
        </div>
        <div className="tab-content">
          {bottomTab === 'scripts' && <ScriptBrowser />}
          {bottomTab === 'textures' && (
            <TextureBrowser
              assets={assets}
              refreshAssets={refreshAssets}
            />
          )}
          {bottomTab === 'prefabs' && (
            <PrefabBrowser
              onInstantiate={(prefabData) => {
                setSceneData(prev => {
                  const next = JSON.parse(JSON.stringify(prev))
                  const prefab = prefabData.prefab
                  const entity = {
                    editorId: `${prefab.name.toLowerCase()}_${Date.now()}`,
                    name: prefab.name,
                    category: prefab.category || 'Environment',
                    components: JSON.parse(JSON.stringify(prefab.components))
                  }
                  next.scene.entities.push(entity)
                  setSelectedEntityIndex(next.scene.entities.length - 1)
                  return next
                })
              }}
              onEditPrefab={(name, data) => {
                setSceneCache(sceneData) // Save scene data
                setEditingPrefab({ name, data })
                setSceneData(data) // Load prefab into sceneData for editing
                setSelectedEntityIndex(0) // Select root
              }}
            />
          )}
        </div>
      </div>
    </div>
  )
}

export default App
