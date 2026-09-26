#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer, cura, Neon
 */
class StigmaService {
public:
	static bool notifyEquipAction(model::gameobjects::player::Player& player, model::gameobjects::Item& resultItem, int64_t slot);
	static void onPlayerLogin(model::gameobjects::player::Player& player);
	static void removeLinkedStigmaSkills(model::gameobjects::player::Player& player);
	static void addLinkedStigmaSkills(model::gameobjects::player::Player& player);
private:
	static int32_t getLinkedStigmaLearnSkill(model::gameobjects::player::Player& player);
public:
	static bool isEquipped(model::gameobjects::player::Player& player, int32_t itemId);
	static bool isEquipped(model::gameobjects::player::Player& player, int32_t neededCount, std::initializer_list<int32_t> itemIds);
private:
	static int32_t getPossibleStigmaCount(model::gameobjects::player::Player& player);
	static bool isCompleteQuest(model::gameobjects::player::Player& player);
	static int32_t getPossibleAdvancedStigmaCount(model::gameobjects::player::Player& player);
	static bool isPossibleEquippedStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& item);
public:
	static void chargeStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& stigma, model::gameobjects::Item& chargeStone);
private:
	static void addStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t stigmaLevel);
public:
	static void removeStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t stigmaLevel,
		bool notifyPlayer);
};

} // namespace aion::gameserver::services
