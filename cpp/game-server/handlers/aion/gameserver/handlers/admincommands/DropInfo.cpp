#include "aion/gameserver/handlers/admincommands/DropInfo.h"

#include <limits>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropGroup.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/drop/NpcDrop.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(DropInfo);

// Java DropInfo.java:27-32
DropInfo::DropInfo()
	: AdminCommand("dropinfo", "Shows drop information of your target.",
		  " - Lists drops of the selected NPC for your level range.\n"
		  "all - Lists all drops of the selected NPC.\n") {
}

// Java DropInfo.java:34-83
void DropInfo::execute(Player& player, std::span<const std::string> params) {
	using geoEngine::math::JavaFloat;
	runtime::Ptr<VisibleObject> visibleObject = player.getTarget();

	runtime::Ptr<Npc> npc = runtime::as<Npc>(visibleObject); // parity= if (!(visibleObject instanceof Npc npc)) {
	if (npc == nullptr) { // parity: (the same)
		sendInfo(player);
		return;
	}

	bool showAll = params.size() > 0 && params[0] == "all"; // parity= boolean showAll = params.length > 0 && params[0].equals("all");
	const NpcDrop* npcDrop = DataManager::CUSTOM_NPC_DROP->getNpcDrop(npc->getNpcId()); // parity= NpcDrop npcDrop = DataManager.CUSTOM_NPC_DROP.getNpcDrop(npc.getNpcId());
	DropModifiers dropModifiers = DropRegistrationService::getInstance().createDropModifiers(*npc, player, player.getLevel());
	dropModifiers.setMaxDropsPerGroup(std::numeric_limits<int32_t>::max()); // parity= dropModifiers.setMaxDropsPerGroup(Integer.MAX_VALUE);

	std::array<int32_t, 2> counts{0, 0}; // parity= int[] counts = { 0, 0 };
	std::string info = "[" + name(*npc) + "'s drops]";
	if (npcDrop != nullptr) {
		for (const DropGroup& dropGroup : npcDrop->getDropGroup()) { // parity= for (DropGroup dropGroup : npcDrop.getDropGroup()) {
			if (dropGroup.getRace() == Race::PC_ALL || dropGroup.getRace() == dropModifiers.getDropRace()) {
				info += "\nCustom drop group: " + dropGroup.getName() + ", max drops: " + std::to_string(dropGroup.getMaxItems()); // parity= info += "\nCustom drop group: " + dropGroup.getName() + ", max drops: " + dropGroup.getMaxItems();
				counts[1]++;
				for (const std::unique_ptr<Drop>& drop : dropGroup.getDrop()) { // parity= for (Drop drop : dropGroup.getDrop()) {
					float finalChance = dropModifiers.calculateDropChance(drop->getChance(), dropGroup.isUseLevelBasedChanceReduction());
					if (!showAll && finalChance <= 0)
						continue;
					info += "\n\t" + ChatUtil::item(drop->getItemId()) + "\tBase chance: " + JavaFloat::toString(drop->getChance()) + "%, effective: " + // parity= info += "\n\t" + ChatUtil.item(drop.getItemId()) + "\tBase chance: " + drop.getChance() + "%, effective: " + finalChance + "%";
						JavaFloat::toString(finalChance) + "%"; // parity: (the same)
					counts[0]++;
				}
			}
		}
	}

	// if npc ai == quest_use_item it will be always excluded from global drops
	bool isNpcQuest = npc->getAi().getName() == "quest_use_item"; // parity= boolean isNpcQuest = npc.getAi().getName().equals("quest_use_item");
	if (!isNpcQuest) {
		bool hasGlobalNpcExclusions = DropRegistrationService::getInstance().hasGlobalNpcExclusions(*npc);
		bool isAllowedDefaultGlobalDropNpc = DropRegistrationService::getInstance().isAllowedDefaultGlobalDropNpc(*npc, dropModifiers.isDropNpcChest());
		// instances with WorldDropType.NONE must not have global drops (example Arenas)
		if (!hasGlobalNpcExclusions && npc->getWorldDropType() != WorldDropType::NONE) {
			std::vector<const GlobalRule*> globalRules; // parity: (DataManager.GLOBAL_DROP_DATA.getAllRules(), below)
			for (const GlobalRule& rule : DataManager::GLOBAL_DROP_DATA->getAllRules()) // parity: (the same)
				globalRules.push_back(&rule); // parity: (the same)
			info += collectDropInfo("Global", globalRules, *npc, dropModifiers, isAllowedDefaultGlobalDropNpc, showAll, // parity= info += collectDropInfo("Global", DataManager.GLOBAL_DROP_DATA.getAllRules(), npc, dropModifiers, isAllowedDefaultGlobalDropNpc, showAll, counts);
				counts); // parity: (the same)
		}
		if (!hasGlobalNpcExclusions || dropModifiers.isDropNpcChest())
			info += collectDropInfo("Event", EventService::getInstance().getActiveEventDropRules()->snapshot(), *npc, dropModifiers, // parity= info += collectDropInfo("Event", EventService.getInstance().getActiveEventDropRules(), npc, dropModifiers, isAllowedDefaultGlobalDropNpc, showAll, counts);
				isAllowedDefaultGlobalDropNpc, showAll, counts); // parity: (the same)
	}

	info += "\n" + std::to_string(counts[0]) + " total drops available in " + std::to_string(counts[1]) + " drop groups" + // parity= info += "\n" + counts[0] + " total drops available in " + counts[1] + " drop groups" + (showAll ? "." : " on your level.");
		(showAll ? "." : " on your level."); // parity: (the same)
	sendInfo(player, info);
}

// Java DropInfo.java:85-113
std::string DropInfo::collectDropInfo(const std::string& dropGroupPrefix, const std::vector<const GlobalRule*>& rules, Npc& npc,
	DropModifiers& dropModifiers, bool isAllowedDefaultGlobalDropNpc, bool showAll, std::array<int32_t, 2>& counts) {
	using geoEngine::math::JavaFloat;
	std::string info = "";
	for (const GlobalRule* rule : rules) { // parity= for (GlobalRule rule : rules) {
		// if getGlobalRuleNpcs() != null means drops are for specified npcs (like named drops) so the default restrictions will be ignored
		if (isAllowedDefaultGlobalDropNpc || rule->getGlobalRuleNpcs() != nullptr) {

			float chance = DropRegistrationService::getInstance().calculateEffectiveChance(rule, npc, dropModifiers);
			if (!showAll && chance <= 0)
				continue;

			std::vector<const GlobalDropItem*> drops = DropRegistrationService::getInstance().collectDrops(rule, npc, dropModifiers); // parity= List<GlobalDropItem> drops = DropRegistrationService.getInstance().collectDrops(rule, npc, dropModifiers);
			if (!drops.empty()) { // parity= if (!drops.isEmpty()) {
				info += "\n" + dropGroupPrefix + " drop group: \"" + rule->getRuleName() + "\", max drops: " + std::to_string(rule->getMaxDropRule()); // parity= info += "\n" + dropGroupPrefix + " drop group: \"" + rule.getRuleName() + "\", max drops: " + rule.getMaxDropRule();
				if (rule->getMemberLimit() != 1)
					info += ", member limit: " + std::to_string(rule->getMemberLimit()); // parity= info += ", member limit: " + rule.getMemberLimit();
				info += "\n\tBase chance: " + JavaFloat::toString(rule->getChance()) + "%, effective: " + JavaFloat::toString(chance) + "%"; // parity= info += "\n\tBase chance: " + rule.getChance() + "%, effective: " + chance + "%";
				counts[1]++;
				for (const GlobalDropItem* item : drops) { // parity= for (GlobalDropItem item : drops) {
					info += "\n\t" + ChatUtil::item(item->getId());
					if (item->getChance() != 100.0f) // parity= if (item.getChance() != 100f)
						info += "\tSub chance: " + JavaFloat::toString(item->getChance()) + "%"; // parity= info += "\tSub chance: " + item.getChance() + "%";
					counts[0]++;
				}
			}
		}
	}
	return info;
}

} // namespace aion::gameserver::handlers::admincommands
