#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The Reian next to the Eternal Challenge's teleporter ("custom_instance_reian_teleport_shouter"): tells a player in 15 m whether he can enter
 * today, at most every half hour per player (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceReianTeleportShouter.java.
 */
class CustomInstanceReianTeleportShouter : public GeneralNpcAI {
public:
	explicit CustomInstanceReianTeleportShouter(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleCreatureMoved(Creature& creature) override;

private:
	static runtime::ConcurrentHashMap<int32_t, int64_t>& lastShoutToPlayer();
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
