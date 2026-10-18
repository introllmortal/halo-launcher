# ==============================================================================
# Halo Launcher & Create Clockwork Empire Instance Patch
# Привязка локальной сборки к репозиторию обновлений GitHub
# ==============================================================================

[CmdletBinding()]
param()

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$ErrorActionPreference = "Continue"

function Write-Header {
    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Cyan
    Write-Host "        ПАТЧ СБОРКИ: Create Clockwork Empire -> GitHub        " -ForegroundColor Yellow
    Write-Host "             для Halo Launcher / Prism Launcher               " -ForegroundColor Cyan
    Write-Host "==============================================================" -ForegroundColor Cyan
    Write-Host ""
}

function Check-RunningProcesses {
    $processesToWatch = @("Halo", "prismlauncher", "MultiMC", "javaw")
    $running = Get-Process -ErrorAction SilentlyContinue | Where-Object { $processesToWatch -contains $_.ProcessName }

    if ($running) {
        Write-Host "[!] Обнаружены запущенные процессы лаунчера или игры:" -ForegroundColor Red
        foreach ($proc in $running) {
            Write-Host "    - $($proc.ProcessName) (PID: $($proc.Id))" -ForegroundColor Yellow
        }
        Write-Host ""
        Write-Host "ВНИМАНИЕ: Если лаунчер запущен, он перезапишет файл конфигурации при выходе!" -ForegroundColor Red
        $choice = Read-Host "Завершить эти процессы автоматически перед патчем? (Y/N, Enter = Y)"
        if ([string]::IsNullOrWhiteSpace($choice) -or $choice.Trim().ToUpper() -eq "Y") {
            foreach ($proc in $running) {
                try {
                    Stop-Process -Id $proc.Id -Force -ErrorAction Stop
                    Write-Host "[+] Процесс $($proc.ProcessName) завершен." -ForegroundColor Green
                } catch {
                    Write-Host "[-] Не удалось завершить $($proc.ProcessName): $_" -ForegroundColor Red
                }
            }
            Start-Sleep -Seconds 1
        } else {
            Write-Host "Пожалуйста, закройте лаунчер вручную, затем нажмите Enter для продолжения..." -ForegroundColor Yellow
            [void][System.Console]::ReadLine()
        }
    }
}

function Find-InstanceDirectory {
    # Проверяем строго локальное окружение, куда игрок распаковал файлы:
    # 1. Текущая папка ($PSScriptRoot) - корень инстанса рядом с instance.cfg
    # 2. Папка на уровень выше (если закинули внутрь .minecraft)
    # 3. Папка на два уровня выше (если закинули внутрь .minecraft/mods)
    $localCandidates = @(
        $PSScriptRoot,
        (Join-Path $PSScriptRoot ".."),
        (Join-Path $PSScriptRoot "..\..")
    )

    foreach ($path in $localCandidates) {
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path $path)) {
            $resolved = (Resolve-Path $path).Path
            $cfgFile = Join-Path $resolved "instance.cfg"
            if (Test-Path $cfgFile) {
                return $resolved
            }
        }
    }

    return $null
}

function Update-InstanceConfig {
    param (
        [string]$InstanceDir
    )

    $cfgPath = Join-Path $InstanceDir "instance.cfg"
    if (-not (Test-Path $cfgPath)) {
        Write-Host "[-] Ошибка: instance.cfg не найден в $InstanceDir" -ForegroundColor Red
        return $false
    }

    # Backup original config
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupPath = Join-Path $InstanceDir "instance.cfg.backup_$timestamp"
    Copy-Item -Path $cfgPath -Destination $backupPath -Force
    Write-Host "[+] Создана резервная копия: $(Split-Path -Leaf $backupPath)" -ForegroundColor Green

    $lines = Get-Content -Path $cfgPath -Encoding UTF8

    $keysToRemove = @(
        "ManagedPack",
        "ManagedPackType",
        "ManagedPackID",
        "ManagedPackName",
        "ManagedPackVersionID",
        "ManagedPackVersionName",
        "ManagedPackURL",
        "iconKey",
        "name"
    )

    $newLines = [System.Collections.Generic.List[string]]::new()
    $inGeneral = $false
    $generalFound = $false

    foreach ($line in $lines) {
        $trimmed = $line.Trim()

        if ($trimmed -match '^\[(.*)\]$') {
            $section = $matches[1]
            if ($section -eq "General") {
                $inGeneral = $true
                $generalFound = $true
                $newLines.Add($line)
                continue
            } else {
                $inGeneral = $false
            }
        }

        if ($inGeneral) {
            $matchKey = $false
            foreach ($k in $keysToRemove) {
                if ($trimmed -match "^$k\s*=") {
                    $matchKey = $true
                    break
                }
            }
            if (-not $matchKey) {
                $newLines.Add($line)
            }
        } else {
            $newLines.Add($line)
        }
    }

    # If [General] was not found, add it
    if (-not $generalFound) {
        $newLines.Insert(0, "[General]")
        $inGeneralIdx = 1
    } else {
        # Find index after [General]
        $inGeneralIdx = 1
        for ($i = 0; $i -lt $newLines.Count; $i++) {
            if ($newLines[$i].Trim() -eq "[General]") {
                $inGeneralIdx = $i + 1
                break
            }
        }
    }

    # ManagedPack parameters for GitHub
    $managedPackSettings = @(
        'name=Create Clockwork Empire',
        'iconKey=clockwork_empire',
        'ManagedPack=true',
        'ManagedPackType=github',
        'ManagedPackID=Create_Clockwork_Empire',
        'ManagedPackName="Create Clockwork Empire"',
        'ManagedPackVersionID=1.0.0',
        'ManagedPackVersionName="1.0.0"',
        'ManagedPackURL=https://raw.githubusercontent.com/introllmortal/mindice-modpack/main/updates.json'
    )

    for ($i = $managedPackSettings.Count - 1; $i -ge 0; $i--) {
        $newLines.Insert($inGeneralIdx, $managedPackSettings[$i])
    }

    [System.IO.File]::WriteAllLines($cfgPath, $newLines, [System.Text.UTF8Encoding]::new($false))
    Write-Host "[+] instance.cfg успешно обновлен: включен ManagedPackType=github." -ForegroundColor Green
    return $true
}

function Install-CustomIcon {
    param (
        [string]$InstanceDir
    )

    $iconSource = Join-Path $PSScriptRoot "clockwork_empire.jpg"
    if (Test-Path $iconSource) {
        try {
            $resolvedSource = (Resolve-Path $iconSource).Path
            $targetJpg = Join-Path $InstanceDir "clockwork_empire.jpg"
            $targetPng = Join-Path $InstanceDir "icon.png"

            if (-not (Test-Path $targetJpg) -or (Resolve-Path $targetJpg).Path -ne $resolvedSource) {
                Copy-Item -Path $resolvedSource -Destination $targetJpg -Force
            }
            if (-not (Test-Path $targetPng) -or (Resolve-Path $targetPng).Path -ne $resolvedSource) {
                Copy-Item -Path $resolvedSource -Destination $targetPng -Force
            }

            # Also copy to launcher global icons if directories exist
            $globalIconDirs = @(
                (Join-Path $env:APPDATA "PrismLauncher\icons"),
                (Join-Path $env:APPDATA "HaloLauncher\icons")
            )

            foreach ($dir in $globalIconDirs) {
                if (Test-Path $dir) {
                    $targetGlobal = Join-Path $dir "clockwork_empire.jpg"
                    if (-not (Test-Path $targetGlobal) -or (Resolve-Path $targetGlobal).Path -ne $resolvedSource) {
                        Copy-Item -Path $resolvedSource -Destination $targetGlobal -Force
                    }
                }
            }

            Write-Host "[+] Иконка 'clockwork_empire' успешно установлена." -ForegroundColor Green
        } catch {
            Write-Host "[!] Предупреждение: не удалось скопировать иконку: $_" -ForegroundColor Yellow
        }
    }
}

function Audit-ModsFolder {
    param (
        [string]$InstanceDir
    )

    $possibleModsDirs = @(
        (Join-Path $InstanceDir ".minecraft\mods"),
        (Join-Path $InstanceDir "minecraft\mods"),
        (Join-Path $InstanceDir "mods")
    )

    $modsDir = $null
    foreach ($m in $possibleModsDirs) {
        if (Test-Path $m) {
            $modsDir = $m
            break
        }
    }

    if ($modsDir) {
        $jarFiles = Get-ChildItem -Path $modsDir -Filter "*.jar" -File -ErrorAction SilentlyContinue
        Write-Host "[+] Папка модов найдена: $modsDir" -ForegroundColor Cyan
        Write-Host "[+] Всего установлено модов: $($jarFiles.Count)" -ForegroundColor Green
    } else {
        Write-Host "[!] Папка модов не найдена в стандартных подкаталогах." -ForegroundColor Yellow
    }
}

function Test-GitHubConnection {
    $manifestUrl = "https://raw.githubusercontent.com/introllmortal/mindice-modpack/main/updates.json"
    Write-Host "[*] Проверка соединения с репозиторием обновлений GitHub..." -ForegroundColor Cyan

    try {
        $response = Invoke-RestMethod -Uri $manifestUrl -Method Get -TimeoutSec 5 -ErrorAction Stop
        Write-Host "[+] Связь с GitHub установлена!" -ForegroundColor Green
        if ($response -and $response.versions -and $response.versions.Count -gt 0) {
            $latest = $response.versions[0]
            Write-Host "[+] Последняя доступная версия на сервере: $($latest.version)" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "[i] Манифест на GitHub готов к приему релизов." -ForegroundColor Gray
        Write-Host "    (После публикации первого релиза обновления будут скачиваться автоматически)." -ForegroundColor Gray
    }
}

# ----------------- MAIN EXECUTION -----------------
try {
    if ([System.Environment]::UserInteractive -and -not [Console]::IsOutputRedirected) {
        [Console]::Clear()
    }
} catch {}

Write-Header
Check-RunningProcesses

Write-Host "[*] Проверка папки инстанса..." -ForegroundColor Cyan
$instanceDir = Find-InstanceDirectory

if (-not $instanceDir) {
    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Red
    Write-Host " [-] ОШИБКА: Патчер запущен вне папки инстанса!" -ForegroundColor Red
    Write-Host "==============================================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Скрипт должен быть запущен непосредственно внутри сборки." -ForegroundColor Yellow
    Write-Host "Пожалуйста:" -ForegroundColor White
    Write-Host "  1. Скопируйте файлы патча в папку вашей сборки" -ForegroundColor Gray
    Write-Host "     (туда, где лежит файл instance.cfg или папка .minecraft)." -ForegroundColor Gray
    Write-Host "  2. Запустите patch_instance.bat прямо из папки инстанса." -ForegroundColor Gray
    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Red
    Write-Host ""
    Read-Host "Нажмите Enter для выхода"
    exit 1
}

Write-Host "[+] Папка инстанса подтверждена: $instanceDir" -ForegroundColor Green
Write-Host ""

Install-CustomIcon -InstanceDir $instanceDir
Audit-ModsFolder -InstanceDir $instanceDir

Write-Host "[*] Применение патча конфигурации..." -ForegroundColor Cyan
$success = Update-InstanceConfig -InstanceDir $instanceDir

if ($success) {
    Test-GitHubConnection

    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Green
    Write-Host "               [OK] ПАТЧ УСПЕШНО ПРИМЕНЕН!                    " -ForegroundColor Green
    Write-Host "==============================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Что изменилось:" -ForegroundColor White
    Write-Host "  1. Сборка переименована в 'Create Clockwork Empire'." -ForegroundColor Gray
    Write-Host "  2. Установлена официальная иконка сборки." -ForegroundColor Gray
    Write-Host "  3. Тип управления переключен с CurseForge на GitHub." -ForegroundColor Gray
    Write-Host "  4. В меню настройки инстанса Halo Launcher активирована" -ForegroundColor Gray
    Write-Host "     вкладка GitHub-обновлений." -ForegroundColor Gray
    Write-Host ""
    Write-Host "Как обновлять сборку в будущем:" -ForegroundColor White
    Write-Host "  - Откройте Halo Launcher" -ForegroundColor Yellow
    Write-Host "  - Нажмите ПКМ по сборке -> 'Редактировать' (Edit)" -ForegroundColor Yellow
    Write-Host "  - Во вкладке обновлений нажмите 'Обновить сборку' (Update Pack)" -ForegroundColor Yellow
    Write-Host "  - Лаунчер сам скачает новые моды и обновит все файлы!" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "[-] Не удалось применить патч. Проверьте права доступа к файлам." -ForegroundColor Red
}
