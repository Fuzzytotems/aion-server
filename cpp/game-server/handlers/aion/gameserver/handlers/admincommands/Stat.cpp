#include "aion/gameserver/handlers/admincommands/Stat.h"

#include <array>
#include <memory>
#include <typeinfo>

#include "aion/commons/utils/ClassName.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/AbsoluteStatsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Stat);

Stat::Stat()
	: AdminCommand("stat", "Shows and modifies any stats.",
		  "list - Lists all stats.\n"
		  "<stat> - Shows your target's active stat functions for the given stat.\n"
		  "<stat> <value> - Sets your target's stat to the given value.\n"
		  "abs <stat set ID> - Applies fixed stats of the given stats_set ID from absolute_stats.xml to your target.\n"
		  "cancel - Cancels all active stat overrides for your target.\n"
		  "Stat parameters accept lowercase and abbreviated formats, such as flytime or flyt instead of FLY_TIME.\n") {
}

// Java Stat.java:48-86
void Stat::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<VisibleObject> target = admin.getTarget() == nullptr ? runtime::Ptr<VisibleObject>(&admin) : admin.getTarget();
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (creature == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	if (params.size() == 1 && equalsIgnoreCase("list", params[0])) {
		listStats(admin);
	} else if (params.size() == 1 && equalsIgnoreCase("cancel", params[0])) {
		cancelStatOverrides(admin, *creature);
	} else if (params.size() == 1) {
		showStatFunctions(admin, *creature, params[0]);
	} else if (params.size() == 2 && !equalsIgnoreCase("abs", params[0])) {
		setStat(admin, *creature, params[0], commons::utils::parseInt(params[1]));
	} else if (params.size() == 2 && equalsIgnoreCase("abs", params[0])) {
		const ModifiersTemplate* template_ = DataManager::ABSOLUTE_STATS_DATA->getTemplate(commons::utils::parseInt(params[1]));
		if (template_ == nullptr) {
			sendInfo(admin, "Invalid stat set ID.");
			return;
		}
		for (const std::unique_ptr<model::stats::calc::functions::StatFunction>& m : template_->getModifiers())
			applyStatFunction(*creature, *m);
		sendInfo(admin, "Applied absolute stats to " + name(*creature) + ".");
	} else {
		sendInfo(admin);
	}
}

// Java Stat.java:88-92
void Stat::showStatFunctions(Player& admin, Creature& target, std::string_view searchStat) {
	std::optional<StatEnum> stat = findStat(admin, searchStat);
	if (stat)
		showActiveStatFunctions(admin, target, *stat);
}

// Java Stat.java:94-104
std::optional<StatEnum> Stat::findStat(Player& admin, std::string_view searchStat) {
	std::vector<StatEnum> stats = findPossibleMatches(searchStat);
	if (stats.size() != 1) {
		std::string message = "There is no stat with that name.";
		if (!stats.empty()) {
			message += " Possible matches:\n\t";
			for (size_t i = 0; i < stats.size(); i++)
				message += (i == 0 ? "" : "\n\t") + std::string(xml::enumName(stats[i]));
		}
		sendInfo(admin, message);
		return std::nullopt;
	}
	return stats.front();
}

// Java Stat.java:106-122
std::vector<StatEnum> Stat::findPossibleMatches(std::string_view searchStatParam) {
	using commons::utils::StringUtils::replace;
	using commons::utils::StringUtils::toLowerCase;
	if (commons::utils::StringUtils::utf16Length(searchStatParam) < 2)
		return {};
	std::vector<StatEnum> possibleMatches;
	std::string searchStat = toLowerCase(searchStatParam);
	std::string searchStatShort = replace(searchStat, "_", "");
	const auto& names = xml::EnumTraits<StatEnum>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++) {
		StatEnum stat = static_cast<StatEnum>(ordinal);
		std::string statName = toLowerCase(names[ordinal]);
		std::string statNameShort = replace(statName, "_", "");
		if (searchStatShort == statNameShort)
			return {stat};
		if (statNameShort.starts_with(searchStatShort) || statName.find(searchStat) != std::string::npos) {
			possibleMatches.push_back(stat);
		}
	}
	return possibleMatches;
}

// Java Stat.java:124-137
void Stat::showActiveStatFunctions(Player& admin, Creature& target, StatEnum stat) {
	std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> stats = target.getGameStats()->getStatsSorted(stat);
	std::string targetInfo = admin.equals(target) ? "You currently have " : name(target) + " currently has ";
	std::string statName = ChatUtil::color(xml::enumName(stat), utils::JavaColor::WHITE);
	if (stats.empty()) {
		sendInfo(admin, targetInfo + "no active " + statName + " functions.");
		return;
	}
	sendInfo(admin, targetInfo + std::to_string(stats.size()) + " active " + statName + " function(s):");
	// Java: Collectors.groupingBy(f -> f, LinkedHashMap::new, Collectors.counting()) - the first-seen order, equal infos counted together
	std::vector<std::pair<StatFunctionInfo, int64_t>> counted;
	for (const runtime::Ptr<model::stats::calc::functions::IStatFunction>& function : stats) {
		StatFunctionInfo info(*function);
		auto it = std::ranges::find_if(counted, [&info](const auto& entry) { return entry.first == info; });
		if (it != counted.end())
			it->second++;
		else
			counted.emplace_back(std::move(info), 1);
	}
	for (const auto& [info, count] : counted)
		sendInfo(admin, ChatUtil::leftPad(count, 3) + "x " + info.toString());
}

// Java Stat.java:139-142
void Stat::listStats(Player& admin) {
	std::string stats;
	const auto& names = xml::EnumTraits<StatEnum>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++)
		stats += (ordinal == 0 ? "" : "\n\t") + std::string(names[ordinal]);
	sendInfo(admin, "List of stats:\n\t" + stats);
}

// Java Stat.java:144-151
void Stat::setStat(Player& admin, Creature& target, std::string_view searchStat, int32_t value) {
	std::optional<StatEnum> stat = findStat(admin, searchStat);
	if (!stat)
		return;
	runtime::Ref<model::stats::calc::functions::RcStatFunction<CommandStatFunction>> function =
		model::stats::calc::functions::RcStatFunction<CommandStatFunction>::create(*stat, value);
	applyStatFunction(target, *function);
	std::string targetInfo = admin.equals(target) ? "Your " : name(target) + "'s ";
	sendInfo(admin, targetInfo + ChatUtil::color(xml::enumName(*stat), utils::JavaColor::WHITE) + " is now set to " + std::to_string(value) + ".");
}

// Java Stat.java:153-157
void Stat::applyStatFunction(Creature& creature, model::stats::calc::functions::StatFunction& statFunction) {
	model::stats::calc::StatOwner& statOwner = CommandStatOwner::get(statFunction.getName());
	creature.getGameStats()->endEffect(statOwner);
	creature.getGameStats()->addEffect(runtime::Ptr<model::stats::calc::StatOwner>(&statOwner),
		{runtime::Ptr<model::stats::calc::functions::IStatFunction>(&statFunction)});
}

// Java Stat.java:159-163
void Stat::cancelStatOverrides(Player& admin, Creature& target) {
	CommandStatOwner::forEach([&target](model::stats::calc::StatOwner& owner) { target.getGameStats()->endEffect(owner); });
	std::string targetInfo = admin.equals(target) ? "Your" : name(target) + "'s";
	sendInfo(admin, targetInfo + " stat overrides have been canceled.");
}

// ---- CommandStatFunction (Stat.java:165-183) ----------------------------------------------------------------------------------------------

Stat::CommandStatFunction::CommandStatFunction(StatEnum name, int32_t value) : StatFunction(name, value, true) {
}

// C++: the parameter is stat2 (Java: stat), which would hide StatFunction::stat
void Stat::CommandStatFunction::apply(model::stats::calc::Stat2& stat2, const std::unordered_set<CalculationType>& /*calculationTypes*/) {
	stat2.setBonusRate(1.0f);
	stat2.setFinalRate(1.0f);
	stat2.setBonus(static_cast<float>(getValue()) - stat2.getExactCurrentWithoutBonus());
}

int32_t Stat::CommandStatFunction::getPriority() const {
	return 120;
}

// ---- CommandStatOwner (Stat.java:185-196) -------------------------------------------------------------------------------------------------

/**
 * Java: statOwnerByStat.computeIfAbsent(stat, CommandStatOwner::new) on a static EnumMap. C++: the owners of all constants are created
 * together on first use (a function-local static, thread-safe), immortal; forEach visits every one. Ending an owner that never added a function
 * changes nothing (CreatureGameStats.endEffect removes nothing and does not call onStatsChange), so visiting the unused ones is invisible.
 */
model::stats::calc::StatOwner& Stat::CommandStatOwner::get(StatEnum stat) {
	static const std::vector<std::unique_ptr<CommandStatOwner>>* const owners = [] {
		auto* created = new std::vector<std::unique_ptr<CommandStatOwner>>(); // immortal, like the Java static map
		for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ordinal++)
			created->push_back(std::make_unique<CommandStatOwner>(static_cast<StatEnum>(ordinal)));
		return created;
	}();
	return *(*owners)[static_cast<size_t>(stat)];
}

void Stat::CommandStatOwner::forEach(const std::function<void(model::stats::calc::StatOwner&)>& consumer) {
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ordinal++)
		consumer(get(static_cast<StatEnum>(ordinal)));
}

// ---- StatFunctionInfo (Stat.java:198-229) -------------------------------------------------------------------------------------------------

Stat::StatFunctionInfo::StatFunctionInfo(model::stats::calc::functions::IStatFunction& f)
	: value(f.getValue()), bonus(f.isBonus()), priority(f.getPriority()), owner(f.getOwner()), type([&f] {
		  auto* proxy = dynamic_cast<model::stats::calc::functions::StatFunctionProxy*>(&f);
		  return statFunctionClassName(proxy != nullptr ? *proxy->getProxiedFunction() : f);
	  }()) {
}

bool Stat::StatFunctionInfo::operator==(const StatFunctionInfo& other) const {
	if (value != other.value || bonus != other.bonus || priority != other.priority || type != other.type)
		return false;
	// Java Objects.equals of the owners: identity, except AionObject (an Item) equals by object id
	if (owner.get() == other.owner.get())
		return true;
	const auto* objectA = dynamic_cast<const model::gameobjects::AionObject*>(owner.get());
	const auto* objectB = dynamic_cast<const model::gameobjects::AionObject*>(other.owner.get());
	return objectA != nullptr && objectB != nullptr && objectA->equals(*objectB);
}

std::string Stat::StatFunctionInfo::toString() const {
	std::string info = isOverrideFunction() ? "=" + std::to_string(value) : value >= 0 ? "+" + std::to_string(value) : std::to_string(value);
	if (type == "CommandStatFunction") {
		info = ChatUtil::color(info, utils::JavaColor::CYAN);
	} else {
		if (type == "StatRateFunction")
			info += "%";
		info = ChatUtil::color(info, value < 0 ? utils::JavaColor::RED : bonus ? utils::JavaColor::GREEN : utils::JavaColor::WHITE);
		if (bonus)
			info += " bonus";
	}
	info += ", priority: " + std::to_string(priority);
	info += ", type: " + type;
	info += ", owner: " + (owner == nullptr ? std::string("none") : utils::simpleClassName(typeid(*owner)));
	if (auto* effect = dynamic_cast<Effect*>(owner.get()))
		info += " (skill ID " + std::to_string(effect->getSkillId()) + ": " + effect->getSkillTemplate()->getL10n() + ")";
	// Java: enchantEffect.getItemSlot() != null - the C++ EnchantEffect has MAIN_HAND where Java has null (docs/deviations/C2.md)
	else if (auto* enchantEffect = dynamic_cast<EnchantEffect*>(owner.get()))
		info += " (" + std::string(xml::enumName(enchantEffect->getItemSlot())) + ")";
	else if (auto* l10n = dynamic_cast<L10n*>(owner.get()))
		info += " (" + l10n->getL10n() + ")";
	return info;
}

bool Stat::StatFunctionInfo::isOverrideFunction() const {
	return type == "CommandStatFunction" || type == "StatAbsFunction" || type == "StatSetFunction";
}

std::string Stat::statFunctionClassName(model::stats::calc::functions::IStatFunction& f) {
	std::string name = commons::utils::getSimpleClassName(typeid(f));
	// RcStatFunction<aion::...::StatAddFunction> -> StatAddFunction (Java creates the subclass itself)
	if (name.starts_with("RcStatFunction<") && name.ends_with(">")) {
		name = name.substr(std::string_view("RcStatFunction<").size(), name.size() - std::string_view("RcStatFunction<").size() - 1);
		if (size_t scope = name.rfind("::"); scope != std::string::npos)
			name = name.substr(scope + 2);
		return name;
	}
	return utils::simpleClassName(typeid(f));
}

} // namespace aion::gameserver::handlers::admincommands
