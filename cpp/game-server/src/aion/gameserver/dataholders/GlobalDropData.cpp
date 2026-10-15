#include "aion/gameserver/dataholders/GlobalDropData.h"

#include <memory>
#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

namespace aion::gameserver::dataholders {

using model::templates::globaldrops::GlobalDropNpc;
using model::templates::globaldrops::GlobalDropNpcName;
using model::templates::globaldrops::GlobalDropNpcNames;
using model::templates::globaldrops::GlobalDropNpcs;
using model::templates::globaldrops::GlobalRule;
using model::templates::globaldrops::StringFunction;
using model::templates::npc::NpcTemplate;

void GlobalDropData::processRules(const std::vector<const NpcTemplate*>& npcs) {
	// Java: List<NpcTemplate> npcList = new ArrayList<>(npcs);
	for (GlobalRule& gr : globalDropRules) {
		if (gr.getGlobalRuleNpcNames() != nullptr) {
			std::vector<GlobalDropNpc> allowedNpcs = getAllowedNpcs(gr, npcs);
			if (!allowedNpcs.empty()) {
				gr.setNpcs(std::make_unique<GlobalDropNpcs>());
				// Java: gr.getGlobalRuleNpcs().addNpcs(allowedNpcs)
				const_cast<GlobalDropNpcs*>(gr.getGlobalRuleNpcs())->addNpcs(std::move(allowedNpcs));
				// Java: gr.getGlobalRuleNpcNames().getGlobalDropNpcNames().clear() (the names object belongs to the unpublished rule)
				const_cast<GlobalDropNpcNames*>(gr.getGlobalRuleNpcNames())->getGlobalDropNpcNames().clear();
			}
		}
	}
}

std::vector<GlobalDropNpc> GlobalDropData::getAllowedNpcs(const GlobalRule& rule, const std::vector<const NpcTemplate*>& npcs) {
	std::vector<GlobalDropNpc> allowedNpcs;
	// Java: allowedNpcs = rule.getGlobalRuleNpcs().getGlobalDropNpcs() appends to the rule's own list, which processRules replaces by a new
	// GlobalDropNpcs holding that same list; the C++ copy gives the same final list
	if (rule.getGlobalRuleNpcs() != nullptr)
		allowedNpcs = rule.getGlobalRuleNpcs()->getGlobalDropNpcs();
	if (rule.getGlobalRuleNpcNames() != nullptr) {
		for (const GlobalDropNpcName& gdNpcName : rule.getGlobalRuleNpcNames()->getGlobalDropNpcNames()) {
			const std::string value = gdNpcName.getValue();
			const std::string lowerValue = commons::utils::StringUtils::toLowerCase(value);
			for (const NpcTemplate* npc : npcs) {
				const std::string name = npc->getName();
				bool matches = false;
				switch (gdNpcName.getFunction()) {
					case StringFunction::CONTAINS:
						matches = name.find(lowerValue) != std::string::npos;
						break;
					case StringFunction::END_WITH:
						matches = name.ends_with(lowerValue);
						break;
					case StringFunction::START_WITH:
						matches = name.starts_with(lowerValue);
						break;
					case StringFunction::EQUALS:
						matches = commons::utils::StringUtils::equalsIgnoreCase(name, value);
						break;
					default:
						break;
				}
				if (!matches)
					continue;
				GlobalDropNpc gdNpc;
				gdNpc.setNpcId(npc->getTemplateId());
				// Java: if (!allowedNpcs.contains(gdNpc)) - GlobalDropNpc has no equals, so the new object is never contained
				allowedNpcs.push_back(gdNpc);
			}
		}
	}
	return allowedNpcs;
}

int32_t GlobalDropData::size() const {
	return static_cast<int32_t>(globalDropRules.size());
}

} // namespace aion::gameserver::dataholders
