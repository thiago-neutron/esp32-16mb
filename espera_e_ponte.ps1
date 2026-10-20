# espera_e_ponte.ps1 -- espera o ESP reaparecer no USB e abre a ponte SOZINHO.
#
# POR QUE existe: nao ha' liga/desliga na bancada — a alimentacao e' soldada, entao para pôr o
# cartao no Pi e preciso arrancar o USB. Isso corta a energia do ESP e o modo ponte (flag em
# RAM) morre junto. Se eu so mandasse "ponte" depois, o boot do Linux ja teria passado.
#
# ENTAO: este script fica olhando a enumeracao da porta. No instante em que o COM3 volta, ele
# espera o ESP terminar o proprio boot (~3 s ate PRONTA, o BLEDevice::init e' o lento) e manda
# "ponte". Como o Linux do Pi Zero W leva dezenas de segundos ate o login, da' tempo de sobra
# de pegar praticamente todo o console.
#
# Uso (de dentro do WSL):
#   ... -File espera_e_ponte.ps1 -Segundos 240
#
# Nao reseta nada: DTR/RTS ficam em falso o tempo todo.

param(
  [int]$Segundos = 240,
  [int]$EsperaBootMs = 4500,
  [int]$TimeoutEsperaS = 600,
  [string]$Port = 'COM3'
)

Write-Host "aguardando $Port reaparecer (limite $TimeoutEsperaS s)..."
$t0 = Get-Date
while (-not ([System.IO.Ports.SerialPort]::getportnames() -contains $Port)) {
  if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutEsperaS) {
    Write-Host "TIMEOUT: $Port nao voltou."
    exit 1
  }
  Start-Sleep -Milliseconds 200
}
$dt = [math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
Write-Host "$Port apareceu depois de $dt s. Dando $EsperaBootMs ms para o ESP terminar o boot..."
Start-Sleep -Milliseconds $EsperaBootMs

# A porta pode existir na lista e ainda recusar Open() por um instante enquanto o driver
# termina de subir. Tenta algumas vezes antes de desistir.
$p = $null
for ($i = 0; $i -lt 25; $i++) {
  try {
    $p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
    $p.ReadTimeout = 500
    $p.DtrEnable = $false
    $p.RtsEnable = $false
    $p.Open()
    break
  } catch {
    $p = $null
    Start-Sleep -Milliseconds 300
  }
}
if ($null -eq $p) { Write-Host "ERRO: $Port apareceu mas nao abriu."; exit 2 }

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Write-Host -NoNewline $s } } catch {}
    Start-Sleep -Milliseconds 40
  }
}

Write-Host "----- abrindo a ponte -----"
$p.Write("ponte`n")
Drena 600

Write-Host "`n----- console do Pi por $Segundos s -----"
Drena ($Segundos * 1000)

$p.Close()
Write-Host "`n----- fim (a ponte continua aberta na placa) -----"
