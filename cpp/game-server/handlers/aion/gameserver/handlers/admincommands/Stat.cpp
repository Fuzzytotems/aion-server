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

	runtime::Ptr<VisibleObject> target = admin.getTarget() == nullptr ? runtime::Ptr<VisibleObject>(&admin) : admin.getTarget(); // parity= VisibleObject target = admin.getTarget() == null ? admin : admin.getTarget();
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
		for (const std::unique_ptr<model::stats::calc::functions::StatFunction>& m : template_->getModifiers()) // parity= template.getModifiers().forEach(m -> applyStatFunction(creature, m));
			applyStatFunction(*creature, *m); // parity: (continued)
		sendInfo(admin, "Applied absolute stats to " + name(*creature) + ".");
	} else {
		sendInfo(admin);
	}
}

// Java Stat.java:88-92
void Stat::showStatFunctions(Player& admin, Creature& target, std::string_view searchStat) {
	std::optional<StatEnum> stat = findStat(admin, searchStat);
	if (stat) // parity= if (stat != null)
		showActiveStatFunctions(admin, target, *stat);
}

// Java Stat.java:94-104
std::optional<StatEnum> Stat::findStat(Player& admin, std::string_view searchStat) {
	std::vector<StatEnum> stats = findPossibleMatches(searchStat);
	if (stats.size() != 1) {
		std::string message = "There is no stat with that name.";
		if (!stats.empty()) {
			message += " Possible matches:\n\t"; // parity= message += " Possible matches:\n\t" + stats.stream().map(Enum::name).collect(Collectors.joining("\n\t"));
			for (size_t i = 0; i < stats.size(); i++) // parity: (continued)
				message += (i == 0 ? "" : "\n\t") + std::string(xml::enumName(stats[i])); // parity: (continued)
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
	if (commons::utils::StringUtils::utf16Length(searchStatParam) < 2) // parity= if (searchStat.length() < 2)
		return {}; // parity= return List.of();
	std::vector<StatEnum> possibleMatches;
	std::string searchStat = toLowerCase(searchStatParam);
	std::string searchStatShort = replace(searchStat, "_", "");
	const auto& names = xml::EnumTraits<StatEnum>::names; // parity: the names the next loop iterates (StatEnum.values() in ordinal order)
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++) { // parity= for (StatEnum stat : StatEnum.values()) {
		StatEnum stat = static_cast<StatEnum>(ordinal); // parity: (continued)
		std::string statName = toLowerCase(names[ordinal]); // parity= String statName = stat.name().toLowerCase();
		std::string statNameShort = replace(statName, "_", "");
		if (searchStatShort == statNameShort)
			return {stat}; // parity= return List.of(stat);
		if (statNameShort.starts_with(searchStatShort) || statName.find(searchStat) != std::string::npos) { // parity= if (statNameShort.startsWith(searchStatShort) || statName.contains(searchStat)) {
			possibleMatches.push_back(stat);
		}
	}
	return possibleMatches;
}

// Java Stat.java:124-137
void Stat::showActiveStatFunctions(Player& admin, Creature& target, StatEnum stat) {
	std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> stats = target.getGameStats()->getStatsSorted(stat);
	std::string targetInfo = admin.equals(target) ? "You currently have " : name(target) + " currently has ";
	std::string statName = ChatUtil::color(xml::enumName(stat), utils::JavaColor::WHITE); // parity= String statName = ChatUtil.color(stat.name(), Color.WHITE);
	if (stats.empty()) {
		sendInfo(admin, targetInfo + "no active " + statName + " functions.");
		return;
	}
	sendInfo(admin, targetInfo + std::to_string(stats.size()) + " active " + statName + " function(s):");
	// Java: Collectors.groupingBy(f -> f, LinkedHashMap::new, Collectors.counting()) - the first-seen order, equal infos counted together
	std::vector<std::pair<StatFunctionInfo, int64_t>> counted; // parity= stats.stream().map(StatFunctionInfo::new).collect(Collectors.groupingBy(f -> f, LinkedHashMap::new, Collectors.counting())).forEach((info, count) -> sendInfo(admin, ChatUtil.leftPad(count, 3) + "x " + info));
	for (const runtime::Ptr<model::stats::calc::functions::IStatFunction>& function : stats) { // parity: (continued)
		StatFunctionInfo info(*function); // parity: (continued)
		auto it = std::ranges::find_if(counted, [&info](const auto& entry) { return entry.first == info; }); // parity: (continued)
		if (it != counted.end()) // parity: (continued)
			it->second++; // parity: (continued)
		else // parity: (continued)
			counted.emplace_back(std::move(info), 1); // parity: (continued)
	} // parity: (continued)
	for (const auto& [info, count] : counted) // parity: (continued)
		sendInfo(admin, ChatUtil::leftPad(count, 3) + "x " + info.toString()); // parity: (continued)
}

// Java Stat.java:139-142
void Stat::listStats(Player& admin) {
	std::string stats; // parity= String stats = Arrays.stream(StatEnum.values()).map(Enum::name).collect(Collectors.joining("\n\t"));
	const auto& names = xml::EnumTraits<StatEnum>::names; // parity: (continued)
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++) // parity: (continued)
		stats += (ordinal == 0 ? "" : "\n\t") + std::string(names[ordinal]); // parity: (continued)
	sendInfo(admin, "List of stats:\n\t" + stats);
}

// Java Stat.java:144-151
void Stat::setStat(Player& admin, Creature& target, std::string_view searchStat, int32_t value) {
	std::optional<StatEnum> stat = findStat(admin, searchStat);
	if (!stat) // parity= if (stat == null)
		return;
	runtime::Ref<model::stats::calc::functions::RcStatFunction<CommandStatFunction>> function = // parity: the new CommandStatFunction(stat, value) of the next line (RcStatFunction, docs/deviations/C2.md)
		model::stats::calc::functions::RcStatFunction<CommandStatFunction>::create(*stat, value); // parity: (continued)
	applyStatFunction(target, *function); // parity= applyStatFunction(target, new CommandStatFunction(stat, value));
	std::string targetInfo = admin.equals(target) ? "Your " : name(target) + "'s ";
	sendInfo(admin, targetInfo + ChatUtil::color(xml::enumName(*stat), utils::JavaColor::WHITE) + " is now set to " + std::to_string(value) + "."); // parity= sendInfo(admin, targetInfo + ChatUtil.color(stat.name(), Color.WHITE) + " is now set to " + value + ".");
}

// Java Stat.java:153-157
void Stat::applyStatFunction(Creature& creature, model::stats::calc::functions::StatFunction& statFunction) {
	model::stats::calc::StatOwner& statOwner = CommandStatOwner::get(statFunction.getName());
	creature.getGameStats()->endEffect(statOwner);
	creature.getGameStats()->addEffect(runtime::Ptr<model::stats::calc::StatOwner>(&statOwner), // parity= creature.getGameStats().addEffect(statOwner, List.of(statFunction));
		{runtime::Ptr<model::stats::calc::functions::IStatFunction>(&statFunction)}); // parity: (continued)
}

// Java Stat.java:159-163
void Stat::cancelStatOverrides(Player& admin, Creature& target) {
	CommandStatOwner::forEach([&target](model::stats::calc::StatOwner& owner) { target.getGameStats()->endEffect(owner); });
	std::string targetInfo = admin.equals(target) ? "Your" : name(target) + "'s";
	sendInfo(admin, targetInfo + " stat overrides have been canceled.");
}

// ---- CommandStatFunction (Stat.java:165-183) ----------------------------------------------------------------------------------------------

Stat::CommandStatFunction::CommandStatFunction(StatEnum name, int32_t value) : StatFunction(name, value, true) { // parity= public CommandStatFunction(StatEnum name, int value) { super(name, value, true);
}

// C++: the parameter is stat2 (Java: stat), which would hide StatFunction::stat
void Stat::CommandStatFunction::apply(model::stats::calc::Stat2& stat2, const std::unordered_set<CalculationType>& /*calculationTypes*/) { // parity= public void apply(Stat2 stat, Set<CalculationType> calculationTypes) {
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
model::stats::calc::StatOwner& Stat::CommandStatOwner::get(StatEnum stat) { // parity= record CommandStatOwner(StatEnum stat) implements StatOwner { static final Map<StatEnum, StatOwner> statOwnerByStat = new EnumMap<>(StatEnum.class); static StatOwner get(StatEnum stat) { // the record header and its field (Stat.java:172-174) live in Stat.h
	static const std::vector<std::unique_ptr<CommandStatOwner>>* const owners = [] { // parity= return statOwnerByStat.computeIfAbsent(stat, CommandStatOwner::new); // all owners created on first use, docs/deviations/C2.md
		auto* created = new std::vector<std::unique_ptr<CommandStatOwner>>(); // parity: (continued; immortal, like the Java static map)
		for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ordinal++) // parity: (continued)
			created->push_back(std::make_unique<CommandStatOwner>(static_cast<StatEnum>(ordinal))); // parity: (continued)
		return created; // parity: (continued)
	}(); // parity: (continued)
	return *(*owners)[static_cast<size_t>(stat)]; // parity: (continued)
}

void Stat::CommandStatOwner::forEach(const std::function<void(model::stats::calc::StatOwner&)>& consumer) { // parity= static void forEach(Consumer<StatOwner> consumer) {
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ordinal++) // parity= statOwnerByStat.values().forEach(consumer);
		consumer(get(static_cast<StatEnum>(ordinal))); // parity: (continued)
}

// ---- StatFunctionInfo (Stat.java:198-229) -------------------------------------------------------------------------------------------------

Stat::StatFunctionInfo::StatFunctionInfo(model::stats::calc::functions::IStatFunction& f) // parity= record StatFunctionInfo(int value, boolean bonus, int priority, StatOwner owner, String type) { StatFunctionInfo(IStatFunction f) { // the record header (Stat.java:185) lives in Stat.h
	: value(f.getValue()), bonus(f.isBonus()), priority(f.getPriority()), owner(f.getOwner()), type([&f] { // parity= this(f.getValue(), f.isBonus(), f.getPriority(), f.getOwner(), (f instanceof StatFunctionProxy p ? p.getProxiedFunction() : f).getClass().getSimpleName());
		  auto* proxy = dynamic_cast<model::stats::calc::functions::StatFunctionProxy*>(&f); // parity: (continued)
		  return statFunctionClassName(proxy != nullptr ? *proxy->getProxiedFunction() : f); // parity: (continued)
	  }()) { // parity: (continued)
}

bool Stat::StatFunctionInfo::operator==(const StatFunctionInfo& other) const { // parity: Java's implicit record equality (Objects.equals of each component), docs/deviations/C2.md
	if (value != other.value || bonus != other.bonus || priority != other.priority || type != other.type) // parity: (the same)
		return false; // parity: (the same)
	// Java Objects.equals of the owners: identity, except AionObject (an Item) equals by object id
	if (owner.get() == other.owner.get()) // parity: (the same)
		return true; // parity: (the same)
	const auto* objectA = dynamic_cast<const model::gameobjects::AionObject*>(owner.get()); // parity: (the same)
	const auto* objectB = dynamic_cast<const model::gameobjects::AionObject*>(other.owner.get()); // parity: (the same)
	return objectA != nullptr && objectB != nullptr && objectA->equals(*objectB); // parity: (the same)
}

std::string Stat::StatFunctionInfo::toString() const {
	std::string info = isOverrideFunction() ? "=" + std::to_string(value) : value >= 0 ? "+" + std::to_string(value) : std::to_string(value); // parity= String info = isOverrideFunction() ? "=" + value : value >= 0 ? "+" + value : "" + value;
	if (type == "CommandStatFunction") { // parity= if (type.equals(CommandStatFunction.class.getSimpleName())) {
		info = ChatUtil::color(info, utils::JavaColor::CYAN);
	} else {
		if (type == "StatRateFunction") // parity= if (type.equals(StatRateFunction.class.getSimpleName()))
			info += "%";
		info = ChatUtil::color(info, value < 0 ? utils::JavaColor::RED : bonus ? utils::JavaColor::GREEN : utils::JavaColor::WHITE);
		if (bonus)
			info += " bonus";
	}
	info += ", priority: " + std::to_string(priority);
	info += ", type: " + type;
	info += ", owner: " + (owner == nullptr ? std::string("none") : utils::simpleClassName(typeid(*owner))); // parity= info += ", owner: " + (owner == null ? "none" : owner.getClass().getSimpleName());
	if (auto* effect = dynamic_cast<Effect*>(owner.get())) // parity= if (owner instanceof Effect effect)
		info += " (skill ID " + std::to_string(effect->getSkillId()) + ": " + effect->getSkillTemplate()->getL10n() + ")";
	// Java: enchantEffect.getItemSlot() != null - the C++ EnchantEffect has MAIN_HAND where Java has null (docs/deviations/C2.md)
	else if (auto* enchantEffect = dynamic_cast<EnchantEffect*>(owner.get())) // parity= else if (owner instanceof EnchantEffect enchantEffect && enchantEffect.getItemSlot() != null)
		info += " (" + std::string(xml::enumName(enchantEffect->getItemSlot())) + ")";
	else if (auto* l10n = dynamic_cast<L10n*>(owner.get())) // parity= else if (owner instanceof L10n l10n)
		info += " (" + l10n->getL10n() + ")";
	return info;
}

bool Stat::StatFunctionInfo::isOverrideFunction() const {
	return type == "CommandStatFunction" || type == "StatAbsFunction" || type == "StatSetFunction"; // parity= return type.equals(CommandStatFunction.class.getSimpleName()) || type.equals(StatAbsFunction.class.getSimpleName()) || type.equals(StatSetFunction.class.getSimpleName());
}

std::string Stat::statFunctionClassName(model::stats::calc::functions::IStatFunction& f) { // parity: Java's getClass().getSimpleName() of a StatFunction, unwrapping RcStatFunction<T> (docs/deviations/C2.md)
	std::string name = commons::utils::getSimpleClassName(typeid(f)); // parity: (the same)
	// RcStatFunction<aion::...::StatAddFunction> -> StatAddFunction (Java creates the subclass itself)
	if (name.starts_with("RcStatFunction<") && name.ends_with(">")) { // parity: (the same)
		name = name.substr(std::string_view("RcStatFunction<").size(), name.size() - std::string_view("RcStatFunction<").size() - 1); // parity: (the same)
		if (size_t scope = name.rfind("::"); scope != std::string::npos) // parity: (the same)
			name = name.substr(scope + 2); // parity: (the same)
		return name; // parity: (the same)
	}
	return utils::simpleClassName(typeid(f)); // parity: (the same)
}

} // namespace aion::gameserver::handlers::admincommands
