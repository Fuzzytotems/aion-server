#pragma once

#include <cstdint>
#include <set>
#include <string_view>

namespace aion::gameserver::tools::regscan {

/**
 * Java: QuestSpawnAnalyzer.parseSpawnNpcIds - adds the npc ids matched by the pattern <tt>\bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))?</tt> in the
 * raw text (comments and strings included, matches may span lines), with Java's find() semantics: the search resumes after each match.
 * \b and \d are ASCII (java.util.regex without UNICODE_CHARACTER_CLASS, JDK 19+).
 */
void findSpawnNpcIds(std::string_view text, std::set<int32_t>& npcIds);

} // namespace aion::gameserver::tools::regscan
