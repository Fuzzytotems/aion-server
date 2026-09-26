#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/craft/ExpertQuestsList.h"
#include "aion/gameserver/model/craft/MasterQuestsList.h"

namespace aion::gameserver::model::craft {

/**
 * Companions of the generated enums MasterQuestsList and ExpertQuestsList (docs/design/static-data.md §2.5): Java's constructor data and the static
 * getQuestIds lookups as free functions. The quest id arrays are immutable (Java never writes them).
 */

/**
 * Java: MasterQuestsList.getQuestIds(craftSkillId, race)
 *
 * @throws IllegalArgumentException
 *           "Invalid craftSkillId: <id> or race: <race>" if no constant matches
 */
std::span<const int32_t> getMasterQuestIds(int32_t craftSkillId, Race race);

/**
 * Java: ExpertQuestsList.getQuestIds(craftSkillId, race)
 *
 * @throws IllegalArgumentException
 *           "Invalid craftSkillId: <id> or race: <race>" if no constant matches
 */
std::span<const int32_t> getExpertQuestIds(int32_t craftSkillId, Race race);

} // namespace aion::gameserver::model::craft
