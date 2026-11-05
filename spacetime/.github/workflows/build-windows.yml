@echo off
chcp 65001 >nul
echo === SPACE/TIME: локальная сборка (нужны Git, CMake, Visual Studio 2022 с C++) ===
cmake -S . -B build || goto :err
cmake --build build --config Release --target SPACE_TIME_VST3 --parallel || goto :err
echo.
echo ГОТОВО. Плагин здесь: build\SPACE_TIME_artefacts\Release\VST3\SPACE-TIME.vst3
echo Скопируйте папку SPACE-TIME.vst3 в C:\Program Files\Common Files\VST3\
pause
exit /b 0
:err
echo.
echo ОШИБКА СБОРКИ. Скопируйте текст выше и отправьте мне.
pause
exit /b 1
