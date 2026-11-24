import { API_URL } from '../api';
import { useEffect, useState, useRef, useCallback } from 'react';
import { io } from 'socket.io-client';

export function useSocket(onSceneUpdate, onJoined) {
  const [connected, setConnected] = useState(false);
  const socketRef = useRef(null);

  const onSceneUpdateRef = useRef(onSceneUpdate);
  const onJoinedRef = useRef(onJoined);
  useEffect(() => {
    onSceneUpdateRef.current = onSceneUpdate;
    onJoinedRef.current = onJoined;
  }, [onSceneUpdate, onJoined]);

  useEffect(() => {
    const socket = io(API_URL || undefined);
    socketRef.current = socket;

    socket.on('connect', () => setConnected(true));
    socket.on('disconnect', () => setConnected(false));
    socket.on('scene_update', (data) => {
      if (onSceneUpdateRef.current) onSceneUpdateRef.current(data);
    });

    socket.on('joined', (info) => onJoinedRef.current?.(info));

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
