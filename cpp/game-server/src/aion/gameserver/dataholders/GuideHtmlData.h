#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/GuideHtmlData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.GuideHtmlData.
 * <p>
 * C++: the lists point into the bound `guideTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6). getTemplateByTitle
 * searches the lists in Java's HashMap<Integer, List> iteration order, computed by afterUnmarshal.
 *
 * @author xTz
 */
class GuideHtmlData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GuideHtmlData.xml.inc"
private:
	std::unordered_map<int32_t, std::vector<const model::templates::Guides::GuideTemplate*>> templates;
	/** C++ only: the keys of templates in Java's HashMap iteration order */
	std::vector<int32_t> keysInHashOrder;
	static constexpr int32_t CLASS_ALL = 255;

	void addTemplate(const model::templates::Guides::GuideTemplate& template_);

public:
	int32_t size() const;

	/** unordered (no Java caller iterates the map; getTemplateByTitle searches in Java's order) */
	const std::unordered_map<int32_t, std::vector<const model::templates::Guides::GuideTemplate*>>& getTemplates() const;

	/** @return the first template with the title, nullptr (Java null) if there is none */
	const model::templates::Guides::GuideTemplate* getTemplateByTitle(std::string_view title) const;

	std::vector<const model::templates::Guides::GuideTemplate*> getTemplatesFor(model::PlayerClass playerClass, model::Race race, int32_t level) const;

private:
	static int32_t makeHash(int32_t classType, int32_t race, int32_t level);
};

} // namespace aion::gameserver::dataholders
