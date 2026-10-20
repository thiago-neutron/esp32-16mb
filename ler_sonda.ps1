# ler_sonda.ps1 -- le a serial do ESP32 a partir do WSL.
#
# POR QUE existe: o WSL2 NAO enxerga porta COM (os /dev/ttyS* nao sao as COM do Windows).
# O arduino-cli aqui e o .exe do Windows, entao grava normal -- mas para LER o boot e rodar
# comandos foi preciso este atalho por PowerShell.
#
# Uso (de dentro do WSL):
#   /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile \
#     -ExecutionPolicy Bypass -File 'C:\Users\thigas\Documents\PROJETOS\esp32-16mb\ler_sonda.ps1'
#
# Ele reseta a placa pelo par DTR/RTS (para pegar o boot desde o STEP1), captura 6 s e
# depois roda 'led cal', 'sis' e 'bat'. Read-only: nao grava nada na placa.
# Trocar a porta/comandos: edite abaixo.

$p = New-Object System.IO.Ports.SerialPort 'COM3',115200,'None',8,'One'
$p.ReadTimeout = 500
$p.DtrEnable = $false
$p.RtsEnable = $false
$p.Open()

# pulso de reset: EN em nivel baixo e volta
$p.DtrEnable = $false; $p.RtsEnable = $true
Start-Sleep -Milliseconds 150
$p.RtsEnable = $false

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Write-Host -NoNewline $s } } catch {}
    Start-Sleep -Milliseconds 60
  }
}

Write-Host "----- BOOT -----"
Drena 6000

Write-Host "`n----- led cal -----"
$p.Write("led cal`n")
Drena 2500

Write-Host "`n----- sis -----"
$p.Write("sis`n")
Drena 2000

Write-Host "`n----- bat -----"
$p.Write("bat`n")
Drena 2000

$p.Close()
Write-Host "`n----- fim -----"
