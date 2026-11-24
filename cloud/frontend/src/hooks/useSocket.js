import { API_URL } from '../api';
import { useEffect, useState, useRef, useCallback } from 'react';
import { io } from 'socket.io-client';

export function useSocket(onSceneUpdate, { onAddress, onGameLog, onError } = {}) {
  const [connected, setConnected] = useState(false);
  const socketRef = useRef(null);

  const onSceneUpdateRef = useRef(onSceneUpdate);
  const handlersRef = useRef({});
  useEffect(() => {
    onSceneUpdateRef.current = onSceneUpdate;
    handlersRef.current = { onAddress, onGameLog, onError };
  }, [onSceneUpdate, onAddress, onGameLog, onError]);

  useEffect(() => {
    const socket = io(API_URL || undefined);
    socketRef.current = socket;

    socket.on('connect', () => setConnected(true));
    socket.on('disconnect', () => setConnected(false));
    socket.on('scene_update', (data) => {
      if (onSceneUpdateRef.current) onSceneUpdateRef.current(data);
    });

    socket.on('joined', (info) => handlersRef.current.onAddress?.(info.address ?? null));
    socket.on('game_status', (status) => handlersRef.current.onAddress?.(status.address));
    socket.on('game_log', (event) => handlersRef.current.onGameLog?.(event));
    socket.on('error', (err) => handlersRef.current.onError?.(err?.message));

    return () => socket.close();
  }, []);

  const joinProject = useCallback((projectId, token) => {
    socketRef.current?.emit('join_project', projectId, token);
  }, []);

  // Resolves to { ok, error } once the server has accepted or rejected the update
  const sendUpdate = useCallback(async (projectId, update) => {
    if (!socketRef.current?.connected) return { ok: false, error: 'Not connected' };
    try {
      return await socketRef.current.timeout(5000).emitWithAck('editor_update', { projectId, update });
    } catch {
      return { ok: false, error: 'No response from server' };
    }
  }, []);

  return { connected, joinProject, sendUpdate };
}
