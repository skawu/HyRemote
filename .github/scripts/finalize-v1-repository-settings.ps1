param(
    [string]$Repo = "skawu/HyRemote"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI 'gh' is required. Install/authenticate it before finalizing repository settings."
}

gh auth status | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI authentication is not valid."
}

$restoreScript = Join-Path $PSScriptRoot "restore-v1-workflows.ps1"
if (-not (Test-Path $restoreScript)) {
    throw "Missing workflow recovery helper: $restoreScript"
}

Write-Host "Restoring V1 authority workflows..."
& $restoreScript -Repo $Repo
if ($LASTEXITCODE -ne 0) {
    throw "V1 workflow recovery failed."
}

Write-Host "Enabling automatic deletion of merged head branches..."
gh api --method PATCH "repos/$Repo" -F delete_branch_on_merge=true | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Could not enable delete_branch_on_merge."
}

Write-Host "Enabling private vulnerability reporting..."
gh api --method PUT "repos/$Repo/private-vulnerability-reporting" | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Could not enable private vulnerability reporting."
}

$repoState = gh api "repos/$Repo" --jq '{default_branch: .default_branch, visibility: .visibility, delete_branch_on_merge: .delete_branch_on_merge}' | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
    throw "Could not verify repository settings."
}
if ($repoState.default_branch -ne "main") {
    throw "Unexpected default branch '$($repoState.default_branch)'; expected 'main'."
}
if ($repoState.visibility -ne "public") {
    throw "Unexpected repository visibility '$($repoState.visibility)'; expected 'public' for the V1 publication gate."
}
if (-not $repoState.delete_branch_on_merge) {
    throw "delete_branch_on_merge is still false after the update."
}

$privateReporting = gh api "repos/$Repo/private-vulnerability-reporting" --jq '.enabled'
if ($LASTEXITCODE -ne 0) {
    throw "Could not verify private vulnerability reporting."
}
if (($privateReporting | Out-String).Trim().ToLowerInvariant() -ne "true") {
    throw "Private vulnerability reporting is not enabled."
}

Write-Host "Repository settings PASS: V1 workflows active, merged head branches auto-delete, private vulnerability reporting enabled."
Write-Host "Branch-history cleanup is intentionally separate/destructive. Run:"
Write-Host "  pwsh .github/scripts/prune-stale-branches.ps1 -Execute"
Write-Host "Settings/cleanup are governance prerequisites only; they do not constitute #104 or #109 product acceptance."
