#include "aion/gameserver/model/templates/item/actions/ItemActions.h"

#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"
#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"
#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.h"
#include "aion/gameserver/model/templates/item/actions/RemodelAction.h"
#include "aion/gameserver/model/templates/item/actions/RideAction.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {
/** Java: the first action with `action instanceof A`, null if there is none (an absent <actions> list binds as an empty list) */
template <class A>
const A* firstActionOf(const std::vector<std::unique_ptr<AbstractItemAction>>& itemActions) {
	for (const std::unique_ptr<AbstractItemAction>& action : itemActions) {
		if (const A* typed = dynamic_cast<const A*>(action.get()))
			return typed;
	}
	return nullptr;
}
} // namespace

const std::vector<std::unique_ptr<AbstractItemAction>>& ItemActions::getItemActions() const {
	// Java: itemActions == null ? Collections.emptyList() : itemActions - an absent list binds as the empty vector
	return itemActions;
}

const EnchantItemAction* ItemActions::getEnchantAction() const {
	return firstActionOf<EnchantItemAction>(itemActions);
}

const SummonHouseObjectAction* ItemActions::getHouseObjectAction() const {
	return firstActionOf<SummonHouseObjectAction>(itemActions);
}

const CraftLearnAction* ItemActions::getCraftLearnAction() const {
	return firstActionOf<CraftLearnAction>(itemActions);
}

const DecorateAction* ItemActions::getDecorateAction() const {
	return firstActionOf<DecorateAction>(itemActions);
}

const DyeAction* ItemActions::getDyeAction() const {
	return firstActionOf<DyeAction>(itemActions);
}

const AdoptPetAction* ItemActions::getAdoptPetAction() const {
	return firstActionOf<AdoptPetAction>(itemActions);
}

const RemodelAction* ItemActions::getRemodelAction() const {
	return firstActionOf<RemodelAction>(itemActions);
}

const PolishAction* ItemActions::getPolishAction() const {
	return firstActionOf<PolishAction>(itemActions);
}

const TuningAction* ItemActions::getTuningAction() const {
	return firstActionOf<TuningAction>(itemActions);
}

const SkillUseAction* ItemActions::getSkillUseAction() const {
	return firstActionOf<SkillUseAction>(itemActions);
}

const RideAction* ItemActions::getRideAction() const {
	return firstActionOf<RideAction>(itemActions);
}

} // namespace aion::gameserver::model::templates::item::actions
