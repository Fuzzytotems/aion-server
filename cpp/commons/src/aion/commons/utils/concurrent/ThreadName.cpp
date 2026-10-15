#include "aion/commons/utils/concurrent/ThreadName.h"

#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::utils::concurrent {

namespace {

std::string& threadName() {
	thread_local std::string name = [] {
		std::ostringstream id;
		id << std::this_thread::get_id();
		return "thread-" + id.str();
	}();
	return name;
}

} // namespace

void setCurrentThreadName(std::string_view name) {
	threadName() = name;
#ifdef _WIN32
	std::u16string wide = StringUtils::toUtf16(name);
	SetThreadDescription(GetCurrentThread(), reinterpret_cast<const wchar_t*>(wide.c_str()));
#endif
}

const std::string& getCurrentThreadName() {
	return threadName();
}

} // namespace aion::commons::utils::concurrent
