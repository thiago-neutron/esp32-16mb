# cmd_sonda.ps1 -- manda comandos avulsos no terminal do ESP e mostra a resposta.
#
# POR QUE existe alem do ler_sonda.ps1: aquele tem a sequencia de bring-up cravada (boot +
# led cal + sis + bat). Este aceita QUALQUER comando, com ou sem reset, sem editar arquivo.
#
# Uso (de dentro do WSL):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File '...\cmd_sonda.ps1' -Cmds 'help','bat'
#   powershell.exe ... -File '...\cmd_sonda.ps1' -Reset -Cmds 'sis'      # reseta antes
#
# NAO use para 'ponte': a ponte e' uma sessao interativa, nao um comando com resposta.
# Read-only na flash.

param(
  [string[]]$Cmds = @('help'),
  [switch]$Reset,
  [string]$Port = 'COM3',
  [int]$EsperaMs = 1500
)

$p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$p.ReadTimeout = 500
$p.DtrEnable = $false     # ANTES do Open(): sem isto o driver pode pulsar EN e resetar sozinho
$p.RtsEnable = $false
$p.Open()

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Write-Host -NoNewline $s } } catch {}
    Start-Sleep -Milliseconds 60
  }
}

if ($Reset) {
  Write-Host "----- reset (DTR/RTS) + boot -----"
  $p.DtrEnable = $false; $p.RtsEnable = $true
  Start-Sleep -Milliseconds 150
  $p.RtsEnable = $false
  Drena 6000
}

foreach ($c in $Cmds) {
  Write-Host "`n----- $c -----"
  $p.Write("$c`n")
  Drena $EsperaMs
}

$p.Close()
Write-Host "`n----- fim -----"
