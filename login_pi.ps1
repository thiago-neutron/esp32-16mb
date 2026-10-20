# login_pi.ps1 -- loga no console do Pi pela ponte do ESP e roda comandos.
#
# PRE-REQUISITO: o ESP tem que estar em modo ponte (LED magenta). Se nao estiver, mande
# "ponte" antes com cmd_sonda.ps1 -- este script NAO abre a ponte, porque se ele mandasse
# "ponte" com a ponte ja aberta, a palavra iria como texto para o console do Pi.
#
# POR QUE adaptativo e nao com esperas fixas: o Pi Zero W responde em tempos que variam com
# a carga; login as cegas erra e a gente descobre 40 s depois. Aqui cada passo espera o
# TEXTO que o proprio Pi imprime (login: / assword: / prompt).
#
# A SENHA NAO TEM DEFAULT de proposito: passe -Pass na chamada. Arquivo versionado nao e'
# lugar de credencial, mesmo que ela ja esteja no README.
#
# Uso (de dentro do WSL):
#   ... -File login_pi.ps1 -Pass '<senha>' -Cmds 'uname -a','ip -br a'
#
# Nao reseta a placa. Saida em captura_login.txt (arquivo separado do boot).

param(
  [Parameter(Mandatory=$true)][string]$Pass,
  [string]$User = 'thigas',
  [string[]]$Cmds = @(),
  [int]$EsperaCmdMs = 3000,
  [int]$TimeoutPassoS = 25,
  [string]$Port = 'COM3',
  [string]$Saida = 'C:\Users\thigas\Documents\PROJETOS\esp32-16mb\captura_login.txt'
)

$sw = New-Object System.IO.StreamWriter($Saida, $false)
$sw.AutoFlush = $true
function Log([string]$t) { Write-Host -NoNewline $t; $sw.Write($t) }
function LogLn([string]$t) { Write-Host $t; $sw.WriteLine($t) }

$p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$p.ReadTimeout = 500
$p.DtrEnable = $false
$p.RtsEnable = $false
$p.Open()

# Espera um padrao aparecer, devolvendo tudo que leu. Nao usa -match direto no fluxo porque
# o texto chega picado: acumula num buffer e testa o buffer inteiro a cada volta.
function EsperaPor([string]$regex, [int]$timeoutS) {
  $buf = ''
  $fim = (Get-Date).AddSeconds($timeoutS)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Log $s; $buf += $s } } catch {}
    if ($buf -match $regex) { return $true }
    Start-Sleep -Milliseconds 60
  }
  return $false
}

LogLn "----- acordando o console -----"
$p.Write("`n")
if (-not (EsperaPor '(login:|\$ $|# $|~\s*\$)' $TimeoutPassoS)) {
  LogLn "`n[!] nem login nem prompt em $TimeoutPassoS s."
  $p.Close(); $sw.Close(); exit 1
}

LogLn "`n----- usuario -----"
$p.Write("$User`n")
if (EsperaPor '[Pp]assword:' $TimeoutPassoS) {
  LogLn "`n----- senha -----"
  $p.Write("$Pass`n")
  if (-not (EsperaPor '(\$|#)\s*$' $TimeoutPassoS)) {
    LogLn "`n[!] senha enviada mas nao veio prompt. Login incorrect?"
  }
} else {
  LogLn "`n[!] nao pediu senha (ja logado? getty ocupado?)"
}

foreach ($c in $Cmds) {
  LogLn "`n----- \$ $c -----"
  $p.Write("$c`n")
  $fim = (Get-Date).AddMilliseconds($EsperaCmdMs)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Log $s } } catch {}
    Start-Sleep -Milliseconds 50
  }
}

$p.Close()
$sw.Close()
Write-Host "`n----- fim. Saida em $Saida -----"
