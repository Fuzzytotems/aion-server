#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold model.templates.walker.RouteStep`. It
// stands in for the hand-written class of the static data port (P4-09).

#include <cstdint>

#include "aion/gameserver/model/templates/walker/RouteStep.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.RouteStep (test shell). @author KKnD, Rolandas */
class RouteStep : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/RouteStep.xml.inc"
public:
	int32_t getStepIndex() const { return stepIndex; }
	void setStepIndex(int32_t index) { stepIndex = index; }
	bool isLastStep() const { return lastStep; }
	void setIsLastStep(bool last) { lastStep = last; }

protected:
	RouteStep() = default; // Java: protected RouteStep() (the binder creates it through XmlBinding<RouteStep>::create)

private:
	int32_t stepIndex = 0;
	bool lastStep = false;
};

} // namespace aion::gameserver::model::templates::walker
