import { useState } from 'react'

export default function BottomPanel({ logs = [] }) {
  const [activeTab, setActiveTab] = useState('console')

  return (
    <div className="bottom-panel">
      <div className="tab-bar">
        <button className={`tab-button ${activeTab === 'console' ? 'active' : ''}`} onClick={() => setActiveTab('console')}>Console</button>
        <button className={`tab-button ${activeTab === 'assets' ? 'active' : ''}`} onClick={() => setActiveTab('assets')}>Assets</button>
      </div>
      <div className="tab-content">
        {activeTab === 'console' && (
          <div className="console-logs">
            {logs.length === 0 ? (
              <div className="empty-state" style={{ color: 'var(--text-secondary)' }}>No logs to display</div>
            ) : (
              logs.map((log, i) => (
                <div key={i} className="log-entry" style={{ padding: '4px 8px', borderBottom: '1px solid var(--border)' }}>
                  <span style={{ color: '#888', marginRight: '8px' }}>[{new Date(log.timestamp).toLocaleTimeString()}]</span>
                  <span>{log.message}</span>
                </div>
              ))
            )}
          </div>
        )}
        {activeTab === 'assets' && (
          <div className="empty-state">
            Assets are managed in the cloud server. (Placeholder)
          </div>
        )}
      </div>
    </div>
  )
}
