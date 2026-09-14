#include "aion/gameserver/services/EnchantService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

bool EnchantService::breakItem(model::gameobjects::player::Player& player, model::gameobjects::Item& targetItem, model::gameobjects::Item& parentItem) {
	AION_UNPORTED();
}

int32_t EnchantService::calculateEffectiveLevel(model::enchants::EnchantmentStone enchantmentStone) {
	AION_UNPORTED();
}

int32_t EnchantService::calculateEffectiveLevel(model::templates::item::ItemQuality itemQuality, int32_t itemLevel) {
	AION_UNPORTED();
}

bool EnchantService::enchantItem(model::gameobjects::player::Player& player, model::gameobjects::Item& enchantmentStoneItem, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem) {
	AION_UNPORTED();
}

void EnchantService::enchantItemAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& supplementItem, int32_t currentEnchant, bool success) {
	AION_UNPORTED();
}

void EnchantService::setEnchantLevel(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t enchantLevel) {
	AION_UNPORTED();
}

void EnchantService::applyEnchantEffect(model::gameobjects::Item& targetItem, model::gameobjects::player::Player& owner, int32_t enchantLevel) {
	AION_UNPORTED();
}

bool EnchantService::socketManastone(model::gameobjects::player::Player& player, model::gameobjects::Item& manastone, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem, int32_t fusionedWeaponLevel) {
	AION_UNPORTED();
}

bool EnchantService::socketManastoneAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& supplementItem, int32_t targetWeapon, bool result) {
	AION_UNPORTED();
}

int32_t EnchantService::getEquipBuff(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void EnchantService::amplifyItem(runtime::Ptr<model::gameobjects::player::Player> player, int32_t targetItemObjId, int32_t materialId, int32_t toolId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
