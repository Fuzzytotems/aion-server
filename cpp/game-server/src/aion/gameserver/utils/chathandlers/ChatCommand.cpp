#include "aion/gameserver/utils/chathandlers/ChatCommand.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::utils::chathandlers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.chathandlers.ChatCommand");

namespace {

/**
 * Java ChatUtil.color(message, Color.WHITE): `[color:<message>;1 1 1]` (DecimalFormat ".##" formats 255 / 255f as "1"). A local helper
 * because the constructor runs while the command registry is built; ChatCommandTest checks it equals ChatUtil::color(message, WHITE).
 */
std::string colorWhite(std::string_view message) {
	return std::string("[color:").append(message).append(";1 1 1]");
}

/** Java String.isBlank() for the ASCII whitespace Character.isWhitespace accepts. */
bool isBlank(std::string_view text) {
	return std::ranges::all_of(text, [](unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1C && c <= 0x1F); });
}

/** Java String.trim(): strips the characters <= U+0020 at both ends. */
std::string_view javaTrim(std::string_view text) {
	while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ')
		text.remove_prefix(1);
	while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ')
		text.remove_suffix(1);
	return text;
}

/** Java `text.split("\n")`: the pieces between newlines without the trailing empty pieces. */
std::vector<std::string_view> splitLines(std::string_view text) {
	std::vector<std::string_view> lines;
	size_t start = 0;
	for (size_t end = text.find('\n'); end != std::string_view::npos; end = text.find('\n', start)) {
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	lines.push_back(text.substr(start));
	while (lines.size() > 1 && lines.back().empty())
		lines.pop_back();
	return lines;
}

/** Java String.equalsIgnoreCase for the ASCII text of a command line */
bool equalsIgnoreCase(std::string_view a, std::string_view b) {
	return a.size() == b.size() &&
		   std::ranges::equal(a, b, [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

/** Java String.toLowerCase() (ASCII: enum names) */
std::string toLowerCase(std::string text) {
	std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

/** Java `text.split("\\.")` without the trailing empty pieces */
std::vector<std::string> splitDots(std::string_view text) {
	std::vector<std::string> parts;
	size_t start = 0;
	for (size_t dot = text.find('.'); dot != std::string_view::npos; dot = text.find('.', start)) {
		parts.emplace_back(text.substr(start, dot - start));
		start = dot + 1;
	}
	parts.emplace_back(text.substr(start));
	while (parts.size() > 1 && parts.back().empty())
		parts.pop_back();
	return parts;
}

/**
 * Java `String.join(" ", enumName.split("(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])"))`: a space between a lower and an upper case letter,
 * and between two upper case letters when a lower case one follows ("SiegeRace" -> "Siege Race", "NPCType" -> "NPC Type")
 */
std::string splitCamelCase(std::string_view name) {
	std::string out;
	for (size_t i = 0; i < name.size(); i++) {
		if (i > 0) {
			const auto prev = static_cast<unsigned char>(name[i - 1]), cur = static_cast<unsigned char>(name[i]);
			const bool lowerUpper = std::islower(prev) && std::isupper(cur);
			const bool upperUpperLower =
				std::isupper(prev) && std::isupper(cur) && i + 1 < name.size() && std::islower(static_cast<unsigned char>(name[i + 1]));
			if (lowerUpper || upperUpperLower)
				out += ' ';
		}
		out += name[i];
	}
	return out;
}

/** Java String.replace(CharSequence, CharSequence). */
void replaceAll(std::string& text, std::string_view from, std::string_view to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
}

} // namespace

ChatCommand::ChatCommand(std::string_view prefixValue, std::string_view aliasValue, std::string_view descriptionValue,
	std::string_view syntaxInfoValue)
	: prefix(prefixValue), alias(aliasValue), description(descriptionValue), syntaxInfo(parseSyntaxInfo(syntaxInfoValue)) {
}

ChatCommand::~ChatCommand() = default;

// Java ChatCommand.java:60-79
bool ChatCommand::run(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	if (params.size() == 1 && equalsIgnoreCase(params[0], "help")) {
		sendInfo(player, "Command: " + ChatUtil::color(getAliasWithPrefix(), JavaColor::WHITE) + "\n\t" +
							 (getDescription().empty() ? std::string("No description available.") : getDescription()) + "\n" + getSyntaxInfo());
		return true;
	}

	try {
		try {
			execute(player, params);
		} catch (const runtime::IllegalArgumentException& e) {
			std::optional<std::string> message = toErrorMessage(e);
			if (message)
				sendInfo(player, *message);
			else
				sendInfo(player); // Java sendInfo(player, (String) null): the syntax info
		}
	} catch (const std::exception& t) { // Java: catch (Throwable t)
		runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
		log.error("Exception executing chat command \"" + getAliasWithPrefix() + " " + join(params, 0) + "\" - Player: " + player.getName() +
					  ", Target: " + (target ? target->toString() : std::string("null")),
			t);
		return false;
	}
	return true;
}

std::string ChatCommand::parseSyntaxInfo(std::string_view value) {
	std::string sb = "Syntax:";
	if (isBlank(value)) {
		sb.append("\n\tNo syntax info available.");
	} else {
		bool containsSquareBrackets = false;
		for (std::string_view info : splitLines(value)) {
			size_t separator = info.find(" - "); // Java: info.split(" - ", 2)
			if (separator != std::string_view::npos) {
				std::string_view syntax = info.substr(0, separator);
				if (!containsSquareBrackets && syntax.find('[') != std::string_view::npos)
					containsSquareBrackets = true;
				sb.append("\n\t").append(colorWhite(getAliasWithPrefix())).append(1, ' ');
				static const std::regex parameterWords(R"(([^<>\[\]| ]+))");
				std::string highlighted = std::regex_replace(std::string(syntax), parameterWords, colorWhite("$1"));
				replaceAll(highlighted, "[[color:f;", "[[color:f\xE2\x80\x8B;"); // Java: "[[color:f​;" (UTF-8 zero width space)
				sb.append(javaTrim(highlighted));
				sb.append(" - ");
				sb.append(info.substr(separator + 3));
			} else {
				sb.append("\n").append(info);
			}
		}
		if (containsSquareBrackets)
			sb.append("\nNote: Parameters enclosed in square brackets are optional.");
	}
	return sb;
}

int8_t ChatCommand::getLevel() {
	auto accessLevels = configs::administration::CommandsConfig::ACCESS_LEVELS.get();
	auto level = accessLevels->find(getAliasForLevel());
	if (level == accessLevels->end())
		throw runtime::NullPointerException("Missing access level for " + prefix + getAliasForLevel());
	return level->second;
}

// Java ChatCommand.java:155-182. C++: Java's null message is an exception whose what() is empty. The enum arm reads the constants from
// EnumConstantException (utils::enumValueOf's exception) where Java parses the message and calls Class.forName; another IllegalArgumentException
// with Java's "No enum constant " text gets its name from the message and lists no values, where Java's Class.forName of an unknown class logs
// "Could not get enum values" and lists nothing too.
std::optional<std::string> ChatCommand::toErrorMessage(const runtime::IllegalArgumentException& e) {
	std::string msg = e.what();
	if (msg.empty()) // Java null (or ""): a NumberFormatException still answers "Invalid number."
		return dynamic_cast<const commons::utils::NumberFormatException*>(&e) ? std::optional<std::string>("Invalid number.") : std::nullopt;
	if (msg.starts_with("No enum constant ")) { // "No enum constant com.aionemu.gameserver.model.siege.SiegeRace.invalidName"
		std::vector<std::string> enumParts = splitDots(std::string_view(msg).substr(17));
		std::string enumName = enumParts.size() >= 2 ? enumParts[enumParts.size() - 2] : enumParts.back(); // -> "SiegeRace"
		enumName = toLowerCase(splitCamelCase(enumName));                                                  // -> "siege race"
		msg = "Invalid " + enumName + ".";
		if (const auto* enumException = dynamic_cast<const EnumConstantException*>(&e)) {
			std::string values;
			for (const std::string& value : enumException->getAllValues())
				values += (values.empty() ? "" : ", ") + value;
			msg += "\nPossible values:\n" + values;
		} else {
			log.error("Could not get enum values for " + enumName);
		}
	} else if (dynamic_cast<const commons::utils::NumberFormatException*>(&e)) { // parseInt and parseLong don't provide nice error messages
		if (msg.starts_with("For input string: "))
			msg = "Invalid number: " + msg.substr(18);
		else
			msg = "Invalid number.";
	}
	return msg;
}

// Java ChatCommand.java:194-206. C++: an empty span is Java's empty or null message
void ChatCommand::sendInfo(model::gameobjects::player::Player& player, std::span<const std::string> message) {
	std::string sb;
	if (!message.empty()) {
		for (size_t i = 0; i < message.size(); i++) {
			if (i > 0)
				sb += '\n';
			sb += message[i];
		}
	} else {
		sb += getSyntaxInfo();
	}
	for (const std::string& part : ChatUtil::split(sb))
		PacketSendUtility::sendMessage(player, part);
}

// Java ChatCommand.java:208-210: Stream.of(params).skip(startIndex).collect(joining(" "))
std::string ChatCommand::join(std::span<const std::string> params, int32_t startIndex) {
	std::string joined;
	bool first = true;
	for (size_t i = static_cast<size_t>(std::max(0, startIndex)); i < params.size(); i++) {
		if (!first)
			joined += ' ';
		joined += params[i];
		first = false;
	}
	return joined;
}

// Java ChatCommand.java:215-221: every Java VisibleObjectTemplate implements L10n, so an object with a template answers its l10n name
std::string ChatCommand::name(model::gameobjects::VisibleObject& visibleObject) {
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&visibleObject))
		return ChatUtil::charName(*player);
	if (const model::templates::VisibleObjectTemplate* objectTemplate = visibleObject.getObjectTemplate())
		return objectTemplate->getL10n();
	return visibleObject.getName();
}

// Java ChatCommand.java:226-231
std::string ChatCommand::worldName(int32_t worldId) {
	const model::templates::world::WorldMapTemplate* worldTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(worldId);
	if (worldTemplate == nullptr)
		return std::to_string(worldId);
	return worldTemplate->getL10nId() != 0 ? worldTemplate->getL10n() : worldTemplate->getName();
}

// Java ChatCommand.java:238-241
void ChatCommand::info(model::gameobjects::player::Player& /*player*/, std::optional<std::string_view> /*message*/) {
	throw runtime::UnsupportedOperationException(
		"Please don't call me and don't override me! Use sendInfo() instead. Syntax info can be initialized in constructor.");
}

} // namespace aion::gameserver::utils::chathandlers
