# arma_e_loga.ps1 -- espera o replug, abre a ponte, espera o login e LOGA sozinho.
#
# POR QUE existe: o Pi travou depois do boot (oops de kernel no dbus, NULL deref em socket
# AF_UNIX). O getty sobe DEPOIS do oops, entao existe uma janela com login vivo antes do
# travamento. Login manual em duas idas e vindas perde essa janela; aqui e' tudo numa corrida
# so, disparada pela propria enumeracao da porta.
#
# ORDEM (cada passo espera o TEXTO do passo anterior, nada de tempo fixo):
#   porta some -> porta volta -> ESP boota -> "ponte" -> "login:" -> usuario -> senha -> prompt
#   -> comandos
#
# A SENHA NAO TEM DEFAULT: passe -Pass. Arquivo versionado nao e' lugar de credencial.
#
# Uso (de dentro do WSL):
#   ... -File arma_e_loga.ps1 -Pass '<senha>' -Cmds 'dmesg -T | tail -40','ip -br a'

param(
  [Parameter(Mandatory=$true)][string]$Pass,
  [string]$User = 'thigas',
  [string[]]$Cmds = @(),
  [switch]$SemReplug,
  [int]$EsperaBootMs = 4500,
  [int]$TimeoutPortaS = 600,
  [int]$TimeoutLoginS = 180,
  [int]$EsperaCmdMs = 4000,
  [string]$Port = 'COM3',
  [string]$Saida = 'C:\Users\thigas\Documents\PROJETOS\esp32-16mb\captura_login.txt'
)

$sw = New-Object System.IO.StreamWriter($Saida, $false)
$sw.AutoFlush = $true
function Log([string]$t)   { Write-Host -NoNewline $t; $sw.Write($t) }
function LogLn([string]$t) { Write-Host $t; $sw.WriteLine($t) }
function EstaPresente { return ([System.IO.Ports.SerialPort]::getportnames() -contains $Port) }

if (-not $SemReplug) {
  LogLn "[1/4] ARRANQUE O USB. Esperando $Port sumir..."
  $t0 = Get-Date
  while (EstaPresente) {
    if (((Get-Date)-$t0).TotalSeconds -gt $TimeoutPortaS) { LogLn "TIMEOUT: nao sumiu."; $sw.Close(); exit 1 }
    Start-Sleep -Milliseconds 200
  }
  LogLn "[2/4] sumiu. PLUGUE DE VOLTA. Esperando $Port voltar..."
  $t0 = Get-Date
  while (-not (EstaPresente)) {
    if (((Get-Date)-$t0).TotalSeconds -gt $TimeoutPortaS) { LogLn "TIMEOUT: nao voltou."; $sw.Close(); exit 1 }
    Start-Sleep -Milliseconds 200
  }
  LogLn "[3/4] voltou. $EsperaBootMs ms para o ESP terminar o boot..."
  Start-Sleep -Milliseconds $EsperaBootMs
}

$p = $null
for ($i = 0; $i -lt 25; $i++) {
  try {
    $p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
    $p.ReadTimeout = 500
    $p.DtrEnable = $false
    $p.RtsEnable = $false
    $p.Open(); break
  } catch { $p = $null; Start-Sleep -Milliseconds 300 }
}
if ($null -eq $p) { LogLn "ERRO: $Port nao abriu."; $sw.Close(); exit 2 }

function EsperaPor([string]$regex, [int]$timeoutS) {
  $buf = ''
  $fim = (Get-Date).AddSeconds($timeoutS)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Log $s; $buf += $s } } catch {}
    if ($buf -match $regex) { return $true }
    Start-Sleep -Milliseconds 50
  }
  return $false
}

if (-not $SemReplug) {
  LogLn "`n[4/4] abrindo a ponte (LED tem que ficar MAGENTA)"
  $p.Write("ponte`n")
  Start-Sleep -Milliseconds 700
}

# Enter ANTES de esperar: um getty que ja imprimiu o prompt NAO o reimprime sozinho. Sem
# isto, reconectar numa sessao ja aberta fica 180 s em silencio e parece que a ponte caiu.
$p.Write("`n")
LogLn "`n----- esperando o login (ate $TimeoutLoginS s) -----"
if (-not (EsperaPor 'login:' $TimeoutLoginS)) {
  LogLn "`n[!] nao chegou no login em $TimeoutLoginS s."
  $p.Close(); $sw.Close(); exit 3
}

LogLn "`n----- LOGIN: usuario -----"
$p.Write("$User`n")
if (-not (EsperaPor '[Pp]assword:' 25)) { LogLn "`n[!] nao pediu senha." ; $p.Close(); $sw.Close(); exit 4 }
$p.Write("$Pass`n")
if (-not (EsperaPor '(\$|#)\s' 25)) { LogLn "`n[!] senha enviada, prompt nao veio (Login incorrect?)" }
else { LogLn "`n----- DENTRO -----" }

foreach ($c in $Cmds) {
  LogLn "`n----- `$ $c -----"
  $p.Write("$c`n")
  $fim = (Get-Date).AddMilliseconds($EsperaCmdMs)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Log $s } } catch {}
    Start-Sleep -Milliseconds 50
  }
}

$p.Close(); $sw.Close()
Write-Host "`n----- fim. Saida em $Saida -----"
