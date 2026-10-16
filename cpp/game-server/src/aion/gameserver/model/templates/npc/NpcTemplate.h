#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

	std::string toString() const;

	/** @return the aggro range, at least 10 */
	int32_t getMinimumShoutRange() const { return aggrorange < 10 ? 10 : aggrorange; }

	/** @return the bound radius of the xml, VisibleObjectTemplate::getBoundRadius() (BoundRadius::DEFAULT) if there is none */
	const BoundRadius* getBoundRadius() const override;

	/** @return the type, NpcTemplateType::NONE if absent */
	NpcTemplateType getNpcTemplateType() const { return npcTemplateType.value_or(NpcTemplateType::NONE); }

	/** @return the abyss type, AbyssNpcType::NONE if absent */
	AbyssNpcType getAbyssNpcType() const { return abyssNpcType.value_or(AbyssNpcType::NONE); }

	int32_t getTalkDistance() const { return talkInfo == nullptr ? 2 : talkInfo->getDistance(); }

	int32_t getTalkDelay() const { return talkInfo == nullptr ? 0 : talkInfo->getDelay(); }

	/** @return the func dialog ids, nullptr (Java null) without talk info or func_dialogs */
	const std::vector<int32_t>* getFuncDialogIds() const;

	/** @return True if the npc supports this function/action. */
	bool supportsAction(int32_t dialogActionId) const;

	/** @throws NullPointerException without a massive_loot element (Java) */
	int32_t getMassiveLootCount() const;

	/** @throws NullPointerException without a massive_loot element (Java) */
	int32_t getMassiveLootItem() const;

	/** @throws NullPointerException without a massive_loot element (Java) */
	int32_t getMassiveLootMinLevel() const;

	/** @throws NullPointerException without a massive_loot element (Java) */
	int32_t getMassiveLootMaxLevel() const;

	/** @return if no data is present for the talk */
	bool canInteract() const { return talkInfo != nullptr; }

	/** @return the hasDialog */
	bool isDialogNpc() const { return talkInfo != nullptr && talkInfo->isDialogNpc(); }

private:
	/** Java `private int npcId`: not bound itself, set by the annotated setter setXmlUid (npc_id, the XmlID) */
	int32_t npcId = 0;

	/** the massive loot element; @throws NullPointerException if there is none */
	const MassiveLoot& requireMassiveLoot() const;
};

} // namespace aion::gameserver::model::templates::npc
