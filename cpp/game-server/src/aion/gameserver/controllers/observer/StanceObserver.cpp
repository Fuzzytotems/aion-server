#include "aion/gameserver/controllers/observer/StanceObserver.h"

#include <string>

#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::controllers::observer {

StanceObserver::StanceObserver(model::gameobjects::player::Player& playerValue, int32_t stanceSkillIdValue)
	: ActionObserver(ObserverType::ALL), player(playerValue), stanceSkillId(stanceSkillIdValue) {
}

StanceObserver::~StanceObserver() = default;

runtime::Ref<StanceObserver> StanceObserver::create(model::gameobjects::player::Player& playerValue, int32_t stanceSkillIdValue) {
	return runtime::makeRef<StanceObserver>(playerValue, stanceSkillIdValue);
}

void StanceObserver::startSkillCast(skillengine::model::Skill& skill) {
	const std::string& stack = skill.getSkillTemplate()->getStack();
	if (!stack.starts_with("ITEM_") && !stack.starts_with("REMEDY_") && !stack.starts_with("POTION_")) // pots and scrolls don't stop stance
		player->getController().stopStance();
}

void StanceObserver::itemused(model::gameobjects::Item& item) {
	const model::templates::item::actions::ItemActions* actions = item.getItemTemplate()->getActions();
	if (actions != nullptr && actions->getSkillUseAction() == nullptr) // skill actions are checked in startSkillCast, here we stop on RideAction etc.
		player->getController().stopStance();
}

void StanceObserver::abnormalsetted(skillengine::effect::AbnormalState state) {
	if ((controllers::detail::getAbnormalStateId(state) & controllers::detail::getAbnormalStateId(skillengine::effect::AbnormalState::STANCE_OFF)) != 0)
		player->getController().stopStance();
}

} // namespace aion::gameserver::controllers::observer
