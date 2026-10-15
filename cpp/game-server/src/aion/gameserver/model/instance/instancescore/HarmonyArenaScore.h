#pragma once

#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * The Harmony arenas' score: the PvP arena score over group rewards.
 *
 * @author xTz
 */
class HarmonyArenaScore : public PvPArenaScore {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<playerreward::HarmonyGroupReward>> groups{AION_LOCK_CLASS(HarmonyArenaScore::groups)};

protected:
	explicit HarmonyArenaScore(world::WorldMapInstance& instance);
	~HarmonyArenaScore() override;

public:
	static runtime::Ref<HarmonyArenaScore> create(world::WorldMapInstance& instance);

	/** null if no group holds the player */
	runtime::Ptr<playerreward::HarmonyGroupReward> getGroupReward(int32_t playerId);

	std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> getHarmonyGroupInside();

	std::vector<runtime::Ptr<gameobjects::player::Player>> getPlayersInside(playerreward::HarmonyGroupReward& group);

	void addHarmonyGroup(playerreward::HarmonyGroupReward& reward);

	/** Java: the live list; C++ a snapshot */
	std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> getGroups();

	int32_t getRank(playerreward::PvPArenaPlayerReward& reward) override;

	int32_t getTotalPoints() override;

	void clear() override;
};

} // namespace aion::gameserver::model::instance::instancescore
