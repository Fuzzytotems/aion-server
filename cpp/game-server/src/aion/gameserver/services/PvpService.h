#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/event/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/bounty/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Sarynth, Estrayl
 */
class PvpService : public runtime::Immortal {
private:
	runtime::ArrayList<const model::templates::bounty::KillBountyTemplate*> killBounties{AION_LOCK_CLASS(PvpService::killBounties)};
	runtime::HashMap<int32_t, runtime::Ref<model::event::Headhunter>> headhunters{AION_LOCK_CLASS(PvpService::headhunters)};
	PvpService();
public:
	static PvpService& getInstance(); // Java singleton
private:
	void sendBountyReward(model::gameobjects::player::Player& player, model::templates::bounty::BountyType type, int32_t killScore);
public:
	void finalizeHeadhuntingSeason();
	void doReward(model::gameobjects::player::Player& victim);
	runtime::Ptr<model::event::Headhunter> getHeadhunterById(int32_t objId); // synchronized
	void doReward(model::gameobjects::player::Player& victim, float apWinMulti);
private:
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> findMembersToCountKillFor(model::gameobjects::player::Player& winner,
		model::gameobjects::player::Player& victim);
	void logKill(model::gameobjects::player::Player& winner, model::gameobjects::player::Player& victim,
		const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& assistedGroup);
	bool rewardPlayerTeam(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& teamMember, model::gameobjects::player::Player& victim,
		int32_t damage, int32_t totalDamage, float apWinMulti);
	void updateKillQuests(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& killers, model::gameobjects::player::Player& victim);
public:
	runtime::HashMap<int32_t, runtime::Ref<model::event::Headhunter>>& getAllHeadhunters() { return this->headhunters; }
	runtime::Ptr<model::event::Headhunter> getHeadhunter(int32_t hunterId);
};

} // namespace aion::gameserver::services
