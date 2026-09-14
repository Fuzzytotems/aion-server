#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold model.templates.walker.WalkerTemplate`.
// It stands in for the hand-written class of the static data port (P4-09); the hook ports only the step indexes.

#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.WalkerTemplate (test shell). @author KKnD */
class WalkerTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.inc"
public:
	const std::string& getRowValues() const { return rowValues; }
};

/** Java: step indexes and the last-step flag (the WALK_BACK and formation parts are not part of the slice) */
inline void WalkerTemplate::afterUnmarshal(xml::LoadContext&, const xml::XmlParent&) {
	for (size_t i = 0; i < routeStepList.size(); ++i) {
		routeStepList[i]->setStepIndex(static_cast<int32_t>(i));
		routeStepList[i]->setIsLastStep(i + 1 == routeStepList.size());
	}
}

} // namespace aion::gameserver::model::templates::walker
