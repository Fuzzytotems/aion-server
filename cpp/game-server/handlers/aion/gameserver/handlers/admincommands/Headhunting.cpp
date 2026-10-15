#include "aion/gameserver/handlers/admincommands/Headhunting.h"

#include <algorithm>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dao/HeadhuntingDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Headhunting);

// Java: an EnumMap's keys in ordinal order (fieldmap's HashMap shim stores the maps)
// Java: List.of(items...)
runtime::Ref<runtime::RcArrayList<runtime::Ref<RewardItem>>> Headhunting::listOf(std::initializer_list<runtime::Ref<RewardItem>> items) { // parity: (List.of)
	runtime::Ref<runtime::RcArrayList<runtime::Ref<RewardItem>>> list = runtime::RcArrayList<runtime::Ref<RewardItem>>::create(AION_LOCK_CLASS(Headhunting::rewards#list)); // parity: (the same)
	for (const runtime::Ref<RewardItem>& item : items) // parity: (the same)
		list->add(item); // parity: (the same)
	return list; // parity: (the same)
} // parity: (the same)

template <typename Map> // parity: (C++ template)
auto Headhunting::keysInOrdinalOrder(const Map& map) { // parity: (the EnumMap iteration order)
	auto keys = map.keySet().toVector(); // parity: (the same)
	std::sort(keys.begin(), keys.end()); // parity: (the same)
	return keys; // parity: (the same)
} // parity: (the same)

// parity= private static final Logger log = LoggerFactory.getLogger("EVENT_LOG"); (in the bodies that log)
// parity= private final Map<Integer, List<RewardItem>> rewards = new LinkedHashMap<>(); (Headhunting.h)

// Java Headhunting.java:43-71
Headhunting::Headhunting()
	: AdminCommand("headhunting", "Manages seasonal headhunting event.",
		  "analyze - Analyzes the season.\n"
		  "show <rewards|results> - Shows the registered rewards or analyzed results.\n"
		  "clear - Clears the analyzed results.\n"
		  "addKills <player ID> <kills> - Adds headhunting kills for the specified player.\n"
		  "finalize <true|false> - Finalizes the season (clears all references) and rewards all participants if requested.\n") {

	// Initialize seasonal headhunting rewards
	rewards.put(1, listOf({ // parity= rewards.put(1, List.of(
		RewardItem::create(164002276, 5), // Eternal War Battle Scroll // parity= new RewardItem(164002276, 5),
		RewardItem::create(188950017, 3), // Special Courier Pass (Abyss Eternal/Lv. 61-65) // parity= new RewardItem(188950017, 3),
		RewardItem::create(186000051, 15) // Major Ancient Crown // parity= new RewardItem(186000051, 15)
	})); // parity= ));
	rewards.put(2, listOf({ // parity= rewards.put(2, List.of(
		RewardItem::create(164002276, 3), // Eternal War Battle Scroll // parity= new RewardItem(164002276, 3),
		RewardItem::create(188950017, 3), // Special Courier Pass (Abyss Eternal/Lv. 61-65) // parity= new RewardItem(188950017, 3),
		RewardItem::create(186000051, 10) // Major Ancient Crown // parity= new RewardItem(186000051, 10)
	})); // parity= ));
	rewards.put(3, listOf({ // parity= rewards.put(3, List.of(
		RewardItem::create(164002276, 1), // Eternal War Battle Scroll // parity= new RewardItem(164002276, 1),
		RewardItem::create(188950017, 2), // Special Courier Pass (Abyss Eternal/Lv. 61-65) // parity= new RewardItem(188950017, 2),
		RewardItem::create(186000051, 5) // Major Ancient Crown // parity= new RewardItem(186000051, 5)
	})); // parity= ));
	consolationRewards.add(RewardItem::create(186000051, 1)); // Major Ancient Crown // parity= consolationRewards.add(new RewardItem(186000051, 1));
}

// Java Headhunting.java:73-115
void Headhunting::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() == 0) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}
	const std::string& command = params[0]; // parity= switch (params[0]) {
	if (command == "analyze") { // parity= case "analyze":
		if (analyzeSeason(admin))
			sendInfo(admin, "Season successfully analyzed.");
	} else if (command == "clear") { // parity= case "clear":
		if (results.get() != nullptr) { // parity= if (results != null) {
			results.set(nullptr); // parity= results = null;
			sendInfo(admin, "Results successfully cleared.");
		}
	} else if (command == "show") { // parity= case "show":
		if (params.size() == 1) // parity= if (params.length == 1)
			sendInfo(admin);
		else if (commons::utils::StringUtils::equalsIgnoreCase(params[1], "rewards")) // parity= else if (params[1].equalsIgnoreCase("rewards"))
			showRewards(admin);
		else if (commons::utils::StringUtils::equalsIgnoreCase(params[1], "results")) // parity= else if (params[1].equalsIgnoreCase("results"))
			showResults(admin);
	} else if (command == "addKills") { // parity= case "addKills":
		if (params.size() < 3) { // parity= if (params.length < 3) {
			sendInfo(admin);
			return;
		}
		addKills(admin, commons::utils::parseInt(params[1]), commons::utils::parseInt(params[2])); // parity= addKills(admin, Integer.parseInt(params[1]), Integer.parseInt(params[2]));
	} else if (command == "finalize") { // parity= case "finalize":
		if (params.size() < 2) { // parity= if (params.length < 2) {
			sendInfo(admin);
			return;
		}
		bool shouldReward = commons::utils::StringUtils::equalsIgnoreCase(params[1], "true"); // parity= boolean shouldReward = Boolean.parseBoolean(params[1]);
		finalizeSeason(admin, shouldReward);
	} else { // parity= default:
		sendInfo(admin);
	}
}

// Java Headhunting.java:117-134
void Headhunting::addKills(Player& admin, int32_t playerId, int32_t kills) {
	if (!configs::main::EventsConfig::ENABLE_HEADHUNTING.load()) { // parity= if (!EventsConfig.ENABLE_HEADHUNTING) {
		sendInfo(admin, "Season is not active.");
		return;
	}
	runtime::Ptr<model::event::Headhunter> hunter = services::PvpService::getInstance().getHeadhunterById(playerId);
	if (hunter != nullptr) {
		int32_t newKills = hunter->getKills() + kills;
		if (newKills < 0)
			newKills = 0;

		hunter->setKills(newKills);
		hunter->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED); // parity= hunter.setPersistentState(PersistentState.UPDATE_REQUIRED);
		sendInfo(admin, "Successfully added " + std::to_string(kills) + " kills for player [ID=" + std::to_string(playerId) + ", name=" // parity= sendInfo(admin, "Successfully added " + kills + " kills for player [ID=" + playerId + ", name=" + PlayerService.getPlayerName(playerId)
			+ services::player::PlayerService::getPlayerName(playerId).value_or("null") + ", currentKills=" + std::to_string(newKills) + "]."); // parity= + ", currentKills=" + newKills + "].");
	} else {
		sendInfo(admin, "The player could not be found. Wrong player ID.");
	}
}

// Java Headhunting.java:136-160
bool Headhunting::analyzeSeason(Player& admin) {
	if (results.get() != nullptr) { // parity= if (results != null) {
		sendInfo(admin, "Season is still ascertained. Use <show> paramater to get a survey with the final information "
						"or use <reward> to execute the reward progress."); // parity= + "or use <reward> to execute the reward progress.");
		return false;
	}
	auto& headhunters = services::PvpService::getInstance().getAllHeadhunters(); // parity= final Map<Integer, Headhunter> headhunters = PvpService.getInstance().getAllHeadhunters();
	results.set(Results::create(AION_LOCK_CLASS(Headhunting::results))); // parity= results = new EnumMap<>(Race.class);
	results.get()->put(model::Race::ASMODIANS, ClassMap::create(AION_LOCK_CLASS(Headhunting::results#class))); // parity= results.put(Race.ASMODIANS, new EnumMap<>(PlayerClass.class));
	results.get()->put(model::Race::ELYOS, ClassMap::create(AION_LOCK_CLASS(Headhunting::results#class))); // parity= results.put(Race.ELYOS, new EnumMap<>(PlayerClass.class));
	for (runtime::Ptr<model::event::Headhunter> hunter : headhunters.values()) { // parity= for (Headhunter hunter : headhunters.values()) {
		runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = services::player::PlayerService::getOrLoadPlayerCommonData(hunter->getHunterId());
		if (pcd == nullptr) {
			static const auto log = commons::logging::LoggerFactory::getLogger("EVENT_LOG"); // parity: (the logger)
			log.warn("PlayerID: " + std::to_string(hunter->getHunterId()) + " did not exist anymore."); // parity= log.warn("PlayerID: " + hunter.getHunterId() + " did not exist anymore.");
			continue;
		}
		model::PlayerClass pc = pcd->getPlayerClass();
		model::Race race = pcd->getRace();
		runtime::Ptr<ClassMap> raceMap = results.get()->get(race); // parity: (results.get(race) of the two Java lines below)
		if (raceMap == nullptr) // parity: (Java: results.get(race) is null for a third race: its NullPointerException)
			throw runtime::NullPointerException("results.get(" + std::string(xml::enumName(race)) + ")"); // parity: (the same)
		raceMap->putIfAbsent(pc, HunterList::create(AION_LOCK_CLASS(Headhunting::results#list))); // parity= results.get(race).putIfAbsent(pc, new ArrayList<>());
		runtime::Ptr<HunterList> hunterz = raceMap->get(pc); // parity= List<Headhunter> hunterz = results.get(race).get(pc);
		hunterz->add(runtime::Ref<model::event::Headhunter>(*hunter)); // parity= hunterz.add(hunter);
		hunterz->sort([](const runtime::Ptr<model::event::Headhunter>& a, const runtime::Ptr<model::event::Headhunter>& b) { return a->compareTo(*b); }); // parity= hunterz.sort(null);
	}
	return true;
}

// Java Headhunting.java:162-189
void Headhunting::showResults(Player& admin) {
	runtime::Ptr<Results> analyzed = results.get(); // parity: (the field read)
	if (analyzed == nullptr) { // parity= if (results == null) {
		sendInfo(admin, "There are no results available! The headhunting season needs to be analyzed before rewarding.");
		return;
	} else if (analyzed->isEmpty()) { // parity= } else if (results.isEmpty()) {
		results.set(nullptr); // parity= results = null;
		sendInfo(admin, "No players participated, yet. Empty reward map was cleared.");
		return;
	}
	std::string builder; // parity= StringBuilder builder = new StringBuilder();
	for (model::Race race : keysInOrdinalOrder(*analyzed)) { // parity= for (Race race : results.keySet()) {
		runtime::Ptr<ClassMap> raceMap = analyzed->get(race); // parity= Map<PlayerClass, List<Headhunter>> raceMap = results.get(race);
		builder += "<hr><center>" + std::string(xml::enumName(race)) + "</center>"; // parity= builder.append("<hr><center>").append(race).append("</center>");
		for (model::PlayerClass pc : keysInOrdinalOrder(*raceMap)) { // parity= for (PlayerClass pc : raceMap.keySet()) {
			builder += "<br><br>" + std::string(xml::enumName(pc)) + "<br>"; // parity= builder.append("<br><br>").append(pc.toString()).append("<br>");
			runtime::Ptr<HunterList> hunterz = raceMap->get(pc); // parity= List<Headhunter> hunterz = raceMap.get(pc);
			for (int32_t pos = 0; pos < hunterz->size(); pos++) { // parity= for (int pos = 0; pos < hunterz.size(); pos++) {
				runtime::Ptr<model::event::Headhunter> hunter = hunterz->get(pos); // parity= Headhunter hunter = hunterz.get(pos);
				if (pos + 1 > static_cast<int32_t>(rewards.size()) && hunter->getKills() < configs::main::EventsConfig::HEADHUNTING_CONSOLATION_PRIZE_KILLS.load()) // parity= if (pos + 1 > rewards.size() && hunter.getKills() < EventsConfig.HEADHUNTING_CONSOLATION_PRIZE_KILLS)
					break;

				std::string name = services::player::PlayerService::getPlayerName(hunter->getHunterId()).value_or("null"); // parity= String name = PlayerService.getPlayerName(hunter.getHunterId());
				builder += "<br>" + std::to_string(pos + 1) + ". " + name + " Kills: " + std::to_string(hunter->getKills()); // parity= builder.append("<br>").append(pos + 1).append(". ").append(name).append(" Kills: ").append(hunter.getKills());
			}
		}
	}
	std::optional<std::string> page = cache::HTMLCache::getInstance().getHTML("headhunting.xhtml"); // parity= HTMLService.showHTML(admin, HTMLCache.getInstance().getHTML("headhunting.xhtml").replace("HUNTERZ", builder));
	if (!page) // parity: Java's replace of a null page
		throw runtime::NullPointerException("headhunting.xhtml is not cached"); // parity: (the same)
	services::HTMLService::showHTML(admin, commons::utils::StringUtils::replace(*page, "HUNTERZ", builder)); // parity: (the same)
}

// Java Headhunting.java:191-209
void Headhunting::showRewards(Player& admin) {
	if (rewards.isEmpty()) { // parity= if (rewards.isEmpty()) {
		sendInfo(admin, "There are no rewards available!");
		return;
	}
	std::string builder; // parity= StringBuilder builder = new StringBuilder();
	builder += "<hr><center>Rewards</center><br><hr>"; // parity= builder.append("<hr><center>Rewards</center><br><hr>");
	for (const auto& entry : rewards.snapshot()) { // parity= rewards.forEach((rank, items) -> {
		builder += "<br><br><br>Rank: " + std::to_string(entry.getKey()) + "<br>"; // parity= builder.append("<br><br><br>Rank: ").append(rank).append("<br>");
		for (runtime::Ptr<RewardItem> item : *entry.getValue()) // parity= for (RewardItem item : items)
			builder += "<br>" + std::to_string(item->getCount()) + "x " + dataholders::DataManager::ITEM_DATA->getItemTemplate(item->getId())->getName(); // parity= builder.append("<br>").append(item.getCount()).append("x ").append(DataManager.ITEM_DATA.getItemTemplate(item.getId()).getName());
	} // parity= });
	if (!consolationRewards.isEmpty()) { // parity= if (!consolationRewards.isEmpty()) {
		builder += "<br><br>Consolation prize for >= " + std::to_string(configs::main::EventsConfig::HEADHUNTING_CONSOLATION_PRIZE_KILLS.load()) + " kills<br>"; // parity= builder.append("<br><br>Consolation prize for >= ").append(EventsConfig.HEADHUNTING_CONSOLATION_PRIZE_KILLS).append(" kills<br>");
		for (runtime::Ptr<RewardItem> item : consolationRewards) // parity= for (RewardItem item : consolationRewards)
			builder += "<br>" + std::to_string(item->getCount()) + "x " + dataholders::DataManager::ITEM_DATA->getItemTemplate(item->getId())->getName(); // parity= builder.append("<br>").append(item.getCount()).append("x ").append(DataManager.ITEM_DATA.getItemTemplate(item.getId()).getName());
	}
	std::optional<std::string> page = cache::HTMLCache::getInstance().getHTML("headhunting.xhtml"); // parity= HTMLService.showHTML(admin, HTMLCache.getInstance().getHTML("headhunting.xhtml").replace("HUNTERZ", builder));
	if (!page) // parity: Java's replace of a null page
		throw runtime::NullPointerException("headhunting.xhtml is not cached"); // parity: (the same)
	services::HTMLService::showHTML(admin, commons::utils::StringUtils::replace(*page, "HUNTERZ", builder)); // parity: (the same)
}

// Java Headhunting.java:211-251
void Headhunting::rewardPlayers(Player& admin) {
	static const auto log = commons::logging::LoggerFactory::getLogger("EVENT_LOG"); // parity: (the logger)
	int32_t rewardedPlayers = 0;
	int32_t sentMails = 0;
	runtime::Ptr<Results> analyzed = results.get(); // parity: (the field read)
	for (model::Race race : keysInOrdinalOrder(*analyzed)) { // parity= for (Race race : results.keySet()) {
		runtime::Ptr<ClassMap> raceMap = analyzed->get(race); // parity= Map<PlayerClass, List<Headhunter>> raceMap = results.get(race);
		for (model::PlayerClass pc : keysInOrdinalOrder(*raceMap)) { // parity= for (PlayerClass pc : raceMap.keySet()) {
			runtime::Ptr<HunterList> hunterz = raceMap->get(pc); // parity= List<Headhunter> hunterz = raceMap.get(pc);
			for (int32_t pos = 0; pos < hunterz->size(); pos++) { // parity= for (int pos = 0; pos < hunterz.size(); pos++) {
				runtime::Ptr<model::event::Headhunter> hunter = hunterz->get(pos); // parity= final Headhunter hunter = hunterz.get(pos);
				std::vector<runtime::Ptr<RewardItem>> items; // parity= List<RewardItem> items = null;
				bool hasItems = false; // parity: (items != null)
				if (pos < 3) {
					runtime::Ptr<runtime::RcArrayList<runtime::Ref<RewardItem>>> rankItems = rewards.get(pos + 1); // parity= items = rewards.get(pos + 1);
					if (rankItems != nullptr) { // parity: (the same)
						items = rankItems->snapshot(); // parity: (the same)
						hasItems = true; // parity: (the same)
					} // parity: (the same)
				} else if (hunter->getKills() >= configs::main::EventsConfig::HEADHUNTING_CONSOLATION_PRIZE_KILLS.load()) { // parity= else if (hunter.getKills() >= EventsConfig.HEADHUNTING_CONSOLATION_PRIZE_KILLS)
					items = consolationRewards.snapshot(); // parity= items = consolationRewards;
					hasItems = true; // parity: (the same)
				}

				if (!hasItems || items.empty()) // parity= if (items == null || items.isEmpty())
					continue;

				std::string name = services::player::PlayerService::getPlayerName(hunter->getHunterId()).value_or("null"); // parity= String name = PlayerService.getPlayerName(hunter.getHunterId());
				std::string rank; // parity= String rank = switch (pos) {
				switch (pos) {
					case 0: // parity= case 0 -> "1st";
						rank = "1st"; // parity: (the same)
						break;
					case 1: // parity= case 1 -> "2nd";
						rank = "2nd"; // parity: (the same)
						break;
					case 2: // parity= case 2 -> "3rd";
						rank = "3rd"; // parity: (the same)
						break;
					default: // parity= default -> "consolation";
						rank = "consolation"; // parity: (the same)
						break;
				} // parity= };
				for (runtime::Ptr<RewardItem> item : items) { // parity= for (RewardItem item : items) {
					if (services::mail::SystemMailService::sendMail("Headhunting Corp", name, "Rewards", // parity= if (SystemMailService.sendMail("Headhunting Corp", name, "Rewards",
							"We congratulate you for reaching the " + rank + " rank in this season with a total of " + std::to_string(hunter->getKills()) + " kills.", item->getId(), // parity= "We congratulate you for reaching the " + rank + " rank in this season with a total of " + hunter.getKills() + " kills.", item.getId(),
							item->getCount(), 0, model::gameobjects::LetterType::BLACKCLOUD)) { // parity= item.getCount(), 0, LetterType.BLACKCLOUD)) {
						sentMails++;
					} else {
						log.error("Failed to send reward mail to player " + name + " for rank " + rank + ". (ItemId: " + std::to_string(item->getId()) + " Count: " // parity= log.error("Failed to send reward mail to player " + name + " for rank " + rank + ". (ItemId: " + item.getId() + " Count: "
							+ std::to_string(item->getCount()) + ")."); // parity= + item.getCount() + ").");
					}
				}
				log.info("[Race: " + std::string(xml::enumName(race)) + "] [PlayerClass: " + std::string(xml::enumName(pc)) + "] [Rank: " + rank // parity= log.info("[Race: " + race + "] [PlayerClass: " + pc + "] [Rank: " + rank + "] [RewardedPlayer: " + name + "] [Kills: " + hunter.getKills() + "]");
					+ "] [RewardedPlayer: " + name + "] [Kills: " + std::to_string(hunter->getKills()) + "]"); // parity: (the same)
				rewardedPlayers++;
			}
		}
	}
	sendInfo(admin, "Successfully rewarded " + std::to_string(rewardedPlayers) + " Players and sent " + std::to_string(sentMails) + " Mails."); // parity= sendInfo(admin, "Successfully rewarded " + rewardedPlayers + " Players and sent " + sentMails + " Mails.");
}

// Java Headhunting.java:253-276
void Headhunting::finalizeSeason(Player& admin, bool shouldReward) {
	if (configs::main::EventsConfig::ENABLE_HEADHUNTING.load()) { // parity= if (EventsConfig.ENABLE_HEADHUNTING) {
		sendInfo(admin, "Season is still active! You have to disable headhunting to reward participants.");
		return;
	} else if (shouldReward && results.get() == nullptr) { // parity= } else if (shouldReward && results == null) {
		sendInfo(admin, "No results available! The headhunting season needs to be analyzed before rewarding.");
		return;
	} else if (shouldReward && results.get()->isEmpty()) { // parity= } else if (shouldReward && results.isEmpty()) {
		results.set(nullptr); // parity= results = null;
		sendInfo(admin, "No participation in this season. Empty reward map was cleared.");
		return;
	}

	if (shouldReward)
		rewardPlayers(admin);

	services::PvpService::getInstance().finalizeHeadhuntingSeason();
	dao::HeadhuntingDAO::clearTables();
	results.set(nullptr); // parity= results = null;
	sendInfo(admin, "Successfully cleared all references for this season and finished archiving.");
}

} // namespace aion::gameserver::handlers::admincommands
