#pragma once

#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.xml.h"

#include <any>
#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.EnchantItemAction.
 * <p>
 * C++ notes (header request m5b3-h01, m5b3-plan.md D8): CM_MANASTONE.java:79-86 constructs this action directly (`new EnchantItemAction()`,
 * a default-constructed template) and calls canAct and the non-virtual five-argument act, which the varargs override calls with
 * `(null, 1)` (EnchantItemAction.java:81-83). In the five-argument act only the supplement item is nullable (hub-headers.md §5.1: the
 * override passes null at that position).
 * The varargs override dereferences parentItem and targetItem to call the overload, so a null throws Java's NullPointerException there.
 * Java's overload, given a null targetItem, first attaches its ItemUseObserver (EnchantItemAction.java:105) and throws only at :108. No
 * caller reaches that: canAct answers false for a null parent or target (:48, :50) and CM_USE_ITEM.java:101-122 calls act only after it,
 * and CM_MANASTONE.java:68 and :71 return before the call when either item is missing.
 *
 * @author Nemiroff, Wakizashi, vlog
 */
class EnchantItemAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	// m5b3-plan.md D8: keeps the base's act visible beside the five-argument overload (hub-headers.md §9.1); redundant while this class
	// declares the override itself, which a using-declaration of the same signature does not conflict with
	using AbstractItemAction::act;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	/** necessary overloading to not change AbstractItemAction */
	void act(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem,
		runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const;

private:
	/**
	 * Check, if the item enchant will be successful
	 * <p>
	 * C++ (header request m5c-h01): supplementItem is nullable like the five-argument act's, which hands it on (the varargs act passes null).
	 *
	 * @param parentItem the enchantment-/manastone to insert
	 * @param targetItem the current item to enchant
	 * @param supplementItem the item to increase the enchant chance (if exists)
	 * @param targetWeapon the fused weapon (if exists)
	 * @return true if successful
	 */
	bool isSuccess(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem,
		runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const;

public:
	/** Java: max_level != null ? max_level : 0 */
	int32_t getMaxLevel() const;

	/** Java: min_level != null ? min_level : 0 */
	int32_t getMinLevel() const;

	/** package-private in Java */
	bool isSupplementAction() const;

private:
	bool checkSupplementLevel(gameobjects::player::Player& player, const ItemTemplate* supplementTemplate,
		const ItemTemplate* targetItemTemplate) const;
};

} // namespace aion::gameserver::model::templates::item::actions
