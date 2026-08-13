import React, { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';

function getCookie(name) {
  const value = `; ${document.cookie}`;
  const parts = value.split(`; ${name}=`);
  if (parts.length === 2) return parts.pop().split(';').shift();
  return null;
}

const Dashboard = () => {
  const [user, setUser] = useState(null);
  const [projects, setProjects] = useState([]);
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  useEffect(() => {
    const userData = localStorage.getItem('user');
    const token = getCookie('token') || localStorage.getItem('token');
    
    if (userData && token) {
      setUser(JSON.parse(userData));
      fetchProjects(token);
    } else {
      navigate('/login');
    }
  }, [navigate]);

  const fetchProjects = async (token) => {
    try {
      const res = await fetch('http://localhost:3001/api/projects', {
        headers: { 'Authorization': `Bearer ${token}` }
      });
      if (res.ok) {
        const data = await res.json();
        setProjects(data);
      } else {
        if (res.status === 401 || res.status === 403) {
           handleLogout();
        }
      }
    } catch (e) {
      console.error(e);
    } finally {
      setLoading(false);
    }
  };

  const createProject = async () => {
    const name = prompt("Enter project name:", "New Project");
    if (!name) return;
    
    const token = getCookie('token') || localStorage.getItem('token');
    try {
      const res = await fetch('http://localhost:3001/api/projects', {
        method: 'POST',
        headers: { 
            'Authorization': `Bearer ${token}`,
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ name })
      });
      if (res.ok) {
        fetchProjects(token);
      }
    } catch (e) {
      console.error(e);
    }
  };

  const handleLogout = () => {
    localStorage.removeItem('user');
    localStorage.removeItem('token');
    document.cookie = 'token=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;';
    navigate('/login');
  };

  const copyToClipboard = (token) => {
    navigator.clipboard.writeText(token);
    // In a real app, show a toast notification here
  };

  return (
    <div className="dashboard">
      <header className="dashboard-header">
        <div className="dashboard-brand">
          <div className="auth-logo-monogram">SE</div>
          ShawntyEngine Cloud
        </div>
        
        <div className="dashboard-user">
          {user?.isSuper && <span style={{marginRight: '8px', background: 'var(--accent)', color: 'white', padding: '2px 6px', borderRadius: '4px', fontSize: '11px', fontWeight: 'bold'}}>SUPER ADMIN</span>}
          <span>{user?.name || 'User'}</span>
          <button className="btn-secondary btn-sm" onClick={handleLogout}>
            Logout
          </button>
        </div>
      </header>

      <main className="dashboard-content">
        <h1 className="dashboard-title">Your Projects</h1>
        
        <div className="project-grid">
          {projects.map((project) => (
            <div key={project.id} className="project-card">
              <div className="project-card-header">
                <div>
                  <h3 className="project-card-title">{project.name}</h3>
                  <div className="project-card-date">Created {new Date(project.created_at).toLocaleDateString()}</div>
                </div>
              </div>
              
              <div className="token-field">
                <code>{project.server_token}</code>
                <button 
                  onClick={() => copyToClipboard(project.server_token)}
                  title="Copy Server Token"
                >
                  <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                    <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
                    <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
                  </svg>
                </button>
              </div>

              <div className="project-card-actions">
                <Link to={`/editor/${project.id}`} className="btn-primary" style={{ textDecoration: 'none', textAlign: 'center' }}>
                  Open Editor
                </Link>
                <button className="btn-secondary">
                  Settings
                </button>
              </div>
            </div>
          ))}

          <div className="project-card new-project-card" onClick={createProject}>
            <div className="new-project-icon">+</div>
            <div>Create New Project</div>
          </div>
        </div>
      </main>
    </div>
  );
};

export default Dashboard;
