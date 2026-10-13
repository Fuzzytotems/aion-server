#include "aion/gameserver/handlers/admincommands/Reload.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Reload);

// Java Reload.java:26-41
Reload::Reload()
	: AdminCommand("reload", "Reloads templates or handlers (static data).",
		  "config - Reloads all configuration settings.\n"
		  "commands - Reloads all chat commands and console commands.\n"
		  "quests - Reloads quest templates and handlers.\n"
		  "skills - Reloads skill templates.\n"
		  "npcskills - Reloads NPC skill rules.\n"
		  "ai - Reloads NPC AI handlers. NPCs must respawn to use the reloaded AI.\n"
		  "events - Reloads event templates and (re)starts events.\n"
		  "arcade - Reloads the Upgrade Arcade reward item list.\n"
		  "decomposables - Reloads content of item bundles.\n"
		  "items - Reloads item templates.\n"
		  "customdrops - Reloads drops for chests.\n") {
}

// Java Reload.java:43-106. The static-data arms are AION_UNPORTED (D3, see Reload.h): HolderRef publishes a holder once, a reload needs it to
// republish (keeping the old holder, static-data.md §3.6) and a loader of one holder from one file; the quests, npcskills and events arms also
// need XMLQuests.setData, NpcSkillData.setNpcSkillTemplates, EventData.setEvents (CP5) and QuestEngine.reload (A-D1).
void Reload::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase; // parity: C++ using-declaration
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	if (equalsIgnoreCase(params[0], "quests")) { // parity= if (params[0].equalsIgnoreCase("quests")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/quest_data/quest_data.xml"); DataManager.QUEST_DATA = JAXBUtil.deserialize(xml, QuestsData.class, "./data/static_data/static_data.xsd"); List<XMLQuest> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/quest_script_data", true); JAXBUtil.deserialize(files, XMLQuests.class, "./data/static_data/static_data.xsd").forEach(e -> templates.addAll(e.getAllQuests())); DataManager.XML_QUESTS.setData(templates); QuestEngine.getInstance().reload(); sendInfo(admin, DataManager.QUEST_DATA.size() + " quest templates loaded (" + QuestEngine.getInstance().getQuestHandlerCount() + " handlers).");
	} else if (equalsIgnoreCase(params[0], "skills")) { // parity= } else if (params[0].equalsIgnoreCase("skills")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/skills/skill_templates.xml"); DataManager.SKILL_DATA = JAXBUtil.deserialize(xml, SkillData.class, "./data/static_data/static_data.xsd"); sendInfo(admin, DataManager.SKILL_DATA.size() + " skills loaded.");
	} else if (equalsIgnoreCase(params[0], "npcskills")) { // parity= } else if (params[0].equalsIgnoreCase("npcskills")) {
		AION_UNPORTED(); // parity= List<NpcSkillTemplates> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/npc_skills", true); JAXBUtil.deserialize(files, NpcSkillData.class, "./data/static_data/static_data.xsd") .forEach(e -> templates.addAll(e.getAllNpcSkillTemplates())); DataManager.NPC_SKILL_DATA.setNpcSkillTemplates(templates); sendInfo(admin, DataManager.NPC_SKILL_DATA.size() + " NPC skills loaded.");
	} else if (equalsIgnoreCase(params[0], "items")) { // parity= } else if (params[0].equalsIgnoreCase("items")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/items/item_templates.xml"); DataManager.ITEM_DATA = JAXBUtil.deserialize(xml, ItemData.class, "./data/static_data/static_data.xsd"); DataManager.ITEM_DATA.cleanup(); sendInfo(admin, DataManager.ITEM_DATA.size() + " item templates loaded.");
	} else if (equalsIgnoreCase(params[0], "ai")) { // parity= } else if (params[0].equalsIgnoreCase("ai")) {
		ai::AIEngine::getInstance().reload(); // parity= AIEngine.getInstance().reload();
		sendInfo(admin, "AI successfully reloaded!");
	} else if (equalsIgnoreCase(params[0], "commands")) { // parity= } else if (params[0].equalsIgnoreCase("commands")) {
		ChatProcessor::getInstance().reload();
		sendInfo(admin, "Chat commands successfully reloaded!");
	} else if (equalsIgnoreCase(params[0], "config")) { // parity= } else if (params[0].equalsIgnoreCase("config")) {
		configs::Config::load(); // parity= Config.load();
		sendInfo(admin, "Configs successfully reloaded!");
	} else if (equalsIgnoreCase(params[0], "customdrops")) { // parity= } else if (params[0].equalsIgnoreCase("customdrops")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/custom_drop/custom_drop.xml"); DataManager.CUSTOM_NPC_DROP = JAXBUtil.deserialize(xml, CustomDrop.class, "./data/static_data/static_data.xsd"); sendInfo(admin, DataManager.CUSTOM_NPC_DROP.size() + " custom drops loaded.");
	} else if (equalsIgnoreCase(params[0], "events")) { // parity= } else if (params[0].equalsIgnoreCase("events")) {
		AION_UNPORTED(); // parity= List<EventTemplate> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/events/timed_events", true); JAXBUtil.deserialize(files, EventData.class, "./data/static_data/static_data.xsd").forEach(e -> templates.addAll(e.getEvents())); EventService.getInstance().stop(); DataManager.EVENT_DATA.setEvents(templates); EventService.getInstance().start(); sendInfo(admin, DataManager.EVENT_DATA.size() + " events loaded.");
	} else if (equalsIgnoreCase(params[0], "arcade")) { // parity= } else if (params[0].equalsIgnoreCase("arcade")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/events/arcadelist.xml"); DataManager.UPGRADE_ARCADE_DATA = JAXBUtil.deserialize(xml, UpgradeArcadeData.class, "./data/static_data/static_data.xsd"); long rewards = DataManager.UPGRADE_ARCADE_DATA.getRewards().stream().mapToLong(l -> l.getArcadeRewardItems().size()).sum(); sendInfo(admin, rewards + " upgrade arcade rewards loaded.");
	} else if (equalsIgnoreCase(params[0], "decomposables")) { // parity= } else if (params[0].equalsIgnoreCase("decomposables")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/decomposable_items/decomposable_items.xml"); DataManager.DECOMPOSABLE_ITEMS_DATA = JAXBUtil.deserialize(xml, DecomposableItemsData.class, "./data/static_data/decomposable_items/decomposable_items.xsd"); sendInfo(admin, DataManager.DECOMPOSABLE_ITEMS_DATA.size() + " item bundles reloaded.");
	} else
		sendInfo(admin);
}

} // namespace aion::gameserver::handlers::admincommands
