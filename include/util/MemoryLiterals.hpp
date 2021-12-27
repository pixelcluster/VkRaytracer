#pragma once

constexpr unsigned long long operator"" _KiB(unsigned long long value) { return value * 1024; }
constexpr unsigned long long operator"" _MiB(unsigned long long value) { return value * 1024 * 1024; }
constexpr unsigned long long operator"" _GiB(unsigned long long value) { return value * 1024 * 1024 * 1024; }
