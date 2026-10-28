# generar_medios_prueba.ps1
#
# Genera los archivos multimedia que usan los tests (EditorTests), con el ffmpeg.exe del
# build propio. Solo crea los archivos que faltan, así que es rápido después de la
# primera vez. Lo ejecuta EditorTests.vcxproj antes de compilar.
#
# Uso: generar_medios_prueba.ps1 -FFmpegBin <carpeta con ffmpeg.exe> -Destino <carpeta>

param(
    [Parameter(Mandatory = $true)][string]$FFmpegBin,
    [Parameter(Mandatory = $true)][string]$Destino
)

$ErrorActionPreference = 'Stop'
$ffmpeg = Join-Path $FFmpegBin 'ffmpeg.exe'
if (-not (Test-Path $ffmpeg)) { throw "No se encontró ffmpeg.exe en '$FFmpegBin'" }
New-Item -ItemType Directory -Force -Path $Destino | Out-Null

# Cada entrada: nombre de archivo -> argumentos de ffmpeg (sin el archivo de salida).
# Se usan códecs de software disponibles en el build (mpeg4, aac, pcm) para que no
# dependan del hardware del equipo.
$medios = [ordered]@{
    # 5 s, 320x240 a 25 fps, con audio (tono de 440 Hz).
    'video_audio.mp4' = '-f lavfi -i testsrc2=size=320x240:rate=25:duration=5 -f lavfi -i sine=frequency=440:sample_rate=48000:duration=5 -c:v mpeg4 -q:v 5 -c:a aac -b:a 128k -shortest'
    # 3 s, 640x360 a 30 fps, sin audio.
    'solo_video.mp4'  = '-f lavfi -i testsrc2=size=640x360:rate=30:duration=3 -c:v mpeg4 -q:v 5 -an'
    # Archivos de solo audio (3 s).
    'tono.wav'        = '-f lavfi -i sine=frequency=660:sample_rate=44100:duration=3 -c:a pcm_s16le'
    'tono.aac'        = '-f lavfi -i sine=frequency=550:sample_rate=48000:duration=3 -c:a aac -b:a 128k'
    'tono.mp3'        = '-f lavfi -i sine=frequency=330:sample_rate=44100:duration=3 -c:a mp3_mf -b:a 128k'
}

foreach ($nombre in $medios.Keys) {
    $salida = Join-Path $Destino $nombre
    if (Test-Path $salida) { continue }
    Write-Host "Generando medio de prueba: $nombre"
    $argumentos = "-hide_banner -loglevel error -y $($medios[$nombre]) `"$salida`""
    $p = Start-Process -FilePath $ffmpeg -ArgumentList $argumentos -NoNewWindow -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "ffmpeg falló al generar $nombre (código $($p.ExitCode))" }
}

# Un "video" falso: texto con extensión .mp4, para probar el rechazo de archivos inválidos.
$falso = Join-Path $Destino 'falso.mp4'
if (-not (Test-Path $falso)) {
    Set-Content -Path $falso -Value 'Esto no es un video, es un archivo de texto.' -Encoding ascii
}
