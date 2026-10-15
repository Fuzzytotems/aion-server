#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .preview: previews equipment and emotion cards.
 * <p>
 * C++: Java's `private static final Map<Integer, ScheduledFuture<?>> PREVIEW_RESETS = new ConcurrentHashMap<>()` is PREVIEW_RESETS(), a
 * function-local static (handler files have no namespace-scope statics). The record ItemParam is the nested struct of the same name.
 */
class Preview : public PlayerCommand {
public:
	Preview();

	void execute(Player& player, std::span<const std::string> params) override;

	/** Java: private record ItemParam(String input, ItemTemplate itemTemplate); public here for the parity line of Preview.cpp */
	struct ItemParam {
		std::string input;
		const ItemTemplate* itemTemplate;

		/** Java: the record's equals (List.remove(Object)) */
		bool operator==(const ItemParam& other) const = default;

		std::optional<int32_t> dyeColor() const;
	};

private:
	static runtime::ConcurrentHashMap<int32_t, runtime::FutureRef>& PREVIEW_RESETS();

	static constexpr int32_t PREVIEW_TIME_SECONDS = 10;

	bool previewEmotion(Player& player, const ItemParam& itemParam);

	void previewEquipment(Player& player, std::vector<ItemParam>& itemParams);

	void previewEquipment(Player& player, const std::vector<const ItemTemplate*>& equipment, std::optional<int32_t> itemColor, const std::string& colorText);

	bool validateForPreview(Player& player, const ItemTemplate* itemTemplate);

	std::vector<ItemParam> parse(std::span<const std::string> params);

	static int64_t addFakeItem(std::vector<runtime::Ref<Item>>& items, int64_t previewItemsSlotMask, int32_t itemId, std::optional<int32_t> itemColor);

	static int64_t getFirstFreeSlot(int64_t targetSlots, int64_t occupiedSlots);

	static void addOwnEquipment(Player& player, std::vector<runtime::Ref<Item>>& previewItems, int64_t previewItemsSlotMask, std::optional<int32_t> itemColor);

	static void schedulePreviewReset(Player& player, int32_t duration, bool previewRobot);

	static void updateRobotAppearance(Player& player, int32_t robotId);
};

} // namespace aion::gameserver::handlers::playercommands
