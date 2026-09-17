param(
    [string]$Repo = "skawu/HyRemote",
    [switch]$Execute
)

$ErrorActionPreference = "Stop"

# One-time V1 branch convergence manifest captured on 2026-09-17.
# Each candidate is SHA-locked. A still-existing ref must match this exact SHA and must not be the
# head of an open PR. A missing ref is treated as already pruned so an interrupted cleanup is safely
# resumable without weakening the checks for branches that still exist.
$Candidates = [ordered]@{
    "api/44-remote-access-facade" = "2f8b6a31e07efd82b3415dd69a4ead6905fda607"
    "arch/5-core-contract" = "c7dfa6398ede747c35240afa918ff650dd31ea4a"
    "bootstrap/11-cmake-ci-baseline" = "f5fedfb0b3ca82346c79a69f5c508dd79e226e71"
    "core/capabilities-hygiene" = "d13b3a65ff3bd99c0a4d11d8647c5e6b95a9b543"
    "core/21-session-frame-pipeline" = "bb9dfe918e10c907efb6dd2a962839eeed9542ca"
    "diag/91-connected-client-count" = "6f315a1b8490ae9e2985c3851641875cf4929838"
    "docs/logo-assets" = "8495369ae832c856879301ab92209491200f64f3"
    "docs/readme-restructure" = "81dc4746eab52e4727e094e0fd51f1de8b540fa4"
    "docs/x86-dual-os-openharmony-roadmap" = "71ac2227fb75fee2a7dfcebd5df1303ce74e0a34"
    "docs/39-sdk-consumption-contract" = "4222151567c8ca0aa90243b529bb45f9a980926f"
    "docs/41-three-mode-examples-contract" = "388d2941ce5f920340a35fce232561f25d12aad3"
    "docs/64-v001-user-guides" = "bf4e73e237432c7d038ee3c3c27daa663874639b"
    "example/71-remote-support-showcase" = "4b3db6fce5513ae83d5488498f038a2ce153d6ce"
    "example/88-qpa-proxy-existing-app" = "e6267f9136eb8d4f7834cedfe7b26b0a236e31b4"
    "examples/71-remote-support-showcase" = "3e43903b2648f5d564e9f7a12e62180c7dbebf6c"
    "feature/94-qpa-deploy" = "bdfb35d949f7d2f6219567adb5a7db13dceca7a3"
    "feature/95-git-flow-release-policy" = "0ad2c7377fd9fce210ce093df67df93a28fc2d46"
    "feature/99-v1-docs-convergence" = "84767c26444d7cc26504c922d6600d98613e963f"
    "feature/101-v1-api-freeze" = "d150291c951a1eadabc0a961ced7ecfacfe71312"
    "feature/102-v1-examples-convergence" = "b8eb418be30d315fcacb761bbfc2b0ff92bc7ad2"
    "feature/107-v1-release-readiness" = "7781f5dd281eb253e68284911014b2970af8e090"
    "feature/109-physical-acceptance-plan" = "d6b3d96687eb1a1b27081fa2904b10e4e90708ef"
    "fix/56-p1-bounded-input-handshake" = "2d6161b0eb6e423e6ab9bde81cabc353a9c1948d"
    "governance/24-versioning-license" = "cdda29421e92e0162806c05fd4620eb9021c1a14"
    "input/29-normalized-schema" = "bbd0164a9aaefd6caba351dd4f142ad1f2354ffe"
    "input/90-release-on-disconnect" = "25df5bae38506d4e4e16b90933ddf9512ca907d5"
    "product/60-v001-examples-e2e" = "51b8993778d815310672ebe18a20a6c16cf91afd"
    "qml/31-declarative-api" = "2a75ff14f17d52080d2f5c558563fd69071c5163"
    "qml/66-qml-basic-e2e" = "82e708b2ecf25f7b222e76a5a109609bb9e77c8f"
    "qml/69-deploy-helper" = "5a9dab6b02329fd1570c80cb0d433ed23d27e308"
    "qpa/62-native-delegate-proxy" = "531eea2363b6ec9fb04eec2f1ebcb4375c997436"
    "qpa/68-native-semantics-interception" = "bb991cde8513b9dd387c1b2e61a174e257d78163"
    "qpa/72-after-68-auto-remoteaccess" = "1c62c53a70137596d34f849d68707a20a3b30d97"
    "qpa/72-auto-remoteaccess-composition" = "1b431fccbca46c6f4dbdd58baf1f1e01fe53a125"
    "qpa/76-after-72-application-surface-model" = "72ec80d77f92deb61a23a2de280a536ffbb326ae"
    "qpa/76-application-surface-model" = "dceb81b6091537012eb015a35841a741d66e4cc6"
    "qpa/76-composite-capture-source" = "cc9a9042276f40e71b393120dde585a9d2f3035f"
    "qpa/76-composite-controller-lifecycle" = "4f6077cad534d3d9fce964b9028bac2d0d6f4cd0"
    "qpa/76-composite-input-routing" = "aca1ab0c23850674590a76025b5d1fd2fff671b4"
    "qpa/76-composite-target-provider" = "6fb93bc13ee3edbb1c453a9cd118c9e51f56b033"
    "qpa/76-production-surface-tracker" = "466d5e8c184a1ddb6711f919b6d8a083ded50310"
    "qpa/76-surface-scope-evidence" = "0da84ab2d28fe2ae3e85f7f7e54446a2cc44b503"
    "qpa/86-capture-classification" = "b6379e1369a2d9353f80b454cd2f36b935f98fb3"
    "quick/28-target-adapter" = "2987120e4ab15b35f236f3099f281f5e866351a2"
    "sdk/43-clean" = "15221ae434f06aa69ae627cfe45dba0ddd6fec17"
    "sdk/43-install-export" = "99ecca8234b17aaac59a1209c3667a1f3a36c2fb"
    "spike/3-qt-capture-architecture" = "6470c458ea8122182d313fda7b3a158973d43591"
    "spike/16-async-capture" = "3cb84135a1bedaa3efed0ec9225d71b83d280689"
    "spike/34-rustvncserver-ffi" = "08d149d16e3eeb0dc85e45acdb0ad8522cd948d3"
    "spike/38-rustvncserver-production-fit" = "8a48ae87ee2188c184e0fd3475d48b787731acc0"
    "transport/38-rustvnc-product-fit" = "9216f2df41aaecb9edd2887a75c2cebd18f64480"
    "transport/53-bounded-rfb" = "fc0ad4c5b156aa3098738e8a4b5493b2e8271c70"
    "widgets/6-capture-adapter" = "a92c7a12a11c81e47721b15f7741ea5cb8a0693d"
}

$ProtectedBranches = @(
    "main",
    "develop",
    "feature/104-v1-ga-acceptance-matrix"
)

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI 'gh' is required. Install/authenticate it before branch cleanup."
}

gh auth status | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI authentication is not valid."
}

$repoSettings = gh api "repos/$Repo" --jq '{default_branch: .default_branch, delete_branch_on_merge: .delete_branch_on_merge}' | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
    throw "Could not read repository settings."
}
if ($repoSettings.default_branch -ne "main") {
    throw "Unexpected default branch '$($repoSettings.default_branch)'; expected 'main'."
}
if (-not $repoSettings.delete_branch_on_merge) {
    Write-Warning "Repository setting delete_branch_on_merge is false. Enable 'Automatically delete head branches' after the one-time cleanup to prevent branch accumulation from recurring."
}

$openHeads = @(
    gh api "repos/$Repo/pulls?state=open&per_page=100" --jq '.[].head.ref'
)
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate open pull-request heads."
}

foreach ($protected in $ProtectedBranches) {
    if ($Candidates.Contains($protected)) {
        throw "Safety invariant violated: protected branch '$protected' appears in cleanup manifest."
    }
}

Write-Host "Preflight: validating $($Candidates.Count) historical branch refs..."
$errors = @()
$remaining = [ordered]@{}
$alreadyPruned = @()
foreach ($entry in $Candidates.GetEnumerator()) {
    $branch = $entry.Key
    $expected = $entry.Value

    if ($openHeads -contains $branch) {
        $errors += "candidate '$branch' now has an open PR"
        continue
    }

    $actual = gh api "repos/$Repo/git/ref/heads/$branch" --jq '.object.sha' 2>$null
    if ($LASTEXITCODE -ne 0) {
        # Missing is the only safe state that permits resumption: GitHub no longer exposes a branch
        # ref for this manifest name, so there is nothing left for this cleanup operation to delete.
        $alreadyPruned += $branch
        continue
    }

    $actual = $actual.Trim()
    if ($actual -ne $expected) {
        $errors += "candidate '$branch' moved: expected $expected, actual $actual"
        continue
    }
    $remaining[$branch] = $expected
}

if ($errors.Count -ne 0) {
    Write-Host "Cleanup aborted before any deletion:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 2
}

Write-Host "Preflight PASS: $($remaining.Count) refs remain at their locked SHAs; $($alreadyPruned.Count) are already absent; none is an open PR head."
if (-not $Execute) {
    Write-Host "Dry run only. Re-run with -Execute to delete the remaining historical refs."
    $remaining.Keys | ForEach-Object { Write-Host "  $_" }
    exit 0
}

foreach ($branch in $remaining.Keys) {
    Write-Host "Deleting $branch"
    gh api --method DELETE "repos/$Repo/git/refs/heads/$branch"
    if ($LASTEXITCODE -ne 0) {
        throw "Branch deletion failed for '$branch'. No later ref was attempted. Re-run the script: already-deleted refs are accepted and every remaining ref will be preflighted again before deletion resumes."
    }
}

Write-Host "Branch convergence complete. Retained long/current refs: main, develop, feature/104-v1-ga-acceptance-matrix."
