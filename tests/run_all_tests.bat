@echo off
REM K-Term Test Suite Runner - Execute All Tests

setlocal

echo ========================================
echo K-Term Test Suite Execution
echo ========================================
echo.

set TOTAL=10
set PASSED=0
set FAILED=0

REM Test 1: Parser Suite
echo [1/%TOTAL%] Running test_parser_suite...
call test_parser_suite.exe
if errorlevel 1 (
    echo [FAILED] test_parser_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_parser_suite
    set /a PASSED+=1
)
echo.

REM Test 2: Attributes & Modes Suite
echo [2/%TOTAL%] Running test_attributes_modes_suite...
call test_attributes_modes_suite.exe
if errorlevel 1 (
    echo [FAILED] test_attributes_modes_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_attributes_modes_suite
    set /a PASSED+=1
)
echo.

REM Test 3: Verification Suite
echo [3/%TOTAL%] Running test_verification_suite...
call test_verification_suite.exe
if errorlevel 1 (
    echo [FAILED] test_verification_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_verification_suite
    set /a PASSED+=1
)
echo.

REM Test 4: Graphics Suite
echo [4/%TOTAL%] Running test_graphics_suite...
call test_graphics_suite.exe
if errorlevel 1 (
    echo [FAILED] test_graphics_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_graphics_suite
    set /a PASSED+=1
)
echo.

REM Test 5: Gateway Suite
echo [5/%TOTAL%] Running test_gateway_suite...
call test_gateway_suite.exe
if errorlevel 1 (
    echo [FAILED] test_gateway_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_gateway_suite
    set /a PASSED+=1
)
echo.

REM Test 6: Networking Suite
echo [6/%TOTAL%] Running test_networking_suite...
call test_networking_suite.exe
if errorlevel 1 (
    echo [FAILED] test_networking_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_networking_suite
    set /a PASSED+=1
)
echo.

REM Test 7: I/O Suite
echo [7/%TOTAL%] Running test_io_suite...
call test_io_suite.exe
if errorlevel 1 (
    echo [FAILED] test_io_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_io_suite
    set /a PASSED+=1
)
echo.

REM Test 8: Fuzzing Suite
echo [8/%TOTAL%] Running test_fuzz_suite...
call test_fuzz_suite.exe
if errorlevel 1 (
    echo [FAILED] test_fuzz_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_fuzz_suite
    set /a PASSED+=1
)
echo.

REM Test 9: Stress Tests
echo [9/%TOTAL%] Running stress_tests...
call stress_tests.exe
if errorlevel 1 (
    echo [FAILED] stress_tests failed!
    set /a FAILED+=1
) else (
    echo [PASSED] stress_tests
    set /a PASSED+=1
)
echo.

REM Test 10: Integration Suite
echo [10/%TOTAL%] Running test_integration_suite...
call test_integration_suite.exe
if errorlevel 1 (
    echo [FAILED] test_integration_suite failed!
    set /a FAILED+=1
) else (
    echo [PASSED] test_integration_suite
    set /a PASSED+=1
)
echo.

echo ========================================
echo Test Execution Summary
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
    echo [SUCCESS] All tests passed!
    exit /b 0
)

endlocal
