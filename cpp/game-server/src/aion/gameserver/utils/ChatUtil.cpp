#include "aion/gameserver/utils/ChatUtil.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <regex>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::utils {

namespace {

namespace StringUtils = commons::utils::StringUtils;

/** Java: AbstractGmCommandPacket.UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER (P5-15; the header does not exist yet) */
constexpr std::u16string_view UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER = u"?";

/**
 * Java: AbstractGmCommandPacket.replaceUnsupportedCommandChars - every code point outside U+0000..U+013E becomes "?" (P5-15; stands in until it
 * exists). The Java regex character class matches code points: a surrogate pair is one match and one "?", a lone surrogate one match as well.
 */
std::u16string replaceUnsupportedCommandChars(std::u16string_view input) {
	std::u16string result;
	result.reserve(input.size());
	for (size_t i = 0; i < input.size(); ++i) {
		char16_t c = input[i];
		if (c <= 0x013E) {
			result += c;
			continue;
		}
		result += UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER;
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 && input[i + 1] <= 0xDFFF)
			++i; // the low surrogate belongs to the same code point
	}
	return result;
}

/** Java: \h (horizontal whitespace) of java.util.regex */
bool isHorizontalWhitespace(char16_t c) noexcept {
	return c == u' ' || c == u'\t' || c == u'\u00A0' || c == u'\u1680' || c == u'\u180E' || (c >= u'\u2000' && c <= u'\u200A') ||
		c == u'\u202F' || c == u'\u205F' || c == u'\u3000';
}

/** Java: String.split("\\h+") (trailing empty strings removed; the input is trimmed, so there is no leading empty string) */
std::vector<std::u16string> splitHorizontalWhitespace(std::u16string_view text) {
	std::vector<std::u16string> parts;
	size_t start = 0;
	for (size_t i = 0; i <= text.size(); ++i) {
		if (i == text.size() || isHorizontalWhitespace(text[i])) {
			parts.emplace_back(text.substr(start, i - start));
			while (i < text.size() && isHorizontalWhitespace(text[i]))
				++i;
			start = i;
			if (i == text.size())
				break;
			--i;
		}
	}
	while (!parts.empty() && parts.back().empty())
		parts.pop_back();
	return parts;
}

/** Deviation: Java Character.isLowerCase(char) only for the letters commons StringUtils maps (ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic) */
bool isLowerCase(char16_t c) noexcept {
	if (c >= u'a' && c <= u'z')
		return true;
	if (c < 0x80)
		return false;
	return StringUtils::toUpperCase(static_cast<char32_t>(c)) != static_cast<char32_t>(c) &&
		StringUtils::toLowerCase(static_cast<char32_t>(c)) == static_cast<char32_t>(c);
}

/** Java: String.indexOf(int ch, int beginIndex, int endIndex) */
int32_t indexOf(std::u16string_view text, char16_t ch, int32_t beginIndex, int32_t endIndex) noexcept {
	beginIndex = std::max(beginIndex, 0);
	endIndex = std::min(endIndex, static_cast<int32_t>(text.size()));
	for (int32_t i = beginIndex; i < endIndex; ++i) {
		if (text[static_cast<size_t>(i)] == ch)
			return i;
	}
	return -1;
}

/** Java: Float.parseFloat */
float parseFloat(std::u16string_view text) {
	return commons::configuration::transformers::NumberParser::parseFloat(StringUtils::toUtf8(text));
}

/** Java: Integer.parseInt */
int32_t parseInt(std::u16string_view text) {
	return commons::utils::parseInt(StringUtils::toUtf8(text));
}

} // namespace

std::string ChatUtil::formatDecimal(double value) {
	if (std::isnan(value))
		return "NaN";
	bool isNegative = std::signbit(value);
	if (std::isinf(value))
		return isNegative ? "-\xE2\x88\x9E" : "\xE2\x88\x9E"; // DecimalFormatSymbols.getInfinity() of Locale.US: U+221E
	// the exact decimal expansion is long enough to decide HALF_EVEN for every value with at most 17 significant digits
	char exact[400];
	auto [end, error] = std::to_chars(exact, exact + sizeof(exact), std::fabs(value), std::chars_format::fixed, 60);
	std::string digits(exact, end);
	size_t point = digits.find('.');
	std::string integerPart = digits.substr(0, point);
	std::string fraction = digits.substr(point + 1, 2);
	std::string rest = digits.substr(point + 3);
	bool roundUp = false;
	if (!rest.empty() && rest[0] > '5') {
		roundUp = true;
	} else if (!rest.empty() && rest[0] == '5') {
		bool exactlyHalf = std::all_of(rest.begin() + 1, rest.end(), [](char c) { return c == '0'; });
		char lastDigit = fraction[1];
		roundUp = !exactlyHalf || ((lastDigit - '0') % 2 == 1);
	}
	std::string number = integerPart + fraction;
	if (roundUp) {
		size_t i = number.size();
		while (i > 0) {
			--i;
			if (number[i] == '9') {
				number[i] = '0';
			} else {
				++number[i];
				break;
			}
			if (i == 0)
				number.insert(number.begin(), '1');
		}
	}
	std::string integerDigits = number.substr(0, number.size() - 2);
	std::string fractionDigits = number.substr(number.size() - 2);
	while (!fractionDigits.empty() && fractionDigits.back() == '0')
		fractionDigits.pop_back();
	integerDigits.erase(0, std::min(integerDigits.find_first_not_of('0'), integerDigits.size()));
	std::string result = isNegative ? "-" : "";
	if (integerDigits.empty() && fractionDigits.empty())
		return result + "0";
	result += integerDigits;
	if (!fractionDigits.empty())
		result += "." + fractionDigits;
	return result;
}

std::string ChatUtil::color(std::string_view message, std::optional<JavaColor> colorValue) {
	JavaColor effective = colorValue.value_or(JavaColor::WHITE);
	return color(message, effective.getRed(), effective.getGreen(), effective.getBlue());
}

std::string ChatUtil::color(std::string_view message, int32_t rgb) {
	return color(message, (rgb & 0xFF0000) >> 16, (rgb & 0xFF00) >> 8, rgb & 0xFF);
}

std::string ChatUtil::color(std::string_view message, int32_t r, int32_t g, int32_t b) {
	return std::format("[color:{};{} {} {}]", message, formatDecimal(static_cast<float>(r) / 255.0f), formatDecimal(static_cast<float>(g) / 255.0f),
		formatDecimal(static_cast<float>(b) / 255.0f));
}

std::string ChatUtil::genderize(std::string_view wordForMales, std::string_view textForFemales) {
	return std::format("{}[f:\"{}\"]", wordForMales, textForFemales);
}

std::string ChatUtil::charName(model::gameobjects::player::Player& player) {
	return std::format("[charname:{};1 1 1]", player.getName(true)); // the 3 parameters are color values, but the client doesn't render them
}

std::string ChatUtil::l10n(int32_t l10nId) {
	if (l10nId == 0)
		return ""; // Java: null
	// client wants the rightmost bit = 1, followed by the id (effectively = l10nId * 2 + 1)
	uint32_t id = (static_cast<uint32_t>(l10nId) << 1) | 1u;
	std::u16string idAsFourBytesString{static_cast<char16_t>(id & 0xFFFF), static_cast<char16_t>((id >> 16) & 0xFFFF)};
	return "$" + StringUtils::toWtf8(idAsFourBytesString); // WTF-8: a lone surrogate unit reaches the wire unchanged (writeS, header request pre-4)
}

std::string ChatUtil::path(model::gameobjects::VisibleObject& object, bool withIdInName) {
	return path(object.getObjectTemplate(), withIdInName);
}

std::string ChatUtil::path(int32_t npcId, bool withIdInName) {
	const model::templates::VisibleObjectTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
	if (template_ != nullptr)
		return path(template_, withIdInName);
	else
		return path(withIdInName ? "Unknown ID " + std::to_string(npcId) : "Unknown ID", npcId);
}

std::string ChatUtil::path(const model::templates::VisibleObjectTemplate* template_, bool withIdInName) {
	std::string name = template_->getL10n();
	if (name.empty()) // Java: null
		name = template_->getName();
	if (withIdInName)
		name = name + " | " + std::to_string(template_->getTemplateId());
	return path(name, template_->getTemplateId());
}

std::string ChatUtil::path(std::string_view npcName) {
	return std::format("[where:{}]", npcName);
}

std::string ChatUtil::path(std::string_view linkName, int32_t npcId) {
	return std::format("[where:{};{}]", linkName, npcId);
}

std::string ChatUtil::position(std::string_view label, world::WorldPosition& pos) {
	return position(label, pos.getMapId(), pos.getX(), pos.getY(), pos.getZ());
}

std::string ChatUtil::position(std::string_view label, int64_t worldId, float x, float y, float z) {
	int8_t subId = 0;
	if (worldId == 400010000) { // abyss map
		if (z > 1800 && z < 2800 && x > 1600 && x < 2700 && y > 1400 && y < 2500)
			subId = 2; // core
		else if (z > 2250)
			subId = 3; // upper
		else
			subId = 1; // lower
	}
	return std::format("[pos:{};{} {} {} {} {}]", label, worldId, formatDecimal(x), formatDecimal(y), formatDecimal(z), subId);
}

runtime::Ref<world::WorldPosition> ChatUtil::getPosition(std::optional<std::string_view> posLink) {
	if (!posLink || !posLink->starts_with("[pos:"))
		return nullptr;
	std::u16string link = StringUtils::toUtf16(*posLink);
	size_t semicolon = link.find(u';');
	size_t bracket = link.find(u']');
	int32_t startIndex = semicolon == std::u16string::npos ? -1 : static_cast<int32_t>(semicolon);
	int32_t endIndex = bracket == std::u16string::npos ? -1 : static_cast<int32_t>(bracket);
	if (startIndex < 0 || startIndex >= endIndex)
		return nullptr;
	// java-bug kept: the substring starts at the ';', so posStr[0] is ";0" or ";400010000" and Integer.parseInt below always throws
	std::u16string_view trimmed = std::u16string_view(link).substr(static_cast<size_t>(startIndex), static_cast<size_t>(endIndex - startIndex));
	while (!trimmed.empty() && trimmed.front() <= u' ')
		trimmed.remove_prefix(1);
	while (!trimmed.empty() && trimmed.back() <= u' ')
		trimmed.remove_suffix(1);
	std::vector<std::u16string> posStr = splitHorizontalWhitespace(trimmed);
	if (posStr.empty())
		posStr.emplace_back(); // Java: "".split(...) yields [""]
	if (posStr[0] == u"0" || posStr[0] == u"1") // if present, strip ely/asmo language restriction flag (0 = ely only, 1 = asmo only)
		posStr.erase(posStr.begin());
	if (posStr.size() < 3)
		return nullptr;
	int32_t mapAndInstanceId = parseInt(posStr[0]);
	float x = parseFloat(posStr[1]);
	float y = parseFloat(posStr[2]);
	float z = posStr.size() > 3 ? parseFloat(posStr[3]) : 0; // client always creates position links with z = 0
	int32_t layer = posStr.size() > 4 ? parseInt(posStr[4]) : 0;
	std::optional<int32_t> zSearchOffset;
	if (layer > 0 && z == 0 && mapAndInstanceId == 400010000) { // abyss
		switch (layer) {
			case 1: // lower
				z = 1700.0f;
				break;
			case 2: // core
				z = 2350.0f;
				break;
			case 3: // upper
				z = 2950.0f;
				break;
		}
		zSearchOffset = -250;
	}
	return parsedCoordsToWorldPosition(mapAndInstanceId, x, y, z == 0 ? std::nullopt : std::optional<float>(z), zSearchOffset);
}

runtime::Ref<world::WorldPosition> ChatUtil::parsedCoordsToWorldPosition(int32_t mapAndInstanceId, float x, float y, std::optional<float> z,
	std::optional<int32_t> zSearchOffset) {
	int32_t instanceId = mapAndInstanceId % 10000;
	int32_t mapId = mapAndInstanceId;
	if (instanceId > 0)
		mapId -= instanceId;
	runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(mapId);
	if (!map)
		return nullptr;
	instanceId += 1; // client counts instanceIds starting at 0, but on server side it starts at 1 (TODO change server side)
	if (instanceId > 1) {
		std::vector<int32_t> availableInstanceIds = map->getAvailableInstanceIds();
		if (std::find(availableInstanceIds.begin(), availableInstanceIds.end(), instanceId) == availableInstanceIds.end())
			instanceId = 1;
	}
	float geoZ;
	if (!z)
		geoZ = world::geo::GeoService::getInstance().getZ(mapId, x, y, 4000, 0, instanceId);
	else if (!zSearchOffset)
		geoZ = world::geo::GeoService::getInstance().getZ(mapId, x, y, *z, instanceId); // search relative to input z (max diff = z ±2)
	else
		geoZ = world::geo::GeoService::getInstance().getZ(mapId, x, y, *z, *z + static_cast<float>(*zSearchOffset), instanceId);
	if (!geoEngine::math::JavaFloat::isNaN(geoZ))
		z = geoZ;
	return !z ? runtime::Ref<world::WorldPosition>() : world::World::getInstance().createPosition(mapId, x, y, *z, 0, instanceId);
}

std::string ChatUtil::item(int32_t itemId) {
	return std::format("[item:{}]", itemId);
}

std::string ChatUtil::itemName(int32_t itemId) {
	return std::format("[item_ex:{}]", itemId);
}

std::string ChatUtil::recipe(int32_t recipeId) {
	return std::format("[recipe:{}]", recipeId);
}

std::string ChatUtil::quest(int32_t questId) {
	return std::format("[quest:{}]", questId);
}

int32_t ChatUtil::getItemId(std::optional<std::string_view> itemStr) {
	return getIdFromString(itemStr, "item", "1[0-9]{8}");
}

int32_t ChatUtil::getQuestId(std::optional<std::string_view> questStr) {
	return getIdFromString(questStr, "quest", "[1-9][0-9]{3,4}");
}

int32_t ChatUtil::getIdFromString(std::optional<std::string_view> input, std::string_view linkAccessor, std::string_view validationPattern) {
	if (!input)
		return 0;
	std::string text(*input);
	std::string linkStart = "[" + std::string(linkAccessor) + ":";
	if (text.starts_with(linkStart))
		text = std::string(StringUtils::trim(std::string_view(text).substr(linkAccessor.size() + 2))); // ASCII prefix: UTF-16 index = byte index
	// Java: Pattern.compile("^(" + validationPattern + ")(?:[^\\d][^\\[]*\\]?$|$)").matcher(input).find(); the patterns match ASCII digits only,
	// and [^\\d][^\\[]* accept any other code units, so matching the UTF-8 bytes gives the same result
	std::regex pattern("^(" + std::string(validationPattern) + ")(?:[^\\d][^\\[]*\\]?$|$)");
	std::smatch m;
	if (std::regex_search(text, m, pattern))
		return commons::utils::parseInt(m[1].str());
	return 0;
}

std::string ChatUtil::getRealCharName(std::string_view name) {
	return getRealCharName(name, false);
}

std::string ChatUtil::getRealCharName(std::string_view nameValue, bool nameIsFromGMCommand) {
	// don't perform expensive checks if name is already qualified
	auto isAsciiLetter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
	if (!nameValue.empty() && std::all_of(nameValue.begin(), nameValue.end(), isAsciiLetter)) // Java: name.matches("^[A-Za-z]+$")
		return Util::convertName(nameValue);
	std::u16string name = StringUtils::toUtf16(nameValue);
	bool replaceUnsupportedChars = nameIsFromGMCommand && name.find(UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER) != std::u16string::npos;
	if (name.empty())
		throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0"); // Java: name.charAt(0)
	char16_t firstChar = name[0];
	if (firstChar == ASMO_NAME_PREFIX || firstChar == ELYOS_NAME_PREFIX)
		name = name.substr(1);
	const std::u16string nameFlag = u"%s";
	auto nameTags = configs::administration::AdminConfig::NAME_TAGS.get();
	for (const std::string& nameFormatValue : *nameTags) {
		std::u16string nameFormat = StringUtils::toUtf16(nameFormatValue);
		size_t flagIndex = nameFormat.find(nameFlag);
		if (flagIndex == std::u16string::npos)
			continue;
		std::u16string namePrefix = nameFormat.substr(0, flagIndex);
		std::u16string nameSuffix = nameFormat.substr(flagIndex + nameFlag.size());
		if (replaceUnsupportedChars) {
			namePrefix = replaceUnsupportedCommandChars(namePrefix);
			nameSuffix = replaceUnsupportedCommandChars(nameSuffix);
		}
		if ((namePrefix + nameSuffix).size() > 0 && name.starts_with(namePrefix) && name.ends_with(nameSuffix)) {
			size_t suffixIndex = name.find(nameSuffix);
			int32_t endIndex = static_cast<int32_t>(suffixIndex) - 1; // java-bug kept: cuts the character before the suffix as well
			size_t end = endIndex > 0 ? static_cast<size_t>(endIndex) : name.size();
			if (end < namePrefix.size()) {
				throw runtime::IndexOutOfBoundsException("begin " + std::to_string(namePrefix.size()) + ", end " + std::to_string(end) + ", length " +
					std::to_string(name.size()));
			}
			name = name.substr(namePrefix.size(), end - namePrefix.size());
			break;
		}
	}
	return Util::convertName(StringUtils::toUtf8(name));
}

std::string ChatUtil::toFactionPrefixedName(model::gameobjects::player::Player& reader, model::gameobjects::player::Player& player) {
	std::string name = player.getName(true);
	if (reader.isStaff())
		name = StringUtils::toUtf8(std::u16string(1, player.getRace() == model::Race::ELYOS ? ELYOS_NAME_PREFIX : ASMO_NAME_PREFIX)) + name;
	return name;
}

std::string ChatUtil::leftPad(int64_t number, int32_t width) {
	std::string num = std::to_string(number);
	if (static_cast<int32_t>(num.size()) >= width)
		return num;
	return std::string(static_cast<size_t>(width) - num.size(), '\t') + num;
}

std::vector<std::string> ChatUtil::split(std::string_view chatMessageValue) {
	// WTF-8 in and out, so the l10n ids of the message keep their code units (header request pre-4)
	std::u16string chatMessage = StringUtils::wtf8ToUtf16(chatMessageValue);
	if (static_cast<int32_t>(chatMessage.size()) <= network::aion::serverpackets::SM_MESSAGE::MESSAGE_SIZE_LIMIT / 2)
		return {std::string(chatMessageValue)};
	std::vector<std::string> parts;
	for (int32_t start = 0, length = static_cast<int32_t>(chatMessage.size()); start < length;) {
		int32_t splitIndex = findSplitIndex(chatMessage, start, length);
		std::u16string_view part = std::u16string_view(chatMessage).substr(static_cast<size_t>(start), static_cast<size_t>(splitIndex - start));
		parts.push_back(StringUtils::toWtf8(part));
		start = splitIndex;
		if (start < length) {
			char16_t splitChar = chatMessage[static_cast<size_t>(start)];
			if (splitChar == u' ' || splitChar == u'\n')
				start++;
		}
	}
	return parts;
}

int32_t ChatUtil::findSplitIndex(std::u16string_view chatMessage, int32_t startIndex, int32_t endIndex) {
	int32_t estimatedDisplayLength = 0;
	int32_t lastNewLineIndex = -1;
	int32_t lastSpaceIndex = -1;
	for (int32_t i = startIndex; i < endIndex; i++) {
		int32_t lengthToAdd = 1;
		switch (chatMessage[static_cast<size_t>(i)]) {
			case u'\n':
				lastNewLineIndex = i;
				break;
			case u' ':
				lastSpaceIndex = i;
				break;
			case u'$': // check for l10n ID
				if (i + 2 < endIndex && (chatMessage[static_cast<size_t>(i + 1)] & 1) == 1) {
					i += 2;
					lengthToAdd += 15; // conservative estimate for the character count of a resolved localized string on the client side
				}
				break;
			case u'[': // check for any link type, such as [quest:1006], [item:182400001], etc.
				if (i + 3 < endIndex && isLowerCase(chatMessage[static_cast<size_t>(i + 1)])) {
					int32_t linkEndIndex = indexOf(chatMessage, u']', i + 2, std::min(i + 40, endIndex));
					if (linkEndIndex == -1)
						break;
					int32_t colonIndex = indexOf(chatMessage, u':', i + 2, linkEndIndex);
					if (colonIndex == -1)
						break;
					int32_t linkLength = linkEndIndex - i;
					i += linkLength;
					lengthToAdd += 30; // conservative estimate for the character count of a rendered chat link on the client side
				}
				break;
			default:
				break;
		}
		estimatedDisplayLength += lengthToAdd;
		if (estimatedDisplayLength >= network::aion::serverpackets::SM_MESSAGE::MESSAGE_SIZE_LIMIT) {
			if (i == startIndex)
				break;
			if (lastNewLineIndex != -1)
				return lastNewLineIndex;
			if (lastSpaceIndex != -1)
				return lastSpaceIndex;
			return i;
		}
	}
	return endIndex;
}

} // namespace aion::gameserver::utils
