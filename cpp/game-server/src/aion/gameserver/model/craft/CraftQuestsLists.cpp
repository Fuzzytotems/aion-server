#include "aion/gameserver/model/craft/CraftQuestsLists.h"

#include <array>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::craft {

namespace {

struct QuestsListEntry {
	std::array<int32_t, 6> questIds;
	size_t questIdCount;
	Race race;
	int32_t craftSkillId;
};

// Java MasterQuestsList constants in ordinal order
constexpr std::array<QuestsListEntry, 14> MASTER_QUESTS{{
	{{19039, 19038}, 2, Race::ELYOS, 40001},     // COOKING_ELYOS
	{{29039, 29038}, 2, Race::ASMODIANS, 40001}, // COOKING_ASMODIANS
	{{19009, 19008}, 2, Race::ELYOS, 40002},     // WEAPONSMITHING_ELYOS
	{{29009, 29008}, 2, Race::ASMODIANS, 40002}, // WEAPONSMITHING_ASMODIANS
	{{19015, 19014}, 2, Race::ELYOS, 40003},     // ARMORSMITHING_ELYOS
	{{29015, 29014}, 2, Race::ASMODIANS, 40003}, // ARMORSMITHING_ASMODIANS
	{{19021, 19020}, 2, Race::ELYOS, 40004},     // TAILORING_ELYOS
	{{29021, 29020}, 2, Race::ASMODIANS, 40004}, // TAILORING_ASMODIANS
	{{19033, 19032}, 2, Race::ELYOS, 40007},     // ALCHEMY_ELYOS
	{{29033, 29032}, 2, Race::ASMODIANS, 40007}, // ALCHEMY_ASMODIANS
	{{19027, 19026}, 2, Race::ELYOS, 40008},     // HANDICRAFTING_ELYOS
	{{29027, 29026}, 2, Race::ASMODIANS, 40008}, // HANDICRAFTING_ASMODIANS
	{{19058, 19057}, 2, Race::ELYOS, 40010},     // MENUSIER_ELYOS
	{{29058, 29057}, 2, Race::ASMODIANS, 40010}, // MENUSIER_ASMODIANS
}};

// Java ExpertQuestsList constants in ordinal order
constexpr std::array<QuestsListEntry, 14> EXPERT_QUESTS{{
	{{1944, 1979, 1978, 3952, 3951, 3950}, 6, Race::ELYOS, 40001},       // COOKING_ELYOS
	{{2934, 2979, 2978, 4956, 4955, 4954}, 6, Race::ASMODIANS, 40001},   // COOKING_ASMODIANS
	{{1941, 1973, 1972, 3943, 3942, 3941}, 6, Race::ELYOS, 40002},       // WEAPONSMITHING_ELYOS
	{{2931, 2973, 2972, 4947, 4946, 4945}, 6, Race::ASMODIANS, 40002},   // WEAPONSMITHING_ASMODIANS
	{{1942, 1975, 1974, 3946, 3945, 3944}, 6, Race::ELYOS, 40003},       // ARMORSMITHING_ELYOS
	{{2912, 2975, 2974, 4950, 4949, 4948}, 6, Race::ASMODIANS, 40003},   // ARMORSMITHING_ASMODIANS
	{{1946, 1983, 1982, 3958, 3957, 3956}, 6, Race::ELYOS, 40004},       // TAILORING_ELYOS
	{{2936, 2983, 2982, 4962, 4961, 4960}, 6, Race::ASMODIANS, 40004},   // TAILORING_ASMODIANS
	{{1945, 1981, 1980, 3955, 3954, 3953}, 6, Race::ELYOS, 40007},       // ALCHEMY_ELYOS
	{{2935, 2981, 2980, 4959, 4958, 4957}, 6, Race::ASMODIANS, 40007},   // ALCHEMY_ASMODIANS
	{{1943, 1977, 1976, 3949, 3948, 3947}, 6, Race::ELYOS, 40008},       // HANDICRAFTING_ELYOS
	{{2933, 2977, 2976, 4953, 4952, 4951}, 6, Race::ASMODIANS, 40008},   // HANDICRAFTING_ASMODIANS
	{{19050, 19053, 19052, 19056, 19055, 19054}, 6, Race::ELYOS, 40010}, // MENUSIER_ELYOS
	{{29050, 29053, 29052, 29056, 29055, 29054}, 6, Race::ASMODIANS, 40010}, // MENUSIER_ASMODIANS
}};

std::span<const int32_t> findQuestIds(const std::array<QuestsListEntry, 14>& entries, int32_t craftSkillId, Race race) {
	for (const QuestsListEntry& entry : entries) {
		if (race == entry.race && craftSkillId == entry.craftSkillId)
			return std::span<const int32_t>(entry.questIds.data(), entry.questIdCount);
	}
	throw runtime::IllegalArgumentException("Invalid craftSkillId: " + std::to_string(craftSkillId) + " or race: " + std::string(xml::enumName(race)));
}

} // namespace

std::span<const int32_t> getMasterQuestIds(int32_t craftSkillId, Race race) {
	return findQuestIds(MASTER_QUESTS, craftSkillId, race);
}

std::span<const int32_t> getExpertQuestIds(int32_t craftSkillId, Race race) {
	return findQuestIds(EXPERT_QUESTS, craftSkillId, race);
}

} // namespace aion::gameserver::model::craft
