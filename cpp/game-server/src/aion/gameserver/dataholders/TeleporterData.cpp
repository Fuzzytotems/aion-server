#include "aion/gameserver/dataholders/TeleporterData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

using model::templates::teleport::TeleporterTemplate;

void TeleporterData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const TeleporterTemplate*> order;
	for (const TeleporterTemplate& template_ : templates) {
		teleporterTemplates.insert_or_assign(template_.getTeleportId(), &template_);
		order.put(template_.getTeleportId(), &template_, detail::javaHashCode(template_.getTeleportId()));
	}
	templatesInHashOrder = order.values();
	// Java: templates = null (the C++ index points into the storage, which stays)
}

int32_t TeleporterData::size() const {
	return static_cast<int32_t>(teleporterTemplates.size());
}

const TeleporterTemplate* TeleporterData::getTeleporterTemplateByNpcId(int32_t npcId) const {
	for (const TeleporterTemplate* template_ : templatesInHashOrder) {
		if (template_->containNpc(npcId))
			return template_;
	}
	return nullptr;
}

const TeleporterTemplate* TeleporterData::getTeleporterTemplateByTeleportId(int32_t teleportId) const {
	auto it = teleporterTemplates.find(teleportId);
	return it != teleporterTemplates.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
