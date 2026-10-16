#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/NpcSkillData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.NpcSkillData.
 * <p>
 * C++: the @XmlTransient index points into the bound `npcSkills` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). setNpcSkillTemplates serves only the //reload command (reload is deferred, design D3). getAllNpcSkillTemplates returns the
 * lists in Java's HashMap<Integer, NpcSkillTemplates> iteration order.
 *
 * @author ATracer
 */
class NpcSkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcSkillData.xml.inc"
private:
	// K1 non-bound member (fieldmap.json: hand-written, const after load); Java HashMap<Integer, NpcSkillTemplates>
	std::unordered_map<int32_t, const model::templates::npcskill::NpcSkillTemplates*> npcSkillData;
	/** C++ only: npcSkillData.values() in Java's HashMap iteration order */
	std::vector<const model::templates::npcskill::NpcSkillTemplates*> skillListsInHashOrder;

public:
	int32_t size() const;

	/** @return the skill list of the npc, nullptr (Java null) if it has none */
	const model::templates::npcskill::NpcSkillTemplates* getNpcSkillList(int32_t id) const;

	/** Java: replaces the lists and runs afterUnmarshal again (//reload only; deferred, design D3) */
	void setNpcSkillTemplates(std::vector<model::templates::npcskill::NpcSkillTemplates> template_);

	const std::vector<const model::templates::npcskill::NpcSkillTemplates*>& getAllNpcSkillTemplates() const;
};

} // namespace aion::gameserver::dataholders
