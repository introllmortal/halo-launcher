<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>QObject</name>
    <message><source>Instance is null.</source><translation>Сборка не выбрана.</translation></message>
    <message><source>Instance root path is empty.</source><translation>Путь к папке сборки пуст.</translation></message>
    <message><source>Cannot modify mods while the instance is running.</source><translation>Нельзя изменять моды, пока сборка запущена.</translation></message>
    <message><source>Instance mods directory path is empty.</source><translation>Путь к папке модов пуст.</translation></message>
    <message><source>No active mod bisect session exists.</source><translation>Нет активной сессии поиска проблемного мода.</translation></message>
    <message><source>Failed to open bisect session file.</source><translation>Не удалось открыть файл сессии поиска.</translation></message>
    <message><source>Bisect session file is malformed.</source><translation>Файл сессии поиска повреждён.</translation></message>
    <message><source>Unsupported bisect session version.</source><translation>Неподдерживаемая версия файла сессии поиска.</translation></message>
    <message><source>Bisect session has an invalid mod list.</source><translation>Некорректный список модов в сессии поиска.</translation></message>
    <message><source>Bisect session has an invalid candidate list.</source><translation>Некорректный список подозреваемых модов в сессии.</translation></message>
    <message><source>Bisect session has an invalid active subset.</source><translation>Некорректный активный набор модов в сессии.</translation></message>
    <message><source>Bisect session metadata is invalid.</source><translation>Метаданные сессии поиска повреждены.</translation></message>
    <message><source>Bisect session step counters are invalid.</source><translation>Счётчики шагов сессии поиска некорректны.</translation></message>
    <message><source>Bisect session candidate sets are inconsistent.</source><translation>Наборы кандидатов в сессии не согласованы.</translation></message>
    <message><source>Bisect session has no active candidates to test.</source><translation>В сессии поиска не осталось модов для проверки.</translation></message>
    <message><source>Bisect session culprit state is inconsistent.</source><translation>Состояние найденного виновника в сессии некорректно.</translation></message>
    <message><source>Failed to prepare directory for bisect session file.</source><translation>Не удалось подготовить папку для файла сессии.</translation></message>
    <message><source>Failed to open bisect session file for writing.</source><translation>Не удалось открыть файл сессии для записи.</translation></message>
    <message><source>Failed to write bisect session file.</source><translation>Не удалось записать файл сессии.</translation></message>
    <message><source>Failed to commit bisect session file.</source><translation>Не удалось сохранить файл сессии на диск.</translation></message>
    <message><source>Mod file &apos;%1&apos; is missing from the mods directory.</source><translation>Файл мода «%1» отсутствует в папке mods.</translation></message>
    <message><source>Failed to enable mod &apos;%1&apos;.</source><translation>Не удалось включить мод «%1».</translation></message>
    <message><source>Cannot disable &apos;%1&apos;: &apos;%1.disabled&apos; already exists.</source><translation>Не удалось отключить «%1»: файл «%1.disabled» уже существует.</translation></message>
    <message><source>Failed to disable mod &apos;%1&apos;.</source><translation>Не удалось отключить мод «%1».</translation></message>
    <message><source>Bisect session has already identified a culprit mod.</source><translation>Виновный мод уже найден в текущей сессии.</translation></message>
    <message><source>Bisect session has no active subset to evaluate.</source><translation>Нет активной группы модов для оценки.</translation></message>
    <message><source>Bisect step yielded no remaining candidate mods.</source><translation>После шага поиска не осталось подозреваемых модов.</translation></message>
    <message><source>No culprit mod has been identified yet.</source><translation>Проблемный мод ещё не определён.</translation></message>
    <message><source>Failed to remove bisect session file.</source><translation>Не удалось удалить файл сессии поиска.</translation></message>
    <message><source>A bisect session already exists for this instance.</source><translation>Для этой сборки уже запущена сессия поиска мода.</translation></message>
    <message><source>Mods directory does not exist.</source><translation>Папка модов не существует.</translation></message>
</context>
<context>
    <name>DeltaSyncTask</name>
    <message><source>Downloading updates...</source><translation>Загрузка обновлений...</translation></message>
    <message><source>Failed to create delta staging directory.</source><translation>Не удалось создать временную папку для дельта-обновления.</translation></message>
    <message><source>Network subsystem unavailable. Please use full archive import.</source><translation>Сетевая подсистема недоступна. Используйте полный импорт архива.</translation></message>
    <message><source>Delta Sync Updates</source><translation>Дельта-обновление</translation></message>
    <message><source>Validating downloaded files...</source><translation>Проверка загруженных файлов...</translation></message>
    <message><source>Delta sync download failed: %1. Please use full archive import.</source><translation>Ошибка загрузки дельта-обновления: %1. Используйте полный импорт архива.</translation></message>
    <message><source>Validation failed: Staged file &apos;%1&apos; does not exist. Please use full archive import.</source><translation>Проверка не пройдена: файл «%1» не найден. Используйте полный импорт архива.</translation></message>
    <message><source>Validation failed: File size mismatch for &apos;%1&apos; (expected %2, got %3). Please use full archive import.</source><translation>Ошибка проверки размера «%1» (ожидалось %2, получено %3). Используйте полный импорт архива.</translation></message>
    <message><source>Validation failed: Error computing hash for &apos;%1&apos;. Please use full archive import.</source><translation>Не удалось вычислить контрольную сумму для «%1». Используйте полный импорт архива.</translation></message>
    <message><source>Validation failed: SHA-1 hash mismatch for &apos;%1&apos; (expected %2, got %3). Please use full archive import.</source><translation>Не совпала контрольная сумма SHA-1 для «%1» (ожидалось %2, получено %3). Используйте полный импорт архива.</translation></message>
    <message><source>Rolling back changes...</source><translation>Откат изменений...</translation></message>
    <message><source>File locked or access denied. Delta sync rolled back. Please use full archive import.</source><translation>Файл заблокирован или нет доступа. Изменения отменены, используйте полный импорт архива.</translation></message>
    <message><source>Preparing rollback journal...</source><translation>Подготовка журнала отката...</translation></message>
    <message><source>Committing updates...</source><translation>Применение обновлений...</translation></message>
    <message><source>Instance is invalid.</source><translation>Некорректная сборка.</translation></message>
    <message><source>Validating delta sync manifest...</source><translation>Проверка манифеста обновления...</translation></message>
    <message><source>Failed to parse delta sync manifest.</source><translation>Не удалось разобрать манифест обновления.</translation></message>
    <message><source>Security violation: Manifest attempts to modify protected system path &apos;%1&apos;</source><translation>Нарушение безопасности: манифест пытается изменить защищённый путь «%1»</translation></message>
    <message><source>Creating pre-update backup snapshot...</source><translation>Создание снимка перед обновлением...</translation></message>
</context>
<context>
    <name>InstanceStorageCleaner</name>
    <message><source>Failed to remove %1</source><translation>Не удалось удалить %1</translation></message>
    <message><source>Instance is null.</source><translation>Сборка не выбрана.</translation></message>
    <message><source>Instance root path is empty.</source><translation>Путь к папке сборки пуст.</translation></message>
    <message><source>Cannot clean storage while the instance is running.</source><translation>Нельзя очищать файлы, пока сборка запущена.</translation></message>
    <message><source>Invalid world directory path.</source><translation>Некорректный путь к папке мира.</translation></message>
    <message><source>World directory does not exist or is not a directory.</source><translation>Папка мира не найдена.</translation></message>
    <message><source>Failed to resolve canonical world directory.</source><translation>Не удалось определить реальный путь к папке мира.</translation></message>
    <message><source>Failed to create world-backups directory.</source><translation>Не удалось создать папку world-backups.</translation></message>
    <message><source>Could not allocate a unique backup archive filename.</source><translation>Не удалось подобрать уникальное имя файла для бэкапа.</translation></message>
    <message><source>Failed to enumerate world directory files.</source><translation>Не удалось прочитать файлы мира.</translation></message>
    <message><source>Failed to open temporary backup archive.</source><translation>Не удалось создать временный архив бэкапа.</translation></message>
    <message><source>Failed to add &apos;%1&apos; to backup archive.</source><translation>Не удалось добавить «%1» в архив бэкапа.</translation></message>
    <message><source>Failed to finalize backup archive.</source><translation>Не удалось завершить создание архива бэкапа.</translation></message>
    <message><source>Failed to move temporary backup archive to destination.</source><translation>Не удалось переместить архив бэкапа в папку назначения.</translation></message>
    <message><source>Cannot back up a world while the instance is running.</source><translation>Нельзя создавать бэкап мира, пока сборка запущена.</translation></message>
</context>
</TS>
