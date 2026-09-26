#include "aion/gameserver/utils/collections/Predicates.h"

#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/runtime/sched/Pin.h"

namespace aion::gameserver::utils::collections {

using model::gameobjects::player::Player;

// callback lambda com.aionemu.gameserver.utils.collections.Predicates.Players@L29:50
const runtime::PinnedCallback<bool(Player&)> Predicates::Players::ONLINE{[](Player& player) { return player.isOnline(); }};

// callback lambda com.aionemu.gameserver.utils.collections.Predicates.Players@L31:57
const runtime::PinnedCallback<bool(Player&)> Predicates::Players::WITH_LOOT_PET{[](Player& player) {
	return player.getPet() && player.getPet()->getObjectTemplate()->containsFunction(model::templates::pet::PetFunctionType::LOOT);
}};

// callback lambda com.aionemu.gameserver.utils.collections.Predicates.Players@L35:11
runtime::PinnedCallback<bool(Player&)> Predicates::Players::sameRace(Player& p) {
	return runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&p}, [&p](Player& player) { return p.getRace() == player.getRace(); });
}

// callback lambda com.aionemu.gameserver.utils.collections.Predicates.Players@L39:11
runtime::PinnedCallback<bool(Player&)> Predicates::Players::allExcept(Player& ignored) {
	return runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&ignored}, [&ignored](Player& player) { return !player.equals(ignored); });
}

// callback lambda com.aionemu.gameserver.utils.collections.Predicates.Players@L43:11
runtime::PinnedCallback<bool(Player&)> Predicates::Players::canBeMentoredBy(Player& mentor) {
	return runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&mentor},
		[&mentor](Player& player) { return player.getLevel() + 10 <= mentor.getLevel(); });
}

} // namespace aion::gameserver::utils::collections
