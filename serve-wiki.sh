#!/bin/bash

# LatticeDB Wiki Server
# Simple script to serve the wiki page locally

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Banner
echo -e "${BLUE}"
echo "╔═══════════════════════════════════════════════╗"
echo "║                                               ║"
echo "║   🔷 LatticeDB Wiki Server                   ║"
echo "║   Next-Gen RDBMS Documentation               ║"
echo "║                                               ║"
echo "╚═══════════════════════════════════════════════╝"
echo -e "${NC}"

# Check if index.html exists
if [ ! -f "index.html" ]; then
    echo -e "${RED}✗ Error: index.html not found in current directory${NC}"
    echo "Please run this script from the LatticeDB root directory"
    exit 1
fi

# Default port
PORT=8000

# Check if port is provided as argument
if [ ! -z "$1" ]; then
    PORT=$1
fi

echo -e "${YELLOW}Starting wiki server...${NC}"
echo ""

# Try different methods to serve the wiki
serve_wiki() {
    # Method 1: Python 3
    if command -v python3 &> /dev/null; then
        echo -e "${GREEN}✓ Using Python 3 HTTP server${NC}"
        echo -e "${BLUE}➜ Wiki URL: http://localhost:$PORT${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
        echo ""
        python3 -m http.server $PORT
        return 0
    fi
    
    # Method 2: Python 2
    if command -v python &> /dev/null; then
        echo -e "${GREEN}✓ Using Python 2 HTTP server${NC}"
        echo -e "${BLUE}➜ Wiki URL: http://localhost:$PORT${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
        echo ""
        python -m SimpleHTTPServer $PORT
        return 0
    fi
    
    # Method 3: PHP
    if command -v php &> /dev/null; then
        echo -e "${GREEN}✓ Using PHP built-in server${NC}"
        echo -e "${BLUE}➜ Wiki URL: http://localhost:$PORT${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
        echo ""
        php -S localhost:$PORT
        return 0
    fi
    
    # Method 4: Node.js http-server
    if command -v npx &> /dev/null; then
        echo -e "${GREEN}✓ Using Node.js http-server${NC}"
        echo -e "${BLUE}➜ Wiki URL: http://localhost:$PORT${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
        echo ""
        npx http-server -p $PORT
        return 0
    fi
    
    # Method 5: Ruby
    if command -v ruby &> /dev/null; then
        echo -e "${GREEN}✓ Using Ruby WEBrick server${NC}"
        echo -e "${BLUE}➜ Wiki URL: http://localhost:$PORT${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
        echo ""
        ruby -run -ehttpd . -p$PORT
        return 0
    fi
    
    # No suitable method found
    echo -e "${RED}✗ Error: No suitable HTTP server found${NC}"
    echo ""
    echo "Please install one of the following:"
    echo "  - Python 3:  brew install python3 (macOS) or apt install python3 (Linux)"
    echo "  - Python 2:  brew install python@2 (macOS) or apt install python (Linux)"
    echo "  - PHP:       brew install php (macOS) or apt install php (Linux)"
    echo "  - Node.js:   brew install node (macOS) or apt install nodejs (Linux)"
    echo ""
    echo "Alternatively, open index.html directly in your browser (some features may not work)"
    return 1
}

# Trap Ctrl+C and cleanup
trap 'echo ""; echo -e "${YELLOW}Stopping server...${NC}"; exit 0' INT

# Try to open browser automatically
open_browser() {
    sleep 2  # Wait for server to start
    local url="http://localhost:$PORT"
    
    if command -v xdg-open &> /dev/null; then
        xdg-open "$url" &> /dev/null
    elif command -v open &> /dev/null; then
        open "$url" &> /dev/null
    elif command -v start &> /dev/null; then
        start "$url" &> /dev/null
    fi
}

# Start server
if serve_wiki; then
    exit 0
else
    exit 1
fi &

# Try to open browser in background
open_browser &

# Wait for server process
wait
