# ShawntyEngine Web Editor: Architectural Deep Dive & Internals

This document is a comprehensive technical guide explaining the inner workings, development phases, JavaScript/React code execution paths, backend services, and inter-process communication pipelines of the **ShawntyEngine Web Editor**.

---

## 1. Architectural Philosophy & Technology Stack

The ShawntyEngine Editor is designed around a **decoupled architecture**. Instead of compiling a heavy, complex visual GUI library directly into the C++ game executable (which increases compile times, limits layout flexibility, and invites memory unsafety), the editor splits concerns:
*   **The Game Loop (C++)** remains lean, high-performance, and focused on rendering, physics, and scripting.
*   **The Interface (React)** runs in a sandboxed web browser, providing a modern, rich, responsive user interface.
*   **The File Bridge (FastAPI)** runs as a local lightweight backend server, acting as the directory filesystem authority and the messaging mediator.

### Technical Stack breakdown:

```
┌────────────────────────────────────────────────────────┐
│                   WEB BROWSER (UI)                     │
│  ┌───────────────────────┐   ┌──────────────────────┐  │
│  │   React 19 Frontend   ├──>│  HTML5 2D Canvas     │  │
│  │  (Inspector/Hierarchy)│   │  (Viewport Gizmos)   │  │
│  └───────────┬───────────┘   └──────────────────────┘  │
└──────────────┼─────────────────────────────────────────┘
               │ HTTP REST APIs / WebSockets
┌──────────────▼─────────────────────────────────────────┐
│                 FASTAPI BACKEND (PY)                   │
│  ┌──────────────────────────────────────────────────┐  │
│  │ Overwrites files inside test_compiled/            │  │
│  │ Broadcasts WS updates to client connections      │  │
│  └───────────┬──────────────────────────────────────┘  │
└──────────────┼─────────────────────────────────────────┘
               │ UDP Datagram Packet ("reload")
┌──────────────▼─────────────────────────────────────────┐
│                 C++ ENGINE GAME LOOP                   │
│  ┌──────────────────────────────────────────────────┐  │
│  │ Non-blocking socket reads reload trigger         │  │
│  │ Wipes memory & parses updated scene/scripts      │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

#### A. Frontend (React 19 + Vite 8 + Vanilla CSS)
*   **React 19**: Selected for its efficient virtual DOM reconciliation, component lifecycle primitives, and custom state preservation hooks.
*   **Vite 8**: Employs Native ES Modules (ESM) to deliver sub-millisecond Hot Module Replacement (HMR) during frontend development.
*   **HTML5 Canvas API**: Used in the `Viewport.jsx` to render the camera-relative editor grid, axis lines, entity bounds, text rendering, and active selection gizmos.
*   **Vanilla CSS**: Provides full responsive layout styling using CSS variables (`--hierarchy-w`, `--inspector-w`), CSS Grid, and custom resize handles.

#### B. Backend (FastAPI + Uvicorn + Python)
*   **FastAPI**: A high-performance Python web framework built on top of Starlette and Pydantic. It leverages async/await and Python's standard `asyncio` loop to handle high-concurrency connections.
*   **Uvicorn**: An ASGI (Asynchronous Server Gateway Interface) web server implementation used to run the FastAPI app.
*   **WebSockets**: Utilized via FastAPI's `WebSocketDisconnect` and client endpoints to broadcast file change notifications to open editor tabs.

#### C. Interprocess Communication (UDP socket)
*   **Python socket**: Imports standard `socket` module.
*   **UDP Protocol**: Employs connectionless, lightweight UDP packets (`SOCK_DGRAM`) sent to loopback address `127.0.0.1:9090` to signal instant hot-reloading.

---

## 2. The Four Development Phases of the Editor

The editor has evolved through four targeted implementation phases to establish the pipeline we have today:

### Phase 1: Basic Scene Serialization & REST CRUD
*   **What was built**: The basic API routes for saving (`PUT`) and loading (`GET`) scene files.
*   **Tech mechanism**: FastAPI reads and writes raw JSON text files inside the C++ engine's binary folder (`test_compiled/scenes/`).
*   **ECS Integration**: The C++ engine uses `SceneSerializer` (which parses JSON using the header-only `nlohmann::json` library) to map JSON nodes to internal component maps.

### Phase 2: Entity Component Visual Editor
*   **What was built**: The inspector forms and hierarchy tree list on the web.
*   **Tech mechanism**: React form controls (`input type="number"`, `input type="checkbox"`, `select`) bind directly to component fields. Whenever a value changes, React creates a new immutable copy of the scene state, triggers a local re-render, and makes an API save call.
*   **Component Fields**: Added support for standard engine components:
    *   `TransformComponent` (Position, Size, Rotation, Local Position).
    *   `SpriteComponent2D` (Sprite Object ID, Animation Frame Index, Rendering Layer).
    *   `ColliderComponent` (Offsets, Size, Trigger mode, Automatic Bounds calculation).
    *   `RigidBodyComponent` (Body type: Static/Dynamic/Kinematic, Mass, Drag, Gravity settings, Elasticity).

### Phase 3: Python Scripting Engine Integration
*   **What was built**: A custom Script Browser and an inspector panel to bind Python behaviors to entities.
*   **Tech mechanism**: 
    *   FastAPI reads files with `.py` extension inside `scripts/` and serves their contents.
    *   In the React frontend, binding a script to an entity dynamically parses the file path and derives the Python class name (e.g., transforming filename `enemy_controller.py` into class name `EnemyController`).
    *   Users can add custom property key-value pairs in the Inspector. These properties are saved in the `.scene` file and injected directly into the Python class fields in the C++ runtime during initialization.

### Phase 4: Asset Manager Pipelines, Prefabs, & UI Mode
*   **What was built**: A comprehensive asset uploading system, spritesheet creator, animation editor, and a dual UI/Game level editor workspace.
*   **Tech mechanism**:
    *   **Asset Uploading**: Textures are uploaded (`UploadFile`) to FastAPI. The backend saves the image to `textures/` and automatically calls endpoints to instantiate matching `.ssheet` and `.objasset` references so they are instantly usable.
    *   **UI Mode**: Translates C++ recursive UI hierarchy arrays into flat entities. When UI Mode is toggled, only UI components are visible. On save, the editor reconstructs the nested tree.
    *   **Drag-and-Drop Hierarchy**: Employs HTML5 drag-and-drop mechanics to adjust parent-child relationships, updating relationships in real-time.

---

## 3. Web Editor Frontend: In-Depth JS Code Workings

Below is a detailed walkthrough of the React/JS files in `editor/src/`.

### A. Global State & App Controller (`App.jsx`)
`App.jsx` controls the global state, processes layout resizing, parses input from custom APIs, and acts as the state synchronization engine.

#### 1. Panel Layout Resizing State
Layout dimensions are managed using three numerical states and a string indicator:
```javascript
const [hierarchyW, setHierarchyW] = useState(240)
const [inspectorW, setInspectorW] = useState(300)
const [bottomH, setBottomH] = useState(200)
const [resizing, setResizing] = useState(null) // 'hierarchy', 'inspector', or 'bottom'
```
An effect listens for mouse movement globally when `resizing` is active, updating dimensions and applying window caps:
```javascript
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
```

#### 2. UI Tree Flattening and Reconstruction
The C++ engine saves UI elements in a nested hierarchy, whereas the Editor requires flat entities. `App.jsx` handles this transformation.

*   **Flattening (`convertUIToEntities`)**:
    Recursively loops over nested UI arrays. It creates a flat entity structure with a unique ID and maps relationships:
    ```javascript
    const convertUIToEntities = (uiArray, entitiesOut, relsOut, parentId = null) => {
      if (!uiArray) return
      for (const ui of uiArray) {
        const editorId = `ui_${Math.random().toString(36).substr(2, 9)}`
        const entity = {
          editorId,
          name: ui.name || 'UIElement',
          category: 'UI',
          components: {
            ui: {
              type: ui.type || 'Panel',
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
    ```
    This function is triggered inside a `useEffect` whenever the scene payload is loaded from the backend API. It strips `scene.ui` out of state and populates `entities` and `relationships`.

*   **Reconstruction (`handleSave`)**:
    When saving, the flat array is reconstructed back into the C++ engine's nested hierarchy format:
    ```javascript
    const buildUI = (editorId) => {
      const entity = uiEntities.find(e => e.editorId === editorId)
      if (!entity) return null
      const uiObj = { name: entity.name, ...entity.components.ui }
      const rel = relationships.find(r => r.parent === editorId)
      if (rel && rel.children) {
        uiObj.children = rel.children.map(buildUI).filter(Boolean)
      }
      return uiObj
    }
    
    // Find all UI elements that have no parent UI element (root UI nodes)
    uiEntities.forEach(uiEnt => {
      const isChild = relationships.some(r => r.children.includes(uiEnt.editorId))
      if (!isChild) {
        rootUI.push(buildUI(uiEnt.editorId))
      }
    })
    
    // Remove UI elements from normal entities list and build C++ relationships
    saveData.scene.entities = saveData.scene.entities.filter(e => e.category !== 'UI')
    saveData.scene.ui = rootUI
    ```

#### 3. Mutation Callbacks
*   `updateEntity(index, updatedEntity)`: Safely clones state and overwrites the target entity depending on whether the editor is in Prefab Edit mode or normal Scene Edit mode.
*   `addEntity()`: Spawns a default Game Entity (with a `transform` component) or a UI Entity (with a `ui` component) based on `uiMode`.
*   `deleteEntity(index)`: Slices the target entity from the array and deletes all its relationship occurrences.
*   `addChild(parentIndex)`: Adds a new entity and registers it in the child list of the selected parent.
*   `parentEntity(childIndex, parentIndex)`: Modifies the relationship map. It pulls the child out of old relationship arrays and moves its ID into the new parent's list.
*   `duplicateEntity(index)`: Copies the component payload, offsets the coordinates by `1.0` in world space to make it visible, and appends it to the entities array.

---

### B. Tree-View Scene Lister (`Hierarchy.jsx`)
`Hierarchy.jsx` processes the flat arrays of entities and relationships to display a tree structure.

```javascript
// Build a tree from flat entities and relationships
const rootIndices = []
const childrenMap = {} // parentId -> array of child indices

relationships.forEach(rel => {
  childrenMap[rel.parent] = []
  rel.children.forEach(childId => {
    const idx = entities.findIndex(e => e.editorId === childId)
    if (idx !== -1) {
      childrenMap[rel.parent].push(idx)
    }
  })
})

// Find root entities (those not present in any children array)
const allChildIndices = new Set(Object.values(childrenMap).flat())
entities.forEach((e, idx) => {
  if (!allChildIndices.has(idx)) {
    rootIndices.push(idx)
  }
})
```

#### 1. Drag and Drop Nesting
HTML5 drag attributes are mapped to functions to perform re-parenting:
*   `draggable` and `onDragStart` set the `draggedIndex` into local state and the payload.
*   `onDragOver` verifies if the target drop index is a descendant of the dragged element to prevent cyclic loops (`isDescendant` check). If it's safe, the element is highlighted.
*   `onDrop` calls the parent's `onParent(draggedIndex, targetIndex)` which saves the configuration.

#### 2. Category Icons & Component Badges
Visual categories are mapped instantly:
*   `CATEGORY_ICONS`: Environment: 🌳, Player: 🎮, Enemy: 👾, Projectile: 💥, UI: 📱.
*   `COMPONENT_BADGES`: transform: 📐, sprite: 🖼️, collider: 📦, rigidbody: ⚙️, animator: 🎬, script: 🐍.

---

### C. Visual Canvas Grid (`Viewport.jsx`)
The viewport uses an HTML5 Canvas API context (`ctx`) to draw the grid and manipulate coordinates.

#### 1. Space Conversion Mathematics
*   **World to Screen (`worldToScreen`)**:
    Maps real-world physical values to viewport canvas coordinates. Flipped Y-axis maps positive coordinates upwards:
    $$\text{screen.x} = \text{canvas.width}/2 + (\text{world.x} - \text{camera.x}) \times \text{camera.zoom}$$
    $$\text{screen.y} = \text{canvas.height}/2 - (\text{world.y} - \text{camera.y}) \times \text{camera.zoom}$$
*   **Screen to World (`screenToWorld`)**:
    Performs the reverse calculation, converting cursor locations to in-game values:
    $$\text{world.x} = \text{camera.x} + \frac{\text{screen.x} - \text{canvas.width}/2}{\text{camera.zoom}}$$
    $$\text{world.y} = \text{camera.y} - \frac{\text{screen.y} - \text{canvas.height}/2}{\text{camera.zoom}}$$

#### 2. UI Pixel Calculation vs. Physics Bounds
*   **Game objects**: Extents are centered on their coordinates. `worldToScreen(pos.x - w/2, pos.y + h/2)` defines the top-left boundary.
*   **UI objects**: Rendered relative to screen space. They are processed using a Pixel Per Unit constant `UI_PPU = 40` and reference screen layout resolution `1280x720` to render UI bounding boxes correctly relative to world coordinates.
    ```javascript
    const UI_PPU = 40
    const REF_W = 1280
    const REF_H = 720
    const worldTopLeftX = -(REF_W / 2) / UI_PPU + (pos[0] / UI_PPU)
    const worldTopLeftY = (REF_H / 2) / UI_PPU - (pos[1] / UI_PPU)
    ```

#### 3. Interaction Handlers
*   **Zoom**: Intercepts `onWheel` scroll, recalculating zoom within bounds `5` to `200`.
*   **Panning**: Listening to middle mouse click (`e.button === 1`) and dragging shifts camera position states `camera.x` and `camera.y`.
*   **Intersection testing & Drag moving**: On left click, `findEntityAt` computes bounding boxes and performs cursor intersection tests. Dragging selected elements translates screen movement to delta values in world coordinates, updating entity structures.

---

### D. Component Details Form (`Inspector.jsx`)
Displays property forms for selected entities.

*   **Color Conversion**:
    The C++ engine expects floating-point values `[0.0, 1.0]` for colors, whereas HTML `<input type="color">` uses Hex codes. The component maps these back and forth:
    ```javascript
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
    ```
*   **Asset Pipeline Integration**:
    *   **Texture upload**: The Sprite component includes a file dialog. When a file is uploaded, the component sends it to `/api/assets/upload`, receives the filename, and automatically hits `/api/spritesheets` and `/api/objects` to instantly generate spritesheet and prefab definitions.
    *   **Animator Clips**: Includes a frame editor. Users can add animation clips, frames, and edit duration parameters. Saving clips writes updates to the corresponding `.anim` files on disk.
    *   **Properties Binder**: Allows binding key-value arguments to Python scripts.

---

### E. CRUD Hooks (`useApi.js` & `useWebSocket.js`)
*   **`useApi.js`**:
    Provides reusable state hooks (`useScenes`, `useScene`, `useAssets`, `useScripts`) using fetch requests mapping to the backend's `/api` routes.
*   **`useWebSocket.js`**:
    Ensures state synchronization across tabs. It opens a WebSocket connection (`/ws`) and triggers a callback when notifications (`scene_updated`, `scene_created`) arrive. The hook includes automatic reconnection logic using exponential backoff:
    ```javascript
    // Exponential backoff capped at 10 seconds
    backoffRef.current = Math.min(backoffRef.current * 2, MAX_BACKOFF_MS);
    ```
    It also sends a keep-alive ping packet (`{"type": "ping"}`) every 30 seconds to prevent servers/routers from shutting down inactive connections.

---

## 4. Web Editor Backend: In-Depth FastAPI Code Workings

The backend is built in Python (`editor/backend/main.py`).

### A. Environment Path Resolution
The backend automatically resolves the destination binary folder (`test_compiled/`) relative to `SHAWNTY_PROJECT`:
```python
PROJECT_ROOT = os.environ.get(
    "SHAWNTY_PROJECT",
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "test_compiled")),
)
```
Upon startup, the server ensures the directory structure exists:
```python
for d in [SCENES_DIR, PREFABS_DIR, SCRIPTS_DIR, OBJECTS_DIR, TEXTURES_DIR, SPRITESHEETS_DIR, ANIMATIONS_DIR]:
    os.makedirs(d, exist_ok=True)
```

### B. WebSocket Dispatcher
Manages live tab synchronization using connection pools:
```python
class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket):
        self.active.remove(ws)

    async def broadcast(self, message: dict):
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                pass
```

### C. Asset Serialization Formats
The backend converts standard JSON representations to the engine's custom text formats:
*   **Spritesheets (`.ssheet`)**:
    Generates index regions from grid parameters:
    ```
    texture:my_texture_name
    0: x=0 y=0 w=32 h=32
    1: x=32 y=0 w=32 h=32
    ```
*   **Animations (`.anim`)**:
    Parses clips into lists and outputs duration parameters:
    ```
    clip:run
    0: frame=0 duration=0.1
    1: frame=1 duration=0.1
    ```
*   **Object Assets (`.objasset`)**:
    Maps textures to animations:
    ```
    spritesheet:player_sheet
    animations:player_anims
    ```

---

## 5. The Real-Time UDP Live-Sync Pipeline

The instant reloading pipeline handles property updates end-to-end:

```
[User moves entity slider]
            │
            ▼ (React updates state)
[React calls PUT /api/scenes/level_name]
            │
            ▼ (FastAPI saves JSON to disk)
[FastAPI triggers socket reload packet]
            │
            ▼ (UDP packet sent to 127.0.0.1:9090)
[C++ Engine parses packet & triggers Reload()]
            │
            ▼ (Clears ECS & parses fresh level)
[Viewport updates in under 1 millisecond]
```

### 1. UDP packet trigger in FastAPI
Whenever an entity or script is updated, FastAPI writes the file to disk and runs:
```python
def _send_reload_packet():
    import socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(b"reload", ("127.0.0.1", 9090))
        sock.close()
    except Exception:
        pass
```

### 2. POSIX non-blocking receiver in C++
Inside `datadrivenscene.cpp`, a UDP listener is bound to port `9090`. It is configured to run asynchronously without blocking the game loop:
```cpp
void DataDrivenScene::InitUdpListener() {
    m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Set socket to non-blocking
    fcntl(m_Socket, F_SETFL, O_NONBLOCK);
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9090);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    bind(m_Socket, (struct sockaddr*)&addr, sizeof(addr));
}
```

### 3. Engine update check
Every frame, `Update` calls the listener to check for update flags:
```cpp
void DataDrivenScene::PollUdpReload() {
    char buffer[128];
    sockaddr_in sender{};
    socklen_t senderLen = sizeof(sender);
    
    // Because it is non-blocking, recvfrom returns instantly
    int bytesReceived = recvfrom(m_Socket, buffer, sizeof(buffer) - 1, 0, 
                                 (struct sockaddr*)&sender, &senderLen);
                                 
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        if (strcmp(buffer, "reload") == 0) {
            // Wipes entities, clears scripts cache, and reloads level
            Reload();
        }
    }
}
```
This decoupling enables hot-reloading in under 1ms, providing immediate visual feedback during level design.
