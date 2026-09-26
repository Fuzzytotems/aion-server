#pragma once

#include <string>
#include <string_view>

namespace aion::commons::utils::concurrent {

/**
 * Sets the name of the calling thread (Java: Thread.setName / Thread.ofPlatform().name(...)). The name shows up in log lines and, on Windows,
 * in the debugger's thread list.
 */
void setCurrentThreadName(std::string_view name);

/**
 * @return the name set via setCurrentThreadName, or "thread-<id>" if none was set.
 */
const std::string& getCurrentThreadName();

} // namespace aion::commons::utils::concurrent
