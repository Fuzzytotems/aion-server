#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/TitleTemplate.xml.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.TitleTemplate. @author xavier */
class TitleTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::stats::calc::StatOwner,
	public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/TitleTemplate.xml.inc"
public:
	/** C++ only: templates are immortal, Ref<StatOwner> does not count them (StatOwner.h) */
	void retain() const noexcept override {}
	void release() const noexcept override {}

	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates
