#include "aion/gameserver/handlers/playercommands/Preview.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/itemset/ItemPart.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Preview);

// Java Preview.java:33 (the static field's initializer, see the header)
runtime::ConcurrentHashMap<int32_t, runtime::FutureRef>& Preview::PREVIEW_RESETS() { // parity: the accessor of the static field below
	static auto* const PREVIEW_RESETS = new runtime::ConcurrentHashMap<int32_t, runtime::FutureRef>(AION_LOCK_CLASS(Preview::PREVIEW_RESETS#stripe)); // parity= private static final Map<Integer, ScheduledFuture<?>> PREVIEW_RESETS = new ConcurrentHashMap<>(); private static final int PREVIEW_TIME_SECONDS = 10;
	return *PREVIEW_RESETS; // parity: (the accessor; the map is never destroyed, as a static of a loaded class)
} // parity: (the accessor)

Preview::Preview()
	: PlayerCommand("preview", "Previews equipment and emotion cards.",
		  "<emotion card item> - Previews the emotion.\n"
		  "<color> - Previews your equipped items in the specified color (dye item, color name or color HEX code).\n"
		  "<item(s)> [color] - Previews the specified equipment on your character (default: standard item color, optional: dye item, color name or color HEX code).\n"
		  "Multiple items can be separated by commas or spaces.\n"
		  "If a single item is given and it's a part of an item set, you will get a preview of the whole item set.\n") {
}

// Java Preview.java:47-56
void Preview::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	std::vector<ItemParam> itemParams = parse(params);
	if (itemParams.empty()) // parity: List.getFirst() of an empty list (every parameter only commas): Java's NoSuchElementException, explicit
		throw runtime::NoSuchElementException("No such element"); // parity: (the same; ChatCommand.run logs it)
	if (!previewEmotion(player, itemParams.front()))
		previewEquipment(player, itemParams);
}

// Java Preview.java:58-74
bool Preview::previewEmotion(Player& player, const ItemParam& itemParam) {
	if (itemParam.itemTemplate == nullptr || itemParam.itemTemplate->getActions() == nullptr)
		return false;
	for (const std::unique_ptr<AbstractItemAction>& itemAction : itemParam.itemTemplate->getActions()->getItemActions()) {
		const EmotionLearnAction* emotionLearnAction = dynamic_cast<const EmotionLearnAction*>(itemAction.get()); // parity: the pattern variable of the instanceof below
		if (emotionLearnAction != nullptr) { // parity= if (itemAction instanceof EmotionLearnAction emotionLearnAction) {
			if ((player.getState() & ~getId(CreatureState::POWERSHARD)) > getId(CreatureState::ACTIVE)) { // prevent bugged animations
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_CURRENT_STANCE());
			} else {
				int32_t targetObjectId = player.getTarget() != nullptr ? player.getTarget()->getObjectId() : 0;
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_EMOTION(player, EmotionType::EMOTE_END));
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_EMOTION(player, EmotionType::EMOTE, emotionLearnAction->getEmotionId(), targetObjectId));
			}
			return true;
		}
	}
	return false;
}

// Java Preview.java:76-91
void Preview::previewEquipment(Player& player, std::vector<ItemParam>& itemParams) {
	std::optional<int32_t> itemColor = std::nullopt; // null = default item color
	std::string colorText = "default";
	for (const ItemParam& itemParam : itemParams) {
		itemColor = itemParam.dyeColor();
		if (itemColor != std::nullopt) {
			if (itemParam.itemTemplate == nullptr)
				colorText = ChatUtil::color("#" + std::format("{:06X}", static_cast<uint32_t>(*itemColor & 0xFFFFFF)), *itemColor); // parity= colorText = ChatUtil.color("#" + String.format("%06X", itemColor & 0xFFFFFF), itemColor);
			else
				colorText = ChatUtil::item(itemParam.itemTemplate->getTemplateId());
			itemParams.erase(std::find(itemParams.begin(), itemParams.end(), itemParam)); // parity= itemParams.remove(itemParam);
			break;
		}
	}
	std::vector<const ItemTemplate*> equipment; // parity= previewEquipment(player, itemParams.stream().map(ItemParam::itemTemplate).toList(), itemColor, colorText);
	for (const ItemParam& itemParam : itemParams) // parity: (the same statement)
		equipment.push_back(itemParam.itemTemplate); // parity: (the same statement)
	previewEquipment(player, equipment, itemColor, colorText); // parity: (the same statement)
}

// Java Preview.java:93-144
void Preview::previewEquipment(Player& player, const std::vector<const ItemTemplate*>& equipment, std::optional<int32_t> itemColor, const std::string& colorText) {
	if (!std::all_of(equipment.begin(), equipment.end(), [this, &player](const ItemTemplate* itemTemplate) { return validateForPreview(player, itemTemplate); })) // parity= if (!equipment.stream().allMatch(itemTemplate -> validateForPreview(player, itemTemplate)))
		return;
	if (itemColor != std::nullopt && !equipment.empty() && std::none_of(equipment.begin(), equipment.end(), [](const ItemTemplate* itemTemplate) { return itemTemplate->isItemDyePermitted(); })) { // parity= if (itemColor != null && !equipment.isEmpty() && equipment.stream().noneMatch(ItemTemplate::isItemDyePermitted)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_CHANGE_ERROR_CANNOTDYE(equipment.front()->getL10n()));
		return;
	}
	std::string itemNames = "";
	int64_t previewItemsSlotMask = 0;
	std::vector<runtime::Ref<Item>> previewItems;
	if (equipment.size() == 1 && equipment.front()->isItemSet()) { // preview whole set
		const ItemSetTemplate* itemSet = equipment.front()->getItemSet();
		for (const ItemPart& part : itemSet->getItempart())
			previewItemsSlotMask |= addFakeItem(previewItems, previewItemsSlotMask, part.getItemId(), itemColor);
	} else {
		for (const ItemTemplate* template_ : equipment) // parity= for (ItemTemplate template : equipment)
			previewItemsSlotMask |= addFakeItem(previewItems, previewItemsSlotMask, template_->getTemplateId(), itemColor);
	}
	int32_t previewRobotId = 0;
	for (const runtime::Ref<Item>& previewItem : previewItems) {
		itemNames += "\n\t" + ChatUtil::item(previewItem->getItemId());
		if (player.isInRobotMode() && previewItem->getItemTemplate()->getItemGroup() == ItemGroup::KEYBLADE)
			previewRobotId = previewItem->getItemTemplate()->getRobotId();
	}
	addOwnEquipment(player, previewItems, previewItemsSlotMask, itemColor);
	std::stable_sort(previewItems.begin(), previewItems.end(), [](const runtime::Ref<Item>& a, const runtime::Ref<Item>& b) { return a->getEquipmentSlot() < b->getEquipmentSlot(); }); // parity= previewItems.sort(Comparator.comparingLong(Item::getEquipmentSlot)); // order by equipment slot ids (ascending) to avoid display bugs
	int32_t display = player.getPlayerSettings()->getDisplay() | network::aion::serverpackets::SM_CUSTOM_SETTINGS::HIDE_LEGION_CLOAK;
	if (std::any_of(previewItems.begin(), previewItems.end(), [](const runtime::Ref<Item>& item) { return item->getEquipmentSlot() == getSlotIdMask(ItemSlot::HELMET); })) { // parity= if (previewItems.stream().anyMatch(item -> item.getEquipmentSlot() == ItemSlot.HELMET.getSlotIdMask())) {
		display &= ~network::aion::serverpackets::SM_CUSTOM_SETTINGS::HIDE_HELMET;
	}
	if (std::any_of(previewItems.begin(), previewItems.end(), [](const runtime::Ref<Item>& item) { return item->getEquipmentSlot() == getSlotIdMask(ItemSlot::PLUME); })) { // parity= if (previewItems.stream().anyMatch(item -> item.getEquipmentSlot() == ItemSlot.PLUME.getSlotIdMask())) {
		display &= ~network::aion::serverpackets::SM_CUSTOM_SETTINGS::HIDE_PLUME;
	}
	std::vector<runtime::Ptr<Item>> appearance(previewItems.begin(), previewItems.end()); // parity: the packet's List<Item>
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CUSTOM_SETTINGS(player.getObjectId(), 1, display, player.getPlayerSettings()->getDeny()));
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_UPDATE_PLAYER_APPEARANCE(player.getObjectId(), appearance)); // parity= PacketSendUtility.sendPacket(player, new SM_UPDATE_PLAYER_APPEARANCE(player.getObjectId(), previewItems));
	int32_t switchRobotAnimationSeconds = 0;
	if (previewRobotId != 0) {
		switchRobotAnimationSeconds = 1;
		updateRobotAppearance(player, previewRobotId);
	}
	schedulePreviewReset(player, PREVIEW_TIME_SECONDS + switchRobotAnimationSeconds, previewRobotId != 0);
	if (equipment.empty())
		sendInfo(player, "Previewing your equipment for " + std::to_string(PREVIEW_TIME_SECONDS) + " seconds in color " + colorText);
	else
		sendInfo(player, "Previewing the following items for " + std::to_string(PREVIEW_TIME_SECONDS) + " seconds (color: " + colorText + "):" + itemNames);
}

// Java Preview.java:146-165
bool Preview::validateForPreview(Player& player, const ItemTemplate* itemTemplate) {
	if (itemTemplate == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NO_TARGET_ITEM());
		return false;
	} else if (itemTemplate->getEquipmentType() == EquipType::NONE) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CHANGE_ITEM_SKIN_PREVIEW_INVALID_COSMETIC());
		return false;
	} else if (itemTemplate->getRace() == player.getOppositeRace()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PREVIEW_INVALID_RACE());
		return false;
	} else if (itemTemplate->getUseLimits()->getGenderPermitted() != std::nullopt && itemTemplate->getUseLimits()->getGenderPermitted() != player.getGender()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PREVIEW_INVALID_GENDER());
		return false;
	} else if (!model::items::isVisible(itemTemplate->getItemSlot())) { // parity= } else if (!ItemSlot.isVisible(itemTemplate.getItemSlot())) {
		sendInfo(player, itemTemplate->getL10n() + " is no visible equipment.");
		return false;
	}
	return true;
}

// Java Preview.java:167-176. The split regex ",|(?<=[^,])(?=\\[)|(?<=[\\]])(?=[^\\[])" (look-behind, which std::regex lacks) is spelled
// out: a comma is consumed; between a character other than a comma and '[', and between ']' and a character other than '[', the text is
// cut without consuming anything; Java's split drops the trailing empty strings and never cuts at index 0 without consuming.
std::vector<Preview::ItemParam> Preview::parse(std::span<const std::string> params) {
	std::vector<ItemParam> itemParams;
	for (const std::string& param : params) {
		std::vector<std::string> ids; // parity= String[] ids = param.split(",|(?<=[^,])(?=\\[)|(?<=[\\]])(?=[^\\[])"); // split on comma and between item tags (square brackets)
		size_t start = 0; // parity: (the same statement)
		for (size_t i = 0; i < param.size(); i++) { // parity: (the same statement)
			if (param[i] == ',') { // parity: (the same statement)
				ids.push_back(param.substr(start, i - start)); // parity: (the same statement)
				start = i + 1; // parity: (the same statement)
			} else if (i > 0 && ((param[i] == '[' && param[i - 1] != ',') || (param[i - 1] == ']' && param[i] != '['))) { // parity: (the same statement)
				ids.push_back(param.substr(start, i - start)); // parity: (the same statement)
				start = i; // parity: (the same statement)
			} // parity: (the same statement)
		} // parity: (the same statement)
		ids.push_back(param.substr(start)); // parity: (the same statement)
		while (ids.size() > 1 && ids.back().empty()) // parity: (the same statement)
			ids.pop_back(); // parity: (the same statement)
		if (ids.size() == 1 && ids.back().empty() && !param.empty()) // parity: (the same statement; all parts empty: Java's split returns no element)
			ids.pop_back(); // parity: (the same statement)
		for (const std::string& id : ids) {
			itemParams.push_back(ItemParam{id, DataManager::ITEM_DATA->getItemTemplate(ChatUtil::getItemId(id))}); // parity= itemParams.add(new ItemParam(id, DataManager.ITEM_DATA.getItemTemplate(ChatUtil.getItemId(id))));
		}
	}
	return itemParams;
}

/**
 * Java Preview.java:178-200
 *
 * @return Equipment slot mask of the preview item, 0 if it was not added
 */
int64_t Preview::addFakeItem(std::vector<runtime::Ref<Item>>& items, int64_t previewItemsSlotMask, int32_t itemId, std::optional<int32_t> itemColor) {
	const ItemTemplate* itemTemplate = DataManager::ITEM_DATA->getItemTemplate(itemId);
	int64_t itemSlotMask = itemTemplate->getItemSlot();
	if (!model::items::isVisible(itemSlotMask)) // parity= if (!ItemSlot.isVisible(itemSlotMask)) // don't add invisible items (like rings or belts)
		return 0;
	int64_t occupiedSlots = previewItemsSlotMask & itemSlotMask;
	if (occupiedSlots == itemSlotMask) // an item of that kind is already present in the list
		return 0;
	if (itemTemplate->isTwoHandWeapon() && occupiedSlots != 0) // only allow two-handed if both hands are free
		return 0;
	if (!itemTemplate->isTwoHandWeapon())
		itemSlotMask = getFirstFreeSlot(itemSlotMask, occupiedSlots); // select the correct slot for CL_MULTISLOT, weapons, earrings and power shards
	runtime::Ref<Item> previewItem = Item::create(0, itemTemplate, 1, true, itemSlotMask); // parity= Item previewItem = new Item(0, itemTemplate, 1, true, itemSlotMask); // ObjId 0 to avoid allocating new IDFactory IDs (it'll not be used anywhere)
	if (itemTemplate->isItemDyePermitted())
		previewItem->setItemColor(itemColor);
	items.push_back(previewItem); // parity= items.add(previewItem);
	return itemSlotMask;
}

// Java Preview.java:202-205
int64_t Preview::getFirstFreeSlot(int64_t targetSlots, int64_t occupiedSlots) {
	int64_t freeSlots = targetSlots & ~occupiedSlots;
	return static_cast<int64_t>(static_cast<uint64_t>(freeSlots) & (0 - static_cast<uint64_t>(freeSlots))); // parity= return Long.lowestOneBit(freeSlots);
}

// Java Preview.java:207-214
void Preview::addOwnEquipment(Player& player, std::vector<runtime::Ref<Item>>& previewItems, int64_t previewItemsSlotMask, std::optional<int32_t> itemColor) {
	bool previewContainsMainHandWeapon = (previewItemsSlotMask & getSlotIdMask(ItemSlot::MAIN_HAND)) != 0;
	for (const runtime::Ptr<Item>& visibleEquipment : player.getEquipment().getEquippedForAppearance()) {
		if (previewContainsMainHandWeapon && visibleEquipment->getItemTemplate()->isOneHandWeapon())
			continue; // don't show own weapon in off-hand if player wants to preview a specific weapon
		previewItemsSlotMask |= addFakeItem(previewItems, previewItemsSlotMask, visibleEquipment->getItemId(), itemColor);
	}
}

// Java Preview.java:216-234
void Preview::schedulePreviewReset(Player& player, int32_t duration, bool previewRobot) {
	PREVIEW_RESETS().compute(player.getObjectId(), [&player, duration, previewRobot](const runtime::Ptr<runtime::Future>& previousTask) -> runtime::FutureRef { // parity= PREVIEW_RESETS.compute(player.getObjectId(), (_, resetTask) -> {
		runtime::FutureRef resetTask = previousTask; // parity: the lambda's parameter
		if (resetTask != nullptr) { // cancel previous scheduled preview reset thread
			if (!previewRobot && player.isInRobotMode()) // restore robot appearance in case it was previewed just a few seconds ago
				updateRobotAppearance(player, player.getRobotId());
			resetTask->cancel(true);
		}
		resetTask = ThreadPoolManager::getInstance().schedule({&player}, [&player, previewRobot] { // parity= resetTask = ThreadPoolManager.getInstance().schedule(() -> {
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CUSTOM_SETTINGS(player));
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_UPDATE_PLAYER_APPEARANCE(player.getObjectId(), player.getEquipment().getEquippedForAppearance()));
			if (previewRobot && player.isInRobotMode())
				updateRobotAppearance(player, player.getRobotId());
			PacketSendUtility::sendMessage(player, "Preview time ended.");
			// lint: L20 the remove runs in the scheduled reset task, after this compute has returned (no nested stripe write)
			PREVIEW_RESETS().remove(player.getObjectId()); // parity= PREVIEW_RESETS.remove(player.getObjectId());
		}, duration * int64_t{1000}); // parity= }, duration * 1000L);
		return resetTask;
	});
}

// Java Preview.java:236-239
void Preview::updateRobotAppearance(Player& player, int32_t robotId) {
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RIDE_ROBOT(player, 0));
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RIDE_ROBOT(player, robotId));
}

// Java Preview.java:241
static_assert(sizeof(Preview::ItemParam) > 0); // parity= private record ItemParam(String input, ItemTemplate itemTemplate) {

// Java Preview.java:243-261: ItemParam.dyeColor
std::optional<int32_t> Preview::ItemParam::dyeColor() const {
	if (itemTemplate != nullptr) {
		if (itemTemplate->getActions() == nullptr || itemTemplate->getActions()->getDyeAction() == nullptr)
			return std::nullopt;
		return itemTemplate->getActions()->getDyeAction()->getColor();
	}
	std::string colorParam = input;
	// try to get color by name
	std::optional<utils::JavaColor> named = utils::JavaColor::byName(commons::utils::StringUtils::toUpperCase(colorParam)); // parity= try { return ((Color) Color.class.getField(colorParam.toUpperCase()).get(null)).getRGB();
	if (named) // parity: the getField that did not throw
		return named->getRGB(); // parity: (the same statement)
	// parity= } catch (Exception e) {
	// try to get color by hex code
	if (commons::utils::StringUtils::utf16Length(colorParam) <= 8) { // parity= if (colorParam.length() <= 8) {
		if (colorParam.starts_with("#")) // parity= if (colorParam.startsWith("#"))
			colorParam = commons::utils::StringUtils::substring(colorParam, 1);
		else if (colorParam.starts_with("0x") || colorParam.starts_with("0X")) // parity= else if (colorParam.startsWith("0x") || colorParam.startsWith("0X"))
			colorParam = commons::utils::StringUtils::substring(colorParam, 2);
	}
	try {
		return commons::utils::parseInt(colorParam, 16); // parity= return Integer.valueOf(colorParam, 16);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException _) {
		return std::nullopt;
	}
}

} // namespace aion::gameserver::handlers::playercommands
