#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/enchants/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer, Wakizashi, Source, vlog, Neon
 */
class EnchantService {
public:
	static bool breakItem(model::gameobjects::player::Player& player, model::gameobjects::Item& targetItem, model::gameobjects::Item& parentItem);
private:
	static int32_t calculateEffectiveLevel(model::enchants::EnchantmentStone enchantmentStone);
	static int32_t calculateEffectiveLevel(model::templates::item::ItemQuality itemQuality, int32_t itemLevel);
public:
	static bool enchantItem(model::gameobjects::player::Player& player, model::gameobjects::Item& enchantmentStoneItem, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem);
	static void enchantItemAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& supplementItem, int32_t currentEnchant, bool success);
	static void setEnchantLevel(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t enchantLevel);
	static void applyEnchantEffect(model::gameobjects::Item& targetItem, model::gameobjects::player::Player& owner, int32_t enchantLevel);
	static bool socketManastone(model::gameobjects::player::Player& player, model::gameobjects::Item& manastone, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem, int32_t fusionedWeaponLevel);
	static bool socketManastoneAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& supplementItem, int32_t targetWeapon, bool result);
	static int32_t getEquipBuff(model::gameobjects::Item& item);
	static void amplifyItem(runtime::Ptr<model::gameobjects::player::Player> player, int32_t targetItemObjId, int32_t materialId, int32_t toolId);
};

} // namespace aion::gameserver::services
