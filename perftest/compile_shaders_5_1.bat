@echo off
setlocal enabledelayedexpansion

for %%f in (*.hlsl) do (
    echo Compiling %%f...
    fxc /T cs_5_1 /E main /Zi /Fo shaders\%%~nf.cso %%f
)
