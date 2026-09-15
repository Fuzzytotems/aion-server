#pragma once

#include <array>
#include <optional>
#include <vector>

#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/siege/AssaulterType.h"
#include "aion/gameserver/model/templates/siegelocation/AssaultData.xml.h"

#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::siegelocation {

/**
 * Java com.aionemu.gameserver.model.templates.siegelocation.AssaultData.
 * <p>
 * C++: the @XmlTransient `processedAssaulters` (an EnumMap) is a C++-only array indexed by the AssaulterType ordinal, nullopt for an absent key
 * (a std::map would lose the noexcept move bound templates need); the immutable RefCounted Assaulters (fieldmap K3) are retained by the immortal
 * template, so Assaulter.h is included for the Ref destructor.
 *
 * @author Estrayl
 */
class AssaultData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/AssaultData.xml.inc"
private:
	/** Java @XmlTransient EnumMap<AssaulterType, List<Assaulter>> processedAssaulters */
	std::array<std::optional<std::vector<runtime::Ref<siege::Assaulter>>>, 8> processedAssaulters;

public:
	/** Java EnumMap: the entry of a type is processedAssaulters[ordinal], nullopt if the type has none */
	const std::array<std::optional<std::vector<runtime::Ref<siege::Assaulter>>>, 8>& getProcessedAssaulters() const { return processedAssaulters; }
};

} // namespace aion::gameserver::model::templates::siegelocation
