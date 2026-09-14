#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/model/templates/npc/NpcTemplate.xml.h"

namespace aion::gameserver::model::templates::npc {

/** Java com.aionemu.gameserver.model.templates.npc.NpcTemplate. @author Luno */
class NpcTemplate : public ::aion::gameserver::model::gameobjects::CreatureTemplate {
#include "aion/gameserver/model/templates/npc/NpcTemplate.xml.inc"
public:
	int32_t getTemplateId() const override { return npcId; }

	int32_t getL10nId() const override { return nameId; }

	std::string getName() const override { return name; }

	/** @return the `ai` attribute, nullopt (Java null) when absent */
	std::optional<std::string> getAiName() const override { return ai.empty() ? std::nullopt : std::optional<std::string>(ai); }

	/** @return the bound radius of the xml, VisibleObjectTemplate::getBoundRadius() (BoundRadius::DEFAULT) if there is none */
	const BoundRadius* getBoundRadius() const override;

private:
	/** Java `private int npcId`: not bound itself, set by the annotated setter setXmlUid (npc_id, the XmlID) */
	int32_t npcId = 0;
};

} // namespace aion::gameserver::model::templates::npc
