#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

// Java private static chunkEarth/chunkSand (HashMap<Race, int[]>, one array per race) and the int[] constants (DecomposeAction.java:40-96).
// C++: file-local constants; the static members fieldmap.py lists (and premiumOphidanRecipe) come with the port of act() (P5-07).
constexpr std::array<int32_t, 69> chunkEarth_ASMODIANS{
  152000051, 152000052, 152000053, 152000054, 152000055, 152000056, 152000057, 152000058, 152000059, 152000061, 152000062, 152000063,
  152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000451, 152000453,
  152000455, 152000457, 152000459, 152000461, 152000463, 152000465, 152000468, 152000470, 152000551, 152000552, 152000553, 152000554,
  152000556, 152000651, 152000652, 152000653, 152000654, 152000656, 152000751, 152000752, 152000753, 152000754, 152000755, 152000756,
  152000757, 152000758, 152000759, 152000760, 152000762, 152000763, 152000851, 152000852, 152000853, 152000854, 152000855, 152000856,
  152000857, 152000858, 152000860, 152000861, 152001051, 152001052, 152001053, 152001055, 152001056};

constexpr std::array<int32_t, 69> chunkEarth_ELYOS{
  152000001, 152000002, 152000003, 152000004, 152000005, 152000006, 152000007, 152000008, 152000009, 152000010, 152000011, 152000012,
  152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000401, 152000403,
  152000405, 152000407, 152000409, 152000411, 152000413, 152000415, 152000417, 152000419, 152000501, 152000502, 152000503, 152000504,
  152000505, 152000601, 152000602, 152000603, 152000604, 152000605, 152000701, 152000702, 152000703, 152000704, 152000705, 152000706,
  152000707, 152000708, 152000709, 152000710, 152000711, 152000712, 152000801, 152000802, 152000803, 152000804, 152000805, 152000806,
  152000807, 152000808, 152000809, 152000810, 152001001, 152001002, 152001003, 152001004, 152001005};

constexpr std::array<int32_t, 33> chunkSand_ASMODIANS{
  152000452, 152000454, 152000301, 152000302, 152000303, 152000456, 152000458, 152000103, 152000203, 152000304, 152000305,
  152000306, 152000460, 152000462, 152000105, 152000205, 152000307, 152000309, 152000311, 152000464, 152000466, 152000108,
  152000208, 152000313, 152000315, 152000317, 152000469, 152000471, 152000114, 152000215, 152000320, 152000322, 152000324};

constexpr std::array<int32_t, 33> chunkSand_ELYOS{152000402, 152000404, 152000301, 152000302, 152000303, 152000406, 152000408, 152000103, 152000203,
                                                  152000304, 152000305, 152000306, 152000410, 152000412, 152000105, 152000205, 152000307, 152000309,
                                                  152000311, 152000414, 152000416, 152000108, 152000208, 152000313, 152000315, 152000317, 152000418,
                                                  152000420, 152000114, 152000215, 152000320, 152000322, 152000324};

constexpr std::array<int32_t, 15> chunkRock{152000104, 152000107, 152000113, 152000204, 152000207, 152000214, 152000307, 152000309,
                                            152000311, 152000313, 152000315, 152000317, 152000320, 152000322, 152000324};

constexpr std::array<int32_t, 8> chunkGemstone{152000112, 152000116, 152000212, 152000213, 152000217, 152000326, 152000327, 152000328};

constexpr std::array<int32_t, 7> scrolls{164000073, 164000134, 164000076, 164000079, 164000122, 164000131, 164000118};

constexpr std::array<int32_t, 6> potion{162000045, 162000079, 162000016, 162000021, 162000027, 162000023};

constexpr std::array<int32_t, 7> lesser_potions{162000003, 162000008, 162000042, 162000022, 162000013, 162000018, 162000047};

constexpr std::array<int32_t, 7> potion_50{162000075, 162000076, 162000077, 162000078, 162000079, 162000080, 162000081};

constexpr std::array<int32_t, 17> illusion_godstones{168000161, 168000162, 168000163, 168000164, 168000165, 168000166,
                                                     168000167, 168000168, 168000169, 168000170, 168000171, 168000172,
                                                     168000173, 168000174, 168000175, 168000176, 168000177};

/** Java validateItemIds(int[]... itemIds) for one array */
void validateItemIds(const dataholders::ItemData& itemData, std::span<const int32_t> ids) {
	for (int32_t itemId : ids) {
		// Java isValidItemId: DataManager.ITEM_DATA.getItemTemplate(itemId) != null
		if (itemData.getItemTemplate(itemId) == nullptr)
			throw runtime::IllegalArgumentException("Decomposable random reward item ID is invalid: " + std::to_string(itemId));
	}
}

} // namespace

bool DecomposeAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void DecomposeAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void DecomposeAction::validateRandomItemIds(const dataholders::ItemData& itemData) {
	// Java: chunkEarth.values(), then chunkSand.values() (HashMap<Race, int[]> order: only the first invalid id of the message can differ)
	validateItemIds(itemData, chunkEarth_ASMODIANS);
	validateItemIds(itemData, chunkEarth_ELYOS);
	validateItemIds(itemData, chunkSand_ASMODIANS);
	validateItemIds(itemData, chunkSand_ELYOS);
	for (std::span<const int32_t> ids : {std::span<const int32_t>(chunkRock), std::span<const int32_t>(chunkGemstone),
	                                     std::span<const int32_t>(scrolls), std::span<const int32_t>(potion), std::span<const int32_t>(lesser_potions),
	                                     std::span<const int32_t>(potion_50), std::span<const int32_t>(illusion_godstones)})
		validateItemIds(itemData, ids);
}

} // namespace aion::gameserver::model::templates::item::actions
