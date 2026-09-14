#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::commons::logging {
class Logger;
} // namespace aion::commons::logging

namespace aion::gameserver::taskmanager {

/**
 * This can be used for periodic calls.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze. The
 * constructor logs and schedules `this::run` at a fixed rate (the method reference at AbstractPeriodicTaskManager.java:18, pinned `{this}`), so
 * it stays unported.
 *
 * @author lord_rex and MrPoke based on l2j-free engines
 */
class AbstractPeriodicTaskManager : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: protected static final Logger log = LoggerFactory.getLogger(AbstractPeriodicTaskManager.class) */
	static const commons::logging::Logger log;

	explicit AbstractPeriodicTaskManager(int32_t period);

	virtual void run() = 0;

	~AbstractPeriodicTaskManager() override;
};

} // namespace aion::gameserver::taskmanager
