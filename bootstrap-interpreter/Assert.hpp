#ifndef SYSMEL_ASSERT_HPP
#define SYSMEL_ASSERT_HPP
#pragma once

namespace Sysmel
{
[[noreturn]] void assertFailureWithMessage(const char *message);

#define sysmelSourceLineToString2(x) #x
#define sysmelSourceLineToString(x) sysmelSourceLineToString2(x)
#define sysmelAssert(expr) if(!(expr)) ::Sysmel::assertFailureWithMessage(__FILE__ ":" sysmelSourceLineToString(__LINE__) ": Assertion failure: " #expr)

} // End of namespace Sysmel

#endif //SYSMEL_ASSERT_HPP