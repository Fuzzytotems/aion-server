#pragma once

#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.h"

#include <memory>
#include <vector>

#include "aion/gameserver/model/templates/item/actions/fwd.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.ItemActions.
 * <p>
 * C++ notes: the typed lookups return the first action of that type, nullptr for Java null (header requests items-3: getCraftLearnAction and
 * getPolishAction, controllers-1: getSkillUseAction, m5b3-h02: getItemActions and the other eight). getItemActions returns the bound list,
 * which is empty where Java's `itemActions` is null (an absent list binds as an empty vector), so Java's `Collections.emptyList()` arm is
 * the same empty list.
 *
 * @author ATracer
 */
class ItemActions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.inc"
public:
	/**
	 * Gets the value of the itemActions property. Objects of the following type(s) are allowed in the list SkillLearnAction, SkillUseAction
	 */
	const std::vector<std::unique_ptr<AbstractItemAction>>& getItemActions() const;

	/** @return the first enchant action, nullptr (Java null) if there is none */
	const EnchantItemAction* getEnchantAction() const;

	/** @return the first summon house object action, nullptr (Java null) if there is none */
	const SummonHouseObjectAction* getHouseObjectAction() const;

	/** @return the first craft learn action, nullptr (Java null) if there is none */
	const CraftLearnAction* getCraftLearnAction() const;

	/** @return the first decorate action, nullptr (Java null) if there is none */
	const DecorateAction* getDecorateAction() const;

	/** @return the first dye action, nullptr (Java null) if there is none */
	const DyeAction* getDyeAction() const;

	/** @return the first adopt pet action, nullptr (Java null) if there is none */
	const AdoptPetAction* getAdoptPetAction() const;

	/** @return the first remodel action, nullptr (Java null) if there is none */
	const RemodelAction* getRemodelAction() const;

	/** @return the first polish action, nullptr (Java null) if there is none */
	const PolishAction* getPolishAction() const;

	/** @return the first tuning action, nullptr (Java null) if there is none */
	const TuningAction* getTuningAction() const;

	/** @return the first skill use action, nullptr (Java null) if there is none */
	const SkillUseAction* getSkillUseAction() const;

	/** @return the first ride action, nullptr (Java null) if there is none */
	const RideAction* getRideAction() const;
};

} // namespace aion::gameserver::model::templates::item::actions
