#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <filesystem>

#include "src/common.h"

// Imported Classes
namespace gram {
class Automaton;
class Grammar;
}  // namespace gram

namespace gram {
// Get information about analysis process
// class Display {
//    public:
//     virtual void show(const gram::Automaton &m, StringView description) = 0;
//     virtual void showSymbols(const gram::Grammar &g, StringView description) = 0;
// };

// // Output information to svg files in given directory.
// // Need `dot` command in path.
// // This is used for contains purposes, and it will suspend the thread when delaying.
// class FileOutputDisplay : public Display {
//    public:
//     enum class FileType { SVG, PNG, PDF };

//    private:
//     std::filesystem::path outDir;
//     long long delay;
//     const char *fileSuffix;

//    public:
//     FileOutputDisplay(const char *dir, long long delayInMs, FileType ft);
//     void show(const gram::Automaton &m, StringView description) override;
//     void showSymbols(const gram::Grammar &g, StringView description) override {
//         throw UnsupportedError();
//     }
// };

// // Ignore inputs
// class VoidDisplay : public Display {
//    public:
//     void show(const gram::Automaton &m, StringView description) override {}
//     void showSymbols(const gram::Grammar &g, StringView description) override {}
// };

// // Output inputs to console
// class ConsoleDisplay : public Display {
//    public:
//     void show(const gram::Automaton &m, StringView description) override;
//     void showSymbols(const gram::Grammar &g, StringView description) override;
// };

};  // namespace gram

#endif