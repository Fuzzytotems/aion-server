#include "aion/gameserver/handlers/admincommands/Megaphone.h"

#include <algorithm>
#include <numeric>
#include <memory>
#include <regex>
#include <string_view>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Megaphone);

// Java Megaphone.java:24 (the static field's initializer, see the header)
const std::vector<Megaphone::MegaphoneChatColor>& Megaphone::colors() { // parity: the accessor of the static field below
	static const std::vector<MegaphoneChatColor> colors = collectColors(); // parity= private static final List<MegaphoneChatColor> colors = collectColors();
	return colors; // parity: (the accessor)
} // parity: (the accessor)

Megaphone::Megaphone()
	: AdminCommand("megaphone", "Sends a message to the global faction chat (client must be started with -megaphone to show the megaphone chat window).",
		  "<none|elyos|asmo> <name> <message> - Sends the message with given sender name and faction prefix.\n"
		  "<color ID> <none|elyos|asmo> <name> <message> - Sends the message in the color of given color ID.\n"
		  "Color IDs: " + colorIds() + "\n") { // parity= "Color IDs: %s\n".formatted(colorIds())) { // the text block's last line and String.formatted
}

// Java Megaphone.java:34-61
void Megaphone::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 3) { // parity= if (params.length < 3) {
		sendInfo(admin);
		return;
	}
	int32_t i = 0;
	static const std::regex digits(R"(\d+)"); // parity: the pattern of String.matches below
	int32_t colorIndex = std::regex_match(params[i], digits) ? commons::utils::parseInt(params[i++]) - 1 : 0; // parity= int colorIndex = params[i].matches("\\d+") ? Integer.parseInt(params[i++]) - 1 : 0;
	if (colorIndex >= static_cast<int32_t>(colors().size())) { // parity= if (colorIndex >= colors.size()) {
		sendInfo(admin, "Invalid color ID.");
		return;
	}
	if (colorIndex < 0) // parity: color ID 0 is colors.get(-1): Java's IndexOutOfBoundsException, explicit
		throw runtime::IndexOutOfBoundsException("Index -1 out of bounds for length " + std::to_string(colors().size())); // parity: (the same; ChatCommand.run logs it)
	int32_t megaphoneItemId = colors()[colorIndex].megaphoneItemId; // parity= int megaphoneItemId = colors.get(colorIndex).megaphoneItemId;
	std::string label = commons::utils::StringUtils::toLowerCase(params[i++]); // parity= String label = params[i++].toLowerCase();
	SM_MEGAPHONE::FactionLabel factionLabel;
	if (std::string_view("none").starts_with(label)) // parity= if ("none".startsWith(label))
		factionLabel = SM_MEGAPHONE::FactionLabel::NONE;
	else if (std::string_view("elyos").starts_with(label)) // parity= else if ("elyos".startsWith(label))
		factionLabel = SM_MEGAPHONE::FactionLabel::ELYOS;
	else if (std::string_view("asmodians").starts_with(label)) // parity= else if ("asmodians".startsWith(label))
		factionLabel = SM_MEGAPHONE::FactionLabel::ASMODIANS;
	else {
		sendInfo(admin);
		return;
	}
	std::string sender = params[i++];
	std::string message = join(params, i);
	PacketSendUtility::broadcastToWorld(SM_MEGAPHONE(factionLabel, sender, message, megaphoneItemId));
}

// Java Megaphone.java:63-75
std::vector<Megaphone::MegaphoneChatColor> Megaphone::collectColors() {
	std::vector<MegaphoneChatColor> colors; // parity= List<MegaphoneChatColor> colors = new ArrayList<>(); // the records are const: sorted by index below
	for (const ItemTemplate* itemTemplate : DataManager::ITEM_DATA->getItemTemplates()) {
		if (itemTemplate->getActions() != nullptr) {
			for (const std::unique_ptr<AbstractItemAction>& itemAction : itemTemplate->getActions()->getItemActions()) {
				const MegaphoneAction* megaphoneAction = dynamic_cast<const MegaphoneAction*>(itemAction.get()); // parity: the instanceof and the casts of the next line
				if (megaphoneAction != nullptr && std::none_of(colors.begin(), colors.end(), [megaphoneAction](const MegaphoneChatColor& c) { return c.color == megaphoneAction->getColor(); })) // parity= if (itemAction instanceof MegaphoneAction && colors.stream().noneMatch(c -> c.color == ((MegaphoneAction) itemAction).getColor()))
					colors.push_back(MegaphoneChatColor{itemTemplate->getTemplateId(), megaphoneAction->getColor()}); // parity= colors.add(new MegaphoneChatColor(itemTemplate.getTemplateId(), ((MegaphoneAction) itemAction).getColor()));
			}
		}
	}
	std::vector<size_t> order(colors.size()); // parity= colors.sort(Comparator.comparingInt((MegaphoneChatColor m) -> m.color).reversed());
	std::iota(order.begin(), order.end(), size_t{0}); // parity: (the same statement: List.sort is stable)
	std::stable_sort(order.begin(), order.end(), [&colors](size_t a, size_t b) { return colors[b].color < colors[a].color; }); // parity: (the same statement)
	std::vector<MegaphoneChatColor> sorted; // parity: (the same statement)
	for (size_t i : order) // parity: (the same statement)
		sorted.push_back(colors[i]); // parity: (the same statement)
	return sorted; // parity= return colors;
}

// Java Megaphone.java:77-85
std::string Megaphone::colorIds() {
	std::string sb; // parity= StringBuilder sb = new StringBuilder();
	for (int32_t i = 0; i < static_cast<int32_t>(colors().size()); i++) { // parity= for (int i = 0; i < colors.size(); i++) {
		if (sb.length() > 0)
			sb += ", "; // parity= sb.append(", ");
		sb += ChatUtil::color(std::to_string(i + 1) + " █", colors()[i].color); // parity= sb.append(ChatUtil.color(i + 1 + " █", colors.get(i).color));
	}
	return sb; // parity= return sb.toString();
}

// Java Megaphone.java:87
static_assert(sizeof(Megaphone::MegaphoneChatColor) == 2 * sizeof(int32_t)); // parity= private record MegaphoneChatColor(int megaphoneItemId, int color) {}

} // namespace aion::gameserver::handlers::admincommands
