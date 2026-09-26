#include "aion/gameserver/utils/chathandlers/ChatCommand.h"

#include <algorithm>
#include <regex>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"

namespace aion::gameserver::utils::chathandlers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.chathandlers.ChatCommand");

namespace {

/**
 * Java ChatUtil.color(message, Color.WHITE): `[color:<message>;1 1 1]` (DecimalFormat ".##" formats 255 / 255f as "1"). A local helper
 * because the constructor needs it before ChatUtil is ported; parseSyntaxInfo calls ChatUtil::color once that exists.
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

bool ChatCommand::run(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	AION_UNPORTED();
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

std::optional<std::string> ChatCommand::toErrorMessage(const runtime::IllegalArgumentException& e) {
	AION_UNPORTED();
}

void ChatCommand::sendInfo(model::gameobjects::player::Player& player, std::span<const std::string> message) {
	AION_UNPORTED();
}

std::string ChatCommand::join(std::span<const std::string> params, int32_t startIndex) {
	AION_UNPORTED();
}

std::string ChatCommand::name(model::gameobjects::VisibleObject& visibleObject) {
	AION_UNPORTED();
}

std::string ChatCommand::worldName(int32_t worldId) {
	AION_UNPORTED();
}

void ChatCommand::info(model::gameobjects::player::Player& player, std::optional<std::string_view> message) {
	// Java: throw new UnsupportedOperationException("Please don't call me and don't override me! Use sendInfo() instead. ...")
	AION_UNPORTED();
}

} // namespace aion::gameserver::utils::chathandlers
