#pragma once

#include "aion/commons/utils/Exception.h"

/**
 * Java exception types used by the runtime kernel and by ported game server code (design §2.1, §3.3, §7.1).
 *
 * All derive from aion::commons::utils::Exception (captured std::stacktrace, optional cause). The commons types (IllegalArgumentException,
 * IllegalStateException, UnsupportedOperationException, IndexOutOfBoundsException, ArithmeticException) are re-exported into this namespace
 * so kernel and game code can name every Java exception from one place.
 */
namespace aion::gameserver::runtime {

using commons::utils::ArithmeticException;
using commons::utils::Exception;
using commons::utils::IllegalArgumentException;
using commons::utils::IllegalStateException;
using commons::utils::IndexOutOfBoundsException;
using commons::utils::UnsupportedOperationException;

/** Java: NullPointerException. Thrown by dereferencing a null Ref, Ptr, Field<Ref>, SelfOrRef or PartSlot (design §2.1). */
class NullPointerException : public Exception {
public:
	using Exception::Exception;
};

/** Java: ClassCastException. Thrown by runtime::cast<To>(...) when the object is not a To (design §2.2). */
class ClassCastException : public Exception {
public:
	using Exception::Exception;
};

/** Java: ArrayIndexOutOfBoundsException. Thrown by Array<T> slot access (design §3.3). */
class ArrayIndexOutOfBoundsException : public IndexOutOfBoundsException {
public:
	using IndexOutOfBoundsException::IndexOutOfBoundsException;
};

/** Java: IllegalMonitorStateException. Thrown by Monitor::unlock when the calling thread does not hold the monitor. */
class IllegalMonitorStateException : public IllegalStateException {
public:
	using IllegalStateException::IllegalStateException;
};

/** Java: NoSuchElementException. Thrown by JavaIterator::next past the end and by getFirst/removeFirst... on empty collections. */
class NoSuchElementException : public Exception {
public:
	using Exception::Exception;
};

/**
 * Java: ConcurrentModificationException. The collection shims never throw it (design §3.3, deviation 16); it exists for ported code that
 * names it in a catch clause.
 */
class ConcurrentModificationException : public Exception {
public:
	using Exception::Exception;
};

/** Java: java.util.concurrent.CancellationException. Thrown by Future::get on a cancelled task. */
class CancellationException : public IllegalStateException {
public:
	using IllegalStateException::IllegalStateException;
};

/** Java: java.util.concurrent.ExecutionException. Thrown by Future::get when the task threw; cause() is the task's exception. */
class ExecutionException : public Exception {
public:
	using Exception::Exception;
};

/** Java: java.util.concurrent.TimeoutException. Thrown by Future::get(timeout, unit) when the task did not finish in time. */
class TimeoutException : public Exception {
public:
	using Exception::Exception;
};

/** Java: java.util.concurrent.RejectedExecutionException. Thrown when a task is submitted to a shut down executor. */
class RejectedExecutionException : public Exception {
public:
	using Exception::Exception;
};

/** Java: InterruptedException. The kernel never interrupts threads; the type exists for ported catch clauses. */
class InterruptedException : public Exception {
public:
	using Exception::Exception;
};

} // namespace aion::gameserver::runtime
