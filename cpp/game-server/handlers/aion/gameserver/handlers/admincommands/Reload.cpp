#include "aion/gameserver/handlers/admincommands/Reload.h"

#include <filesystem>
#include <memory>
#include <typeindex>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.h"
#include "aion/gameserver/dataholders/detail/StaticDataRegistry.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewards.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/utils/xml/XmlUtil.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * Java: JAXBUtil.deserialize(file(s), H.class, schema) - the one-holder load of //reload (owner's decision 2026-10-13): DataManager.init's
 * strict binder over the files, the holders a hook reads (ItemData) taken from the published ones, the IDREFs resolved within the load. The
 * objects a lenient load would retire are kept alive like the holder.
 */
template <typename H>
std::unique_ptr<H> reloadHolderFromFiles(const std::vector<std::filesystem::path>& files) {
	if (files.empty()) // Java: deserialize of no files is an empty list, the setter then empties the holder
		return std::make_unique<H>();
	xml::LoadOptions options;
	options.strict = true;
	options.countryCode = configs::main::GSConfig::SERVER_COUNTRY_CODE.load();
	options.parallelParse = true;
	xml::LoadContext context(options);
	context.setPublishedHolderLookup([](std::type_index type) -> const void* {
		return type == std::type_index(typeid(dataholders::ItemData)) ? static_cast<const void*>(DataManager::ITEM_DATA.get()) : nullptr;
	});
	xml::StaticDataLoader(dataholders::detail::staticDataRegistry()).loadFiles(context, files);
	context.resolveIdRefs();
	std::unique_ptr<H> holder = context.takeHolder<H>();
	std::vector<xml::ErasedHolder> retired = context.takeRetired();
	if (!retired.empty())
		static_cast<void>(new std::vector<xml::ErasedHolder>(std::move(retired))); // immortal, like the published holders
	return holder;
}


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

// Java Reload.java:43-106. The static-data arms (owner's decision 2026-10-13) load the files with loadHolder and republish the holder (the old
// one stays alive, static-data.md §3.6); the quests arm stays AION_UNPORTED: it needs QuestEngine.reload (A-D1).
void Reload::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase; // parity: C++ using-declaration
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	if (equalsIgnoreCase(params[0], "quests")) { // parity= if (params[0].equalsIgnoreCase("quests")) {
		AION_UNPORTED(); // parity= File xml = new File("./data/static_data/quest_data/quest_data.xml"); DataManager.QUEST_DATA = JAXBUtil.deserialize(xml, QuestsData.class, "./data/static_data/static_data.xsd"); List<XMLQuest> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/quest_script_data", true); JAXBUtil.deserialize(files, XMLQuests.class, "./data/static_data/static_data.xsd").forEach(e -> templates.addAll(e.getAllQuests())); DataManager.XML_QUESTS.setData(templates); QuestEngine.getInstance().reload(); sendInfo(admin, DataManager.QUEST_DATA.size() + " quest templates loaded (" + QuestEngine.getInstance().getQuestHandlerCount() + " handlers).");
	} else if (equalsIgnoreCase(params[0], "skills")) { // parity= } else if (params[0].equalsIgnoreCase("skills")) {
		std::filesystem::path xml("./data/static_data/skills/skill_templates.xml"); // parity= File xml = new File("./data/static_data/skills/skill_templates.xml");
		DataManager::SKILL_DATA.republish(reloadHolderFromFiles<dataholders::SkillData>({xml})); // parity= DataManager.SKILL_DATA = JAXBUtil.deserialize(xml, SkillData.class, "./data/static_data/static_data.xsd");
		sendInfo(admin, std::to_string(DataManager::SKILL_DATA->size()) + " skills loaded."); // parity= sendInfo(admin, DataManager.SKILL_DATA.size() + " skills loaded.");
	} else if (equalsIgnoreCase(params[0], "npcskills")) { // parity= } else if (params[0].equalsIgnoreCase("npcskills")) {
		// C++: the files' templates in one holder that replaces the published one (Java: setNpcSkillTemplates on the published holder)
		std::vector<std::filesystem::path> files = utils::xml::XmlUtil::listFiles("./data/static_data/npc_skills", true); // parity= List<NpcSkillTemplates> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/npc_skills", true);
		DataManager::NPC_SKILL_DATA.republish(reloadHolderFromFiles<dataholders::NpcSkillData>(files)); // parity= JAXBUtil.deserialize(files, NpcSkillData.class, "./data/static_data/static_data.xsd") .forEach(e -> templates.addAll(e.getAllNpcSkillTemplates())); DataManager.NPC_SKILL_DATA.setNpcSkillTemplates(templates);
		sendInfo(admin, std::to_string(DataManager::NPC_SKILL_DATA->size()) + " NPC skills loaded."); // parity= sendInfo(admin, DataManager.NPC_SKILL_DATA.size() + " NPC skills loaded.");
	} else if (equalsIgnoreCase(params[0], "items")) { // parity= } else if (params[0].equalsIgnoreCase("items")) {
		std::filesystem::path xml("./data/static_data/items/item_templates.xml"); // parity= File xml = new File("./data/static_data/items/item_templates.xml");
		std::unique_ptr<dataholders::ItemData> items = reloadHolderFromFiles<dataholders::ItemData>({xml}); // parity= DataManager.ITEM_DATA = JAXBUtil.deserialize(xml, ItemData.class, "./data/static_data/static_data.xsd");
		items->cleanup(*DataManager::ITEM_CLEAN_UP); // parity= DataManager.ITEM_DATA.cleanup();
		DataManager::ITEM_DATA.republish(std::move(items)); // parity: (the assignment above)
		sendInfo(admin, std::to_string(DataManager::ITEM_DATA->size()) + " item templates loaded."); // parity= sendInfo(admin, DataManager.ITEM_DATA.size() + " item templates loaded.");
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
		std::filesystem::path xml("./data/static_data/custom_drop/custom_drop.xml"); // parity= File xml = new File("./data/static_data/custom_drop/custom_drop.xml");
		DataManager::CUSTOM_NPC_DROP.republish(reloadHolderFromFiles<dataholders::CustomDrop>({xml})); // parity= DataManager.CUSTOM_NPC_DROP = JAXBUtil.deserialize(xml, CustomDrop.class, "./data/static_data/static_data.xsd");
		sendInfo(admin, std::to_string(DataManager::CUSTOM_NPC_DROP->size()) + " custom drops loaded."); // parity= sendInfo(admin, DataManager.CUSTOM_NPC_DROP.size() + " custom drops loaded.");
	} else if (equalsIgnoreCase(params[0], "events")) { // parity= } else if (params[0].equalsIgnoreCase("events")) {
		// C++: the files' events in one holder that replaces the published one (Java: setEvents on the published holder)
		std::vector<std::filesystem::path> files = utils::xml::XmlUtil::listFiles("./data/static_data/events/timed_events", true); // parity= List<EventTemplate> templates = new ArrayList<>(); Collection<File> files = XmlUtil.listFiles("./data/static_data/events/timed_events", true);
		std::unique_ptr<dataholders::EventData> events = reloadHolderFromFiles<dataholders::EventData>(files); // parity= JAXBUtil.deserialize(files, EventData.class, "./data/static_data/static_data.xsd").forEach(e -> templates.addAll(e.getEvents()));
		services::event::EventService::getInstance().stop(); // parity= EventService.getInstance().stop();
		DataManager::EVENT_DATA.republish(std::move(events)); // parity= DataManager.EVENT_DATA.setEvents(templates);
		services::event::EventService::getInstance().start(); // parity= EventService.getInstance().start();
		sendInfo(admin, std::to_string(DataManager::EVENT_DATA->size()) + " events loaded."); // parity= sendInfo(admin, DataManager.EVENT_DATA.size() + " events loaded.");
	} else if (equalsIgnoreCase(params[0], "arcade")) { // parity= } else if (params[0].equalsIgnoreCase("arcade")) {
		std::filesystem::path xml("./data/static_data/events/arcadelist.xml"); // parity= File xml = new File("./data/static_data/events/arcadelist.xml");
		DataManager::UPGRADE_ARCADE_DATA.republish(reloadHolderFromFiles<dataholders::UpgradeArcadeData>({xml})); // parity= DataManager.UPGRADE_ARCADE_DATA = JAXBUtil.deserialize(xml, UpgradeArcadeData.class, "./data/static_data/static_data.xsd");
		int64_t rewards = 0; // parity= long rewards = DataManager.UPGRADE_ARCADE_DATA.getRewards().stream().mapToLong(l -> l.getArcadeRewardItems().size()).sum();
		for (const auto& l : DataManager::UPGRADE_ARCADE_DATA->getRewards()) // parity: (the stream above)
			rewards += static_cast<int64_t>(l.getArcadeRewardItems().size()); // parity: (the same)
		sendInfo(admin, std::to_string(rewards) + " upgrade arcade rewards loaded."); // parity= sendInfo(admin, rewards + " upgrade arcade rewards loaded.");
	} else if (equalsIgnoreCase(params[0], "decomposables")) { // parity= } else if (params[0].equalsIgnoreCase("decomposables")) {
		std::filesystem::path xml("./data/static_data/decomposable_items/decomposable_items.xml"); // parity= File xml = new File("./data/static_data/decomposable_items/decomposable_items.xml");
		DataManager::DECOMPOSABLE_ITEMS_DATA.republish(reloadHolderFromFiles<dataholders::DecomposableItemsData>({xml})); // parity= DataManager.DECOMPOSABLE_ITEMS_DATA = JAXBUtil.deserialize(xml, DecomposableItemsData.class, "./data/static_data/decomposable_items/decomposable_items.xsd");
		sendInfo(admin, std::to_string(DataManager::DECOMPOSABLE_ITEMS_DATA->size()) + " item bundles reloaded."); // parity= sendInfo(admin, DataManager.DECOMPOSABLE_ITEMS_DATA.size() + " item bundles reloaded.");
	} else
		sendInfo(admin);
}

} // namespace aion::gameserver::handlers::admincommands
