import { useState, useCallback, useEffect } from 'react'
import Toolbar from './components/Toolbar'
import Hierarchy from './components/Hierarchy'
import Inspector from './components/Inspector'
import Viewport from './components/Viewport'
import ScriptBrowser from './components/ScriptBrowser'
import PrefabBrowser from './components/PrefabBrowser'
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

  // --- API data ---
  const { scenes, loading: scenesLoading, refresh: refreshScenes } = useScenes()
  const { scene, loading: sceneLoading, refresh: refreshScene } = useScene(currentSceneName)
  const { assets } = useAssets()
  const { scripts: scriptsList } = useScripts()

  // Local copy of scene data for editing
  const [sceneData, setSceneData] = useState(null)
  // Sync scene data when loaded from API
  useEffect(() => {
    if (scene) {
      setSceneData(JSON.parse(JSON.stringify(scene)))
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
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: `Entity ${next.scene ? next.scene.entities.length + 1 : 1}`,
        category: 'Environment',
        components: {
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
      
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: `Child Entity`,
        category: 'Environment',
        components: {
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
    if (editingPrefab && sceneData) {
      try {
        setSaveStatus('saving')
        await fetch(`/api/prefabs/${editingPrefab.name}`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(sceneData),
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

    if (!currentSceneName || !sceneData) return
    try {
      setSaveStatus('saving')
      await saveScene(currentSceneName, sceneData)
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
    <div className="editor-layout">
      <Toolbar
        scenes={scenes || []}
        currentScene={currentSceneName}
        onSceneChange={setCurrentSceneName}
        onSave={handleSave}
        onAddEntity={addEntity}
        saveStatus={saveStatus}
        connected={connected}
        refreshScenes={refreshScenes}
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
        />
      </div>

      <div className="viewport-area">
        <Viewport
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
            entity.components.transform.position = [newPos.x, newPos.y]
            updateEntity(index, entity)
          }}
          camera={camera}
          onCameraChange={setCamera}
        />
      </div>

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
              scripts={scriptsList}
            />
          </div>
        ) : (
          <Inspector
            entity={selectedEntity}
            entityIndex={selectedEntityIndex}
            onUpdate={(updated) => updateEntity(selectedEntityIndex, updated)}
            assets={assets}
            scripts={scriptsList}
          />
        )}
      </div>

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
        </div>
        <div className="tab-content">
          {bottomTab === 'scripts' && <ScriptBrowser />}
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
