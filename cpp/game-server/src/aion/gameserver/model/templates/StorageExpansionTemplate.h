#pragma once

#include "aion/gameserver/model/templates/StorageExpansionTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.StorageExpansionTemplate. @author Simple */
class StorageExpansionTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/StorageExpansionTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates
