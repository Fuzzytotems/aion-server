#include "aion/gameserver/services/NameRestrictionService.h"

#include <regex>
#include <string>
#include <vector>

#include "aion/commons/configuration/transformers/PatternTransformer.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"

namespace aion::gameserver::services {

namespace {

namespace StringUtils = commons::utils::StringUtils;

/** the UTF-16 text a Java Matcher sees, as the wide string the configured std::wregex patterns match (PatternTransformer.h) */
std::wstring toWide(std::string_view text) {
	return commons::configuration::transformers::PatternTransformer::toWide(text);
}

/** Java String.split(" ") for a single space: the pieces between spaces, trailing empty pieces removed */
std::vector<std::string_view> splitOnSpace(std::string_view text) {
	std::vector<std::string_view> pieces;
	size_t start = 0;
	for (size_t space = text.find(' '); space != std::string_view::npos; space = text.find(' ', start)) {
		pieces.push_back(text.substr(start, space - start));
		start = space + 1;
	}
	pieces.push_back(text.substr(start));
	while (!pieces.empty() && pieces.back().empty())
		pieces.pop_back();
	return pieces;
}

/** Java String.replace(CharSequence, CharSequence): every non-overlapping occurrence, left to right */
std::string replaceAll(std::string text, std::string_view from, std::string_view to) {
	if (from.empty())
		return text; // not reached: split never yields an empty word that is forbidden (equalsIgnoreCase of "" only matches "")
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}

} // namespace

bool NameRestrictionService::isValidName(std::string_view name) {
	auto pattern = configs::main::NameConfig::CHAR_NAME_PATTERN.get();
	return std::regex_match(toWide(name), *pattern);
}

bool NameRestrictionService::isValidPetName(std::string_view name) {
	auto pattern = configs::main::NameConfig::PET_NAME_PATTERN.get();
	return std::regex_match(toWide(name), *pattern);
}

bool NameRestrictionService::isValidLegionName(std::string_view name) {
	auto pattern = configs::main::LegionConfig::LEGION_NAME_PATTERN.get();
	return std::regex_match(toWide(name), *pattern);
}

bool NameRestrictionService::isForbidden(std::string_view name) {
	return containsForbiddenSequence(name) || isForbiddenWord(name);
}

bool NameRestrictionService::containsForbiddenSequence(std::string_view name) {
	auto pattern = configs::main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.get();
	if (!*pattern)
		return false;

	return std::regex_search(toWide(name), **pattern);
}

bool NameRestrictionService::isForbiddenWord(std::string_view string) {
	auto forbiddenWords = configs::main::NameConfig::FORBIDDEN_WORDS.get();
	for (const std::string& s : *forbiddenWords) {
		if (StringUtils::equalsIgnoreCase(string, s))
			return true;
	}
	return false;
}

std::string NameRestrictionService::filterMessage(std::string_view messageValue) {
	std::string message(messageValue);
	for (std::string_view word : splitOnSpace(messageValue)) {
		if (isForbiddenWord(word))
			message = replaceAll(std::move(message), word, std::string(static_cast<size_t>(StringUtils::utf16Length(word)), '*'));
	}
	return message;
}

} // namespace aion::gameserver::services
