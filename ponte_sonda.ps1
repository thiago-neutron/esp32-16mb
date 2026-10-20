# ponte_sonda.ps1 -- fala com o CONSOLE do Pi atraves da ponte do ESP.
#
# COMO FUNCIONA: o comando "ponte" no firmware poe o ESP em modo cabo (USB <-> UART2 cru).
# A UART2 esta soldada na UART do Pi, que ainda tem getty -> e' um console de login do Linux.
#
# FATO QUE FAZ ISTO FUNCIONAR EM ETAPAS: fechar a porta serial NAO reseta o ESP (medido:
# uptime 69s -> 71s atraves de um fecha/abre). Entao o modo ponte SOBREVIVE entre chamadas —
# da' pra abrir uma vez e depois so mandar linha, sem refazer nada. Sair da ponte = reset.
#
# Uso (de dentro do WSL):
#   ... -File ponte_sonda.ps1 -Abrir -Segundos 150         # abre a ponte e escuta
#   ... -File ponte_sonda.ps1 -Envia 'thigas' -Segundos 5  # manda uma linha e le a resposta
#   ... -File ponte_sonda.ps1 -Segundos 20                 # so escuta
#
# NAO reseta a placa em nenhum caso (DTR/RTS ficam em falso).

param(
  [switch]$Abrir,
  [string]$Envia = '',
  [int]$Segundos = 30,
  [string]$Port = 'COM3'
)

$p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$p.ReadTimeout = 500
$p.DtrEnable = $false     # ANTES do Open(): nao deixa o driver pulsar EN
$p.RtsEnable = $false
$p.Open()

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Write-Host -NoNewline $s } } catch {}
    Start-Sleep -Milliseconds 40
  }
}

if ($Abrir) {
  Write-Host "----- abrindo a ponte -----"
  $p.Write("ponte`n")
  Drena 800
}

if ($Envia -ne '') {
  Write-Host "`n----- enviando: [$Envia] -----"
  $p.Write("$Envia`n")
}

Write-Host "`n----- escutando $Segundos s -----"
Drena ($Segundos * 1000)

$p.Close()
Write-Host "`n----- fim (a ponte continua aberta na placa) -----"
