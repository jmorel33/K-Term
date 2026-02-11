@echo off
REM K-Term Test Suite Runner - Certified Build (with Real Situation Library)
REM Execute all tests compiled with full Situation library

setlocal

echo ========================================
echo K-Term Test Suite Execution (CERTIFIED)
echo Mode: FULL SITUATION LIBRARY (NOT MOCKED)
echo ========================================
echo.

set TOTAL=10
set PASSED=0
set FAILED=0

REM Test 1: Parser Suite
echo [1/%TOTAL%] Running test_parser_suite_certified...
call test_parser_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_parser_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_parser_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 2: Attributes & Modes Suite
echo [2/%TOTAL%] Running test_attributes_modes_suite_certified...
call test_attributes_modes_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_attributes_modes_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_attributes_modes_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 3: Verification Suite
echo [3/%TOTAL%] Running test_verification_suite_certified...
call test_verification_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_verification_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_verification_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 4: Graphics Suite
echo [4/%TOTAL%] Running test_graphics_suite_certified...
call test_graphics_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_graphics_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_graphics_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 5: Gateway Suite
echo [5/%TOTAL%] Running test_gateway_suite_certified...
call test_gateway_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_gateway_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_gateway_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 6: Networking Suite
echo [6/%TOTAL%] Running test_networking_suite_certified...
call test_networking_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_networking_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_networking_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 7: I/O Suite
echo [7/%TOTAL%] Running test_io_suite_certified...
call test_io_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_io_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_io_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 8: Fuzzing Suite
echo [8/%TOTAL%] Running test_fuzz_suite_certified...
call test_fuzz_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_fuzz_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_fuzz_suite_certified
    set /a PASSED+=1
)
echo.

REM Test 9: Stress Tests
echo [9/%TOTAL%] Running stress_tests_certified...
call stress_tests_certified.exe
if errorlevel 1 (
    echo [FAILED] stress_tests_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] stress_tests_certified
    set /a PASSED+=1
)
echo.

REM Test 10: Integration Suite
echo [10/%TOTAL%] Running test_integration_suite_certified...
call test_integration_suite_certified.exe
if errorlevel 1 (
    echo [FAILED] test_integration_suite_certified failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_integration_suite_certified
    set /a PASSED+=1
)
echo.

echo ========================================
echo Test Execution Summary (CERTIFIED)
echo ========================================
echo Total: %TOTAL%
echo Passed: %PASSED%
echo Failed: %FAILED%
echo Success Rate: %PASSED%/%TOTAL%
echo ========================================

if %FAILED% gtr 0 (
    echo.
    echo [FAILED] Some tests failed!
    exit /b 1
) else (
    echo.
    echo [SUCCESS] All tests passed with real Situation library!
    exit /b 0
)

endlocal
