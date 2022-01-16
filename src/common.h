#ifndef __LRPARSER_C_H__
#define __LRPARSER_C_H__

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

using String = ::std::string;
using StringView = ::std::string_view;

// Only define types associated with the last void * argument.
// How to display those types are not specified.
enum DisplayType { LOG, AUTOMATON, SYMBOL_TABLE, PARSE_TABLE };

// Data may be NULL if you don't need it.
using DisplayCallable =
    std::function<void(DisplayType type, const char *description, void *data)>;

extern DisplayCallable display;

struct UnimplementedError : public ::std::runtime_error {
    UnimplementedError() : ::std::runtime_error("Operation not implemented") {}
};

struct UnsupportedError : public ::std::runtime_error {
    UnsupportedError() : ::std::runtime_error("Operation not supported") {}
};

#endif