@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

:: === НАСТРОЙКИ ===
set PG_DIR=C:\Program Files\PostgreSQL\17\bin
set EXE_DIR=C:\QtProjects\build-rmo_server-Desktop_Qt_5_15_2_MinGW_64_bit-Debug\debug

:: === Проверка существования папок ===
if not exist "%PG_DIR%" (
    echo [ОШИБКА] PostgreSQL не найден: %PG_DIR%
    pause
    exit /b 1
)

if not exist "%EXE_DIR%" (
    echo [ОШИБКА] Папка с exe не найдена: %EXE_DIR%
    pause
    exit /b 1
)

echo ========================================================
echo  КОПИРОВАНИЕ DLL PostgreSQL 17
echo ========================================================
echo  Источник: %PG_DIR%
echo  Цель:     %EXE_DIR%
echo ========================================================
echo.

:: === Создаём папку sqldrivers ===
if not exist "%EXE_DIR%\sqldrivers" (
    mkdir "%EXE_DIR%\sqldrivers"
    echo [INFO] Создана папка: %EXE_DIR%\sqldrivers
)

:: === Копируем драйвер Qt ===
echo [1/6] Копирование qsqlpsql.dll...
if exist "C:\Qt\5.15.2\mingw81_64\plugins\sqldrivers\qsqlpsql.dll" (
    copy /Y "C:\Qt\5.15.2\mingw81_64\plugins\sqldrivers\qsqlpsql.dll" "%EXE_DIR%\sqldrivers\" >nul
    echo       ✅ Успешно
) else (
    echo       ❌ Файл не найден: qsqlpsql.dll
)

:: === Копируем DLL PostgreSQL 17 (OpenSSL 3!) ===
echo [2/6] Копирование libpq.dll...
copy /Y "%PG_DIR%\libpq.dll" "%EXE_DIR%\" >nul && echo       ✅ Успешно || echo       ❌ Ошибка

echo [3/6] Копирование libcrypto-3-x64.dll (OpenSSL 3)...
copy /Y "%PG_DIR%\libcrypto-3-x64.dll" "%EXE_DIR%\" >nul && echo       ✅ Успешно || echo       ❌ Ошибка

echo [4/6] Копирование libssl-3-x64.dll (OpenSSL 3)...
copy /Y "%PG_DIR%\libssl-3-x64.dll" "%EXE_DIR%\" >nul && echo       ✅ Успешно || echo       ❌ Ошибка

echo [5/6] Копирование libintl-9.dll...
copy /Y "%PG_DIR%\libintl-9.dll" "%EXE_DIR%\" >nul && echo       ✅ Успешно || echo       ❌ Ошибка

echo [6/6] Копирование libiconv-2.dll...
copy /Y "%PG_DIR%\libiconv-2.dll" "%EXE_DIR%\" >nul && echo       ✅ Успешно || echo       ❌ Ошибка

echo.
echo ========================================================
echo  ГОТОВО! Перезапустите программу.
echo ========================================================
pause