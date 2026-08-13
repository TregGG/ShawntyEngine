import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';

const Login = () => {
  const [isLogin, setIsLogin] = useState(true);
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [username, setUsername] = useState('');
  const navigate = useNavigate();

  const [error, setError] = useState(null);

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError(null);
    try {
      const endpoint = isLogin ? '/api/auth/login' : '/api/auth/register';
      const res = await fetch(`http://localhost:3001${endpoint}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email, password })
      });
      const data = await res.json();
      
      if (!res.ok) {
        setError(data.error || "Authentication failed");
        return;
      }
      
      if (!isLogin) {
        // Auto-login after register
        const loginRes = await fetch('http://localhost:3001/api/auth/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ email, password })
        });
        const loginData = await loginRes.json();
        if (loginRes.ok) {
           document.cookie = `token=${loginData.token}; path=/; max-age=${7 * 24 * 60 * 60}`;
           localStorage.setItem('user', JSON.stringify(loginData.user));
           navigate('/dashboard');
        }
      } else {
        document.cookie = `token=${data.token}; path=/; max-age=${7 * 24 * 60 * 60}`;
        localStorage.setItem('user', JSON.stringify(data.user));
        navigate('/dashboard');
      }
    } catch (err) {
      setError("Network error. Is the backend running?");
    }
  };

  return (
    <div className="auth-page">
      <div className="auth-particles">
        {[...Array(15)].map((_, i) => (
          <div 
            key={i} 
            className="auth-particle"
            style={{
              left: `${Math.random() * 100}%`,
              top: `${Math.random() * 100}%`,
              animationDelay: `${Math.random() * 5}s`,
              animationDuration: `${5 + Math.random() * 5}s`
            }}
          />
        ))}
      </div>
      
      <div className="auth-card">
        <div className="auth-logo">
          <div className="auth-logo-monogram">SE</div>
          <h1>ShawntyEngine Cloud</h1>
        </div>
        
        <form className="auth-form" onSubmit={handleSubmit}>
          {!isLogin && (
            <div className="auth-input-group">
              <label>Username</label>
              <input 
                type="text" 
                placeholder="Enter your username" 
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                required 
              />
            </div>
          )}
          
          <div className="auth-input-group">
            <label>Email / Username</label>
            <input 
              type="text" 
              placeholder="Enter your email or username" 
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              required 
            />
          </div>
          
          <div className="auth-input-group">
            <label>Password</label>
            <input 
              type="password" 
              placeholder="Enter your password" 
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              required 
            />
          </div>
          
          {error && <div style={{ color: 'var(--error)', marginBottom: '10px', fontSize: '13px', textAlign: 'center' }}>{error}</div>}
          
          <button type="submit" className="btn-primary auth-submit">
            {isLogin ? 'Sign In to Cloud' : 'Create Account'}
          </button>
        </form>
        
        <div className="auth-toggle">
          {isLogin ? "Don't have an account?" : "Already have an account?"}
          <button type="button" onClick={() => setIsLogin(!isLogin)}>
            {isLogin ? 'Sign up' : 'Log in'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default Login;
