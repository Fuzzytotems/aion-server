#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/ShieldData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ShieldData.
 * <p>
 * C++: an absent list is the empty bound vector (Java creates an empty list lazily). addAll has no caller in Java and would append to the
 * immutable published holder, so it is not declared.
 *
 * @author Wakizashi
 */
class ShieldData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ShieldData.xml.inc"
public:
	int32_t size() const;

	const std::vector<model::templates::shield::ShieldTemplate>& getShieldTemplates() const;
};

} // namespace aion::gameserver::dataholders
