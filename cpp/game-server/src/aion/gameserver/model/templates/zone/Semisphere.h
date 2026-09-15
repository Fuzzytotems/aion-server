#pragma once

#include "aion/gameserver/model/templates/zone/Semisphere.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Semisphere. @author Rolandas */
class Semisphere : public ::aion::gameserver::model::templates::zone::Sphere {
#include "aion/gameserver/model/templates/zone/Semisphere.xml.inc"
public:
	Semisphere() = default;

	Semisphere(float xValue, float yValue, float zValue, float radius) : Sphere(xValue, yValue, zValue, radius) {}
};

} // namespace aion::gameserver::model::templates::zone
