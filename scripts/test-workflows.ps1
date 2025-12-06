#!/usr/bin/env pwsh
# test-workflows.ps1 - Test GitHub workflows locally for LogViewer

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("all", "windows", "linux", "macos", "wx", "qt")]
    [string]$Target = "all",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",
    
    [Parameter(Mandatory=$false)]
    [switch]$DryRun,
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

# Colors for output
$Green = "`e[32m"
$Red = "`e[31m"
$Yellow = "`e[33m"
$Blue = "`e[34m"
$Reset = "`e[0m"

function Write-ColorOutput {
    param($Message, $Color)
    Write-Host "$Color$Message$Reset"
}

function Test-Prerequisites {
    Write-ColorOutput "🔍 Checking prerequisites..." $Blue
    
    # Check if act is installed
    try {
        $actVersion = act --version 2>$null
        Write-ColorOutput "✅ act is installed: $actVersion" $Green
    } catch {
        Write-ColorOutput "❌ act is not installed. Please install it first." $Red
        Write-ColorOutput "   Install with: winget install nektos.act" $Yellow
        return $false
    }
    
    # Check if Docker is available
    try {
        docker --version 2>$null | Out-Null
        Write-ColorOutput "✅ Docker is available" $Green
        $dockerAvailable = $true
    } catch {
        Write-ColorOutput "⚠️  Docker is not available. Using dry-run mode only." $Yellow
        $dockerAvailable = $false
    }
    
    return $dockerAvailable
}

function Test-WorkflowSyntax {
    param($WorkflowFile)
    
    Write-ColorOutput "📝 Validating workflow syntax: $WorkflowFile" $Blue
    
    try {
        if ($Verbose) {
            act -n -W $WorkflowFile -v
        } else {
            act -n -W $WorkflowFile
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput "✅ Workflow syntax is valid" $Green
            return $true
        } else {
            Write-ColorOutput "❌ Workflow syntax validation failed" $Red
            return $false
        }
    } catch {
        Write-ColorOutput "❌ Error validating workflow: $_" $Red
        return $false
    }
}

function Test-Workflow {
    param($WorkflowFile, $JobName = "", $Matrix = @{})
    
    $workflowName = Split-Path $WorkflowFile -Leaf
    Write-ColorOutput "🚀 Testing workflow: $workflowName" $Blue
    
    # Build act command
    $actArgs = @()
    
    if ($DryRun -or -not $dockerAvailable) {
        $actArgs += "-n"
        Write-ColorOutput "   Running in dry-run mode" $Yellow
    }
    
    $actArgs += "-W"
    $actArgs += $WorkflowFile
    
    if ($JobName) {
        $actArgs += "-j"
        $actArgs += $JobName
    }
    
    # Add matrix parameters
    foreach ($key in $Matrix.Keys) {
        $actArgs += "--matrix"
        $actArgs += "$key`:$($Matrix[$key])"
    }
    
    if ($Verbose) {
        $actArgs += "-v"
    }
    
    Write-ColorOutput "   Command: act $($actArgs -join ' ')" $Blue
    
    try {
        & act @actArgs
        
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput "✅ Workflow test passed: $workflowName" $Green
            return $true
        } else {
            Write-ColorOutput "❌ Workflow test failed: $workflowName" $Red
            return $false
        }
    } catch {
        Write-ColorOutput "❌ Error running workflow: $_" $Red
        return $false
    }
}

function Test-WindowsWorkflow {
    $workflowFile = ".github/workflows/cmake-windows.yml"
    $results = @()
    
    Write-ColorOutput "`n🪟 Testing Windows Workflows" $Blue
    Write-ColorOutput "=" * 50 $Blue
    
    # Test workflow syntax first
    if (-not (Test-WorkflowSyntax $workflowFile)) {
        return $false
    }
    
    $frameworks = @("wx", "qt")
    
    foreach ($framework in $frameworks) {
        Write-ColorOutput "`n  Testing framework: $framework" $Yellow
        
        $matrix = @{
            "gui_framework" = $framework
            "build_type" = $BuildType
        }
        
        $result = Test-Workflow -WorkflowFile $workflowFile -JobName "build" -Matrix $matrix
        $results += $result
    }
    
    return ($results -notcontains $false)
}

function Test-LinuxWorkflow {
    $workflowFile = ".github/workflows/cmake-linux.yml"
    $results = @()
    
    Write-ColorOutput "`n🐧 Testing Linux Workflows" $Blue
    Write-ColorOutput "=" * 50 $Blue
    
    # Test workflow syntax first
    if (-not (Test-WorkflowSyntax $workflowFile)) {
        return $false
    }
    
    $frameworks = @("wx", "qt")
    $compilers = @("gcc", "clang")
    
    foreach ($framework in $frameworks) {
        foreach ($compiler in $compilers) {
            Write-ColorOutput "`n  Testing framework: $framework, compiler: $compiler" $Yellow
            
            $matrix = @{
                "gui_framework" = $framework
                "build_type" = $BuildType
                "compiler" = $compiler
            }
            
            $result = Test-Workflow -WorkflowFile $workflowFile -JobName "build" -Matrix $matrix
            $results += $result
        }
    }
    
    return ($results -notcontains $false)
}

function Test-MacOSWorkflow {
    $workflowFile = ".github/workflows/cmake-macos.yml"
    $results = @()
    
    Write-ColorOutput "`n🍎 Testing macOS Workflows" $Blue
    Write-ColorOutput "=" * 50 $Blue
    
    # Test workflow syntax first
    if (-not (Test-WorkflowSyntax $workflowFile)) {
        return $false
    }
    
    $frameworks = @("wx", "qt")
    
    foreach ($framework in $frameworks) {
        Write-ColorOutput "`n  Testing framework: $framework" $Yellow
        
        $matrix = @{
            "gui_framework" = $framework
            "build_type" = $BuildType
        }
        
        $result = Test-Workflow -WorkflowFile $workflowFile -JobName "build" -Matrix $matrix
        $results += $result
    }
    
    return ($results -notcontains $false)
}

function Test-SpecificFramework {
    param($Framework)
    
    Write-ColorOutput "`n🎯 Testing $Framework framework across all platforms" $Blue
    Write-ColorOutput "=" * 60 $Blue
    
    $results = @()
    $workflows = @(
        ".github/workflows/cmake-windows.yml",
        ".github/workflows/cmake-linux.yml",
        ".github/workflows/cmake-macos.yml"
    )
    
    foreach ($workflow in $workflows) {
        $platform = if ($workflow -match "windows") { "Windows" } 
                   elseif ($workflow -match "linux") { "Linux" } 
                   else { "macOS" }
        
        Write-ColorOutput "`n  Testing $Framework on $platform" $Yellow
        
        $matrix = @{
            "gui_framework" = $Framework
            "build_type" = $BuildType
        }
        
        if ($workflow -match "linux") {
            # Test with both compilers on Linux
            foreach ($compiler in @("gcc", "clang")) {
                $matrix["compiler"] = $compiler
                Write-ColorOutput "    Compiler: $compiler" $Blue
                $result = Test-Workflow -WorkflowFile $workflow -JobName "build" -Matrix $matrix
                $results += $result
            }
        } else {
            $result = Test-Workflow -WorkflowFile $workflow -JobName "build" -Matrix $matrix
            $results += $result
        }
    }
    
    return ($results -notcontains $false)
}

# Main execution
Write-ColorOutput "🧪 LogViewer Workflow Tester" $Blue
Write-ColorOutput "=" * 60 $Blue
Write-ColorOutput "Target: $Target | Build Type: $BuildType | Dry Run: $DryRun" $Yellow
Write-ColorOutput ""

# Check prerequisites
$dockerAvailable = Test-Prerequisites

if (-not $dockerAvailable -and -not $DryRun) {
    Write-ColorOutput "🔄 Enabling dry-run mode due to missing Docker" $Yellow
    $DryRun = $true
}

$allPassed = $true

try {
    switch ($Target.ToLower()) {
        "all" {
            Write-ColorOutput "`n🌐 Testing all workflows" $Blue
            $allPassed = (Test-WindowsWorkflow) -and 
                        (Test-LinuxWorkflow) -and 
                        (Test-MacOSWorkflow)
        }
        "windows" {
            $allPassed = Test-WindowsWorkflow
        }
        "linux" {
            $allPassed = Test-LinuxWorkflow
        }
        "macos" {
            $allPassed = Test-MacOSWorkflow
        }
        "wx" {
            $allPassed = Test-SpecificFramework -Framework "wx"
        }
        "qt" {
            $allPassed = Test-SpecificFramework -Framework "qt"
        }
    }
} catch {
    Write-ColorOutput "❌ Unexpected error: $_" $Red
    $allPassed = $false
}

# Summary
Write-ColorOutput "`n" ""
Write-ColorOutput "=" * 60 $Blue
if ($allPassed) {
    Write-ColorOutput "🎉 All workflow tests passed!" $Green
    exit 0
} else {
    Write-ColorOutput "💥 Some workflow tests failed!" $Red
    exit 1
}
