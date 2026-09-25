#pragma once

#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.h"

#include <any>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <vector>

#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.DecomposeAction.
 * <p>
 * C++ (header request m5c-h02): the private helpers and the static reward tables of Java's act and finishUse are declared here. The tables are
 * constants (hub-headers.md §11.1, fieldmap.toml): Java fills the three maps only in its static initializer and never writes the arrays. The
 * anonymous ItemUseObserver of act is the callback struct fieldmap.py prints, defined in DecomposeAction.cpp.
 *
 * @author oslo(a00441234)
 */
class DecomposeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.inc"
private:
	// Java private static Map<Race, int[]> (DecomposeAction.java:40-42), filled only by the static initializer (:60-97); defined in the .cpp
	static const std::map<Race, std::vector<int32_t>> chunkEarth;
	static const std::map<Race, std::vector<int32_t>> chunkSand;
	static const std::map<Race, std::vector<int32_t>> premiumOphidanRecipe;

	// Java private static int[] with literal contents, never written (DecomposeAction.java:44-58)
	static constexpr std::array<int32_t, 15> chunkRock{152000104, 152000107, 152000113, 152000204, 152000207, 152000214, 152000307, 152000309,
		152000311, 152000313, 152000315, 152000317, 152000320, 152000322, 152000324};
	static constexpr std::array<int32_t, 8> chunkGemstone{152000112, 152000116, 152000212, 152000213, 152000217, 152000326, 152000327, 152000328};
	static constexpr std::array<int32_t, 7> scrolls{164000073, 164000134, 164000076, 164000079, 164000122, 164000131, 164000118};
	static constexpr std::array<int32_t, 6> potion{162000045, 162000079, 162000016, 162000021, 162000027, 162000023};
	static constexpr std::array<int32_t, 7> lesser_potions{162000003, 162000008, 162000042, 162000022, 162000013, 162000018, 162000047};
	static constexpr std::array<int32_t, 7> potion_50{162000075, 162000076, 162000077, 162000078, 162000079, 162000080, 162000081};
	static constexpr std::array<int32_t, 17> illusion_godstones{168000161, 168000162, 168000163, 168000164, 168000165, 168000166, 168000167,
		168000168, 168000169, 168000170, 168000171, 168000172, 168000173, 168000174, 168000175, 168000176, 168000177};

public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

private:
	/** @param targetItem nullable: act's target item, which CM_USE_ITEM passes as null unless the client names one (CM_USE_ITEM.java:64-72) */
	bool postValidate(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem) const;

	/** @param targetItem nullable, as in postValidate */
	void finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		const ExtractedItemsCollection* selectedCollection) const;

	/**
	 * Add to result collection only items wich suits player's level
	 * <p>
	 * C++: Java's null list is nullptr (DecomposableItemsData::getInfoByItemId), and the null Java returns for it std::nullopt
	 */
	std::optional<std::vector<const ExtractedItemsCollection*>> filterItemsByLevel(gameobjects::player::Player& player,
		const std::vector<ExtractedItemsCollection>* itemsCollections) const;

	bool containsSpecialCubeItems(const std::vector<ExtractedItemsCollection>& itemGroups, gameobjects::player::Player& player) const;

public:
	/**
	 * Checks that every random reward item id of the decompose action exists: the last DataManager post-processing step.
	 * <p>
	 * C++: Java reads DataManager.ITEM_DATA, which is not published yet during the C++ post-processing, so the holder is passed (static-data.md
	 * §3.3, header request holders-2).
	 *
	 * @throws IllegalArgumentException("Decomposable random reward item ID is invalid: <id>") for the first unknown id
	 */
	static void validateRandomItemIds(const dataholders::ItemData& itemData);
};

} // namespace aion::gameserver::model::templates::item::actions
