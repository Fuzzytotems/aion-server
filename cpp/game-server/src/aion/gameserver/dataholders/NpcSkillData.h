#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/NpcSkillData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.NpcSkillData.
 * <p>
 * C++: the @XmlTransient index points into the bound `npcSkills` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). setNpcSkillTemplates and getAllNpcSkillTemplates serve only the //reload command (reload is deferred, design D3).
 *
 * @author ATracer
 */
class NpcSkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcSkillData.xml.inc"
private:
	// fieldmap: K1 non-bound member (hand-written, const after load); Java HashMap<Integer, NpcSkillTemplates>
	std::unordered_map<int32_t, const model::templates::npcskill::NpcSkillTemplates*> npcSkillData;

public:
	int32_t size() const;

	/** @return the skill list of the npc, nullptr (Java null) if it has none */
	const model::templates::npcskill::NpcSkillTemplates* getNpcSkillList(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
