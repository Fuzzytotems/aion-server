#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/dataholders/MaterialData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.MaterialData.
 * <p>
 * C++: the @XmlTransient index points into the bound `materialTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6; Java
 * sets the list to null).
 *
 * @author Rolandas
 */
class MaterialData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MaterialData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::materials::MaterialTemplate*> materialsById;
	std::unordered_set<int32_t> skillIds;

public:
	/** @return the material template, nullptr (Java null) if there is none */
	const model::templates::materials::MaterialTemplate* getTemplate(int32_t materialId) const;

	bool isMaterialSkill(int32_t skillId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
