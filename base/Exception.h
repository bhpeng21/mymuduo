#pragma once

#include "Types.h"
#include <exception>

namespace mymuduo
{

class Exception : public std::exception
{
public:
    Exception(string what);
    ~Exception() noexcept override = default;

    // default copy-ctor and operator= are okay.

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

    const char *stackTrace() const noexcept
    {
        return stack_.c_str();
    }

private:
    string message_;
    string stack_;
};

}  // namespace mymuduo