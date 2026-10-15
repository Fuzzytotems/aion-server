#include "aion/gameserver/handlers/admincommands/RemoveCd.h"

#include <unordered_map>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/handlers/consolecommands/Clearusercoolt.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(RemoveCd);

RemoveCd::RemoveCd()
	: AdminCommand("removecd", "Clears cooldowns for skills, items and instances.",
		  " - Removes all item and skill cooldowns of your target.\n"
		  "instance all - Removes all instance cooldowns of your target.\n"
		  "instance <world ID> - Removes the specified instance cooldown of your target.\n"
		  "Note: Any actions default to your character, if no player is targeted.\n") {
}

// Java RemoveCd.java:30-70
void RemoveCd::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget());
	Player& target = targetPlayer != nullptr ? *targetPlayer : admin;
	if (params.empty()) {
		if (target.getSkillCoolDowns() != nullptr) {
			int64_t nowMillis = commons::utils::currentTimeMillis();
			std::vector<int32_t> cooldownIds; // parity= List<Integer> cooldownIds = target.getSkillCoolDowns().entrySet().stream().filter(e -> e.getValue() > nowMillis).map(Entry::getKey).toList();
			for (const auto& entry : target.getSkillCoolDowns()->snapshot()) { // parity: (continued)
				if (entry.value > nowMillis) // parity: (continued)
					cooldownIds.push_back(entry.key); // parity: (continued)
			} // parity: (continued)
			PacketSendUtility::sendPacket(target, SM_SKILL_COOLDOWN(target, cooldownIds));
			target.getSkillCoolDowns()->clear();
		}
		removeItemCooldowns(target);
		target.getHouseObjectCooldowns()->clear();
		if (target.equals(admin)) {
			sendInfo(admin, "Your item and skill cooldowns were removed.");
		} else {
			sendInfo(admin, "You have removed item and skill cooldowns of " + name(target) + '.');
			sendInfo(target, name(admin) + " removed your item and skill cooldowns.");
		}
	} else if (equalsIgnoreCase(params[0], "instance") && params.size() >= 2) {
		if (equalsIgnoreCase(params[1], "all")) {
			consolecommands::Clearusercoolt::clearAllInstanceCooldowns(admin, target);
		} else {
			int32_t worldId = commons::utils::parseInt(params[1]);
			if (target.getPortalCooldownList().isPortalUseDisabled(worldId)) {
				target.getPortalCooldownList().removePortalCooldown(worldId);
				target.getPortalCooldownList().sendEntryInfo(worldId);
				if (target.equals(admin)) {
					sendInfo(admin, "Your instance cooldown for " + worldName(worldId) + " was removed.");
				} else {
					sendInfo(admin, "You have removed the instance cooldown for " + worldName(worldId) + " of " + name(target) + '.');
					sendInfo(target, name(admin) + " removed your instance cooldown for " + worldName(worldId) + ".");
				}
			} else
				sendInfo(admin, (target.equals(admin) ? std::string("You have") : name(target) + " has") + " no cooldown on " + worldName(worldId) + "."); // parity= sendInfo(admin, (target.equals(admin) ? "You have" : name(target) + " has") + " no cooldown on " + worldName(worldId) + ".");
		}
	} else {
		sendInfo(admin);
	}
}

// Java RemoveCd.java:72-79
void RemoveCd::removeItemCooldowns(Player& player) {
	std::unordered_map<int32_t, runtime::Ptr<ItemCooldown>> dummyCds; // parity= Map<Integer, ItemCooldown> dummyCds = new HashMap<>(); // 4.8 client ignores reuseTime <= currentTime, but sending old cds + useDelay 0 works
	std::vector<runtime::Ref<ItemCooldown>> created; // parity: keeps the dummies alive until the packet is written (docs/deviations/C2.md)
	for (const auto& en : player.getItemCoolDowns().snapshot()) { // parity= for (Entry<Integer, ItemCooldown> en : player.getItemCoolDowns().entrySet()) {
		runtime::Ref<ItemCooldown> dummy = ItemCooldown::create(en.value->getReuseTime(), 0); // parity= dummyCds.put(en.getKey(), new ItemCooldown(en.getValue().getReuseTime(), 0));
		dummyCds[en.key] = dummy; // parity: (continued)
		created.push_back(dummy); // parity: (continued)
		player.removeItemCoolDown(en.key); // parity= player.removeItemCoolDown(en.getKey());
	}
	PacketSendUtility::sendPacket(player, SM_ITEM_COOLDOWN(dummyCds));
}

} // namespace aion::gameserver::handlers::admincommands
