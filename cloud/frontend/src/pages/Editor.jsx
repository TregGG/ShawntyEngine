import { useState, useCallback, useEffect } from 'react'
import { useParams, Link, useNavigate } from 'react-router-dom'

function getCookie(name) {
  const value = `; ${document.cookie}`;
  const parts = value.split(`; ${name}=`);
  if (parts.length === 2) return parts.pop().split(';').shift();
  return null;
}
import Toolbar from '../components/Toolbar'
import Hierarchy from '../components/Hierarchy'
import Inspector from '../components/Inspector'
import Viewport from '../components/Viewport'
import BottomPanel from '../components/BottomPanel'
import ProjectSettingsModal from '../components/ProjectSettingsModal'
import { useSocket } from '../hooks/useSocket'

// Default scene data for a new project
const DEFAULT_SCENE = {
  entities: [
    {
      editorId: 'entity_camera',
      name: 'Camera',
      category: 'Environment',
      components: {
        transform: { position: [0, 0], size: [1, 1], rotation: 0 }
      }
    },
    {
      editorId: 'entity_player',
      name: 'Player',
      category: 'Player',
      components: {
        transform: { position: [2, 0], size: [1, 1], rotation: 0 },
        sprite: { objectId: 'player', frameIndex: 0, layer: 'Player' }
      }
    },
    {
      editorId: 'entity_ground',
      name: 'Ground',
      category: 'Environment',
      components: {
        transform: { position: [0, -3], size: [10, 1], rotation: 0 },
        collider: { autoBounds: true, isTrigger: false }
      }
    }
  ],
  relationships: []
}

// Dummy project name mapping
const PROJECT_NAMES = {
  'proj_123': 'My Awesome Game',
  'proj_456': 'Test Level',
  'proj_789': 'Platformer Demo'
}

function Editor() {
  const { projectId } = useParams()
  const navigate = useNavigate()
  const [serverProjectName, setServerProjectName] = useState('')
  const projectName = serverProjectName || projectId

  // Scene state
  const [scenes, setScenes] = useState(['main'])
  const [currentScene, setCurrentScene] = useState('main')
  const [sceneData, setSceneData] = useState({ scene: JSON.parse(JSON.stringify(DEFAULT_SCENE)) })
  const [selectedEntityIndex, setSelectedEntityIndex] = useState(null)
  const [saveStatus, setSaveStatus] = useState(null)
  const [uiMode, setUiMode] = useState(false)
  const [consoleLogs, setConsoleLogs] = useState([])
  const [showSettings, setShowSettings] = useState(false)

  useEffect(() => {
    const fetchScene = async () => {
       const token = getCookie('token') || localStorage.getItem('token')
       if (!token) {
           navigate('/login')
           return
       }
       try {
         const res = await fetch(`http://localhost:3001/api/projects/${projectId}/scene`, {
            headers: { 'Authorization': `Bearer ${token}` }
         })
         if (res.ok) {
            const data = await res.json()
            setServerProjectName(data.name)
            setSceneData({ scene: data.scene })
         } else if (res.status === 401 || res.status === 403 || res.status === 404) {
            navigate('/dashboard')
         }
       } catch (err) {
         console.error(err)
       }
    }
    fetchScene()
  }, [projectId, navigate])

  // Layout resizing
  const [hierarchyW, setHierarchyW] = useState(240)
  const [inspectorW, setInspectorW] = useState(300)
  const [bottomH, setBottomH] = useState(200)
  const [resizing, setResizing] = useState(null)

  // Camera
  const [camera, setCamera] = useState({ x: 0, y: 0, zoom: 40 })

  // Socket connection
  const { connected, joinProject, sendUpdate } = useSocket((update) => {
    setConsoleLogs(prev => [...prev, { timestamp: Date.now(), message: `Received: ${JSON.stringify(update)}` }])
  })

  // Join project room on connect
  useEffect(() => {
    if (connected && projectId) {
      const token = localStorage.getItem('token') || 'dummy_token'
      joinProject(projectId, token)
      setConsoleLogs(prev => [...prev, { timestamp: Date.now(), message: `Connected to project ${projectId}` }])
    }
  }, [connected, projectId, joinProject])

  // Resize handlers
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

  // Entity helpers
  const entities = sceneData?.scene?.entities || []
  const selectedEntity = selectedEntityIndex !== null ? entities[selectedEntityIndex] : null

  const updateEntity = useCallback((index, updatedEntity) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      next.scene.entities[index] = updatedEntity
      return next
    })
    // Broadcast update via WebSocket
    sendUpdate(projectId, {
      action: 'update_entity',
      index,
      entity: updatedEntity
    })
  }, [projectId, sendUpdate])

  const addEntity = useCallback(() => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const isUI = uiMode
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: isUI ? `UI Element ${next.scene.entities.filter(e => e.category === 'UI').length + 1}` : `Entity ${next.scene.entities.length + 1}`,
        category: isUI ? 'UI' : 'Environment',
        components: isUI ? {
          ui: { type: 'Panel', position: [0, 0], size: [100, 40], backgroundColor: [0.5, 0.5, 0.5, 1.0] }
        } : {
          transform: { position: [0, 0], size: [1, 1], rotation: 0 }
        }
      }
      next.scene.entities.push(newEntity)
      setSelectedEntityIndex(next.scene.entities.length - 1)
      return next
    })
  }, [uiMode])

  const deleteEntity = useCallback((index) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const entityId = next.scene.entities[index].editorId
      next.scene.entities.splice(index, 1)
      if (next.scene.relationships) {
        next.scene.relationships = next.scene.relationships.map(rel => ({
          ...rel,
          children: rel.children.filter(id => id !== entityId)
        })).filter(rel => rel.parent !== entityId && rel.children.length > 0)
      }
      return next
    })
    setSelectedEntityIndex(null)
  }, [])

  const addChild = useCallback((parentIndex) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const isUI = uiMode
      const newEntity = {
        editorId: `entity_${Date.now()}`,
        name: isUI ? 'Child UI Element' : 'Child Entity',
        category: isUI ? 'UI' : 'Environment',
        components: isUI ? {
          ui: { type: 'Panel', position: [0, 0], size: [100, 40], backgroundColor: [0.5, 0.5, 0.5, 1.0] }
        } : {
          transform: { position: [0, 0], size: [1, 1], rotation: 0 }
        }
      }
      const parentEntity = next.scene.entities[parentIndex]
      if (!parentEntity.editorId) parentEntity.editorId = `entity_${Date.now()}_parent`
      next.scene.entities.push(newEntity)
      if (!next.scene.relationships) next.scene.relationships = []
      let rel = next.scene.relationships.find(r => r.parent === parentEntity.editorId)
      if (!rel) {
        rel = { parent: parentEntity.editorId, children: [] }
        next.scene.relationships.push(rel)
      }
      rel.children.push(newEntity.editorId)
      setSelectedEntityIndex(next.scene.entities.length - 1)
      return next
    })
  }, [uiMode])

  const parentEntity = useCallback((childIndex, parentIndex) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const childEntityId = next.scene.entities[childIndex].editorId
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
  }, [])

  const duplicateEntity = useCallback((index) => {
    setSceneData(prev => {
      const next = JSON.parse(JSON.stringify(prev))
      const original = next.scene.entities[index]
      const clone = JSON.parse(JSON.stringify(original))
      clone.editorId = `${original.editorId}_copy_${Date.now()}`
      clone.name = `${original.name} (Copy)`
      if (clone.components?.transform?.position) {
        clone.components.transform.position[0] += 1.0
      }
      next.scene.entities.splice(index + 1, 0, clone)
      setSelectedEntityIndex(index + 1)
      return next
    })
  }, [])

  // Save handler
  const handleSave = useCallback(async () => {
    setSaveStatus('saving')
    sendUpdate(projectId, { action: 'save_scene', scene: sceneData })
    setConsoleLogs(prev => [...prev, { timestamp: Date.now(), message: `Scene ${currentScene} saved to cloud` }])
    setTimeout(() => {
      setSaveStatus('saved')
      setTimeout(() => setSaveStatus(null), 2000)
    }, 500)
  }, [sceneData, projectId, sendUpdate, currentScene])

  // Keyboard shortcuts
  useEffect(() => {
    const handler = (e) => {
      // Ignore if user is typing in an input
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') {
        return;
      }
      
      if (e.ctrlKey && e.key === 's') {
        e.preventDefault()
        handleSave()
      }
      if (e.key === 'Delete' && selectedEntityIndex !== null) {
        deleteEntity(selectedEntityIndex)
      }
      if (e.key.toLowerCase() === 'f' && selectedEntityIndex !== null) {
        const entity = entities[selectedEntityIndex]
        const t = entity?.components?.transform || entity?.components?.ui
        if (t && t.position) {
          let targetZoom = 60 // default zoom for framing
          if (t.size && t.size[0] > 0 && t.size[1] > 0) {
            targetZoom = Math.max(10, Math.min(150, 300 / Math.max(t.size[0], t.size[1])))
          }
          setCamera({
            x: t.position[0],
            y: t.position[1],
            zoom: targetZoom
          })
        }
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [handleSave, selectedEntityIndex, deleteEntity, entities])

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
        projectName={projectName}
        onSave={handleSave}
        onAddEntity={addEntity}
        saveStatus={saveStatus}
        connected={connected}
        uiMode={uiMode}
        setUiMode={setUiMode}
        scenes={scenes}
        currentScene={currentScene}
        onSceneChange={setCurrentScene}
        onCreateScene={name => { setScenes(s => [...s, name]); setCurrentScene(name); }}
        onDeleteScene={() => {
           const next = scenes.filter(s => s !== currentScene);
           setScenes(next);
           setCurrentScene(next[0]);
        }}
        onOpenProjectSettings={() => setShowSettings(true)}
      />

      <ProjectSettingsModal 
        show={showSettings} 
        onClose={() => setShowSettings(false)} 
        prefabs={['player', 'enemy', 'item']} 
      />

      <div className="panel hierarchy-panel">
        <Hierarchy
          entities={entities}
          relationships={sceneData?.scene?.relationships || []}
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
          entities={entities}
          selectedIndex={selectedEntityIndex}
          onSelect={setSelectedEntityIndex}
          onEntityMove={(index, newPos) => {
            const entity = JSON.parse(JSON.stringify(entities[index]))
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
        <Inspector
          entity={selectedEntity}
          entityIndex={selectedEntityIndex}
          onUpdate={(updated) => updateEntity(selectedEntityIndex, updated)}
        />
      </div>

      <div className="resizer-horizontal" style={{ gridArea: 'bottom', alignSelf: 'start', marginTop: '-5px' }} onMouseDown={() => setResizing('bottom')} />

      <BottomPanel logs={consoleLogs} />
    </div>
  )
}

export default Editor
