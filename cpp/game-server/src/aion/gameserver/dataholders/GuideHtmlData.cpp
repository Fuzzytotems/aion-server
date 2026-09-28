#include "aion/gameserver/dataholders/GuideHtmlData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::dataholders {

using model::templates::Guides::GuideTemplate;

void GuideHtmlData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const std::unique_ptr<GuideTemplate>& template_ : guideTemplates)
		addTemplate(*template_);
	detail::JavaHashMapOrder<int32_t, bool> order;
	for (const std::unique_ptr<GuideTemplate>& template_ : guideTemplates) {
		model::Race race = template_->getRace().value_or(model::Race::PC_ALL);
		int32_t classId = template_->getPlayerClass() ? xml::enumOrdinal(*template_->getPlayerClass()) : CLASS_ALL;
		int32_t hash = makeHash(classId, xml::enumOrdinal(race), template_->getLevel());
		order.put(hash, true, detail::javaHashCode(hash));
	}
	keysInHashOrder = order.keys();
	// Java: guideTemplates = null (the C++ lists point into the storage, which stays)
}

void GuideHtmlData::addTemplate(const GuideTemplate& template_) {
	model::Race race = template_.getRace().value_or(model::Race::PC_ALL); // Java: if (race == null) race = Race.PC_ALL
	int32_t classId = !template_.getPlayerClass() ? CLASS_ALL : xml::enumOrdinal(*template_.getPlayerClass());
	int32_t hash = makeHash(classId, xml::enumOrdinal(race), template_.getLevel());
	templates[hash].push_back(&template_);
}

int32_t GuideHtmlData::size() const {
	return static_cast<int32_t>(templates.size());
}

const std::unordered_map<int32_t, std::vector<const GuideTemplate*>>& GuideHtmlData::getTemplates() const {
	return templates;
}

const GuideTemplate* GuideHtmlData::getTemplateByTitle(std::string_view title) const {
	for (int32_t key : keysInHashOrder) {
		for (const GuideTemplate* t : templates.at(key)) {
			if (t->getTitle() == title)
				return t;
		}
	}
	return nullptr;
}

std::vector<const GuideTemplate*> GuideHtmlData::getTemplatesFor(model::PlayerClass playerClass, model::Race race, int32_t level) const {
	std::vector<const GuideTemplate*> guideTemplate;
	auto addAll = [&](int32_t hash) {
		auto it = templates.find(hash);
		if (it != templates.end())
			guideTemplate.insert(guideTemplate.end(), it->second.begin(), it->second.end());
	};
	addAll(makeHash(xml::enumOrdinal(playerClass), xml::enumOrdinal(race), level));                // classRaceSpecificTemplates
	addAll(makeHash(xml::enumOrdinal(playerClass), xml::enumOrdinal(model::Race::PC_ALL), level)); // classSpecificTemplates
	addAll(makeHash(CLASS_ALL, xml::enumOrdinal(race), level));                                    // raceSpecificTemplates
	addAll(makeHash(CLASS_ALL, xml::enumOrdinal(model::Race::PC_ALL), level));                     // generalTemplates
	return guideTemplate;
}

int32_t GuideHtmlData::makeHash(int32_t classType, int32_t race, int32_t level) {
	uint32_t result = static_cast<uint32_t>(classType) << 8;
	result = (result | static_cast<uint32_t>(race)) << 8;
	return static_cast<int32_t>(result | static_cast<uint32_t>(level));
}

} // namespace aion::gameserver::dataholders
