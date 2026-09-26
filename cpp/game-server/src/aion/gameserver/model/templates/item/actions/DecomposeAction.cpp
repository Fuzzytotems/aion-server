#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/Chance.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ExtractedItemsCollection.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/RandomItem.h"
#include "aion/gameserver/model/templates/item/RandomType.h"
#include "aion/gameserver/model/templates/item/RandomTypeInfo.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIRST_SHOW_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.templates.item.actions.DecomposeAction");

// Java private static chunkEarth/chunkSand/premiumOphidanRecipe (HashMap<Race, int[]>, one array per race), put by the static initializer in
// this order (DecomposeAction.java:60-97)
const std::map<Race, std::vector<int32_t>> DecomposeAction::chunkEarth{
  {Race::ASMODIANS,
    {152000051, 152000052, 152000053, 152000054, 152000055, 152000056, 152000057, 152000058, 152000059, 152000061, 152000062, 152000063,
      152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000451, 152000453,
      152000455, 152000457, 152000459, 152000461, 152000463, 152000465, 152000468, 152000470, 152000551, 152000552, 152000553, 152000554,
      152000556, 152000651, 152000652, 152000653, 152000654, 152000656, 152000751, 152000752, 152000753, 152000754, 152000755, 152000756,
      152000757, 152000758, 152000759, 152000760, 152000762, 152000763, 152000851, 152000852, 152000853, 152000854, 152000855, 152000856,
      152000857, 152000858, 152000860, 152000861, 152001051, 152001052, 152001053, 152001055, 152001056}},
  {Race::ELYOS,
    {152000001, 152000002, 152000003, 152000004, 152000005, 152000006, 152000007, 152000008, 152000009, 152000010, 152000011, 152000012,
      152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000401, 152000403,
      152000405, 152000407, 152000409, 152000411, 152000413, 152000415, 152000417, 152000419, 152000501, 152000502, 152000503, 152000504,
      152000505, 152000601, 152000602, 152000603, 152000604, 152000605, 152000701, 152000702, 152000703, 152000704, 152000705, 152000706,
      152000707, 152000708, 152000709, 152000710, 152000711, 152000712, 152000801, 152000802, 152000803, 152000804, 152000805, 152000806,
      152000807, 152000808, 152000809, 152000810, 152001001, 152001002, 152001003, 152001004, 152001005}}};

const std::map<Race, std::vector<int32_t>> DecomposeAction::chunkSand{
  {Race::ASMODIANS,
    {152000452, 152000454, 152000301, 152000302, 152000303, 152000456, 152000458, 152000103, 152000203, 152000304, 152000305,
      152000306, 152000460, 152000462, 152000105, 152000205, 152000307, 152000309, 152000311, 152000464, 152000466, 152000108,
      152000208, 152000313, 152000315, 152000317, 152000469, 152000471, 152000114, 152000215, 152000320, 152000322, 152000324}},
  {Race::ELYOS,
    {152000402, 152000404, 152000301, 152000302, 152000303, 152000406, 152000408, 152000103, 152000203, 152000304, 152000305,
      152000306, 152000410, 152000412, 152000105, 152000205, 152000307, 152000309, 152000311, 152000414, 152000416, 152000108,
      152000208, 152000313, 152000315, 152000317, 152000418, 152000420, 152000114, 152000215, 152000320, 152000322, 152000324}}};

const std::map<Race, std::vector<int32_t>> DecomposeAction::premiumOphidanRecipe{
  {Race::ASMODIANS,
    {152230698, 152230699, 152230700, 152230701, 152230702, 152230703, 152230704, 152230759, 152230760, 152230761, 152230762, 152230763,
      152230764, 152230839, 152230840, 152230841, 152230842, 152230843, 152230844, 152230845, 152231021, 152231022, 152231023, 152231107,
      152231108, 152231253, 152231254, 152231255, 152231256, 152231257, 152231258, 152231313, 152231314, 152231315, 152231316, 152231317,
      152231318, 152231385, 152231386, 152231387, 152231388, 152231389, 152231390, 152231403, 152231404, 152231405, 152231406, 152231407,
      152231408, 152231421, 152231422, 152231423, 152231424, 152231425, 152231426, 152231439, 152231440, 152231441, 152231442, 152231443,
      152231444, 152231566}},
  {Race::ELYOS,
    {152220709, 152220710, 152220711, 152220712, 152220713, 152220714, 152220715, 152220770, 152220771, 152220772, 152220773, 152220774,
      152220775, 152220850, 152220851, 152220852, 152220853, 152220854, 152220855, 152220856, 152221032, 152221033, 152221034, 152221118,
      152221119, 152221264, 152221265, 152221266, 152221267, 152221268, 152221269, 152221324, 152221325, 152221326, 152221327, 152221328,
      152221329, 152221396, 152221397, 152221398, 152221399, 152221400, 152221401, 152221414, 152221415, 152221416, 152221417, 152221418,
      152221419, 152221432, 152221433, 152221434, 152221435, 152221436, 152221437, 152221450, 152221451, 152221452, 152221453, 152221454,
      152221455, 152221576}}};

namespace {

namespace Rnd = commons::utils::Rnd;
using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_FIRST_SHOW_DECOMPOSABLE;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using services::item::ItemPacketService;
using services::item::ItemService;
using utils::PacketSendUtility;

/** Java validateItemIds(int[]... itemIds) for one array */
void validateItemIds(const dataholders::ItemData& itemData, std::span<const int32_t> ids) {
	for (int32_t itemId : ids) {
		// Java isValidItemId: DataManager.ITEM_DATA.getItemTemplate(itemId) != null
		if (itemData.getItemTemplate(itemId) == nullptr)
			throw runtime::IllegalArgumentException("Decomposable random reward item ID is invalid: " + std::to_string(itemId));
	}
}

/** Java isValidItemId(itemId) (DecomposeAction.java:420-422) at run time */
bool isValidItemId(int32_t itemId) {
	return dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId) != nullptr;
}

/** Java int addition (wraps) */
int32_t javaAdd(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int multiplication (wraps) */
int32_t javaMul(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java `randomType.name().contains(part)` */
bool nameContains(RandomType randomType, std::string_view part) {
	return xml::enumName(randomType).find(part) != std::string_view::npos;
}

/** Java `Rnd.get(selectedStones).getTemplateId()`: Rnd.get of an empty list is null, which the call dereferences */
int32_t randomTemplateId(const std::vector<const ItemTemplate*>& selectedStones) {
	const ItemTemplate* const* selected = Rnd::get(selectedStones);
	if (selected == nullptr)
		throw runtime::NullPointerException("Rnd.get(selectedStones)");
	return (*selected)->getTemplateId();
}

/** Java `map.get(player.getRace())` handed to Rnd.get(int[]): a race without an array is a NullPointerException */
int32_t randomIdOf(const std::map<Race, std::vector<int32_t>>& itemIdsByRace, Race race, std::string_view name) {
	auto found = itemIdsByRace.find(race);
	if (found == itemIdsByRace.end())
		throw runtime::NullPointerException(std::string(name) + ".get(" + std::string(xml::enumName(race)) + ")");
	return Rnd::get(found->second);
}

/** Java `new ItemUpdatePredicate(ItemAddType.DECOMPOSABLE, ItemUpdateType.INC_ITEM_COLLECT)` handed to ItemService.addItem */
void addDecomposedItem(Player& player, int32_t itemId, int32_t count) {
	Ref<ItemService::ItemUpdatePredicate> predicate = ItemService::ItemUpdatePredicate::create(ItemPacketService::ItemAddType::DECOMPOSABLE,
		ItemPacketService::ItemUpdateType::INC_ITEM_COLLECT);
	ItemService::addItem(player, itemId, count, true, *predicate);
}

/**
 * Java: the anonymous ItemUseObserver of act (DecomposeAction.java:141-152, fieldmap key DecomposeAction$1), stored in the player's
 * ObserveController until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It reads only
 * the captured parameters.
 */
struct DecomposeAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 145)
	const Ref<Item> parentItem; // captured param Item parentItem (line 146)

	static Ref<DecomposeAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<DecomposeAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_CANCELED(parentItem->getL10n()));
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	DecomposeAction_ItemUseObserver(Player& playerValue, Item& parentItemValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~DecomposeAction_ItemUseObserver() override = default;
};

} // namespace

bool DecomposeAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	if (player.isDead() || !player.isSpawned())
		return false;
	const std::vector<ExtractedItemsCollection>* itemsCollections = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getInfoByItemId(parentItem->getItemId());
	if (itemsCollections == nullptr || itemsCollections->empty()) {
		if (dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getSelectableItems(parentItem->getItemId())) // selectable decomposable
			return true;
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(parentItem->getL10n()));
		return false;
	}
	if (player.getInventory().isFull() || (player.getInventory().isFullSpecialCube() && containsSpecialCubeItems(*itemsCollections, player))) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_INVENTORY_IS_FULL());
		return false;
	}
	return true;
}

void DecomposeAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	std::optional<std::vector<const ResultedItem*>> selectable = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getSelectableItems(parentItem->getItemId());
	if (selectable) {
		std::erase_if(*selectable, [&player](const ResultedItem* item) { return !item->isObtainableFor(player); });
		PacketSendUtility::sendPacket(player, SM_FIRST_SHOW_DECOMPOSABLE(parentItem->getObjectId(), *selectable));
		return;
	}
	const std::vector<ExtractedItemsCollection>* itemsCollections = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getInfoByItemId(parentItem->getItemId());
	std::optional<std::vector<const ExtractedItemsCollection*>> levelSuitableItems = filterItemsByLevel(player, itemsCollections);
	if (!levelSuitableItems) // Java: Chance.selectElement(null) iterates the null collection
		throw runtime::NullPointerException("levelSuitableItems");
	const ExtractedItemsCollection* selectedCollection = Chance::selectElement(*levelSuitableItems);
	if (selectedCollection == nullptr) // Java: selectedCollection.getRandomItems() on the null Chance.selectElement returns for no element
		throw runtime::NullPointerException("selectedCollection");
	if (selectedCollection->getRandomItems().empty() &&
		std::ranges::none_of(selectedCollection->getItems(), [&player](const ResultedItem& i) { return i.isObtainableFor(player); })) {
		log.warn("Empty decomposable " + std::to_string(parentItem->getItemId()) + " for " + player.toString() + ", class: " +
			std::string(xml::enumName(player.getPlayerClass())) + ", level: " + std::to_string(player.getLevel()));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_FAILED(parentItem->getL10n()));
		return;
	}
	int32_t castingDelay = parentItem->getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, *parentItem, targetItem, selectedCollection);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), castingDelay, 0, 0), true);

	Item& parent = *parentItem;
	Ref<DecomposeAction_ItemUseObserver> observer = DecomposeAction_ItemUseObserver::create(player, parent);

	player.getObserveController()->attach(*observer);
	// Java lambda DecomposeAction.java:155-158 (fieldmap DecomposeAction@L155:92: pins this, observer, player, parentItem and targetItem, and
	// captures the template selectedCollection). `this` and the collection are static data (checked by the Pin, no slot); the nullable
	// targetItem (CM_USE_ITEM passes null unless the client names a target) is captured as a Ref (null for Java null)
	DecomposeAction_ItemUseObserver& itemUseObserver = *observer;
	const ExtractedItemsCollection& selected = *selectedCollection;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parent, &selected},
			[this, &player, &itemUseObserver, &parent, &selected, target = Ref<Item>(targetItem)] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parent, Ptr<Item>(target), &selected);
			},
			castingDelay));
}

bool DecomposeAction::postValidate(gameobjects::player::Player& player, gameobjects::Item& parentItem,
	runtime::Ptr<gameobjects::Item> targetItem) const {
	if (!canAct(player, Ptr<Item>(parentItem), targetItem)) {
		return false;
	}
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM());
		return false;
	}
	return true;
}

void DecomposeAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem,
	runtime::Ptr<gameobjects::Item> targetItem, const ExtractedItemsCollection* selectedCollection) const {
	bool validAction = postValidate(player, parentItem, targetItem);
	if (validAction) {
		player.startCooldown(parentItem);
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(parentItem.getL10n()));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
		for (const ResultedItem& resultItem : selectedCollection->getItems()) {
			if (resultItem.isObtainableFor(player)) {
				int32_t count = Rnd::get(resultItem.getMinCount(), resultItem.getMaxCount());
				addDecomposedItem(player, resultItem.getItemId(), count);
			}
		}
		for (const RandomItem& randomItem : selectedCollection->getRandomItems()) {
			std::optional<RandomType> randomTypeValue = randomItem.getType();
			if (randomTypeValue) {
				const RandomType randomType = *randomTypeValue;
				int32_t randomId = 0;
				int32_t i = 0;
				int32_t itemLvl = parentItem.getItemTemplate()->getLevel();
				switch (randomType) {
					case RandomType::ENCHANTMENT:
						do {
							randomId = javaAdd(javaAdd(166000191, utils::JavaMath::round(static_cast<float>(itemLvl) / 100.0f)), Rnd::nextInt(4));
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
					case RandomType::MANASTONE:
					case RandomType::MANASTONE_COMMON_GRADE_10:
					case RandomType::MANASTONE_COMMON_GRADE_20:
					case RandomType::MANASTONE_COMMON_GRADE_30:
					case RandomType::MANASTONE_COMMON_GRADE_40:
					case RandomType::MANASTONE_COMMON_GRADE_50:
					case RandomType::MANASTONE_COMMON_GRADE_60:
					case RandomType::MANASTONE_COMMON_GRADE_70:
					case RandomType::MANASTONE_RARE_GRADE_10:
					case RandomType::MANASTONE_RARE_GRADE_20:
					case RandomType::MANASTONE_RARE_GRADE_30:
					case RandomType::MANASTONE_RARE_GRADE_40:
					case RandomType::MANASTONE_RARE_GRADE_50:
					case RandomType::MANASTONE_RARE_GRADE_60:
					case RandomType::MANASTONE_RARE_GRADE_70:
					case RandomType::MANASTONE_LEGEND_GRADE_10:
					case RandomType::MANASTONE_LEGEND_GRADE_20:
					case RandomType::MANASTONE_LEGEND_GRADE_30:
					case RandomType::MANASTONE_LEGEND_GRADE_40:
					case RandomType::MANASTONE_LEGEND_GRADE_50:
					case RandomType::MANASTONE_LEGEND_GRADE_60:
					case RandomType::MANASTONE_LEGEND_GRADE_70: {
						if (randomType == RandomType::MANASTONE) // stone level near or equal to item level (if 1, near player level)
							itemLvl = itemLvl % 10 == 0
								? itemLvl
								: javaMul(geoEngine::math::JavaFloat::doubleToInt(
											  std::ceil(static_cast<double>(static_cast<float>(itemLvl == 1 ? player.getLevel() : itemLvl) / 10.0f))),
									  10);
						else
							itemLvl = getLevel(randomType);
						const std::vector<const ItemTemplate*>* stones = dataholders::DataManager::ITEM_DATA->getManastones(itemLvl);
						if (stones == nullptr) {
							log.warn("No lv" + std::to_string(itemLvl) + " manastones found for decomposable random type " +
								std::string(xml::enumName(randomType)));
							break;
						}
						if (randomType != RandomType::MANASTONE) {
							ItemQuality itemQuality;
							if (nameContains(randomType, "RARE"))
								itemQuality = ItemQuality::RARE;
							else if (nameContains(randomType, "LEGEND"))
								itemQuality = ItemQuality::LEGEND;
							else
								itemQuality = ItemQuality::COMMON;
							std::vector<const ItemTemplate*> selectedStones;
							for (const ItemTemplate* t : *stones) {
								if (t->getItemQuality() == itemQuality && t->getName().find(" MP ") == std::string::npos)
									selectedStones.push_back(t);
							}
							randomId = randomTemplateId(selectedStones);
						} else {
							std::vector<const ItemTemplate*> selectedStones;
							for (const ItemTemplate* t : *stones) {
								if (t->getItemQuality() != ItemQuality::LEGEND && t->getName().find(" MP ") == std::string::npos)
									selectedStones.push_back(t);
							}
							randomId = randomTemplateId(selectedStones);
						}
						break;
					}
					case RandomType::SPECIAL_MANASTONE_RARE_GRADE:
					case RandomType::SPECIAL_MANASTONE_LEGEND_GRADE:
					case RandomType::SPECIAL_MANASTONE_UNIQUE_GRADE:
					case RandomType::SPECIAL_MANASTONE_EPIC_GRADE: {
						const std::vector<const ItemTemplate*>* ancientStones = dataholders::DataManager::ITEM_DATA->getAncientManastones(getLevel(randomType));
						if (ancientStones == nullptr) {
							log.warn("No ancient manastones found for decomposable random type " + std::string(xml::enumName(randomType)));
							break;
						}
						ItemQuality itemQuality;
						if (nameContains(randomType, "RARE"))
							itemQuality = ItemQuality::RARE;
						else if (nameContains(randomType, "LEGEND"))
							itemQuality = ItemQuality::LEGEND;
						else if (nameContains(randomType, "UNIQUE"))
							itemQuality = ItemQuality::UNIQUE;
						else if (nameContains(randomType, "EPIC"))
							itemQuality = ItemQuality::EPIC;
						else
							itemQuality = ItemQuality::COMMON;
						std::vector<const ItemTemplate*> selectedStones;
						for (const ItemTemplate* t : *ancientStones) {
							if (t->getItemQuality() == itemQuality && t->getName().find(" MP ") == std::string::npos)
								selectedStones.push_back(t);
						}
						randomId = randomTemplateId(selectedStones);
						break;
					}
					case RandomType::CHUNK_EARTH:
						randomId = randomIdOf(chunkEarth, player.getRace(), "chunkEarth");
						break;
					case RandomType::CHUNK_SAND:
						randomId = randomIdOf(chunkSand, player.getRace(), "chunkSand");
						break;
					case RandomType::PREMIUM_OPHIDAN_RECIPE:
						randomId = randomIdOf(premiumOphidanRecipe, player.getRace(), "premiumOphidanRecipe");
						break;
					case RandomType::CHUNK_ROCK:
						randomId = Rnd::get(chunkRock);
						break;
					case RandomType::CHUNK_GEMSTONE:
						randomId = Rnd::get(chunkGemstone);
						break;
					case RandomType::SCROLLS:
						randomId = Rnd::get(scrolls);
						break;
					case RandomType::POTION:
						randomId = Rnd::get(potion);
						break;
					case RandomType::LESSER_POTIONS:
						randomId = Rnd::get(lesser_potions);
						break;
					case RandomType::POTION_50:
						randomId = Rnd::get(potion_50);
						break;
					case RandomType::ILLUSION_GODSTONE:
						randomId = Rnd::get(illusion_godstones);
						break;
					case RandomType::ANCIENTITEMS:
						do {
							randomId = Rnd::get(186000051, 186000066);
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
					case RandomType::ANCIENT_CROWN:
						do {
							randomId = Rnd::get(186000051, 186000054);
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
					case RandomType::ANCIENT_GOBLET:
						do {
							randomId = Rnd::get(186000055, 186000058);
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
					case RandomType::ANCIENT_SEAL:
						do {
							randomId = Rnd::get(186000059, 186000062);
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
					case RandomType::ANCIENT_ICON:
						do {
							randomId = Rnd::get(186000063, 186000066);
							i++;
							if (i > 50) {
								randomId = 0;
								break;
							}
						} while (!isValidItemId(randomId));
						break;
				}
				if (randomId != 0) {
					int32_t count = Rnd::get(randomItem.getMinCount(), randomItem.getMaxCount());
					addDecomposedItem(player, randomId, count);
				}
			}
		}
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, validAction ? 1 : 2, 0), true);
}

std::optional<std::vector<const ExtractedItemsCollection*>> DecomposeAction::filterItemsByLevel(gameobjects::player::Player& player,
	const std::vector<ExtractedItemsCollection>* itemsCollections) const {
	if (itemsCollections == nullptr) {
		return std::nullopt;
	}
	int32_t playerLevel = player.getLevel();
	std::vector<const ExtractedItemsCollection*> result;
	for (const ExtractedItemsCollection& collection : *itemsCollections) {
		if (collection.getMinLevel() > playerLevel || collection.getMaxLevel() < playerLevel) {
			continue;
		}
		result.push_back(&collection);
	}
	return result;
}

bool DecomposeAction::containsSpecialCubeItems(const std::vector<ExtractedItemsCollection>& itemGroups,
	gameobjects::player::Player& player) const {
	for (const ExtractedItemsCollection& items : itemGroups) {
		if (items.getMinLevel() > player.getLevel() || items.getMaxLevel() < player.getLevel())
			continue;
		for (const ResultedItem& item : items.getItems()) {
			if (item.isObtainableFor(player)) {
				const ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(item.getItemId());
				if (itemTemplate == nullptr)
					log.error("Detected invalid item id during decompose action " + std::to_string(item.getItemId()));
				else if (itemTemplate->getExtraInventoryId() > 0)
					return true;
			}
		}
	}
	return false;
}

void DecomposeAction::validateRandomItemIds(const dataholders::ItemData& itemData) {
	// Java: chunkEarth.values(), then chunkSand.values(). A HashMap with enum keys iterates in the keys' identity-hash order, which Java leaves
	// open; the C++ takes the static initializer's put order, ASMODIANS first (only the first invalid id of the message can differ)
	validateItemIds(itemData, chunkEarth.at(Race::ASMODIANS));
	validateItemIds(itemData, chunkEarth.at(Race::ELYOS));
	validateItemIds(itemData, chunkSand.at(Race::ASMODIANS));
	validateItemIds(itemData, chunkSand.at(Race::ELYOS));
	for (std::span<const int32_t> ids : {std::span<const int32_t>(chunkRock), std::span<const int32_t>(chunkGemstone),
	                                     std::span<const int32_t>(scrolls), std::span<const int32_t>(potion), std::span<const int32_t>(lesser_potions),
	                                     std::span<const int32_t>(potion_50), std::span<const int32_t>(illusion_godstones)})
		validateItemIds(itemData, ids);
}

} // namespace aion::gameserver::model::templates::item::actions
