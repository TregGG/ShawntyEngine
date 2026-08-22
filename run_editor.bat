@echo off
setlocal

set PROJECT_DIR=%~dp0
set BACKEND_DIR=%PROJECT_DIR%editor\backend
set FRONTEND_DIR=%PROJECT_DIR%editor

echo ===========================================
echo Starting ShawntyEngine Web Editor Services
echo ===========================================

echo Starting backend API on port 8000...
cd /d "%BACKEND_DIR%"
if exist venv\Scripts\activate.bat (
    call venv\Scripts\activate.bat
)
start "ShawntyEngine Backend" cmd /c "python main.py > backend.log 2>&1"

echo Starting frontend server on port 3000...
cd /d "%FRONTEND_DIR%"
start "ShawntyEngine Frontend" cmd /c "npm run dev > frontend.log 2>&1"

echo -------------------------------------------
echo Editor services launched in background!
echo - Backend API:  http://localhost:8000 (Logs: editor/backend/backend.log)
echo - Frontend UI:  http://localhost:3000 (Logs: editor/frontend.log)
echo -------------------------------------------
echo Close the newly opened command windows to stop the services.
echo ===========================================

endlocal
