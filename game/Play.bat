@echo off
rem Launch TaG as a game through the Unreal Engine 5.8 executable (see README.md).
rem The packaged build\win\Windows\BasketTaG.exe is unsigned and may be blocked by Smart App Control.
start "" "C:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BasketTaG.uproject" -game -windowed -ResX=1600 -ResY=900
