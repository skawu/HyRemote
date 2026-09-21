param(
    [string]$Repo = "skawu/HyRemote"
)

$ErrorActionPreference = "Stop"

# These workflows were temporarily disabled while the 2026-09-17 Actions backlog was drained.
# The V1 convergence branch now carries per-workflow/per-ref concurrency cancellation, so they must
# be active again before #104 can collect a complete current-candidate evidence envelope.
$RequiredWorkflows = @(
    "V1 GA acceptance",
    "Declarative QML API",
    "Transparent QPA Proxy",
    "Git Flow policy"
)

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI 'gh' is required. Install/authenticate it before restoring workflows."
}

gh auth status | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI authentication is not valid."
}

foreach ($workflow in $RequiredWorkflows) {
    Write-Host "Enabling workflow: $workflow"
    gh workflow enable $workflow --repo $Repo
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to enable workflow '$workflow'."
    }
}

$workflowStates = gh api "repos/$Repo/actions/workflows?per_page=100" --jq '.workflows[] | [.name, .state] | @tsv'
if ($LASTEXITCODE -ne 0) {
    throw "Could not read workflow states after enable operations."
}

$stateByName = @{}
foreach ($line in $workflowStates) {
    $parts = $line -split "`t", 2
    if ($parts.Count -eq 2) {
        $stateByName[$parts[0]] = $parts[1]
    }
}

$errors = @()
foreach ($workflow in $RequiredWorkflows) {
    if (-not $stateByName.ContainsKey($workflow)) {
        $errors += "workflow '$workflow' was not returned by the repository Actions API"
        continue
    }
    if ($stateByName[$workflow] -ne "active") {
        $errors += "workflow '$workflow' state is '$($stateByName[$workflow])', expected 'active'"
    }
}

if ($errors.Count -ne 0) {
    Write-Host "Workflow recovery FAILED:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 2
}

Write-Host "Workflow recovery PASS: all four V1 authority workflows are active."
Write-Host "Do not treat activation itself as #104 evidence; only subsequent step-level Windows/Linux runs count."
