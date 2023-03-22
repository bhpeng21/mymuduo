#pragma once

#include <string_view>

#include "Types.h"

namespace mymuduo
{

// For passing C-style string argument to a function.
class StringArg // copyable
{
public:
    StringArg(const char *str)
        : str_(str)
    {
    }

    StringArg(const string &str)
        : str_(str.c_str())
    {
    }

    const char *c_str() const { return str_; }

private:
    const char *str_;
};

typedef std::string_view StringPiece;

}  // namespace mymuduo