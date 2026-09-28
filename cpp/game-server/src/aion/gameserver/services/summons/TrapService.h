#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/services/summons/fwd.h"

namespace aion::gameserver::services::summons {

/**
 * @author Sykra
 */
class TrapService {
private:
	static constexpr int32_t TRAP_LIMIT_PER_OWNER = 2;
	/** C++: defined in TrapService.cpp (the element destructor needs the complete Trap) */
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayDeque<runtime::Ref<model::gameobjects::Trap>>>> registeredTraps;
public:
	static void registerTrap(int32_t ownerObjId, runtime::Ptr<model::gameobjects::Trap> trap, bool removeExcessTraps);
	static void unregisterTrap(int32_t trapObjId);
};

} // namespace aion::gameserver::services::summons
