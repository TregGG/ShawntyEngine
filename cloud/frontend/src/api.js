// Empty means same origin: the backend serves the build, and Vite proxies in development.
export const API_URL = import.meta.env.VITE_API_URL || '';

export function getToken() {
  const match = document.cookie.match(/(?:^|; )token=([^;]*)/);
  return match ? match[1] : localStorage.getItem('token');
}
