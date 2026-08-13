import { useEffect, useState, useRef, useCallback } from 'react';
import { io } from 'socket.io-client';

export function useSocket(onSceneUpdate) {
  const [connected, setConnected] = useState(false);
  const socketRef = useRef(null);

  const onSceneUpdateRef = useRef(onSceneUpdate);
  useEffect(() => {
    onSceneUpdateRef.current = onSceneUpdate;
  }, [onSceneUpdate]);

  useEffect(() => {
    const socket = io('http://localhost:3001');
    socketRef.current = socket;

    socket.on('connect', () => setConnected(true));
    socket.on('disconnect', () => setConnected(false));
    socket.on('scene_update', (data) => {
      if (onSceneUpdateRef.current) onSceneUpdateRef.current(data);
    });

    return () => socket.close();
  }, []);

  const joinProject = useCallback((projectId, token) => {
    socketRef.current?.emit('join_project', projectId, token);
  }, []);

  const sendUpdate = useCallback((projectId, update) => {
    socketRef.current?.emit('editor_update', { projectId, update });
  }, []);

  return { connected, joinProject, sendUpdate };
}
