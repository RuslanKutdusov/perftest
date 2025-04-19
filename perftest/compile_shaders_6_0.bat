@echo off
setlocal enabledelayedexpansion

for %%f in (*.hlsl) do (
    echo Compiling %%f...
    dxc /T cs_6_0 /E main /Zi /Fo shaders\%%~nf.cso /Fd shaders\ %%f
)
