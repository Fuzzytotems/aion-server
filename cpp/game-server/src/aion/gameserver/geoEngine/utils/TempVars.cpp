#include "aion/gameserver/geoEngine/utils/TempVars.h"

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::geoEngine::utils {

thread_local TempVars::TempVarsStack TempVars::varsLocal{};

TempVars& TempVars::get() {
	TempVarsStack& stack = varsLocal;
	if (stack.index < 0 || stack.index >= STACK_SIZE) // Java: stack.tempVars[stack.index] throws
		throw runtime::ArrayIndexOutOfBoundsException(
			"Index " + std::to_string(stack.index) + " out of bounds for length " + std::to_string(STACK_SIZE));
	std::unique_ptr<TempVars>& slot = stack.tempVars[static_cast<size_t>(stack.index)];
	if (!slot) {
		// Create new
		slot.reset(new TempVars());
	}
	stack.index++;
	slot->isUsed = true;
	return *slot;
}

void TempVars::release() {
	if (!isUsed)
		throw runtime::IllegalStateException("This instance of TempVars was already released!");
	isUsed = false;
	TempVarsStack& stack = varsLocal;
	// Return it to the stack
	stack.index--;
	// Check if it is actually there (Java: stack.tempVars[stack.index] throws for a negative index)
	if (stack.index < 0 || stack.tempVars[static_cast<size_t>(stack.index)].get() != this)
		throw runtime::IllegalStateException("An instance of TempVars has not been released in a called method!");
}

// Deviation: Java leaves an instance marked as used when an exception passes between get() and release()
TempVars::ReleaseGuard::~ReleaseGuard() {
	if (vars->isUsed) {
		try {
			vars->release();
		} catch (...) {
			// an out-of-order release (a later instance still in use) keeps the Java error state; nothing to report while unwinding
		}
	}
}

} // namespace aion::gameserver::geoEngine::utils
