#pragma once

#include <memory>
#include <vector>

#include "aion/gameserver/model/templates/cp/CPRank.xml.h"

namespace aion::gameserver::model::templates::cp {

/** Java com.aionemu.gameserver.model.templates.cp.CPRank. @author Neon */
class CPRank : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/cp/CPRank.xml.inc"
public:
	/** @return the stat functions of the modifiers element, empty (Java Collections.emptyList()) without one */
	const std::vector<std::unique_ptr<::aion::gameserver::model::stats::calc::functions::StatFunction>>& getStatModifiers() const;
};

} // namespace aion::gameserver::model::templates::cp
