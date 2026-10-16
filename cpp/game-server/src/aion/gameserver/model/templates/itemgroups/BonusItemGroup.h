#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.xml.h"

#include "aion/gameserver/model/templates/itemgroups/fwd.h"

namespace aion::gameserver::model::templates::itemgroups {

/**
 * Java com.aionemu.gameserver.model.templates.itemgroups.BonusItemGroup.
 * <p>
 * C++: Java's abstract getItems() would be a new virtual function of a frozen shell. The C++-only member `groupClass`, set by the constructor of
 * each subclass, names the concrete Java class; getItems() of this class returns the entries of the concrete group as ItemRaceEntry pointers
 * (their overridable functions dispatch on the entry class, ItemRaceEntry.h), and each subclass declares its typed non-virtual getItems().
 * Java implements Chance: `Chance::selectElement` is a template that only needs getChance().
 *
 * @author Rolandas
 */
class BonusItemGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.xml.inc"
public:
	/** C++ only: the concrete Java class of a group */
	enum class GroupClass : uint8_t { CRAFT_ITEM, CRAFT_RECIPE, EVENT, MANASTONE, FOOD, MEDICINE, ORE, GATHER, ENCHANT, BOSS, MEDAL };

private:
	/** C++ only: the dispatch tag, set once through the protected constructor of the concrete class */
	GroupClass groupClass = GroupClass::CRAFT_ITEM;

protected:
	/** C++ only: the constructor of the subclasses, naming their concrete class (Java's BonusItemGroup is abstract) */
	explicit BonusItemGroup(GroupClass value) noexcept : groupClass(value) {}

public:
	GroupClass getGroupClass() const { return groupClass; }

	float getChance() const { return chance; }

	/** Java abstract getItems(): the entries of the concrete group (template pointers into the bound list) */
	std::vector<const ItemRaceEntry*> getItems() const;
};

} // namespace aion::gameserver::model::templates::itemgroups
