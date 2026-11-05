# SPACE/TIME v1.0 — tempo-synchronized reverb (VST3, Windows / FL Studio)

JUCE 8 + C++17. 8-line FDN reverb, BPM-driven Pre-Delay / Decay / Reverb Time (logic of the 2Rock calculator).
Full guide in Russian: see the document delivered with this archive ("Инструкция по установке").

Quick build (no installs): upload this folder to a GitHub repo -> Actions -> "Build SPACE-TIME" -> download artifact.
Local build: install Visual Studio 2022 (Desktop development with C++), CMake >= 3.22, Git, then run build_windows.bat.

tests/test_core.cpp checks the timing table and the reverb DSP (RT60, stability, level) with plain g++:
  g++ -O2 -std=c++17 tests/test_core.cpp -o test_core && ./test_core
