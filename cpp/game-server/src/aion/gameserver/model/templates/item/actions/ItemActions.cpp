#include "aion/gameserver/model/templates/item/actions/ItemActions.h"

#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"

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

const CraftLearnAction* ItemActions::getCraftLearnAction() const {
	return firstActionOf<CraftLearnAction>(itemActions);
}

const PolishAction* ItemActions::getPolishAction() const {
	return firstActionOf<PolishAction>(itemActions);
}

const SkillUseAction* ItemActions::getSkillUseAction() const {
	return firstActionOf<SkillUseAction>(itemActions);
}

} // namespace aion::gameserver::model::templates::item::actions
