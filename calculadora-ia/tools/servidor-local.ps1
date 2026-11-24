# servidor-local.ps1 - Simulador y relevo de NVIDIA en tu PC con Windows (sin instalar nada).
# Lo abre "Abrir simulador local.bat" cuando no tienes Node.js. Sirve la carpeta simulador\
# en http://localhost:8000 y, en la misma direccion, reenvia /nvidia/v1/... a la API de NVIDIA.
# Tu API key no se guarda aqui: viaja en cada mensaje desde el navegador.
# Para apagarlo, cierra la ventana.

$ErrorActionPreference = 'Stop'
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\simulador'))
$Nvidia = 'https://integrate.api.nvidia.com'

Add-Type -AssemblyName System.Net.Http
try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }

$Client = New-Object System.Net.Http.HttpClient
$Client.Timeout = [TimeSpan]::FromSeconds(150)

$Types = @{
  '.html' = 'text/html; charset=utf-8'; '.js' = 'text/javascript; charset=utf-8'; '.mjs' = 'text/javascript; charset=utf-8'
  '.css' = 'text/css; charset=utf-8'; '.wasm' = 'application/wasm'; '.json' = 'application/json'
  '.png' = 'image/png'; '.svg' = 'image/svg+xml'; '.ico' = 'image/x-icon'
}

function Send-Bytes($Res, [int]$Status, [string]$Type, [byte[]]$Bytes) {
  $Res.StatusCode = $Status
  $Res.ContentType = $Type
  $Res.Headers.Add('Cache-Control', 'no-store')
  $Res.ContentLength64 = $Bytes.Length
  if ($Bytes.Length -gt 0) { $Res.OutputStream.Write($Bytes, 0, $Bytes.Length) }
}

function Send-Text($Res, [int]$Status, [string]$Type, [string]$Text) {
  Send-Bytes $Res $Status $Type ([System.Text.Encoding]::UTF8.GetBytes($Text))
}

# Reenvia una llamada del simulador a NVIDIA y devuelve la respuesta tal cual
function Invoke-Relay($Req, $Res, [string]$ApiPath) {
  try {
    $method = New-Object System.Net.Http.HttpMethod($Req.HttpMethod)
    $msg = New-Object System.Net.Http.HttpRequestMessage($method, ($Nvidia + $ApiPath))
    $auth = $Req.Headers['Authorization']
    if ($auth) { [void]$msg.Headers.TryAddWithoutValidation('Authorization', $auth) }
    [void]$msg.Headers.TryAddWithoutValidation('Accept', 'application/json')
    if ($Req.HttpMethod -eq 'POST') {
      $reader = New-Object System.IO.StreamReader($Req.InputStream, [System.Text.Encoding]::UTF8)
      $body = $reader.ReadToEnd()
      $reader.Close()
      $msg.Content = New-Object System.Net.Http.StringContent($body, [System.Text.Encoding]::UTF8, 'application/json')
    }
    $resp = $Client.SendAsync($msg).GetAwaiter().GetResult()
    $bytes = $resp.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
    $type = 'application/json'
    if ($resp.Content.Headers.ContentType) { $type = $resp.Content.Headers.ContentType.ToString() }
    if ($resp.Headers.RetryAfter) { $Res.Headers.Add('Retry-After', $resp.Headers.RetryAfter.ToString()) }
    Send-Bytes $Res ([int]$resp.StatusCode) $type $bytes
  } catch {
    $why = $_.Exception.Message -replace '["\\\r\n]', ' '
    Send-Text $Res 502 'application/json' ('{"error":{"message":"No se pudo conectar con NVIDIA: ' + $why + '"}}')
  }
}

# Archivos del simulador (solo dentro de la carpeta simulador\)
function Send-File($Res, [string]$UrlPath) {
  $rel = [System.Uri]::UnescapeDataString($UrlPath).TrimStart('/')
  if ($rel -eq '') { $rel = 'index.html' }
  $full = [System.IO.Path]::GetFullPath((Join-Path $Root $rel))
  $inside = $full.StartsWith($Root + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
  if (-not $inside) { Send-Text $Res 403 'text/plain' 'No permitido'; return }
  if (-not [System.IO.File]::Exists($full)) { Send-Text $Res 404 'text/plain' 'No encontrado'; return }
  $type = $Types[[System.IO.Path]::GetExtension($full).ToLowerInvariant()]
  if (-not $type) { $type = 'application/octet-stream' }
  Send-Bytes $Res 200 $type ([System.IO.File]::ReadAllBytes($full))
}

# Busca un puerto libre entre 8000 y 8010
$Listener = $null
$Port = 8000
for ($p = 8000; $p -le 8010; $p++) {
  $l = New-Object System.Net.HttpListener
  $l.Prefixes.Add("http://localhost:$p/")
  try { $l.Start(); $Listener = $l; $Port = $p; break } catch { $l.Close() }
}
if (-not $Listener) {
  Write-Host 'No se pudo abrir el simulador local: los puertos 8000 a 8010 estan ocupados.'
  Read-Host 'Presiona ENTER para cerrar' | Out-Null
  exit 1
}

$Url = "http://localhost:$Port/"
Write-Host "Simulador local con relevo de NVIDIA: $Url"
Write-Host 'Deja esta ventana abierta mientras lo usas. Para apagarlo, cierrala.'
Start-Process $Url

while ($Listener.IsListening) {
  try { $ctx = $Listener.GetContext() } catch { break }
  $req = $ctx.Request
  $res = $ctx.Response
  try {
    $path = $req.Url.AbsolutePath
    if ($path -eq '/nvidia/health') {
      Send-Text $res 200 'application/json' '{"ok":true,"relay":"local"}'
    } elseif ($path -match '^/nvidia(/v1/(chat/completions|models))$') {
      if ($req.HttpMethod -ne 'GET' -and $req.HttpMethod -ne 'POST') { Send-Text $res 405 'text/plain' 'Metodo no permitido' }
      else { Invoke-Relay $req $res $Matches[1] }
    } elseif ($req.HttpMethod -ne 'GET') {
      Send-Text $res 405 'text/plain' 'Metodo no permitido'
    } else {
      Send-File $res $path
    }
  } catch {
    try { Send-Text $res 500 'text/plain' ('Error: ' + $_.Exception.Message) } catch { }
  } finally {
    try { $res.Close() } catch { }
  }
}
