#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Estrayl, Neon
 */
class BonusPackService : public runtime::Immortal {
private:
	runtime::HashMap<int32_t, int32_t> rewards{AION_LOCK_CLASS(BonusPackService::rewards)}; // Java: = new HashMap<>()
public:
	static BonusPackService& getInstance(); // Java singleton
private:
	BonusPackService();
	~BonusPackService();
public:
	void addPlayerCustomReward(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
