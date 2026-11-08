@echo off
rem TaG を起動する（Unreal Engine 5.8 の実行ファイルでゲームとして開く）。
rem パッケージ版 build\win\Windows\BasketTaG.exe は署名がないため、Windows の Smart App Control などに止められることがある。
start "" "C:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BasketTaG.uproject" -game -windowed -ResX=1600 -ResY=900
