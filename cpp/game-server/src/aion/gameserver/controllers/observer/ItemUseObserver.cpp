#include "aion/gameserver/controllers/observer/ItemUseObserver.h"

#include "aion/gameserver/controllers/observer/ObserverType.h"

namespace aion::gameserver::controllers::observer {

ItemUseObserver::ItemUseObserver() : ActionObserver(ObserverType::ALL) {
}

ItemUseObserver::~ItemUseObserver() = default;

void ItemUseObserver::attack(model::gameobjects::Creature& creature, int32_t skillId) {
	abort();
}

void ItemUseObserver::attacked(model::gameobjects::Creature& creature, int32_t skillId) {
	abort();
}

void ItemUseObserver::died(model::gameobjects::Creature& creature) {
	abort();
}

void ItemUseObserver::dotattacked(model::gameobjects::Creature& creature, skillengine::model::Effect& dotEffect) {
	abort();
}

void ItemUseObserver::equip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	abort();
}

void ItemUseObserver::unequip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	abort();
}

void ItemUseObserver::moved() {
	abort();
}

void ItemUseObserver::startSkillCast(skillengine::model::Skill& skill) {
	abort();
}

void ItemUseObserver::sit() {
	abort();
}

void ItemUseObserver::endSkillCast(skillengine::model::Skill& skill) {
	abort();
}

void ItemUseObserver::itemused(model::gameobjects::Item& item) {
	abort();
}

void ItemUseObserver::boostSkillCost(skillengine::model::Skill& skill) {
	abort();
}

} // namespace aion::gameserver::controllers::observer
