import { useState, useEffect, useRef, useCallback } from 'react';

// ---------------------------------------------------------------------------
// Reconnect constants
// ---------------------------------------------------------------------------
const INITIAL_BACKOFF_MS = 1000;
const MAX_BACKOFF_MS = 10000;
const PING_INTERVAL_MS = 30000;

/**
 * useWebSocket – connects to the backend's /ws endpoint for live notifications.
 *
 * @param {function} onMessage  Called with parsed JSON data whenever the server
 *                              sends a message.
 * @returns {{ connected: boolean, send: function }}
 */
export function useWebSocket(onMessage) {
  const [connected, setConnected] = useState(false);

  // Refs survive across renders without causing re-connection loops.
  const wsRef = useRef(null);
  const backoffRef = useRef(INITIAL_BACKOFF_MS);
  const reconnectTimerRef = useRef(null);
  const pingTimerRef = useRef(null);
  const onMessageRef = useRef(onMessage);
  const unmountedRef = useRef(false);

  // Keep the callback ref fresh so we never stale-close over it.
  useEffect(() => {
    onMessageRef.current = onMessage;
  }, [onMessage]);

  // ------------------------------------------------------------------
  // Connection lifecycle
  // ------------------------------------------------------------------
  useEffect(() => {
    unmountedRef.current = false;

    function clearTimers() {
      if (reconnectTimerRef.current) {
        clearTimeout(reconnectTimerRef.current);
        reconnectTimerRef.current = null;
      }
      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
        pingTimerRef.current = null;
      }
    }

    function scheduleReconnect() {
      if (unmountedRef.current) return;
      const delay = backoffRef.current;
      reconnectTimerRef.current = setTimeout(() => {
        if (!unmountedRef.current) connect();
      }, delay);
      // Exponential backoff, capped at MAX_BACKOFF_MS
      backoffRef.current = Math.min(backoffRef.current * 2, MAX_BACKOFF_MS);
    }

    function startPing(ws) {
      pingTimerRef.current = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: 'ping' }));
        }
      }, PING_INTERVAL_MS);
    }

    function connect() {
      clearTimers();

      // Build the WebSocket URL relative to the page origin so Vite's proxy
      // can forward it to the FastAPI backend.
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.host}/ws`;

      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        if (unmountedRef.current) {
          ws.close();
          return;
        }
        setConnected(true);
        backoffRef.current = INITIAL_BACKOFF_MS; // reset backoff on success
        startPing(ws);
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (onMessageRef.current) {
            onMessageRef.current(data);
          }
        } catch {
          // Ignore non-JSON messages (e.g. pong frames)
        }
      };

      ws.onerror = () => {
        // onerror is always followed by onclose – reconnect happens there.
      };

      ws.onclose = () => {
        setConnected(false);
        clearTimers();
        wsRef.current = null;
        scheduleReconnect();
      };
    }

    connect();

    // Cleanup on unmount
    return () => {
      unmountedRef.current = true;
      clearTimers();
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, []); // intentionally runs once on mount

  // ------------------------------------------------------------------
  // Public send helper
  // ------------------------------------------------------------------
  const send = useCallback((data) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(typeof data === 'string' ? data : JSON.stringify(data));
    }
  }, []);

  return { connected, send };
}
