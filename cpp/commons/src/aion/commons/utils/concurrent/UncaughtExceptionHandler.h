#pragma once

#include <exception>
#include <stacktrace>
#include <string>
#include <string_view>

/**
 * Java: com.aionemu.commons.utils.concurrent.UncaughtExceptionHandler - logs exceptions that terminate a thread.
 * <p>
 * In Java, an exception escaping Thread.run() only ends that thread. In C++ it calls std::terminate, which ends the process, so install()
 * registers a terminate handler that logs the exception (with the name of the crashed thread and a stack trace), flushes all logs and then
 * exits: with ExitCode::RESTART on std::bad_alloc (Java: OutOfMemoryError, so the start script restarts the server), otherwise with
 * ExitCode::ERROR_. std::abort() is deliberately not used, since on Windows it can block the process on an error dialog (debug builds) or in
 * Windows Error Reporting (release builds) instead of exiting.
 * <p>
 * Threads that catch exceptions at their entry point themselves (e.g. those created by PriorityThreadFactory) call uncaughtException(), which
 * behaves like the Java handler: log, and exit with ExitCode::RESTART on std::bad_alloc.
 * <p>
 * Exits use std::quick_exit, which runs the functions registered with std::at_quick_exit but no static destructors (other threads may still be
 * running). Register graceful shutdown work (Java: shutdown hooks) there. If std::terminate is called again while the handler runs on the same
 * thread (e.g. by such a function), the process ends immediately with ExitCode::ERROR_ (std::_Exit).
 *
 * @author -Nemesiss-
 */
namespace aion::commons::utils::concurrent::UncaughtExceptionHandler {

/**
 * Java: Thread.setDefaultUncaughtExceptionHandler(new UncaughtExceptionHandler()) - installs the std::terminate handler for the calling thread
 * and all threads started afterwards. Call it at the start of main. (The Microsoft C runtime keeps terminate handlers per thread; a TLS callback
 * installs the handler on every new thread. Threads that were already running keep their handler.)
 */
void install();

/**
 * Java: uncaughtException(Thread, Throwable). Logs "Critical Error - Thread [threadName] terminated abnormally:" with the exception. If it is
 * a std::bad_alloc, "Trying to exit gracefully with ExitCode.RESTART..." is logged and the process exits with ExitCode::RESTART from another
 * thread (the calling thread returns).
 */
void uncaughtException(std::string_view threadName, std::exception_ptr exception) noexcept;

/**
 * The terminate handler registered by install(): logs std::current_exception() (or the lack of one) like uncaughtException for the calling
 * thread, flushes the logs and ends the process as described above.
 */
[[noreturn]] void onTerminate() noexcept;

namespace detail {

/**
 * @return the log message (without the header line) for an uncaught exception: its stack trace string, followed by the given trace if the
 * exception did not capture one itself. Exposed for tests.
 */
std::string describe(std::exception_ptr exception, const std::stacktrace& fallbackTrace);

} // namespace detail

} // namespace aion::commons::utils::concurrent::UncaughtExceptionHandler
