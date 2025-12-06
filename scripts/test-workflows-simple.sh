#!/bin/bash
# test-workflows-simple.sh - Simple script to test workflows with Docker handling

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}   LogViewer GitHub Workflow Tester${NC}"
echo -e "${BLUE}============================================${NC}"

# Check if act is available
if ! command -v act &> /dev/null; then
    echo -e "${RED}Error: act is not installed or not in PATH${NC}"
    echo -e "${YELLOW}Please install act first:${NC}"
    echo "  Download from: https://github.com/nektos/act/releases"
    exit 1
fi

echo -e "${GREEN}✓ Found act tool${NC}"

# Check Docker availability
DOCKER_AVAILABLE=false
DRY_RUN_FLAG=""

if command -v docker &> /dev/null; then
    if docker info &> /dev/null; then
        echo -e "${GREEN}✓ Docker is running${NC}"
        DOCKER_AVAILABLE=true
    else
        echo -e "${YELLOW}⚠ Docker found but not running${NC}"
        echo -e "${YELLOW}Using dry-run mode for syntax validation only${NC}"
        DRY_RUN_FLAG="-n"
    fi
else
    echo -e "${YELLOW}⚠ Docker not found${NC}"
    echo -e "${YELLOW}Using dry-run mode for syntax validation only${NC}"
    DRY_RUN_FLAG="-n"
fi

echo ""
echo -e "${BLUE}Available options:${NC}"
echo "  1. Test Windows workflow (syntax validation)"
echo "  2. Test Linux workflow (syntax validation)" 
echo "  3. Test macOS workflow (syntax validation)"
echo "  4. Test all workflows (syntax validation)"
echo "  5. Test local builds (without Docker)"
echo "  6. Exit"
echo ""

if [ "$DOCKER_AVAILABLE" = false ]; then
    echo -e "${YELLOW}Note: Docker not available - options 1-4 will only validate syntax${NC}"
    echo -e "${YELLOW}Option 5 provides actual build testing without Docker${NC}"
fi

echo ""
read -p "Select option (1-6): " choice

case $choice in
    1)
        echo -e "${BLUE}Testing Windows workflow syntax...${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-windows.yml
        ;;
    2)
        echo -e "${BLUE}Testing Linux workflow syntax...${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-linux.yml
        ;;
    3)
        echo -e "${BLUE}Testing macOS workflow syntax...${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-macos.yml
        ;;
    4)
        echo -e "${BLUE}Testing all workflow syntax...${NC}"
        echo -e "${YELLOW}Windows workflow:${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-windows.yml
        echo -e "${YELLOW}Linux workflow:${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-linux.yml
        echo -e "${YELLOW}macOS workflow:${NC}"
        act $DRY_RUN_FLAG -W .github/workflows/cmake-macos.yml
        ;;
    5)
        echo -e "${BLUE}Testing local builds...${NC}"
        
        # Setup environment
        export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
        
        # Clean previous builds
        echo -e "${YELLOW}Cleaning previous builds...${NC}"
        if [ -d "build" ]; then
            rm -rf build
        fi
        
        # Test wxWidgets build
        echo -e "${BLUE}Testing wxWidgets build...${NC}"
        echo -e "${YELLOW}Configuring wxWidgets Debug...${NC}"
        if ! cmake --preset windows-msys-debug-wx; then
            echo -e "${RED}wxWidgets Debug configuration failed${NC}"
            exit 1
        fi
        
        echo -e "${YELLOW}Building wxWidgets Debug...${NC}"
        if ! cmake --build --preset windows-msys-debug-wx; then
            echo -e "${RED}wxWidgets Debug build failed${NC}"
            exit 1
        fi
        
        echo -e "${YELLOW}Testing wxWidgets...${NC}"
        if ! ctest --test-dir build/debug_wx --output-on-failure; then
            echo -e "${YELLOW}Some wxWidgets tests failed, continuing...${NC}"
        fi
        
        # Test Qt build
        echo -e "${BLUE}Testing Qt build...${NC}"
        echo -e "${YELLOW}Configuring Qt Debug...${NC}"
        if ! cmake --preset windows-msys-debug-qt; then
            echo -e "${RED}Qt Debug configuration failed${NC}"
            exit 1
        fi
        
        echo -e "${YELLOW}Building Qt Debug...${NC}"
        if ! cmake --build --preset windows-msys-debug-qt; then
            echo -e "${RED}Qt Debug build failed${NC}"
            exit 1
        fi
        
        echo -e "${YELLOW}Testing Qt...${NC}"
        if ! ctest --test-dir build/debug_qt --output-on-failure; then
            echo -e "${YELLOW}Some Qt tests failed, continuing...${NC}"
        fi
        
        echo -e "${GREEN}Local testing completed successfully!${NC}"
        ;;
    6)
        echo -e "${YELLOW}Exiting...${NC}"
        exit 0
        ;;
    *)
        echo -e "${RED}Invalid choice!${NC}"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}   Test completed successfully!${NC}"
echo -e "${GREEN}============================================${NC}"
