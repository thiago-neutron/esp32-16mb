# testa_cores.ps1 -- roda a demo de cores do LED RGB e segura a porta aberta ate o fim.
#
# POR QUE existe separado do ler_sonda.ps1: aquele RESETA a placa pelo DTR/RTS para pegar o
# boot. Aqui reset e o inimigo -- ele mataria o ciclo no meio. Este abre a porta com DTR/RTS
# ja em falso (nao pulsa o EN) e so fala com o terminal.
#
# POR QUE segura a porta os 60 s: fechar a porta faz o driver CH9102 mexer no par DTR/RTS, o
# que pode resetar a placa e abortar o ciclo. Entao a gente espera a demo terminar sozinha.
#
# Uso (de dentro do WSL):
#   /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile \
#     -ExecutionPolicy Bypass -File 'C:\Users\thigas\Documents\PROJETOS\esp32-16mb\testa_cores.ps1'
#
# Read-only na flash: so manda comando de terminal, nao grava firmware.

$p = New-Object System.IO.Ports.SerialPort 'COM3',115200,'None',8,'One'
$p.ReadTimeout = 500
$p.DtrEnable = $false      # ANTES do Open(): nao deixa o driver pulsar EN/IO0
$p.RtsEnable = $false
$p.Open()

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Write-Host -NoNewline $s } } catch {}
    Start-Sleep -Milliseconds 60
  }
}

Write-Host "----- led cal (releitura da fase no HW) -----"
$p.Write("led cal`n"); Drena 2500

# apaga por 3 s: o escuro e a marca de "ja vai comecar" -- sem isso nao da pra saber
# em que ponto do ciclo o olho entrou.
Write-Host "`n----- apagando 3 s (marca do inicio) -----"
$p.Write("led off`n"); Drena 3000

Write-Host "`n----- led ciclo (7 cores x 1,2 s, repetindo por 60 s) -----"
$p.Write("led ciclo`n"); Drena 62000

Write-Host "`n----- voltando ao automatico -----"
$p.Write("led auto`n"); Drena 1500

$p.Close()
Write-Host "`n----- fim -----"
