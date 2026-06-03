"""
ShawntyEngine Editor — FastAPI Backend
Serves REST APIs for scene/prefab/script CRUD and asset listing.
Optionally broadcasts file-change events over WebSocket.
"""

import os
import json
import asyncio
from pathlib import Path
from typing import Optional
import subprocess

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Request, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse, JSONResponse

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
PROJECT_ROOT = os.environ.get(
    "SHAWNTY_PROJECT",
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "test_compiled")),
)

SCENES_DIR = os.path.join(PROJECT_ROOT, "scenes")
PREFABS_DIR = os.path.join(PROJECT_ROOT, "prefabs")
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "scripts")
OBJECTS_DIR = os.path.join(PROJECT_ROOT, "objects")

# Ensure directories exist
for d in [SCENES_DIR, PREFABS_DIR, SCRIPTS_DIR, OBJECTS_DIR]:
    os.makedirs(d, exist_ok=True)

def _send_reload_packet():
    import socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(b"reload", ("127.0.0.1", 9090))
        sock.close()
    except Exception:
        pass

# ---------------------------------------------------------------------------
# App setup
# ---------------------------------------------------------------------------
app = FastAPI(title="ShawntyEngine Editor API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# WebSocket manager — broadcasts file changes to connected editors
# ---------------------------------------------------------------------------
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

manager = ConnectionManager()

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            # Keep connection alive; client can send pings
            data = await ws.receive_text()
            if data == "ping":
                await ws.send_text("pong")
    except WebSocketDisconnect:
        manager.disconnect(ws)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _list_files(directory: str, extension: str) -> list[str]:
    """List filenames (without extension) in a directory."""
    if not os.path.isdir(directory):
        return []
    return sorted(
        Path(f).stem
        for f in os.listdir(directory)
        if f.endswith(extension)
    )

def _list_files_full(directory: str, extension: str) -> list[str]:
    """List filenames (with extension) in a directory."""
    if not os.path.isdir(directory):
        return []
    return sorted(
        f for f in os.listdir(directory)
        if f.endswith(extension)
    )

# ---------------------------------------------------------------------------
# Scene endpoints
# ---------------------------------------------------------------------------
@app.get("/api/scenes")
async def list_scenes():
    return _list_files(SCENES_DIR, ".scene")

@app.get("/api/scenes/{name}")
async def get_scene(name: str):
    filepath = os.path.join(SCENES_DIR, f"{name}.scene")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Scene '{name}' not found")
    with open(filepath, "r") as f:
        return json.load(f)

@app.put("/api/scenes/{name}")
async def update_scene(name: str, request: Request):
    filepath = os.path.join(SCENES_DIR, f"{name}.scene")
    body = await request.json()
    with open(filepath, "w") as f:
        json.dump(body, f, indent=4)
    # Notify connected clients
    await manager.broadcast({"type": "scene_updated", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.post("/api/scenes")
async def create_scene(request: Request):
    body = await request.json()
    name = body.get("name", "new_scene")
    filepath = os.path.join(SCENES_DIR, f"{name}.scene")
    if os.path.exists(filepath):
        raise HTTPException(409, f"Scene '{name}' already exists")
    scene_data = {
        "version": 1,
        "scene": {
            "name": name,
            "camera": {"position": [0.0, 0.0], "scale": 1.0},
            "entities": [],
        },
    }
    with open(filepath, "w") as f:
        json.dump(scene_data, f, indent=4)
    await manager.broadcast({"type": "scene_created", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.delete("/api/scenes/{name}")
async def delete_scene(name: str):
    filepath = os.path.join(SCENES_DIR, f"{name}.scene")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Scene '{name}' not found")
    os.remove(filepath)
    await manager.broadcast({"type": "scene_deleted", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

# ---------------------------------------------------------------------------
# Prefab endpoints
# ---------------------------------------------------------------------------
@app.get("/api/prefabs")
async def list_prefabs():
    return _list_files(PREFABS_DIR, ".prefab")

@app.get("/api/prefabs/{name}")
async def get_prefab(name: str):
    filepath = os.path.join(PREFABS_DIR, f"{name}.prefab")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Prefab '{name}' not found")
    with open(filepath, "r") as f:
        return json.load(f)

@app.put("/api/prefabs/{name}")
async def update_prefab(name: str, request: Request):
    filepath = os.path.join(PREFABS_DIR, f"{name}.prefab")
    body = await request.json()
    with open(filepath, "w") as f:
        json.dump(body, f, indent=4)
    await manager.broadcast({"type": "prefab_updated", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.post("/api/prefabs")
async def create_prefab(request: Request):
    body = await request.json()
    name = body.get("name", "new_prefab")
    filepath = os.path.join(PREFABS_DIR, f"{name}.prefab")
    if os.path.exists(filepath):
        raise HTTPException(409, f"Prefab '{name}' already exists")
    prefab_data = {
        "version": 1,
        "prefab": {
            "name": name,
            "category": "Environment",
            "components": {
                "transform": {"position": [0.0, 0.0], "size": [1.0, 1.0]},
            },
        },
    }
    with open(filepath, "w") as f:
        json.dump(prefab_data, f, indent=4)
    await manager.broadcast({"type": "prefab_created", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

# ---------------------------------------------------------------------------
# Script endpoints
# ---------------------------------------------------------------------------
SCRIPT_TEMPLATE = '''"""
{name} — Auto-generated script
"""
import shawnty


class {classname}:
    def OnStart(self, entity, input):
        print(f"[{classname}] Started on {{entity.get_name()}}")

    def OnUpdate(self, entity, dt, input):
        pass
'''

@app.get("/api/scripts")
async def list_scripts():
    return _list_files_full(SCRIPTS_DIR, ".py")

import re

@app.get("/api/scripts/{name}/metadata")
async def get_script_metadata(name: str):
    filepath = os.path.join(SCRIPTS_DIR, name)
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Script '{name}' not found")
    with open(filepath, "r") as f:
        content = f.read()
    
    metadata = {}
    # Find patterns like: # @export_enum(scenes) var_name
    pattern = re.compile(r'#\s*@export_enum\(([^)]+)\)\s+([a-zA-Z0-9_]+)')
    for match in pattern.finditer(content):
        options_raw = match.group(1).strip()
        var_name = match.group(2).strip()
        
        # Parse options
        if options_raw == "scenes":
            options = ["scenes"] # Special keyword for frontend
        else:
            options = [opt.strip() for opt in options_raw.split(",")]
            
        metadata[var_name] = {
            "type": "enum",
            "options": options
        }
        
    return metadata

@app.get("/api/scripts/{name}")
async def get_script(name: str):
    filepath = os.path.join(SCRIPTS_DIR, name)
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Script '{name}' not found")
    with open(filepath, "r") as f:
        return {"name": name, "content": f.read()}

@app.put("/api/scripts/{name}")
async def update_script(name: str, request: Request):
    filepath = os.path.join(SCRIPTS_DIR, name)
    body = await request.json()
    content = body.get("content", "")
    with open(filepath, "w") as f:
        f.write(content)
    await manager.broadcast({"type": "script_updated", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.post("/api/scripts")
async def create_script(request: Request):
    body = await request.json()
    name = body.get("name", "new_script")
    if not name.endswith(".py"):
        name += ".py"
    filepath = os.path.join(SCRIPTS_DIR, name)
    if os.path.exists(filepath):
        raise HTTPException(409, f"Script '{name}' already exists")
    # Derive class name from filename
    classname = Path(name).stem.replace("_", " ").title().replace(" ", "")
    content = SCRIPT_TEMPLATE.format(name=name, classname=classname)
    with open(filepath, "w") as f:
        f.write(content)
    await manager.broadcast({"type": "script_created", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

# ---------------------------------------------------------------------------
# Asset management — textures, spritesheets, animations, objects
# ---------------------------------------------------------------------------
TEXTURES_DIR = os.path.join(PROJECT_ROOT, "textures")
SPRITESHEETS_DIR = os.path.join(PROJECT_ROOT, "spritesheets")
ANIMATIONS_DIR = os.path.join(PROJECT_ROOT, "animations")

for d in [TEXTURES_DIR, SPRITESHEETS_DIR, ANIMATIONS_DIR]:
    os.makedirs(d, exist_ok=True)

@app.get("/api/assets")
async def list_assets():
    """List all available assets."""
    objects = _list_files(OBJECTS_DIR, ".objasset")
    textures = sorted(f for f in os.listdir(TEXTURES_DIR) if not f.startswith('.')) if os.path.isdir(TEXTURES_DIR) else []
    spritesheets = _list_files(SPRITESHEETS_DIR, ".ssheet")
    animations = _list_files(ANIMATIONS_DIR, ".anim")
    return {"objects": objects, "textures": textures, "spritesheets": spritesheets, "animations": animations}

# --- Texture upload ---
@app.post("/api/assets/upload")
async def upload_texture(file: UploadFile = File(...)):
    allowed = ('.png', '.tga', '.jpg', '.jpeg', '.bmp')
    ext = Path(file.filename).suffix.lower()
    if ext not in allowed:
        raise HTTPException(400, f"Unsupported format: {ext}. Use: {allowed}")
    
    name = Path(file.filename).stem
    # If already a TGA, save directly
    if ext == '.tga':
        dest = os.path.join(TEXTURES_DIR, f"{name}.tga")
        with open(dest, "wb") as f:
            content = await file.read()
            f.write(content)
    else:
        # Save temp file
        temp_path = os.path.join(TEXTURES_DIR, f"temp_{file.filename}")
        with open(temp_path, "wb") as f:
            content = await file.read()
            f.write(content)
        
        # Convert using ImageMagick 'convert' or fallback to 'magick'
        tga_path = os.path.join(TEXTURES_DIR, f"{name}.tga")
        try:
            result = subprocess.run(
                ["convert", temp_path, tga_path],
                capture_output=True,
                text=True
            )
            if result.returncode != 0:
                # Try 'magick' fallback
                result = subprocess.run(
                    ["magick", temp_path, tga_path],
                    capture_output=True,
                    text=True
                )
                if result.returncode != 0:
                    raise Exception(f"ImageMagick failed: {result.stderr}")
        except Exception as e:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise HTTPException(500, f"Image conversion to TGA failed: {str(e)}")
        finally:
            if os.path.exists(temp_path):
                os.remove(temp_path)
                
    await manager.broadcast({"type": "texture_uploaded", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name, "filename": f"{name}.tga"}

@app.get("/api/textures/{name}/image")
async def get_texture_image(name: str):
    tga_path = os.path.join(TEXTURES_DIR, f"{name}.tga")
    png_path = os.path.join(TEXTURES_DIR, f"{name}.png")
    
    if not os.path.exists(tga_path):
        raise HTTPException(404, f"Texture '{name}' not found")
        
    if not os.path.exists(png_path):
        try:
            subprocess.run(["convert", tga_path, png_path], check=True)
        except Exception:
            try:
                subprocess.run(["magick", tga_path, png_path], check=True)
            except Exception as e:
                raise HTTPException(500, f"Failed to convert TGA to PNG: {str(e)}")
                
    return FileResponse(png_path, media_type="image/png")

@app.delete("/api/textures/{name}")
async def delete_texture(name: str):
    tga_path = os.path.join(TEXTURES_DIR, f"{name}.tga")
    png_path = os.path.join(TEXTURES_DIR, f"{name}.png")
    
    deleted = False
    if os.path.exists(tga_path):
        os.remove(tga_path)
        deleted = True
    if os.path.exists(png_path):
        os.remove(png_path)
        deleted = True
        
    if not deleted:
        raise HTTPException(404, f"Texture '{name}' not found")
        
    await manager.broadcast({"type": "texture_deleted", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

# --- Spritesheet CRUD ---
@app.get("/api/spritesheets")
async def list_spritesheets():
    return _list_files(SPRITESHEETS_DIR, ".ssheet")

@app.get("/api/spritesheets/{name}")
async def get_spritesheet(name: str):
    filepath = os.path.join(SPRITESHEETS_DIR, f"{name}.ssheet")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Spritesheet '{name}' not found")
    with open(filepath, "r") as f:
        return {"name": name, "content": f.read()}

@app.put("/api/spritesheets/{name}")
async def update_spritesheet(name: str, request: Request):
    filepath = os.path.join(SPRITESHEETS_DIR, f"{name}.ssheet")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Spritesheet '{name}' not found")
    body = await request.json()
    with open(filepath, "w") as f:
        f.write(body.get("content", ""))
    await manager.broadcast({"type": "spritesheet_updated", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.post("/api/spritesheets")
async def create_spritesheet(request: Request):
    body = await request.json()
    name = body.get("name", "new_sheet")
    texture = body.get("texture", name)
    width = body.get("width", 32)
    height = body.get("height", 32)
    cols = body.get("cols", 1)
    rows = body.get("rows", 1)
    filepath = os.path.join(SPRITESHEETS_DIR, f"{name}.ssheet")
    lines = [f"texture:{texture}"]
    idx = 0
    for row in range(rows):
        for col in range(cols):
            lines.append(f"{idx}: x={col * width} y={row * height} w={width} h={height}")
            idx += 1
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    _send_reload_packet()
    return {"status": "ok", "name": name, "frameCount": idx}

@app.post("/api/spritesheets/{name}/append")
async def append_spritesheet_frame(name: str, file: UploadFile = File(...)):
    ssheet_path = os.path.join(SPRITESHEETS_DIR, f"{name}.ssheet")
    if not os.path.exists(ssheet_path):
        raise HTTPException(404, f"Spritesheet '{name}' not found")
        
    with open(ssheet_path, "r") as f:
        content = f.read()
        
    lines = content.strip().split("\n")
    texture_name = name
    frames = []
    
    for line in lines:
        line = line.strip()
        if line.startswith("texture:"):
            texture_name = line.split(":", 1)[1].strip()
        elif ":" in line and "=" in line:
            parts = line.split(":", 1)
            idx = int(parts[0].strip())
            frame_data = {}
            for p in parts[1].strip().split():
                if "=" in p:
                    k, v = p.split("=", 1)
                    frame_data[k] = int(v)
            frames.append((idx, frame_data))
            
    next_idx = len(frames)
    next_x = 0
    if frames:
        frames.sort(key=lambda x: x[0])
        last_frame = frames[-1][1]
        next_x = last_frame.get("x", 0) + last_frame.get("w", 32)
        
    temp_path = os.path.join(TEXTURES_DIR, f"temp_append_{file.filename}")
    with open(temp_path, "wb") as f:
        f.write(await file.read())
        
    new_w, new_h = 32, 32
    try:
        ident_res = subprocess.run(
            ["identify", "-format", "%w %h", temp_path],
            capture_output=True,
            text=True,
            check=True
        )
        parts = ident_res.stdout.strip().split()
        if len(parts) == 2:
            new_w, new_h = int(parts[0]), int(parts[1])
    except Exception:
        try:
            ident_res = subprocess.run(
                ["magick", "identify", "-format", "%w %h", temp_path],
                capture_output=True,
                text=True,
                check=True
            )
            parts = ident_res.stdout.strip().split()
            if len(parts) == 2:
                new_w, new_h = int(parts[0]), int(parts[1])
        except Exception:
            pass
            
    tga_path = os.path.join(TEXTURES_DIR, f"{texture_name}.tga")
    png_path = os.path.join(TEXTURES_DIR, f"{texture_name}.png")
    
    try:
        if os.path.exists(tga_path):
            subprocess.run(["convert", tga_path, temp_path, "+append", tga_path], check=True)
        else:
            subprocess.run(["convert", temp_path, tga_path], check=True)
        if os.path.exists(png_path):
            os.remove(png_path)
    except Exception as e:
        try:
            if os.path.exists(tga_path):
                subprocess.run(["magick", tga_path, temp_path, "+append", tga_path], check=True)
            else:
                subprocess.run(["magick", temp_path, tga_path], check=True)
            if os.path.exists(png_path):
                os.remove(png_path)
        except Exception as e2:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise HTTPException(500, f"Failed to append image: {str(e2)}")
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)
            
    new_frame_line = f"{next_idx}: x={next_x} y=0 w={new_w} h={new_h}"
    with open(ssheet_path, "a") as f:
        f.write(new_frame_line + "\n")
        
    await manager.broadcast({"type": "spritesheet_updated", "name": name})
    _send_reload_packet()
    
    return {
        "status": "ok",
        "frameIndex": next_idx,
        "x": next_x,
        "y": 0,
        "w": new_w,
        "h": new_h
    }

# --- Animation CRUD ---
@app.get("/api/animations")
async def list_animations():
    return _list_files(ANIMATIONS_DIR, ".anim")

@app.get("/api/animations/{name}")
async def get_animation(name: str):
    filepath = os.path.join(ANIMATIONS_DIR, f"{name}.anim")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Animation '{name}' not found")
    with open(filepath, "r") as f:
        content = f.read()
    clips = []
    current_clip = None
    for line in content.strip().split("\n"):
        line = line.strip()
        if line.startswith("clip:"):
            if current_clip:
                clips.append(current_clip)
            current_clip = {"name": line.split(":", 1)[1].strip(), "frames": []}
        elif current_clip and ":" in line:
            parts = line.split(":", 1)[1].strip().split()
            frame_data = {}
            for p in parts:
                if "=" in p:
                    k, v = p.split("=", 1)
                    try:
                        frame_data[k] = float(v) if "." in v else int(v)
                    except ValueError:
                        frame_data[k] = v
            current_clip["frames"].append(frame_data)
    if current_clip:
        clips.append(current_clip)
    return {"name": name, "clips": clips}

@app.put("/api/animations/{name}")
async def update_animation(name: str, request: Request):
    filepath = os.path.join(ANIMATIONS_DIR, f"{name}.anim")
    body = await request.json()
    clips = body.get("clips", [])
    lines = []
    for clip in clips:
        lines.append(f"clip:{clip['name']}")
        for i, frame in enumerate(clip.get("frames", [])):
            lines.append(f"{i}: frame={frame.get('frame', i)} duration={frame.get('duration', 0.1)}")
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    await manager.broadcast({"type": "animation_updated", "name": name})
    _send_reload_packet()
    return {"status": "ok", "name": name}

@app.post("/api/animations")
async def create_animation(request: Request):
    body = await request.json()
    name = body.get("name", "new_anim")
    filepath = os.path.join(ANIMATIONS_DIR, f"{name}.anim")
    if os.path.exists(filepath):
        raise HTTPException(409, f"Animation '{name}' already exists")
    clips = body.get("clips", [{"name": "idle", "frames": [{"frame": 0, "duration": 1.0}]}])
    lines = []
    for clip in clips:
        lines.append(f"clip:{clip['name']}")
        for i, frame in enumerate(clip.get("frames", [])):
            lines.append(f"{i}: frame={frame.get('frame', i)} duration={frame.get('duration', 0.1)}")
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    _send_reload_packet()
    return {"status": "ok", "name": name}

# --- Object asset CRUD ---
@app.get("/api/objects/{name}")
async def get_object(name: str):
    filepath = os.path.join(OBJECTS_DIR, f"{name}.objasset")
    if not os.path.exists(filepath):
        raise HTTPException(404, f"Object '{name}' not found")
    with open(filepath, "r") as f:
        content = f.read()
    data = {}
    for line in content.strip().split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            data[k.strip()] = v.strip()
    return {"name": name, **data}

@app.post("/api/objects")
async def create_object(request: Request):
    body = await request.json()
    name = body.get("name", "new_object")
    spritesheet = body.get("spritesheet", name)
    animations = body.get("animations", name)
    filepath = os.path.join(OBJECTS_DIR, f"{name}.objasset")
    with open(filepath, "w") as f:
        f.write(f"spritesheet:{spritesheet}\n")
        f.write(f"animations:{animations}\n")
    _send_reload_packet()
    return {"status": "ok", "name": name}

# ---------------------------------------------------------------------------
# Serve the React frontend (production build)
# ---------------------------------------------------------------------------
FRONTEND_DIR = os.path.join(os.path.dirname(__file__), "..", "dist")

if os.path.isdir(FRONTEND_DIR):
    app.mount("/assets", StaticFiles(directory=os.path.join(FRONTEND_DIR, "assets")), name="static-assets")

    @app.get("/{full_path:path}")
    async def serve_frontend(full_path: str):
        """Serve React SPA — all non-API routes get index.html."""
        file_path = os.path.join(FRONTEND_DIR, full_path)
        if os.path.isfile(file_path):
            return FileResponse(file_path)
        return FileResponse(os.path.join(FRONTEND_DIR, "index.html"))

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import uvicorn
    print(f"ShawntyEngine Editor API")
    print(f"Project root: {PROJECT_ROOT}")
    print(f"Scenes: {SCENES_DIR}")
    print(f"Prefabs: {PREFABS_DIR}")
    print(f"Scripts: {SCRIPTS_DIR}")
    uvicorn.run(app, host="0.0.0.0", port=8000)
