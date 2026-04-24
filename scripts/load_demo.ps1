param(
    [string]$BaseUrl = "http://localhost:8080",
    [string]$ApiKey = "1234567",
    [int]$ConcurrentRequests = 40,
    [int]$DrainPollSeconds = 15
)

$headers = @{ Authorization = $ApiKey }

Write-Host "== Overload Demo =="
Write-Host "Tip: start the server with MAX_INFLIGHT_REQUESTS=2 for a clearer demo."
Write-Host "Sending $ConcurrentRequests concurrent requests to /order/list ..."

$jobs = @()
for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $jobs += Start-Job -ScriptBlock {
        param($url, $authHeader)
        try {
            $response = Invoke-WebRequest -Uri "$url/order/list" -Headers $authHeader -UseBasicParsing
            [pscustomobject]@{
                StatusCode = [int]$response.StatusCode
                Error = $null
            }
        } catch {
            $statusCode = 0
            if ($_.Exception.Response -and $_.Exception.Response.StatusCode) {
                $statusCode = [int]$_.Exception.Response.StatusCode
            }

            [pscustomobject]@{
                StatusCode = $statusCode
                Error = $_.Exception.Message
            }
        }
    } -ArgumentList $BaseUrl, $headers
}

$results = $jobs | Wait-Job | Receive-Job
$jobs | Remove-Job -Force | Out-Null

$successCount = ($results | Where-Object { $_.StatusCode -eq 200 }).Count
$overloadCount = ($results | Where-Object { $_.StatusCode -eq 503 }).Count
$otherCount = $results.Count - $successCount - $overloadCount

Write-Host "200 responses : $successCount"
Write-Host "503 responses : $overloadCount"
Write-Host "Other results  : $otherCount"

Write-Host ""
Write-Host "== Drain / Readiness Demo =="
Write-Host "Trigger shutdown now in the server terminal with Ctrl+C."
Write-Host "Polling /readiness once per second for up to $DrainPollSeconds seconds ..."

for ($i = 0; $i -lt $DrainPollSeconds; $i++) {
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/readiness" -UseBasicParsing
        Write-Host "[$i] readiness status code: $([int]$response.StatusCode) body: $($response.Content)"
    } catch {
        $statusCode = "connection_failed"
        if ($_.Exception.Response -and $_.Exception.Response.StatusCode) {
            $statusCode = [int]$_.Exception.Response.StatusCode
        }
        Write-Host "[$i] readiness probe result: $statusCode"
    }
    Start-Sleep -Seconds 1
}
