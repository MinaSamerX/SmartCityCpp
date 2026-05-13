@echo off
title Smart City Test Runner
color 0A

cd /d "%~dp0"

echo ==========================================
echo        SMART CITY TEST RUNNER
echo ==========================================
echo.

echo Running SmartCityApp...
.\build\bin\SmartCityApp.exe

echo.
echo Running Graph Test...
.\build\bin\test_graph.exe

echo.
echo Running Hashtable Test...
.\build\bin\test_hashtable.exe

echo.
echo Running Integration Test...
.\build\bin\test_integration.exe

echo.
echo Running Priority Queue Test...
.\build\bin\test_priorityqueue.exe

echo.
echo Running QuadTree Test...
.\build\bin\test_quadtree.exe

echo.
echo ==========================================
echo All executables finished.
echo ==========================================

pause