#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::utils::concurrent::UncaughtExceptionHandler {

namespace {

const logging::Logger& log() {
	static const logging::Logger instance = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.concurrent.UncaughtExceptionHandler");
	return instance;
}

bool isOutOfMemory(const std::exception_ptr& exception) noexcept {
	if (!exception)
		return false;
	try {
		std::rethrow_exception(exception);
	} catch (const std::bad_alloc&) {
		return true;
	} catch (...) {
		return false;
	}
}

void appendTrace(std::string& out, const std::stacktrace& trace) {
	std::vector<std::string> frames;
	frames.reserve(trace.size());
	size_t first = 0;
	for (const auto& frame : trace) {
		frames.push_back(std::to_string(frame));
		// skip the frames of the terminate handling and exception dispatching, starting the trace at the throw expression
		if (frames.back().find("CxxThrowException") != std::string::npos || frames.back().find("__cxa_throw") != std::string::npos)
			first = frames.size();
	}
	for (size_t i = first; i < frames.size(); i++) {
		out += "\n\tat ";
		out += frames[i];
	}
}

void logUncaught(std::string_view threadName, const std::exception_ptr& exception, const std::stacktrace& fallbackTrace) {
	std::string message = "Critical Error - Thread [";
	message += threadName;
	message += "] terminated abnormally:";
	// passed as the exception of the message, so layouts print it like logback prints the Throwable (e.g. in the Discord code block)
	log().logWithThrowableText(spdlog::level::err, message, detail::describe(exception, fallbackTrace));
}

void flushLogs() noexcept {
	try {
		logging::LoggerFactory::flushAll();
	} catch (...) {
	}
}

[[noreturn]] void exitForRestart() noexcept {
	flushLogs();
	std::quick_exit(ExitCode::RESTART);
}

std::atomic<bool> installed{false};

#ifdef _MSC_VER
/**
 * The Microsoft C runtime keeps terminate handlers per thread (each new thread starts with the default handler, which aborts silently). This
 * TLS callback runs on every thread start, so install() also covers threads started later.
 */
void NTAPI onThreadAttach(PVOID, DWORD reason, PVOID) {
	if (reason == DLL_THREAD_ATTACH && installed.load(std::memory_order_relaxed))
		std::set_terminate(&onTerminate);
}
#endif

} // namespace

#ifdef _MSC_VER
#ifdef _M_IX86
#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_aion_UncaughtExceptionHandler_tlsCallback")
#else
#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:aion_UncaughtExceptionHandler_tlsCallback")
#endif
#pragma const_seg(".CRT$XLB")
extern "C" const PIMAGE_TLS_CALLBACK aion_UncaughtExceptionHandler_tlsCallback = &onThreadAttach;
#pragma const_seg()
#endif

namespace detail {

std::string describe(std::exception_ptr exception, const std::stacktrace& fallbackTrace) {
	std::string text;
	if (!exception) {
		text = "std::terminate called without an active exception";
		appendTrace(text, fallbackTrace);
		return text;
	}
	try {
		std::rethrow_exception(exception);
	} catch (const std::exception& e) {
		text = toStackTraceString(e);
		if (!dynamic_cast<const Exception*>(&e)) // only utils::Exception captures the stack trace where it was thrown
			appendTrace(text, fallbackTrace);
	} catch (...) {
		text = "<unknown exception type>";
		appendTrace(text, fallbackTrace);
	}
	return text;
}

} // namespace detail

void install() {
	installed = true;
	std::set_terminate(&onTerminate);
}

void uncaughtException(std::string_view threadName, std::exception_ptr exception) noexcept {
	bool outOfMemory = isOutOfMemory(exception);
	try {
		// the stack is already unwound here, so a stack trace of the current position would be misleading
		logUncaught(threadName, exception, std::stacktrace());
		if (outOfMemory)
			log().error("Trying to exit gracefully with ExitCode.RESTART..."); // we shouldn't even try to regain memory at this point
	} catch (...) {
		std::fputs("Critical Error - an uncaught exception could not be logged\n", stderr);
	}
	if (outOfMemory) {
		try {
			std::thread(&exitForRestart).detach(); // async, like Java
		} catch (...) {
			exitForRestart();
		}
	}
}

void onTerminate() noexcept {
	thread_local bool handling = false;
	if (handling) // std::terminate was called again while handling it (e.g. by the logging code or an at_quick_exit handler)
		std::_Exit(ExitCode::ERROR_);
	handling = true;

	// if several threads crash at the same time, only the first one logs and ends the process, the others wait for it
	static std::mutex terminateMutex;
	terminateMutex.lock();

	std::exception_ptr exception = std::current_exception();
	bool outOfMemory = isOutOfMemory(exception);
	try {
		// on MSVC, terminate handlers run before the stack is unwound, so the current stack trace shows where the exception was thrown
		logUncaught(getCurrentThreadName(), exception, std::stacktrace::current(1));
		if (outOfMemory)
			log().error("Trying to exit gracefully with ExitCode.RESTART...");
	} catch (...) {
		std::fputs("Critical Error - std::terminate was called and the exception could not be logged\n", stderr);
	}
	if (outOfMemory)
		exitForRestart();
	flushLogs();
	// Deviation: Java only ends the crashed thread. std::abort() is not used: the Microsoft C runtime shows a modal "abort() has been called" dialog
	// in debug builds and invokes Windows Error Reporting (__fastfail) in release builds, which can keep an unattended server from exiting.
	std::quick_exit(ExitCode::ERROR_);
}

} // namespace aion::commons::utils::concurrent::UncaughtExceptionHandler
