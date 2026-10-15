#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/npcshout/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * This class is handling NPC shouts
 *
 * @author Rolandas, Neon
 */
class NpcShoutsService : public runtime::Immortal {
private:
	/** Java: inner class NpcShoutTask implements Runnable (used only by the bodies, defined in NpcShoutsService.cpp) */
	class NpcShoutTask;
	runtime::ConcurrentHashMap<int32_t, int64_t> shoutCooldowns{AION_LOCK_CLASS(NpcShoutsService::shoutCooldowns#stripe)};
	NpcShoutsService();
public:
	void registerShoutTask(model::gameobjects::Npc& npc);
	void removeShoutCooldown(model::gameobjects::Npc& npc);
	bool mayShout(model::gameobjects::Npc& npc);
	void shoutRandom(model::gameobjects::Npc& sender, runtime::Ptr<model::gameobjects::player::Player> target,
		const std::vector<const model::templates::npcshout::NpcShout*>& shouts, int32_t shoutCooldown);
	void shout(runtime::Ptr<model::gameobjects::Npc> sender, runtime::Ptr<model::gameobjects::player::Player> target,
		const model::templates::npcshout::NpcShout* shout, int32_t shoutCooldown);
	static NpcShoutsService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
