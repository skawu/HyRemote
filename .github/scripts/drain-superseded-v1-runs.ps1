param(
    [string]$Repo = "skawu/HyRemote",
    [string]$Branch = "feature/104-v1-ga-acceptance-matrix",
    [switch]$Execute
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI 'gh' is required. Install/authenticate it before draining stale Actions runs."
}

gh auth status | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI authentication is not valid."
}

function Get-BranchHead {
    param([string]$Repository, [string]$BranchName)
    $escaped = [uri]::EscapeDataString($BranchName)
    $sha = gh api "repos/$Repository/git/ref/heads/$escaped" --jq '.object.sha'
    if ($LASTEXITCODE -ne 0) {
        throw "Could not resolve branch head for '$BranchName'."
    }
    return (($sha | Out-String).Trim())
}

$head = Get-BranchHead -Repository $Repo -BranchName $Branch
Write-Host "Current protected candidate HEAD: $head"

# Only PR runs for this branch are considered. Keep every current-HEAD run even if queued; only an
# older head SHA may be cancelled. --paginate is required because the pre-concurrency backlog can span
# more than one page.
$rows = @(
    gh api --paginate "repos/$Repo/actions/runs?branch=$Branch&per_page=100" `
      --jq '.workflow_runs[] | select(.event == "pull_request") | select(.status == "queued" or .status == "in_progress" or .status == "pending") | [.id, .head_sha, .name, .status] | @tsv'
)
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate Actions runs for '$Branch'."
}

$candidates = @()
$current = @()
foreach ($row in $rows) {
    if ([string]::IsNullOrWhiteSpace($row)) {
        continue
    }
    $parts = $row -split "`t", 4
    if ($parts.Count -ne 4) {
        throw "Unexpected Actions row: $row"
    }
    $item = [pscustomobject]@{
        Id = [long]$parts[0]
        Sha = $parts[1]
        Name = $parts[2]
        Status = $parts[3]
    }
    if ($item.Sha -eq $head) {
        $current += $item
    } else {
        $candidates += $item
    }
}

Write-Host "Current-HEAD runs retained: $($current.Count)"
$current | Sort-Object Name, Id | ForEach-Object {
    Write-Host "  KEEP   $($_.Id)  $($_.Status)  $($_.Name)"
}

Write-Host "Superseded queued/in-progress PR runs: $($candidates.Count)"
$candidates | Sort-Object Name, Id | ForEach-Object {
    Write-Host "  STALE  $($_.Id)  $($_.Status)  $($_.Name)  $($_.Sha)"
}

if (-not $Execute) {
    Write-Host "Dry run only. Re-run with -Execute to cancel only the STALE runs above."
    exit 0
}

# Close the race with new commits: if the branch moved after enumeration, cancel nothing and let the
# operator rerun from a fresh snapshot.
$headBeforeCancel = Get-BranchHead -Repository $Repo -BranchName $Branch
if ($headBeforeCancel -ne $head) {
    throw "Branch moved during preflight: was $head, now $headBeforeCancel. No run was cancelled."
}

foreach ($run in $candidates) {
    Write-Host "Cancelling superseded run $($run.Id): $($run.Name) [$($run.Status)]"
    gh api --method POST "repos/$Repo/actions/runs/$($run.Id)/cancel" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to cancel run $($run.Id). Stop and rerun the script; current-HEAD runs remain protected by SHA."
    }
}

Write-Host "Actions drain complete. Current-HEAD runs were not touched."
Write-Host "After the queue drains, restore the four V1 authority workflows with:"
Write-Host "  pwsh .github/scripts/restore-v1-workflows.ps1"
