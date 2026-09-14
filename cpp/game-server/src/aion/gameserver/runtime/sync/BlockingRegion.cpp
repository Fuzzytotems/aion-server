#include "aion/gameserver/runtime/sync/BlockingRegion.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

namespace aion::gameserver::runtime {

BlockingRegion::BlockingRegion(const char* what, std::source_location where) noexcept {
	ThreadContext& context = ThreadContext::current();
	if (context.blockingDepth++ == 0)
		context.setBlocking(what, where, commons::utils::nanoTime());
	if (CHECKED && context.heldLockCount.load(std::memory_order_relaxed) > 0)
		LockOrderValidator::getInstance().onBlocking(what, where);
}

BlockingRegion::~BlockingRegion() {
	ThreadContext& context = ThreadContext::current();
	if (--context.blockingDepth == 0)
		context.clearBlocking();
}

} // namespace aion::gameserver::runtime
