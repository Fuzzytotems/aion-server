#include "SpawnIds.h"

namespace aion::gameserver::tools::regscan {

namespace {

bool isDigit(char c) noexcept {
	return c >= '0' && c <= '9';
}

bool isWordChar(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) || c == '_';
}

/** @return true if text[at..at+6) are six ASCII digits */
bool sixDigits(std::string_view text, size_t at) noexcept {
	if (at + 6 > text.size())
		return false;
	for (size_t k = at; k < at + 6; k++) {
		if (!isDigit(text[k]))
			return false;
	}
	return true;
}

int32_t parseSix(std::string_view text, size_t at) noexcept {
	int32_t value = 0;
	for (size_t k = at; k < at + 6; k++)
		value = value * 10 + (text[k] - '0');
	return value;
}

} // namespace

void findSpawnNpcIds(std::string_view text, std::set<int32_t>& npcIds) {
	size_t pos = 0;
	while (true) {
		size_t i = text.find("sp", pos);
		if (i == std::string_view::npos)
			return;
		pos = i + 1; // the next attempt if this position does not match
		if (i > 0 && isWordChar(text[i - 1])) // \b before the word character 's'
			continue;
		size_t j = i + 2;
		if (text.substr(j, 4) == "awn(")
			j += 4;
		else if (j < text.size() && text[j] == '(')
			j += 1;
		else
			continue;
		// [^,\d]* is greedy and only gives back non-digits, so (\d{6}) can only start where the run of non-comma non-digits ends
		size_t k = j;
		while (k < text.size() && text[k] != ',' && !isDigit(text[k]))
			k++;
		if (!sixDigits(text, k))
			continue;
		npcIds.insert(parseSix(text, k));
		size_t end = k + 6;
		if (text.substr(end, 3) == " : " && sixDigits(text, end + 3)) {
			npcIds.insert(parseSix(text, end + 3));
			end += 9;
		}
		pos = end; // Matcher.find resumes after the match
	}
}

} // namespace aion::gameserver::tools::regscan
