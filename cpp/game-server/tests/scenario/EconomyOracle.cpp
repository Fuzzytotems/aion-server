#include "EconomyOracle.h"

#include <charconv>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace aion::gameserver::scenario {

namespace {

using nlohmann::json;

/** the shortest text that reads back as the same value (std::to_chars), so a spot or a distance reaches the oracle unrounded */
template <typename T>
std::string number(T value) {
	char buffer[64];
	const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
	return std::string(buffer, result.ptr);
}

template <typename T>
std::optional<T> optionalOf(const json& node, const char* key) {
	const auto it = node.find(key);
	if (it == node.end() || it->is_null())
		return std::nullopt;
	return it->get<T>();
}

template <typename T>
std::optional<std::array<T, 2>> pairOf(const json& node, const char* key) {
	const auto it = node.find(key);
	if (it == node.end() || it->is_null())
		return std::nullopt;
	return std::array<T, 2>{it->at(0).get<T>(), it->at(1).get<T>()};
}

std::vector<int32_t> intsOf(const json& node, const char* key) {
	std::vector<int32_t> values;
	const auto it = node.find(key);
	if (it != node.end() && !it->is_null())
		for (const json& value : *it)
			values.push_back(value.get<int32_t>());
	return values;
}

/** {"name", "id"[, "value"]}: a system message the oracle names */
EconomyMessage readMessage(const json& node) {
	EconomyMessage message;
	message.name = node.at("name").get<std::string>();
	message.id = node.at("id").get<int32_t>();
	message.value = optionalOf<int64_t>(node, "value");
	return message;
}

EconomySpot readSpot(const json& node) {
	EconomySpot spot;
	spot.x = node.at("x").get<float>();
	spot.y = node.at("y").get<float>();
	spot.z = node.at("z").get<float>();
	spot.distance = node.at("distance").get<double>();
	spot.inTalkRange = node.at("inTalkRange").get<bool>();
	spot.inRangeWithoutPlusOne = node.at("inRangeWithoutPlusOne").get<bool>();
	spot.inRangeCenterToCenter = node.at("inRangeCenterToCenter").get<bool>();
	spot.otherNpcsInTalkRange = intsOf(node, "otherNpcsInTalkRange");
	return spot;
}

EconomyTalk readTalk(const json& node) {
	EconomyTalk talk;
	talk.npcId = node.at("npcId").get<int32_t>();
	talk.name = node.value("name", std::string());
	talk.canInteract = node.at("canInteract").get<bool>();
	talk.talkDistance = node.at("talkDistance").get<int32_t>();
	talk.limit = node.at("limit").get<float>();
	talk.limitWithoutPlusOne = node.at("limitWithoutPlusOne").get<float>();
	talk.limitCenterToCenter = node.at("limitCenterToCenter").get<float>();
	const json& chosen = node.at("chosenSpot");
	talk.x = chosen.at("x").get<float>();
	talk.y = chosen.at("y").get<float>();
	talk.z = chosen.at("z").get<float>();
	talk.distanceFromReference = optionalOf<double>(chosen, "distanceFromReference");
	talk.bandSpot = readSpot(node.at("bandSpot"));
	talk.nearSpot = readSpot(node.at("nearSpot"));
	talk.farSpot = readSpot(node.at("farSpot"));
	if (const json& outOfRange = node.at("outOfRange"); !outOfRange.is_null()) {
		talk.outOfRangeMessage = outOfRange.at("message").get<std::string>();
		talk.outOfRangeMessageId = outOfRange.at("messageId").get<int32_t>();
	}
	if (const json& window = node.at("startWindow"); !window.is_null()) {
		EconomyStartWindow start;
		start.ai = window.at("ai").get<std::string>();
		start.page = optionalOf<int32_t>(window, "page");
		start.questId = window.at("questId").get<int32_t>();
		start.pageValue = window.at("pageValue").get<int32_t>();
		talk.startWindow = start;
	}
	for (const json& arm : node.at("functions")) {
		EconomyFunction function;
		function.action = arm.at("action").get<int32_t>();
		function.name = optionalOf<std::string>(arm, "name");
		function.page = optionalOf<int32_t>(arm, "page");
		function.question = optionalOf<int32_t>(arm, "question");
		talk.functions.push_back(std::move(function));
	}
	return talk;
}

EconomyQuestion readQuestion(const json& node) {
	EconomyQuestion question;
	question.id = node.at("id").get<int32_t>();
	const json& params = node.at("params");
	if (params.size() != question.params.size())
		throw std::runtime_error("m5c-economy: a question with " + std::to_string(params.size()) + " parameters (SM_QUESTION_WINDOW writes 3)");
	for (size_t i = 0; i < params.size(); i++) {
		if (params[i].is_object()) { // ChatUtil.l10n: the UTF-16 code units
			const json& units = params[i].at("utf16");
			question.l10nParams[i] = {units.at(0).get<uint16_t>(), units.at(1).get<uint16_t>(), units.at(2).get<uint16_t>()};
		} else {
			question.params[i] = params[i].get<std::string>();
		}
	}
	question.senderId = node.at("senderId").get<int32_t>();
	question.range = node.at("range").get<int32_t>();
	return question;
}

EconomyItem readItem(const json& node) {
	EconomyItem item;
	item.itemId = node.at("itemId").get<int32_t>();
	item.name = node.value("name", std::string());
	item.level = node.at("level").get<int32_t>();
	item.itemGroup = node.at("itemGroup").get<std::string>();
	item.equipType = node.at("equipType").get<std::string>();
	item.manastoneSlots = node.at("manastoneSlots").get<int32_t>();
	item.maxEnchant = node.at("maxEnchant").get<int32_t>();
	const json& identification = node.at("identification");
	item.canTune = identification.at("canTune").get<bool>();
	item.sqlDefaultLoadsIdentified = identification.at("sqlDefaultLoadsIdentified").get<bool>();
	item.seedTuneCountForUnidentified = optionalOf<int32_t>(identification, "seedTuneCountForUnidentified");
	item.optionalSocketsRange = pairOf<int32_t>(identification, "optionalSocketsRange");
	item.enchantBonusRange = pairOf<int32_t>(identification, "enchantBonusRange");
	item.identifyMessageId = optionalOf<int32_t>(identification, "messageId");
	item.tuneCountAfter = optionalOf<int32_t>(identification, "tuneCountAfter");
	if (const auto it = identification.find("animation"); it != identification.end() && !it->is_null())
		item.identifyAnimation = EconomyAnimation{it->at("time").get<int32_t>(), it->at("start").get<int32_t>(), it->at("end").get<int32_t>(),
		                                          it->at("abort").get<int32_t>()};
	const json& breaking = node.at("breakItem");
	item.breakable = breaking.at("breakable").get<bool>();
	if (item.breakable) {
		for (const json& stone : breaking.at("stones"))
			item.breakStones.emplace_back(stone.at("itemId").get<int32_t>(), stone.at("probability").get<double>());
		item.breakCountRange = pairOf<int32_t>(breaking, "countRange");
		item.breakMessageId = optionalOf<int32_t>(breaking, "messageId");
	}
	const json& equip = node.at("equip");
	item.equipPasses = equip.at("passes").get<bool>();
	item.equipRefusedBy = optionalOf<std::string>(equip, "refusedBy");
	item.equipMessage = optionalOf<std::string>(equip, "message");
	item.equipMessageId = optionalOf<int32_t>(equip, "messageId");
	item.maxLevelRestrict = equip.at("maxLevelRestrict").get<int32_t>();
	item.itemRace = equip.at("itemRace").get<std::string>();
	item.equipNotModelled = equip.at("notModelled").get<std::string>();
	item.requiredLevel = equip.at("requiredLevel").get<int32_t>();
	item.startExpOfRequiredLevel = optionalOf<int64_t>(equip, "startExpOfRequiredLevel");
	item.requiredSkills = intsOf(equip, "requiredSkills");
	item.knownRequiredSkills = intsOf(equip, "knownRequiredSkills");
	if (const auto it = node.find("socketing"); it != node.end())
		for (const json& entry : *it) {
			EconomySocketing socket;
			socket.stoneId = entry.at("stoneId").get<int32_t>();
			socket.canAct = entry.at("canAct").get<bool>();
			socket.fits = entry.at("fits").get<bool>();
			socket.refusedBy = optionalOf<std::string>(entry, "refusedBy");
			socket.slotLevel = optionalOf<int32_t>(entry, "slotLevel");
			socket.socketsRange = pairOf<int32_t>(entry, "socketsRange");
			socket.successChance = optionalOf<float>(entry, "successChance");
			socket.certain = optionalOf<bool>(entry, "certain");
			socket.impossible = optionalOf<bool>(entry, "impossible");
			socket.needsOptionalSocket = optionalOf<bool>(entry, "needsOptionalSocket");
			socket.rate = optionalOf<float>(entry, "rate");
			socket.messageId = optionalOf<int32_t>(entry, "messageId");
			socket.stoneConsumed = optionalOf<bool>(entry, "stoneConsumed");
			socket.auditLog = optionalOf<std::string>(entry, "auditLog");
			if (const auto success = entry.find("success"); success != entry.end() && !success->is_null())
				socket.successMessageId = success->at("messageId").get<int32_t>();
			if (const auto failure = entry.find("failure"); failure != entry.end() && !failure->is_null())
				socket.failureMessageId = failure->at("messageId").get<int32_t>();
			socket.animationMillis = optionalOf<int32_t>(entry, "animationMillis");
			item.socketing.push_back(std::move(socket));
		}
	return item;
}

EconomyDaeva readDaeva(const json& node) {
	EconomyDaeva daeva;
	daeva.playerClass = node.at("class").get<std::string>();
	daeva.startingClass = node.at("startingClass").get<std::string>();
	daeva.race = node.at("race").get<std::string>();
	const json& seed = node.at("seed");
	daeva.exp = seed.at("exp").get<int64_t>();
	daeva.questId = seed.at("quest").at("id").get<int32_t>();
	daeva.questStatus = seed.at("quest").at("status").get<std::string>();
	daeva.oldLevel = seed.at("oldLevel").get<int32_t>();
	daeva.level = node.at("level").get<int32_t>();
	daeva.levelWithoutQuest = node.at("levelWithoutQuest").get<int32_t>();
	const json& world = node.at("enterWorld");
	daeva.learnNewSkills = {world.at("learnNewSkills").at(0).get<int32_t>(), world.at("learnNewSkills").at(1).get<int32_t>()};
	daeva.storedSkills = intsOf(world, "storedSkills");
	for (const json& learned : world.at("learnedSkills"))
		daeva.learnedSkills.push_back({learned.at("skillId").get<int32_t>(), learned.at("level").get<int32_t>(), learned.at("class").get<std::string>()});
	if (const json& swap = world.at("daevaSwap"); !swap.is_null()) {
		daeva.swapRemoved = optionalOf<int32_t>(swap, "removed");
		daeva.swapAdded = optionalOf<int32_t>(swap, "added");
	}
	daeva.skills = intsOf(world, "skills");
	daeva.learnedRecipes = intsOf(world, "learnedRecipes");
	return daeva;
}

EconomyCraft readCraft(const json& node) {
	EconomyCraft craft;
	craft.mapId = node.at("map").get<int32_t>();
	const json& recipe = node.at("recipe");
	craft.recipeId = recipe.at("id").get<int32_t>();
	craft.skillId = recipe.at("skillId").get<int32_t>();
	for (const json& component : recipe.at("components"))
		craft.recipeComponents.emplace_back(component.at(0).get<int32_t>(), component.at(1).get<int64_t>());
	craft.productId = recipe.at("product").at("itemId").get<int32_t>();
	craft.productQuantity = recipe.at("product").at("quantity").get<int64_t>();
	craft.fewestSteps = recipe.at("steps").at("fewest").get<int32_t>();
	craft.mostSteps = recipe.at("steps").at("most").get<int32_t>();
	craft.fewestMillis = recipe.at("finishMillis").at("fewest").get<int64_t>();
	craft.mostMillis = recipe.at("finishMillis").at("most").get<int64_t>();
	craft.interval = recipe.at("timing").at("interval").get<int32_t>();
	craft.firstTickDelay = recipe.at("timing").at("firstTickDelay").get<int32_t>();
	craft.xpReward = recipe.at("xpReward").get<int64_t>();
	craft.playerExp = recipe.at("playerExp").get<int64_t>();
	craft.skillLevelAfter = recipe.at("skillLevelAfter").get<int32_t>();
	craft.masterNpcId = node.at("master").at("npcId").get<int32_t>();
	craft.master = readTalk(node.at("master").at("talk"));
	const json& learn = node.at("learn");
	craft.dialogAction = learn.at("dialogAction").get<int32_t>();
	craft.minCharacterLevel = learn.at("minCharacterLevel").get<int32_t>();
	craft.learnCost = learn.at("cost").get<int64_t>();
	craft.learnSupported = learn.at("supported").get<bool>();
	craft.learnQuestion = readQuestion(learn.at("question"));
	craft.recipesLearnedWithTheSkill = intsOf(learn.at("yes"), "learnedRecipes");
	craft.learnNotEnoughKinah = learn.at("notEnoughKinah").get<std::string>();
	for (const json& entry : node.at("components")) {
		EconomyComponent component;
		component.itemId = entry.at("itemId").get<int32_t>();
		component.quantity = entry.at("quantity").get<int64_t>();
		component.vendor = optionalOf<int32_t>(entry, "vendor");
		for (const json& seller : entry.at("vendors"))
			component.vendors.push_back({seller.at("npcId").get<int32_t>(), seller.at("kinah").get<int64_t>(),
			                             seller.at("distanceFromMaster").get<double>(), readTalk(seller.at("talk"))});
		craft.components.push_back(std::move(component));
	}
	for (const json& seed : node.at("seedItems"))
		craft.seedItems.emplace_back(seed.at("itemId").get<int32_t>(), seed.at("count").get<int64_t>());
	craft.exactKinah = node.at("exactKinah").get<int64_t>();
	const json& seedSpot = node.at("seedSpot");
	craft.seedWorldId = seedSpot.at("worldId").get<int32_t>();
	craft.seedX = seedSpot.at("x").get<float>();
	craft.seedY = seedSpot.at("y").get<float>();
	craft.seedZ = seedSpot.at("z").get<float>();
	const json& tool = node.at("tool");
	craft.toolTemplateId = tool.at("templateId").get<int32_t>();
	for (const json& entry : tool.at("tools"))
		craft.tools.push_back({entry.at("staticId").get<int32_t>(), entry.at("x").get<float>(), entry.at("y").get<float>(), entry.at("z").get<float>(),
		                       entry.at("distanceFromMaster").get<double>()});
	craft.chosenToolStaticId = tool.at("chosen").at("staticId").get<int32_t>();
	craft.checkCraftRange = tool.at("checkCraftRange").get<float>();
	craft.packetRange = tool.at("packetRange").get<float>();
	for (const json& entry : tool.at("spots")) {
		EconomyCraftSpot spot;
		spot.distance = entry.at("distance").get<double>();
		spot.x = entry.at("x").get<float>();
		spot.y = entry.at("y").get<float>();
		spot.z = entry.at("z").get<float>();
		spot.inPacketRange = entry.at("inPacketRange").get<bool>();
		spot.inCheckCraftRange = entry.at("inCheckCraftRange").get<bool>();
		spot.outcome = entry.at("outcome").get<std::string>();
		spot.otherToolsInCheckCraftRange = intsOf(entry, "otherToolsInCheckCraftRange");
		craft.spots.push_back(std::move(spot));
	}
	return craft;
}

void flag(std::vector<std::string>& arguments, const char* name, const std::string& value) {
	arguments.emplace_back(name);
	arguments.push_back(value);
}

} // namespace

std::vector<std::string> economyArguments(const EconomyRequest& request) {
	std::vector<std::string> arguments{"m5c-economy"};
	if (request.staticData)
		flag(arguments, "--static-data", request.staticData->string());
	if (request.profile)
		flag(arguments, "--profile", request.profile->string());
	else if (request.noProfile)
		arguments.emplace_back("--no-profile");
	for (const std::string& setting : request.settings)
		flag(arguments, "--set", setting);
	if (request.mapId)
		flag(arguments, "--map", std::to_string(*request.mapId));
	for (const int32_t npcId : request.npcIds)
		flag(arguments, "--npc", std::to_string(npcId));
	if (request.near)
		flag(arguments, "--near", number((*request.near)[0]) + "," + number((*request.near)[1]) + "," + number((*request.near)[2]));
	if (request.far)
		flag(arguments, "--far", number(*request.far));
	if (request.direction)
		flag(arguments, "--direction", number(*request.direction));
	if (request.recoverExp)
		flag(arguments, "--recover-exp", std::to_string(*request.recoverExp));
	if (request.npcExpands)
		flag(arguments, "--npc-expands", std::to_string(*request.npcExpands));
	if (request.questExpands)
		flag(arguments, "--quest-expands", std::to_string(*request.questExpands));
	if (request.itemExpands)
		flag(arguments, "--item-expands", std::to_string(*request.itemExpands));
	for (const std::string& mail : request.mails)
		flag(arguments, "--mail", mail);
	for (const int32_t itemId : request.itemIds)
		flag(arguments, "--item", std::to_string(itemId));
	for (const int32_t stone : request.manastones)
		flag(arguments, "--manastone", std::to_string(stone));
	if (request.membership)
		flag(arguments, "--membership", std::to_string(*request.membership));
	if (request.playerClass)
		flag(arguments, "--class", *request.playerClass);
	if (request.race)
		flag(arguments, "--race", *request.race);
	if (request.level)
		flag(arguments, "--level", std::to_string(*request.level));
	for (const std::string& influence : request.influences)
		flag(arguments, "--influence", influence);
	if (request.daevaClass)
		flag(arguments, "--daeva", *request.daevaClass);
	if (request.daevaOldLevel)
		flag(arguments, "--daeva-old-level", std::to_string(*request.daevaOldLevel));
	if (request.craftRecipe)
		flag(arguments, "--craft-recipe", std::to_string(*request.craftRecipe));
	if (request.craftTool)
		flag(arguments, "--craft-tool", std::to_string(*request.craftTool));
	if (request.craftMap)
		flag(arguments, "--craft-map", std::to_string(*request.craftMap));
	for (const double distance : request.craftDistances)
		flag(arguments, "--craft-distance", number(distance));
	return arguments;
}

const EconomyTalk& EconomyAnswer::talkOf(int32_t npcId) const {
	for (const EconomyTalk& block : talk)
		if (block.npcId == npcId)
			return block;
	throw std::out_of_range("m5c-economy answered no talk block of npc " + std::to_string(npcId));
}

const EconomyItem& EconomyAnswer::item(int32_t itemId) const {
	for (const EconomyItem& block : items)
		if (block.itemId == itemId)
			return block;
	throw std::out_of_range("m5c-economy answered no item block of item " + std::to_string(itemId));
}

EconomyAnswer parseEconomy(std::string_view economyJson) {
	const json answer = json::parse(economyJson);
	if (answer.value("format", std::string()) != "aion-m5c-economy")
		throw std::runtime_error("not an m5c-economy answer: format " + answer.value("format", std::string("(none)")));
	EconomyAnswer economy;
	economy.mapId = answer.at("map").get<int32_t>();
	for (const auto& [race, prices] : answer.at("prices").items()) {
		const json& bytes = prices.at("smPrices");
		economy.smPrices[race] = {bytes.at(0).get<int32_t>(), bytes.at(1).get<int32_t>(), bytes.at(2).get<int32_t>()};
	}
	for (const json& block : answer.at("talk"))
		economy.talk.push_back(readTalk(block));
	if (const json& recovery = answer.at("recovery"); !recovery.is_null()) {
		EconomyRecovery value;
		value.recoverableExp = recovery.at("recoverableExp").get<int64_t>();
		value.price = optionalOf<int64_t>(recovery, "price");
		if (const auto it = recovery.find("question"); it != recovery.end() && !it->is_null())
			value.question = readQuestion(*it);
		value.messageId = optionalOf<int32_t>(recovery, "messageId");
		if (const auto it = recovery.find("yes"); it != recovery.end() && !it->is_null()) {
			value.yesKinahDelta = it->at("kinahDelta").get<int64_t>();
			value.yesExpDelta = it->at("expDelta").get<int64_t>();
			value.yesRecoverableExpAfter = it->at("recoverableExpAfter").get<int64_t>();
			for (const json& message : it->at("messages"))
				value.yesMessages.push_back(readMessage(message));
		}
		if (const auto it = recovery.find("notEnoughKinah"); it != recovery.end() && !it->is_null())
			value.notEnoughKinah = readMessage(*it);
		economy.recovery = std::move(value);
	}
	for (const json& block : answer.at("cube")) {
		EconomyCube cube;
		cube.npcId = block.at("npcId").get<int32_t>();
		cube.answer = block.at("answer").get<std::string>();
		cube.price = optionalOf<int64_t>(block, "price");
		if (const auto it = block.find("question"); it != block.end() && !it->is_null())
			cube.question = readQuestion(*it);
		if (const auto it = block.find("yes"); it != block.end() && !it->is_null()) {
			cube.npcExpandsAfter = it->at("npcExpandsAfter").get<int32_t>();
			cube.yesKinahDelta = it->at("kinahDelta").get<int64_t>();
			cube.cubeSlotsAdded = it->at("cubeSlotsAdded").get<int32_t>();
			cube.yesMessage = readMessage(it->at("message"));
			const json& update = it->at("smCubeUpdate");
			cube.smCubeUpdate = EconomyCubeUpdate{update.at("action").get<int32_t>(), update.at("storage").get<int32_t>(),
			                                      update.at("npcExpands").get<int32_t>(), update.at("questExpands").get<int32_t>(),
			                                      update.at("itemExpands").get<int32_t>()};
		}
		if (const auto it = block.find("notEnoughKinah"); it != block.end() && !it->is_null())
			cube.notEnoughKinah = readMessage(*it);
		cube.messageId = optionalOf<int32_t>(block, "messageId");
		economy.cube.push_back(std::move(cube));
	}
	if (const json& removal = answer.at("manastoneRemoval"); !removal.is_null()) {
		std::map<std::string, int64_t> byRace;
		for (const auto& [race, price] : removal.at("byRace").items())
			byRace[race] = price.get<int64_t>();
		economy.removalPrice = std::move(byRace);
		economy.removalBasePrice = removal.at("basePrice").get<int64_t>();
		economy.removalSucceedMessageId = removal.at("messages").at("succeed").get<int32_t>();
		economy.removalNotEnoughKinahMessageId = removal.at("messages").at("notEnoughKinah").get<int32_t>();
	}
	for (const json& block : answer.at("mail")) {
		EconomyMail mail;
		mail.spec = block.at("spec").get<std::string>();
		if (const auto it = block.find("byRace"); it != block.end()) { // a negative kinah is audited: no costs at all
			mail.itemCommission = block.at("itemCommission").get<int64_t>();
			mail.kinahCommission = block.at("kinahCommission").get<int64_t>();
			mail.serviceBase = block.at("serviceBase").get<int64_t>();
			for (const auto& [race, cost] : it->items())
				mail.byRace[race] = {cost.at("servicePrice").get<int64_t>(), cost.at("total").get<int64_t>()};
		}
		economy.mail.push_back(std::move(mail));
	}
	for (const json& block : answer.at("items"))
		economy.items.push_back(readItem(block));
	const json& character = answer.at("character");
	economy.characterClass = character.at("class").get<std::string>();
	economy.characterRace = character.at("race").get<std::string>();
	economy.characterLevel = character.at("level").get<int32_t>();
	if (const json& learned = character.at("learnedSkills"); !learned.is_null())
		economy.learnedSkills = learned.get<std::vector<int32_t>>();
	if (const auto it = answer.find("daeva"); it != answer.end() && !it->is_null())
		economy.daeva = readDaeva(*it);
	if (const auto it = answer.find("craft"); it != answer.end() && !it->is_null())
		economy.craft = readCraft(*it);
	return economy;
}

EconomyAnswer runEconomy(const Oracle& oracle, const EconomyRequest& request) {
	return parseEconomy(oracle.run(economyArguments(request)));
}

} // namespace aion::gameserver::scenario
