@echo off
REM K-Term Test Suite Compilation - Certified Build (with Real Situation Library)
REM This builds test suites with the full Situation library for certification/validation

setlocal

REM Use MSYS2 GCC 15.1.0
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Set Vulkan SDK path
set VULKAN_SDK=C:\VulkanSDK\1.4.313.2

echo ========================================
echo K-Term Test Suite Compilation (CERTIFIED)
echo Compiler: GCC 15.1.0 (C11)
echo Vulkan SDK: %VULKAN_SDK%
echo Mode: FULL SITUATION LIBRARY (NOT MOCKED)
echo ========================================
echo.

REM Step 1: Compile tinycthread
echo [0/11] Compiling tinycthread...
gcc -c ../../../ext/glfw/deps/tinycthread.c ^
    -o tinycthread.o ^
    -std=c11 ^
    -I../../../ext/glfw/deps ^
    -D_TTHREAD_WIN32_

if errorlevel 1 (
    echo [FAILED] tinycthread compilation failed!
    exit /b 1
)
echo OK tinycthread compiled

REM Counter for tracking
set TOTAL=10
set PASSED=0
set FAILED=0

REM Common include and define flags
REM NOTE: NOT defining KTERM_TESTING - this uses real situation.h
set INCLUDES=-I../.. -I../../../ext -I../../../ext/vulkan -I../../../ext/cgltf -I../../../ext/cglm/include -I../../../ext/glfw/include -I../../../ext/glfw/deps -I"%VULKAN_SDK%\Include"
set DEFINES=-D_TTHREAD_WIN32_ -DSITUATION_USE_VULKAN -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER -DKTERM_ENABLE_DEBUG_OUTPUT=0 -DNDEBUG
set LIBS=-lm -lws2_32 -liphlpapi tinycthread.o -lvulkan-1
set FLAGS=-std=c11 %INCLUDES% %DEFINES% %LIBS%

echo.
echo NOTE: Compiling with REAL Situation library (not mocked)
echo This will produce larger executables (~14MB each) but validates
echo full integration with GPU rendering backend.
echo.

REM Test 1: Parser Suite
echo [1/%TOTAL%] Compiling test_parser_suite (CERTIFIED)...
gcc -o test_parser_suite_certified.exe test_parser_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_parser_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_parser_suite compiled
    set /a PASSED+=1
)

REM Test 2: Attributes & Modes Suite
echo [2/%TOTAL%] Compiling test_attributes_modes_suite (CERTIFIED)...
gcc -o test_attributes_modes_suite_certified.exe test_attributes_modes_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_attributes_modes_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_attributes_modes_suite compiled
    set /a PASSED+=1
)

REM Test 3: Verification Suite
echo [3/%TOTAL%] Compiling test_verification_suite (CERTIFIED)...
gcc -o test_verification_suite_certified.exe test_verification_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_verification_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_verification_suite compiled
    set /a PASSED+=1
)

REM Test 4: Graphics Suite
echo [4/%TOTAL%] Compiling test_graphics_suite (CERTIFIED)...
gcc -o test_graphics_suite_certified.exe test_graphics_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_graphics_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_graphics_suite compiled
    set /a PASSED+=1
)

REM Test 5: Gateway Suite
echo [5/%TOTAL%] Compiling test_gateway_suite (CERTIFIED)...
gcc -o test_gateway_suite_certified.exe test_gateway_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_gateway_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_gateway_suite compiled
    set /a PASSED+=1
)

REM Test 6: Networking Suite
echo [6/%TOTAL%] Compiling test_networking_suite (CERTIFIED)...
gcc -o test_networking_suite_certified.exe test_networking_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_networking_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_networking_suite compiled
    set /a PASSED+=1
)

REM Test 7: I/O Suite
echo [7/%TOTAL%] Compiling test_io_suite (CERTIFIED)...
gcc -o test_io_suite_certified.exe test_io_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_io_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_io_suite compiled
    set /a PASSED+=1
)

REM Test 8: Fuzzing Suite
echo [8/%TOTAL%] Compiling test_fuzz_suite (CERTIFIED)...
gcc -o test_fuzz_suite_certified.exe test_fuzz_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_fuzz_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_fuzz_suite compiled
    set /a PASSED+=1
)

REM Test 9: Stress Tests
echo [9/%TOTAL%] Compiling stress_tests (CERTIFIED)...
gcc -o stress_tests_certified.exe stress_tests.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] stress_tests compilation failed!
    set /a FAILED+=1
) else (
    echo OK stress_tests compiled
    set /a PASSED+=1
)

REM Test 10: Integration Suite
echo [10/%TOTAL%] Compiling test_integration_suite (CERTIFIED)...
gcc -o test_integration_suite_certified.exe test_integration_suite.c %FLAGS%
if errorlevel 1 (
    echo [FAILED] test_integration_suite compilation failed!
    set /a FAILED+=1
) else (
    echo OK test_integration_suite compiled
    set /a PASSED+=1
)

echo.
echo ========================================
echo Compilation Summary (CERTIFIED BUILD)
echo ========================================
echo Total: %TOTAL%
echo Passed: %PASSED%
echo Failed: %FAILED%
echo ========================================

if %FAILED% gtr 0 (
    echo.
    echo [FAILED] Some tests failed to compile!
    exit /b 1
) else (
    echo.
    echo [SUCCESS] All tests compiled successfully with real Situation library!
    echo.
    echo Executables created:
    echo   - test_parser_suite_certified.exe
    echo   - test_attributes_modes_suite_certified.exe
    echo   - test_verification_suite_certified.exe
    echo   - test_graphics_suite_certified.exe
    echo   - test_gateway_suite_certified.exe
    echo   - test_networking_suite_certified.exe
    echo   - test_io_suite_certified.exe
    echo   - test_fuzz_suite_certified.exe
    echo   - stress_tests_certified.exe
    echo   - test_integration_suite_certified.exe
    echo.
    echo Run: run_all_tests_certified.bat
    exit /b 0
)

endlocal
