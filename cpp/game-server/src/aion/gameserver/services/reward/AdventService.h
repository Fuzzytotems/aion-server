#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"

namespace aion::gameserver::services::reward {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * LocalDate is commons::database::Date (hub-headers.md §6).
 *
 * @author Nathan, Estrayl, Neon, Sykra
 */
class AdventService : public runtime::Immortal {
private:
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::templates::rewards::RewardItem>>>> rewards{AION_LOCK_CLASS(AdventService::rewards)}; // Java: = new HashMap<>()
	AdventService();
	~AdventService();
	void addReward(int32_t day, int32_t itemId, int64_t itemCount);
public:
	void onLogin(model::gameobjects::player::Player& player);
	bool isAdventSeason();
private:
	bool isAdventSeason(commons::database::Date date);
public:
	void redeemReward(model::gameobjects::player::Player& player);
	void showTodaysReward(model::gameobjects::player::Player& player);
	static AdventService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::reward
