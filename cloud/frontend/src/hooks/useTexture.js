import { useEffect, useState } from 'react';

// Images can't send an Authorization header, and tokens don't belong in URLs,
// so fetch the texture and expose it as a blob URL.
export function useTexture(url, token, version) {
  const [src, setSrc] = useState(null);

  useEffect(() => {
    if (!url) return;
    let cancelled = false;
    let objectUrl = null;
    fetch(url, { headers: { 'Authorization': `Bearer ${token}` }, cache: 'no-cache' })
      .then(res => res.ok ? res.blob() : null)
      .then(blob => {
        if (cancelled || !blob) return;
        objectUrl = URL.createObjectURL(blob);
        setSrc(objectUrl);
      })
      .catch(err => console.error('Failed to load texture:', err));
    return () => {
      cancelled = true;
      if (objectUrl) URL.revokeObjectURL(objectUrl);
      setSrc(null);
    };
  }, [url, token, version]);

  return src;
}
