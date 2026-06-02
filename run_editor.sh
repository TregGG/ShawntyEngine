#!/bin/bash

# Exit immediately if any command fails
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$PROJECT_DIR/editor/backend"
FRONTEND_DIR="$PROJECT_DIR/editor"

echo "==========================================="
echo "Starting ShawntyEngine Web Editor Services"
echo "==========================================="

# Kill any existing backend or frontend processes running on ports 8000 or 3000
echo "Cleaning up any existing processes on ports 8000 and 3000..."
fuser -k 8000/tcp 2>/dev/null || true
fuser -k 3000/tcp 2>/dev/null || true

# Start FastAPI backend
echo "Starting backend API on port 8000..."
cd "$BACKEND_DIR"
if [ -d "venv" ]; then
    source venv/bin/activate
fi
python main.py > backend.log 2>&1 &
BACKEND_PID=$!

# Start React Frontend
echo "Starting frontend server on port 3000..."
cd "$FRONTEND_DIR"
npm run dev > frontend.log 2>&1 &
FRONTEND_PID=$!

echo "-------------------------------------------"
echo "Editor services launched in background!"
echo "- Backend API:  http://localhost:8000 (Logs: editor/backend/backend.log)"
echo "- Frontend UI:  http://localhost:3000 (Logs: editor/frontend.log)"
echo "-------------------------------------------"
echo "To stop the editor services, press Ctrl+C or run: kill $BACKEND_PID $FRONTEND_PID"
echo "==========================================="

# Keep script running to allow easy Ctrl+C to terminate both background jobs
cleanup() {
    echo ""
    echo "Shutting down editor services..."
    kill $BACKEND_PID $FRONTEND_PID 2>/dev/null || true
    exit 0
}

trap cleanup SIGINT SIGTERM

# Wait for both processes
wait $BACKEND_PID $FRONTEND_PID
