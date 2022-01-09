#ifndef __PORT_INVOKE_H__
#define __PORT_INVOKE_H__

// Windows
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)

#include <cstdio>
#include <cstdarg>

struct PipeActions;

class PipeHandle {
    FILE *stream;
    friend struct PipeActions;

  public:
    PipeHandle(FILE *f) : stream(f) {}
};

struct PipeActions {
    static PipeHandle open(const char *command, const char *type) {
        return _popen(command, type);
    }
    static char *gets(char *buf, int count, PipeHandle h) {
        return std::fgets(buf, count, h.stream);
    }
    static int printf(PipeHandle h, const char *format, ... ) {
        va_list ap;
        va_start(ap, format);

        int result = std::vfprintf(h.stream, format, ap);

        va_end(ap);
        return result;
    }
    static int close(PipeHandle h) { return _pclose(h.stream); }
};

// BSD / Linux
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) ||        \
    defined(__linux__)

#include <cstdio>
#include <cstdarg>

struct PipeActions;

class PipeHandle {
    FILE *stream;
    friend struct PipeActions;

  public:
    PipeHandle(FILE *f) : stream(f) {}
};

struct PipeActions {
    static PipeHandle open(const char *command, const char *type) {
        return popen(command, type);
    }
    static char *gets(char *buf, int count, PipeHandle h) {
        return std::fgets(buf, count, h.stream);
    }
    static int printf(PipeHandle h, const char *format, ... ) {
        va_list ap;
        va_start(ap, format);

        int result = std::vfprintf(h.stream, format, ap);

        va_end(ap);
        return result;
    }
    static int close(PipeHandle h) { return pclose(h.stream); }
};

#else
#error Platform not supported

#endif

#endif