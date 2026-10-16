#include "aion/gameserver/model/templates/siegelocation/AssaultData.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/siege/Assaulter.h"

namespace aion::gameserver::model::templates::siegelocation {

namespace {
/**
 * Java AssaulterType.getSpawnCosts() (the constructor argument `spawnCosts` in ordinal order). The AssaulterType companion belongs to the
 * model.siege chunk and does not exist yet, so the table stays local to this hook.
 */
std::vector<float> spawnCosts(siege::AssaulterType type) {
	switch (type) {
		case siege::AssaulterType::TELEPORT:
			return {};
		case siege::AssaulterType::COMMANDER:
			return {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
		case siege::AssaulterType::FIGHTER:
			return {0.2f, 0.4f, 0.8f, 1.0f};
		case siege::AssaulterType::ASSASSIN:
			return {0.2f, 0.4f, 0.8f, 1.0f};
		case siege::AssaulterType::RANGER:
			return {0.4f, 0.8f, 1.6f, 2.0f};
		case siege::AssaulterType::WITCH:
			return {0.5f, 1.0f, 2.0f, 2.5f};
		case siege::AssaulterType::PRIEST:
			return {0.6f, 1.2f, 2.4f, 3.0f};
		case siege::AssaulterType::GUNNER:
			return {0.4f, 0.8f, 1.6f, 2.0f};
	}
	return {};
}
} // namespace

void AssaultData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	for (const AssaulterTemplate& a : assaulterTemplates) {
		// Deviation: Java throws NullPointerException for an assaulter without type or npc_ids; LoadContext::fail reports it (P4-07b.md)
		if (!a.getAssaulterType() || !a.getNpcIds())
			ctx.fail("Assaulter without type or npc_ids in assault data of dredgion " + std::to_string(dredgionId));
		siege::AssaulterType type = *a.getAssaulterType();
		const std::vector<int32_t>& npcIds = *a.getNpcIds();
		std::vector<float> costs = spawnCosts(type);
		std::vector<runtime::Ref<siege::Assaulter>> processed;
		for (size_t i = 0; i < npcIds.size(); i++) {
			if (type == siege::AssaulterType::TELEPORT)
				processed.push_back(siege::Assaulter::create(npcIds[i], 0.0f, a.getHeadingOffset(), a.getDistanceOffset()));
			else if (i < costs.size())
				processed.push_back(siege::Assaulter::create(npcIds[i], costs[i], a.getHeadingOffset(), a.getDistanceOffset()));
		}
		processedAssaulters[static_cast<size_t>(type)] = std::move(processed);
	}
}

} // namespace aion::gameserver::model::templates::siegelocation
