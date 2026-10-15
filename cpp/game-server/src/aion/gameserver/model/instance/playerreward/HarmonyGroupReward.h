#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::playerreward {

/**
 * A Harmony arena group's reward entry: the group's associated players and its group object id.
 *
 * @author xTz
 */
class HarmonyGroupReward : public PvPArenaPlayerReward {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<autogroup::AGPlayer>> players{AION_LOCK_CLASS(HarmonyGroupReward::players)};
	const int32_t grpObjectId;

protected:
	HarmonyGroupReward(int32_t objectId, int32_t timeBonus, int8_t buffId, int32_t grpObjectId);
	~HarmonyGroupReward() override;

public:
	static runtime::Ref<HarmonyGroupReward> create(int32_t objectId, int32_t timeBonus, int8_t buffId, int32_t grpObjectId);

	/** Java: the live list; C++ a snapshot */
	std::vector<runtime::Ptr<autogroup::AGPlayer>> getAssociatedPlayers() const;

	bool containsPlayer(int32_t objectId) const;

	/** null if no associated player has the object id */
	runtime::Ptr<autogroup::AGPlayer> getAGPlayer(int32_t objectId) const;

	void addPlayer(autogroup::AGPlayer& player);

	int32_t getGrpObjectId() const { return grpObjectId; }
};

} // namespace aion::gameserver::model::instance::playerreward
