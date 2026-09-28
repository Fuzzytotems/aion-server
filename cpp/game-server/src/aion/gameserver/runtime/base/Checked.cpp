#include "aion/gameserver/runtime/base/Checked.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stacktrace>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"

namespace aion::gameserver::runtime {

namespace pct {
std::atomic<const PctHooks*> activeHooks{nullptr};
} // namespace pct

namespace {

std::atomic<CheckFailureHandler> checkFailureHandler{nullptr};

void defaultCheckFailureHandler(const CheckFailure& failure) {
	std::string text = std::string("[AION_CHECKED ") + std::string(failure.check) + "] " + std::string(failure.message) + " at " +
		failure.where.file_name() + ":" + std::to_string(failure.where.line()) + " (" + failure.where.function_name() + ")";
	std::string trace;
	try {
		trace = std::to_string(std::stacktrace::current(2));
	} catch (...) {
	}
	std::fprintf(stderr, "%s\n%s\n", text.c_str(), trace.c_str());
	std::fflush(stderr);
	try {
		const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime");
		log.error(text + "\n" + trace);
	} catch (...) {
	}
}

} // namespace

CheckFailureHandler setCheckFailureHandler(CheckFailureHandler handler) noexcept {
	return checkFailureHandler.exchange(handler);
}

void checkFailed(std::string_view check, std::string_view message, std::source_location where) noexcept {
	CheckFailure failure{check, message, where};
	try {
		if (CheckFailureHandler handler = checkFailureHandler.load(); handler != nullptr)
			handler(failure);
		else
			defaultCheckFailureHandler(failure);
	} catch (...) {
	}
#if defined(_MSC_VER)
	// no "abort() has been called" dialog (Debug CRT) and no Windows Error Reporting hang: death tests and unattended servers must exit
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
	std::abort();
}

} // namespace aion::gameserver::runtime
