import { useState, useEffect, useCallback } from 'react';

// ---------------------------------------------------------------------------
// Base fetch helper
// ---------------------------------------------------------------------------
const API_BASE = '/api';

async function apiFetch(url, options = {}) {
  const res = await fetch(`${API_BASE}${url}`, {
    headers: { 'Content-Type': 'application/json', ...options.headers },
    ...options,
  });
  if (!res.ok) throw new Error(`API error: ${res.status} ${res.statusText}`);
  return res.json();
}

// ---------------------------------------------------------------------------
// Generic list / detail hook factories
// ---------------------------------------------------------------------------

/**
 * Hook that fetches a list from the given endpoint.
 * Returns { data, loading, error, refresh }.
 */
function useList(endpoint) {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const refresh = useCallback(() => {
    setLoading(true);
    setError(null);
    apiFetch(endpoint)
      .then((res) => setData(res))
      .catch((err) => setError(err))
      .finally(() => setLoading(false));
  }, [endpoint]);

  useEffect(() => {
    refresh();
  }, [refresh]);

  return { data, loading, error, refresh };
}

/**
 * Hook that fetches a single resource by name.
 * Re-fetches whenever `name` changes.
 */
function useDetail(basePath, name) {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(!!name);
  const [error, setError] = useState(null);

  const refresh = useCallback(() => {
    if (!name) return;
    setLoading(true);
    setError(null);
    apiFetch(`${basePath}/${encodeURIComponent(name)}`)
      .then((res) => setData(res))
      .catch((err) => setError(err))
      .finally(() => setLoading(false));
  }, [basePath, name]);

  useEffect(() => {
    refresh();
  }, [refresh]);

  return { data, loading, error, refresh };
}

// ---------------------------------------------------------------------------
// Scene hooks
// ---------------------------------------------------------------------------

/** GET /api/scenes – list of scene names */
export function useScenes() {
  const { data: scenes, loading, error, refresh } = useList('/scenes');
  return { scenes, loading, error, refresh };
}

/** GET /api/scenes/{name} – single scene JSON */
export function useScene(name) {
  const { data: scene, loading, error, refresh } = useDetail('/scenes', name);
  return { scene, loading, error, refresh };
}

/** PUT /api/scenes/{name} */
export async function saveScene(name, sceneData) {
  return apiFetch(`/scenes/${encodeURIComponent(name)}`, {
    method: 'PUT',
    body: JSON.stringify(sceneData),
  });
}

/** POST /api/scenes */
export async function createScene(name) {
  return apiFetch('/scenes', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });
}

/** DELETE /api/scenes/{name} */
export async function deleteScene(name) {
  return apiFetch(`/scenes/${encodeURIComponent(name)}`, {
    method: 'DELETE',
  });
}

// ---------------------------------------------------------------------------
// Prefab hooks
// ---------------------------------------------------------------------------

/** GET /api/prefabs */
export function usePrefabs() {
  const { data: prefabs, loading, error, refresh } = useList('/prefabs');
  return { prefabs, loading, error, refresh };
}

/** GET /api/prefabs/{name} */
export function usePrefab(name) {
  const { data: prefab, loading, error, refresh } = useDetail('/prefabs', name);
  return { prefab, loading, error, refresh };
}

/** POST /api/prefabs */
export async function createPrefab(name) {
  return apiFetch('/prefabs', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });
}

// ---------------------------------------------------------------------------
// Script hooks
// ---------------------------------------------------------------------------

/** GET /api/scripts – list of script filenames */
export function useScripts() {
  const { data: scripts, loading, error, refresh } = useList('/scripts');
  return { scripts, loading, error, refresh };
}

/** GET /api/scripts/{name} – returns { name, content } */
export function useScript(name) {
  const { data: script, loading, error, refresh } = useDetail('/scripts', name);
  return { script, loading, error, refresh };
}

/** PUT /api/scripts/{name} */
export async function saveScript(name, content) {
  return apiFetch(`/scripts/${encodeURIComponent(name)}`, {
    method: 'PUT',
    body: JSON.stringify({ content }),
  });
}

/** POST /api/scripts */
export async function createScript(name) {
  return apiFetch('/scripts', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });
}

// ---------------------------------------------------------------------------
// Asset hooks
// ---------------------------------------------------------------------------

/** GET /api/assets – returns { objects: [...] } */
export function useAssets() {
  const { data: assets, loading, error, refresh } = useList('/assets');
  return { assets, loading, error, refresh };
}
