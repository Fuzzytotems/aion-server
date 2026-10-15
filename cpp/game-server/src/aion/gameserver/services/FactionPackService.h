#pragma once

#include <chrono>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * LocalDateTime is std::chrono::local_time<std::chrono::milliseconds> (hub-headers.md §6).
 *
 * @author Estrayl, Neon
 */
class FactionPackService : public runtime::Immortal {
private:
	const std::chrono::local_time<std::chrono::milliseconds> elyosMinCreationTime;
	const std::chrono::local_time<std::chrono::milliseconds> elyosMaxCreationTime;
	const std::chrono::local_time<std::chrono::milliseconds> asmodianMinCreationTime;
	const std::chrono::local_time<std::chrono::milliseconds> asmodianMaxCreationTime;
	runtime::ArrayList<runtime::Ref<model::templates::rewards::RewardItem>> rewards{AION_LOCK_CLASS(FactionPackService::rewards)}; // Java: = new ArrayList<>()
public:
	static FactionPackService& getInstance(); // Java singleton
private:
	FactionPackService();
	~FactionPackService();
public:
	void addPlayerCustomReward(model::gameobjects::player::Player& player);
private:
	void sendRewards(model::gameobjects::player::Player& player, std::chrono::local_time<std::chrono::milliseconds> minCreationTime, std::chrono::local_time<std::chrono::milliseconds> maxCreationTime);
};

} // namespace aion::gameserver::services
