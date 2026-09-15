#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/audit/fwd.h"

namespace aion::gameserver::utils::audit {

/**
 * C++: a singleton (fieldmap K4, base Immortal; hub-headers.md §11.2). The constructor collects the GM skill templates from
 * DataManager.SKILL_DATA, whose getSkillTemplates (P4-09) is not declared yet, so it stays unported and getInstance() throws until then; the
 * other bodies are ported. Deviation: collection results are snapshots of borrowed players (hub-headers.md §7.1), not Java's live
 * values() view.
 *
 * @author MrPoke, Neon
 */
class GMService : public runtime::Immortal {
public:
	static GMService& getInstance();

private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> staffMembers{
		AION_LOCK_CLASS(GMService::staffMembers#stripe)};
	runtime::ArrayList<const skillengine::model::SkillTemplate*> gmSkills{AION_LOCK_CLASS(GMService::gmSkills)};

	GMService();

public:
	/** Java: staffMembers.values() (a live view); C++: a snapshot */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> getOnlineStaffMembers();

	std::vector<runtime::Ptr<model::gameobjects::player::Player>> getAvailableStaffMembers();

	void onPlayerLogin(model::gameobjects::player::Player& player);

	void onPlayerLogout(model::gameobjects::player::Player& player);

	bool isAnnounceable(model::gameobjects::player::Player& player);

private:
	bool isAvailable(model::gameobjects::player::Player& player);

	void broadcastConnectionStatus(model::gameobjects::player::Player& gm, bool connected);

	void scheduleBroadcastLogin(model::gameobjects::player::Player& gm);

public:
	void addGmSkills(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::utils::audit
