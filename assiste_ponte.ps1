# assiste_ponte.ps1 -- abre a ponte do ESP e ASSISTE o console do Pi, gravando em arquivo.
#
# CORRIGE O ERRO DO espera_e_ponte.ps1: aquele esperava o COM3 "aparecer", mas a porta ja
# estava presente quando ele subiu -> o laco saiu na hora e mandou "ponte" ANTES do replug.
# Com -Replug, este aqui espera a porta SUMIR e so entao voltar. E' a unica ordem que funciona
# quando nao ha' liga/desliga e o cartao so entra com o USB arrancado.
#
# POR QUE grava em arquivo com AutoFlush: saida por pipe fica presa no buffer e so aparece no
# fim. Com flush a cada escrita, da' pra acompanhar a captura enquanto ela ainda roda.
#
# USO — pegar o boot do Pi do zero (arranque o USB DEPOIS de rodar):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File assiste_ponte.ps1 -Replug -Segundos 240
#
# USO — Pi ja ligado, so quero o prompt de login:
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File assiste_ponte.ps1 -Segundos 60
#
# Nao reseta a placa (DTR/RTS em falso). Nao grava firmware.

param(
  [switch]$Replug,
  [int]$Segundos = 240,
  [int]$EsperaBootMs = 4500,
  [int]$TimeoutEsperaS = 600,
  [string]$Port = 'COM3',
  [string]$Saida = 'C:\Users\thigas\Documents\PROJETOS\esp32-16mb\captura_ponte.txt'
)

$sw = New-Object System.IO.StreamWriter($Saida, $false)
$sw.AutoFlush = $true          # sem isto a captura so aparece quando o processo morre

function Log([string]$t) { Write-Host -NoNewline $t; $sw.Write($t) }
function LogLn([string]$t) { Write-Host $t; $sw.WriteLine($t) }

function EstaPresente { return ([System.IO.Ports.SerialPort]::getportnames() -contains $Port) }

if ($Replug) {
  LogLn "[1/3] ARRANQUE O USB AGORA. Esperando $Port SUMIR..."
  $t0 = Get-Date
  while (EstaPresente) {
    if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutEsperaS) { LogLn "TIMEOUT: nao sumiu."; $sw.Close(); exit 1 }
    Start-Sleep -Milliseconds 200
  }
  LogLn "[2/3] sumiu. Ponha o cartao e pluge de volta. Esperando $Port VOLTAR..."
  $t0 = Get-Date
  while (-not (EstaPresente)) {
    if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutEsperaS) { LogLn "TIMEOUT: nao voltou."; $sw.Close(); exit 1 }
    Start-Sleep -Milliseconds 200
  }
  LogLn "[3/3] voltou. Dando $EsperaBootMs ms para o ESP terminar o proprio boot..."
  Start-Sleep -Milliseconds $EsperaBootMs
}

# A porta pode constar na lista e ainda recusar Open() enquanto o driver termina de subir.
$p = $null
for ($i = 0; $i -lt 25; $i++) {
  try {
    $p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
    $p.ReadTimeout = 500
    $p.DtrEnable = $false      # ANTES do Open(): nao deixa o driver pulsar EN
    $p.RtsEnable = $false
    $p.Open()
    break
  } catch { $p = $null; Start-Sleep -Milliseconds 300 }
}
if ($null -eq $p) { LogLn "ERRO: $Port nao abriu."; $sw.Close(); exit 2 }

function Drena([int]$ms) {
  $fim = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $fim) {
    try { $s = $p.ReadExisting(); if ($s) { Log $s } } catch {}
    Start-Sleep -Milliseconds 40
  }
}

LogLn "----- mandando 'ponte' (o LED tem que ficar MAGENTA) -----"
$p.Write("ponte`n")
Drena 800

# Um Enter puro faz o getty redesenhar o prompt de login. Sem isto, se o boot ja passou, a
# tela fica muda e parece que a ponte nao funcionou — quando na verdade so nao havia nada novo.
LogLn "`n----- Enter para acordar o getty -----"
$p.Write("`n")
Drena 1200

LogLn "`n----- console do Pi por $Segundos s -----"
Drena ($Segundos * 1000)

$p.Close()
$sw.Close()
Write-Host "`n----- fim. Captura em $Saida -----"
