// Out-of-line checks of Ref/Ptr/cast (NPE, C1 scope stamps, C8 destructor context, ClassCastException).

#include "aion/gameserver/runtime/lifetime/Ref.h"

#include <string>

#include "aion/commons/utils/ClassName.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"

namespace aion::gameserver::runtime::detail {

uint64_t currentScopeStamp() noexcept {
#if AION_CHECKED
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr ? context->scopeId.load(std::memory_order_relaxed) : 0;
#else
	return 0;
#endif
}

void throwNullPointer(const std::type_info& type) {
	throw NullPointerException("Dereferencing a null reference to " + commons::utils::getClassName(type));
}

void throwStaleBorrow(const std::type_info& type, uint64_t stamp, uint64_t currentStamp) {
	throw IllegalStateException("Ptr<" + commons::utils::getClassName(type) + "> used outside the task scope it was borrowed in (scope " +
		std::to_string(stamp) + ", current " + std::to_string(currentStamp) + ", C1)");
}

void checkNotInDestructorContext(const std::type_info& type) noexcept {
	if (ThreadContext* context = ThreadContext::currentIfRegistered(); context != nullptr && context->destructorContextDepth > 0) [[unlikely]] {
		std::string name;
		try {
			name = commons::utils::getClassName(type);
		} catch (...) {
		}
		checkFailed("C8", "dereference of " + name + " inside a destructor run by the Reclaimer (destructors must be release-only, design §2.7)");
	}
}

void throwClassCast(const std::type_info& from, const std::type_info& to) {
	throw ClassCastException("class " + commons::utils::getClassName(from) + " cannot be cast to class " + commons::utils::getClassName(to));
}

} // namespace aion::gameserver::runtime::detail
