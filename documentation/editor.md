# ShawntyEngine Editor

The ShawntyEngine Editor is a powerful, web-based tool for designing levels, managing assets, and editing scenes in real-time. It separates the heavy UI framework from the C++ engine, relying on a modern **React + Vite** frontend and a **FastAPI (Python)** backend.

---

## 1. Architecture

- **Frontend (React)**: Provides a dynamic, glassmorphic UI with drag-and-drop support, property inspectors, and live asset management.
- **Backend (FastAPI)**: Serves as the bridge between the UI and the engine's local filesystem. It provides REST endpoints for asset CRUD operations and WebSockets for real-time state synchronization.
- **Engine (C++)**: The engine uses `DataDrivenScene` to parse `.scene` JSON files. It can hot-reload scenes when the editor modifies the JSON files on disk.

---

## 2. Setting Up the Editor

### Prerequisites
- **Node.js** (v18+) and `npm`
- **Python** (3.10+)

### Starting the Backend
The backend manages file I/O for the project directory (`test_compiled/`).
```bash
cd editor/backend
python3 -m venv venv
source venv/bin/activate
pip install fastapi uvicorn websockets
python main.py
```
*The backend will run on `http://localhost:8000`.*

### Starting the Frontend
```bash
cd editor
npm install
npm run dev
```
*The frontend will run on `http://localhost:5173`. Open this in your browser.*

---

## 3. Core Features

### Scene Editing
- **Hierarchy Panel**: View and manage all entities in the current `.scene` file.
- **Inspector Panel**: Select an entity to modify its `Transform`, `Sprite`, `RigidBody`, and attached Python `Scripts`. Changes are saved immediately to the JSON file.

### Asset Management
The editor provides full CRUD support for the engine's custom asset formats:
- **Textures (`.tga`, `.png`)**: Upload images directly through the UI.
- **Spritesheets (`.ssheet`)**: Define sprite regions and frames.
- **Animations (`.anim`)**: Create animation clips specifying frame durations.
- **Objects (`.objasset`)**: Link spritesheets and animations together for easy prefabbing.

### Real-time Sync
Because the editor writes directly to the `.scene` JSON and asset files, the C++ engine can instantly reflect changes using hot-reloading when you click "Save" in the editor.
