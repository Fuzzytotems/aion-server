#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/walker/RouteStep.xml.h"

namespace aion::gameserver::model::templates::walker {

/**
 * Java com.aionemu.gameserver.model.templates.walker.RouteStep.
 * <p>
 * C++: the @XmlTransient `stepIndex` and `isLastStep` are C++-only members, set by WalkerTemplate's hook (and by FixPath on the new template it
 * builds) before the template is used; `z` is the runtime-mutable Field of the member block.
 *
 * @author KKnD, Rolandas
 */
class RouteStep : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/RouteStep.xml.inc"
private:
	/** Java @XmlTransient int stepIndex */
	int32_t stepIndex = 0;
	/** Java @XmlTransient boolean isLastStep */
	bool isLastStep_ = false;

protected:
	RouteStep() = default; // Java: protected RouteStep()

public:
	RouteStep(float x, float y, float z, int32_t restTime);

	int32_t getStepIndex() const { return stepIndex; }

	void setStepIndex(int32_t value) { this->stepIndex = value; }

	bool isLastStep() const { return isLastStep_; }

	void setIsLastStep(bool value) { this->isLastStep_ = value; }
};

} // namespace aion::gameserver::model::templates::walker
