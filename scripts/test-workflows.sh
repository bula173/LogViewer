#!/bin/bash
# test-workflows.sh - Unix shell script for testing GitHub workflows locally

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_color() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to check prerequisites
check_prerequisites() {
    print_color $BLUE "🔍 Checking prerequisites..."
    
    # Check if act is installed
    if command -v act &> /dev/null; then
        local act_version=$(act --version)
        print_color $GREEN "✅ act is installed: $act_version"
    else
        print_color $RED "❌ act is not installed. Please install it first."
        print_color $YELLOW "   Install with: curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash"
        return 1
    fi
    
    # Check if Docker is available
    if command -v docker &> /dev/null; then
        print_color $GREEN "✅ Docker is available"
        DOCKER_AVAILABLE=true
    else
        print_color $YELLOW "⚠️  Docker is not available. Using dry-run mode only."
        DOCKER_AVAILABLE=false
    fi
    
    return 0
}

# Function to test workflow syntax
test_workflow_syntax() {
    local workflow_file=$1
    print_color $BLUE "📝 Validating workflow syntax: $workflow_file"
    
    if act -n -W "$workflow_file" &>/dev/null; then
        print_color $GREEN "✅ Workflow syntax is valid"
        return 0
    else
        print_color $RED "❌ Workflow syntax validation failed"
        return 1
    fi
}

# Function to test a workflow
test_workflow() {
    local workflow_file=$1
    local job_name=$2
    local matrix_params=$3
    
    local workflow_name=$(basename "$workflow_file")
    print_color $BLUE "🚀 Testing workflow: $workflow_name"
    
    # Build act command
    local act_args=()
    
    if [[ "$DRY_RUN" == "true" ]] || [[ "$DOCKER_AVAILABLE" == "false" ]]; then
        act_args+=("-n")
        print_color $YELLOW "   Running in dry-run mode"
    fi
    
    act_args+=("-W" "$workflow_file")
    
    if [[ -n "$job_name" ]]; then
        act_args+=("-j" "$job_name")
    fi
    
    # Add matrix parameters
    if [[ -n "$matrix_params" ]]; then
        IFS=',' read -ra PARAMS <<< "$matrix_params"
        for param in "${PARAMS[@]}"; do
            act_args+=("--matrix" "$param")
        done
    fi
    
    if [[ "$VERBOSE" == "true" ]]; then
        act_args+=("-v")
    fi
    
    print_color $BLUE "   Command: act ${act_args[*]}"
    
    if act "${act_args[@]}"; then
        print_color $GREEN "✅ Workflow test passed: $workflow_name"
        return 0
    else
        print_color $RED "❌ Workflow test failed: $workflow_name"
        return 1
    fi
}

# Function to test Windows workflow
test_windows_workflow() {
    local workflow_file=".github/workflows/cmake-windows.yml"
    local all_passed=true
    
    print_color $BLUE "\n🪟 Testing Windows Workflows"
    print_color $BLUE "=================================================="
    
    # Test workflow syntax first
    if ! test_workflow_syntax "$workflow_file"; then
        return 1
    fi
    
    local frameworks=("wx" "qt")
    
    for framework in "${frameworks[@]}"; do
        print_color $YELLOW "\n  Testing framework: $framework"
        
        local matrix="gui_framework:$framework,build_type:$BUILD_TYPE"
        
        if ! test_workflow "$workflow_file" "build" "$matrix"; then
            all_passed=false
        fi
    done
    
    [[ "$all_passed" == "true" ]]
}

# Function to test Linux workflow
test_linux_workflow() {
    local workflow_file=".github/workflows/cmake-linux.yml"
    local all_passed=true
    
    print_color $BLUE "\n🐧 Testing Linux Workflows"
    print_color $BLUE "=================================================="
    
    # Test workflow syntax first
    if ! test_workflow_syntax "$workflow_file"; then
        return 1
    fi
    
    local frameworks=("wx" "qt")
    local compilers=("gcc" "clang")
    
    for framework in "${frameworks[@]}"; do
        for compiler in "${compilers[@]}"; do
            print_color $YELLOW "\n  Testing framework: $framework, compiler: $compiler"
            
            local matrix="gui_framework:$framework,build_type:$BUILD_TYPE,compiler:$compiler"
            
            if ! test_workflow "$workflow_file" "build" "$matrix"; then
                all_passed=false
            fi
        done
    done
    
    [[ "$all_passed" == "true" ]]
}

# Function to test macOS workflow
test_macos_workflow() {
    local workflow_file=".github/workflows/cmake-macos.yml"
    local all_passed=true
    
    print_color $BLUE "\n🍎 Testing macOS Workflows"
    print_color $BLUE "=================================================="
    
    # Test workflow syntax first
    if ! test_workflow_syntax "$workflow_file"; then
        return 1
    fi
    
    local frameworks=("wx" "qt")
    
    for framework in "${frameworks[@]}"; do
        print_color $YELLOW "\n  Testing framework: $framework"
        
        local matrix="gui_framework:$framework,build_type:$BUILD_TYPE"
        
        if ! test_workflow "$workflow_file" "build" "$matrix"; then
            all_passed=false
        fi
    done
    
    [[ "$all_passed" == "true" ]]
}

# Function to test specific framework
test_specific_framework() {
    local framework=$1
    local all_passed=true
    
    print_color $BLUE "\n🎯 Testing $framework framework across all platforms"
    print_color $BLUE "============================================================"
    
    local workflows=(
        ".github/workflows/cmake-windows.yml"
        ".github/workflows/cmake-linux.yml"
        ".github/workflows/cmake-macos.yml"
    )
    
    for workflow in "${workflows[@]}"; do
        local platform
        if [[ "$workflow" == *"windows"* ]]; then
            platform="Windows"
        elif [[ "$workflow" == *"linux"* ]]; then
            platform="Linux"
        else
            platform="macOS"
        fi
        
        print_color $YELLOW "\n  Testing $framework on $platform"
        
        if [[ "$workflow" == *"linux"* ]]; then
            # Test with both compilers on Linux
            for compiler in "gcc" "clang"; do
                print_color $BLUE "    Compiler: $compiler"
                local matrix="gui_framework:$framework,build_type:$BUILD_TYPE,compiler:$compiler"
                if ! test_workflow "$workflow" "build" "$matrix"; then
                    all_passed=false
                fi
            done
        else
            local matrix="gui_framework:$framework,build_type:$BUILD_TYPE"
            if ! test_workflow "$workflow" "build" "$matrix"; then
                all_passed=false
            fi
        fi
    done
    
    [[ "$all_passed" == "true" ]]
}

# Function to show usage
show_usage() {
    print_color $BLUE "Usage: $0 [OPTIONS] [TARGET]"
    echo ""
    print_color $YELLOW "TARGETS:"
    echo "  all      - Test all workflows (default)"
    echo "  windows  - Test Windows workflow only"
    echo "  linux    - Test Linux workflow only"
    echo "  macos    - Test macOS workflow only"
    echo "  wx       - Test wxWidgets framework across all platforms"
    echo "  qt       - Test Qt framework across all platforms"
    echo ""
    print_color $YELLOW "OPTIONS:"
    echo "  -d, --dry-run      Run in dry-run mode"
    echo "  -v, --verbose      Enable verbose output"
    echo "  -b, --build-type   Build type (Debug|Release) [default: Debug]"
    echo "  -h, --help         Show this help message"
    echo ""
    print_color $YELLOW "EXAMPLES:"
    echo "  $0                    # Test all workflows"
    echo "  $0 windows           # Test Windows workflow only"
    echo "  $0 wx --verbose      # Test wxWidgets framework with verbose output"
    echo "  $0 --dry-run qt      # Test Qt framework in dry-run mode"
}

# Parse command line arguments
DRY_RUN=false
VERBOSE=false
BUILD_TYPE="Debug"
TARGET="all"

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--dry-run)
            DRY_RUN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -b|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        all|windows|linux|macos|wx|qt)
            TARGET="$1"
            shift
            ;;
        *)
            print_color $RED "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
print_color $BLUE "🧪 LogViewer Workflow Tester"
print_color $BLUE "============================================================"
print_color $YELLOW "Target: $TARGET | Build Type: $BUILD_TYPE | Dry Run: $DRY_RUN"
echo ""

# Check prerequisites
if ! check_prerequisites; then
    exit 1
fi

if [[ "$DOCKER_AVAILABLE" == "false" && "$DRY_RUN" == "false" ]]; then
    print_color $YELLOW "🔄 Enabling dry-run mode due to missing Docker"
    DRY_RUN=true
fi

all_passed=true

case "$TARGET" in
    "all")
        print_color $BLUE "\n🌐 Testing all workflows"
        if ! test_windows_workflow || ! test_linux_workflow || ! test_macos_workflow; then
            all_passed=false
        fi
        ;;
    "windows")
        if ! test_windows_workflow; then
            all_passed=false
        fi
        ;;
    "linux")
        if ! test_linux_workflow; then
            all_passed=false
        fi
        ;;
    "macos")
        if ! test_macos_workflow; then
            all_passed=false
        fi
        ;;
    "wx")
        if ! test_specific_framework "wx"; then
            all_passed=false
        fi
        ;;
    "qt")
        if ! test_specific_framework "qt"; then
            all_passed=false
        fi
        ;;
    *)
        print_color $RED "Unknown target: $TARGET"
        show_usage
        exit 1
        ;;
esac

# Summary
echo ""
print_color $BLUE "============================================================"
if [[ "$all_passed" == "true" ]]; then
    print_color $GREEN "🎉 All workflow tests passed!"
    exit 0
else
    print_color $RED "💥 Some workflow tests failed!"
    exit 1
fi
