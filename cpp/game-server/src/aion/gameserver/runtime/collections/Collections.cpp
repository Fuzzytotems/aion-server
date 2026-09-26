// Non-template parts of the collection shims: exception factories and the thread-local compute frame stack. Compiling the umbrella header here also keeps every shim header self-contained and warning-free in the library build.

#include "aion/gameserver/runtime/collections/Collections.h"

#include <string>

#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

namespace aion::gameserver::runtime::detail {

void throwIndexOutOfBounds(int64_t index, int64_t size) {
	throw IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(size));
}

void throwNoSuchElement(const char* what) {
	throw NoSuchElementException(what);
}

void throwRecursiveUpdate() {
	throw IllegalStateException("Recursive update");
}

void throwModifiedFromCallback() {
	throw IllegalStateException("Collection modified from inside its own element equals/hashCode/compareTo or comparator");
}

void throwNullElement(const char* what) {
	throw NullPointerException(what);
}

void throwClassCastNoNaturalOrder(const char* shim) {
	throw ClassCastException(std::string(shim) + ": element type has no natural ordering (compareTo) and no comparator was given");
}

ComputeFrame*& computeFrameTop() noexcept {
	thread_local ComputeFrame* top = nullptr;
	return top;
}

} // namespace aion::gameserver::runtime::detail
