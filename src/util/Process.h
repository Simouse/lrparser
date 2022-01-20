#ifndef LRPARSER_EXEC_H
#define LRPARSER_EXEC_H

// Windows
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)

#include <windows.h>

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include "src/common.h"
#include "src/util/Formatter.h"

namespace util {

struct Process {
    using Stream = ::HANDLE;

    static int closeStream(Stream stream) { return ::CloseHandle(stream); }

    template <class... Ts>
    static int writeStream(Stream stream, const char *fmt, Ts &&...ts) {
        Formatter f;
        StringView sv = f.formatView(fmt, ts...);
        DWORD n;
        (void)WriteFile(stream, sv.data(), sv.size(), &n, nullptr);
        return static_cast<int>(n);
    }

    // Execute a command and discard output.
    // `args` should be ended with a NULL.
    static void exec(const char *file, char **args) {
        Formatter f;
        String cmd = f.concatArgs(file, args);

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFO siStartInfo;
        BOOL bSuccess = FALSE;

        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        bSuccess =
            CreateProcess(nullptr,
                          const_cast<char *>(cmd.c_str()), // command line
                          nullptr,      // process security attributes
                          nullptr,      // primary thread security attributes
                          TRUE,         // handles are inherited
                          0,            // creation flags
                          nullptr,      // use parent's environment
                          nullptr,      // use parent's current directory
                          &siStartInfo, // STARTUPINFO pointer
                          &piProcInfo); // receives PROCESS_INFORMATION
        if (!bSuccess) {
            throw std::runtime_error("exec(): Cannot create process");
        }
    }

    // Execute a command which has piped stdin.
    // `args` should be ended with a NULL.
    static Stream execw(const char *file, char **args) {
        Formatter f;
        String cmd = f.concatArgs(file, args);

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = nullptr;

        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
            throw std::runtime_error("execw(): Cannot create <stdin> pipe");
        }
        if (!SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error(
                "execw(): Cannot close <stdin> write permission of spawning "
                "process.");
        }

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFO siStartInfo;

        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdInput = hRead;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        BOOL bSuccess =
            CreateProcess(nullptr,
                          const_cast<char *>(cmd.c_str()), // command line
                          nullptr,      // process security attributes
                          nullptr,      // primary thread security attributes
                          TRUE,         // handles are inherited
                          0,            // creation flags
                          nullptr,      // use parent's environment
                          nullptr,      // use parent's current directory
                          &siStartInfo, // STARTUPINFO pointer
                          &piProcInfo); // receives PROCESS_INFORMATION
        if (!bSuccess) {
            throw std::runtime_error("Cannot create process in execw()");
        }

        CloseHandle(hRead);
        return hWrite;
    }

    // Execute a command and get output.
    // `args` should be ended with a NULL.
    static Stream execr(const char *file, char **args) {
        Formatter f;
        String cmd = f.concatArgs(file, args);

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = nullptr;

        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
            throw std::runtime_error("execr(): Cannot create <stdout> pipe");
        }
        if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error(
                "execr(): Cannot close <stdout> read permission of spawning "
                "process.");
        }

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFO siStartInfo;

        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdOutput = hWrite;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        BOOL bSuccess =
            CreateProcess(nullptr,
                          const_cast<char *>(cmd.c_str()), // command line
                          nullptr,      // process security attributes
                          nullptr,      // primary thread security attributes
                          TRUE,         // handles are inherited
                          0,            // creation flags
                          nullptr,      // use parent's environment
                          nullptr,      // use parent's current directory
                          &siStartInfo, // STARTUPINFO pointer
                          &piProcInfo); // receives PROCESS_INFORMATION
        if (!bSuccess) {
            throw std::runtime_error("Cannot create process in execr()");
        }

        CloseHandle(hWrite);
        return hRead;
    }

    // Never wait for termination of a child process.
    static void prevent_zombie() {
        // Nothing for windows to do, no such signal...
    }
};

} // namespace util

// BSD / Linux
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) ||        \
    defined(__linux__)

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>

extern char **environ;

namespace util {

struct Process {

    using Stream = ::std::FILE *;

    static int closeStream(Stream stream) { return ::std::fclose(stream); }

    template <class... Ts>
    static int writeStream(Stream stream, const char *fmt, Ts &&...ts) {
        return fprintf(stream, fmt, ts...);
    }

    // Never wait for termination of a child process.
    static void prevent_zombie() {
        struct sigaction arg;
        arg.sa_handler = SIG_IGN;
        arg.sa_flags = SA_NOCLDWAIT;
        sigaction(SIGCHLD, &arg, NULL);
    }

    // Execute a command and discard output.
    // `args` should be ended with a NULL.
    static void exec(const char *file, char **args) {
        pid_t pid;
        int status = -1;
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attrs;

        posix_spawn_file_actions_init(&actions);
        posix_spawnattr_init(&attrs);

        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                         O_WRONLY | O_APPEND, 0);
        posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETPGROUP);
        status = posix_spawnp(&pid, file, &actions, &attrs, args, environ);

        posix_spawnattr_destroy(&attrs);
        posix_spawn_file_actions_destroy(&actions);

        if (status != 0) {
            ::std::fprintf(
                stderr, "exec: posix_spawnp() failed. File path: %s\n", file);
            ::std::exit(1);
        }
    }

    // Execute a command and get output.
    // `args` should be ended with a NULL.
    static Stream execr(const char *file, char **args) {
        int fds[2];
        pid_t pid;
        int status = -1;
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attrs;

        pipe(fds); // fd[1] ===> fd[0]

        posix_spawn_file_actions_init(&actions);
        posix_spawnattr_init(&attrs);

        posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, fds[0]); // Write only
        posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETPGROUP);
        status = posix_spawnp(&pid, file, &actions, &attrs, args, environ);

        posix_spawnattr_destroy(&attrs);
        posix_spawn_file_actions_destroy(&actions);

        if (status != 0) {
            ::std::fprintf(
                stderr, "execr: posix_spawnp() failed. File path: %s\n", file);
            ::std::exit(1);
        }

        // Close write pipe
        close(fds[1]);
        ::std::FILE *f = fdopen(fds[0], "r");

        return f;
    }

    // Execute a command which has piped stdin.
    // `args` should be ended with a NULL.
    static Stream execw(const char *file, char **args) {
        int fds[2];
        pid_t pid;
        int status = -1;
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attrs;

        pipe(fds); // fd[1] ===> fd[0]

        posix_spawn_file_actions_init(&actions);
        posix_spawnattr_init(&attrs);

        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                         O_WRONLY | O_APPEND, 0);
        posix_spawn_file_actions_adddup2(&actions, fds[0], STDIN_FILENO);
        posix_spawn_file_actions_addclose(&actions, fds[1]); // Read only
        posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETPGROUP);
        status = posix_spawnp(&pid, file, &actions, &attrs, args, environ);

        posix_spawnattr_destroy(&attrs);
        posix_spawn_file_actions_destroy(&actions);

        if (status != 0) {
            ::std::fprintf(
                stderr, "execw: posix_spawnp() failed. File path: %s\n", file);
            ::std::exit(1);
        }

        // Close read pipe
        close(fds[0]);
        ::std::FILE *f = fdopen(fds[1], "w");

        return f;
    }
};

} // namespace util

#else
#error Platform not supported

#endif
#endif