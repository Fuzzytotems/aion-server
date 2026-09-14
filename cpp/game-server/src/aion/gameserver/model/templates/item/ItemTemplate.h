#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/item/ItemTemplate.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ItemTemplate. @author Luno, ATracer */
class ItemTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/item/ItemTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return description; }

	/** @return the name, empty (Java: "" for null) if absent */
	std::string getName() const override { return name; }

	int32_t getTemplateId() const override { return itemId; }

private:
	/** Java `private int itemId`: not bound itself, set by the annotated setter setXmlUid (id, the XmlID) */
	int32_t itemId = 0;
};

} // namespace aion::gameserver::model::templates::item
