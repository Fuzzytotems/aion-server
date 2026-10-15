#include "aion/gameserver/dataholders/WalkerData.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/spawnengine/WalkerGroupType.h"

namespace aion::gameserver::dataholders {

using model::templates::walker::RouteStep;
using model::templates::walker::WalkerTemplate;

void WalkerData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.WalkerData");
	for (const WalkerTemplate& route : walkerlist) {
		if (walkerlistData.putIfAbsent(route.getRouteId(), &route))
			log.warn("Duplicate route ID: " + route.getRouteId());
	}
	// Java: walkerlist.clear(); walkerlist = null (the C++ index points into the storage, which stays)
}

int32_t WalkerData::size() const {
	return static_cast<int32_t>(walkerlistData.size());
}

const WalkerTemplate* WalkerData::getWalkerTemplate(std::string_view routeId) const {
	const auto* entry = walkerlistData.get(std::string(routeId));
	return entry != nullptr ? *entry : nullptr;
}

// Java WalkerData.java:52-56: FixPath's list of new templates (a WalkerData of its own, never the published one)
void WalkerData::addTemplate(std::unique_ptr<WalkerTemplate> newTemplate) {
	walkerlist.push_back(std::move(*newTemplate)); // Java: if (walkerlist == null) walkerlist = new ArrayList<>(); walkerlist.add(newTemplate)
}

// Java WalkerData.java:58-75 (m5j CP5, D14 = (a)): the JAXB marshaller with JAXB_FORMATTED_OUTPUT writes the list as <npc_walker> into
// ./data/static_data/npc_walker/generated_npc_walker_<routeId>.xml (relative to the working directory); the list is cleared afterwards
void WalkerData::saveData(std::string_view routeId) {
	using geoEngine::math::JavaFloat;
	const std::filesystem::path xml("./data/static_data/npc_walker/generated_npc_walker_" + std::string(routeId) + ".xml");
	std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<npc_walker>\n";
	auto attribute = [&out](const char* name, std::string_view value) {
		out += std::string(" ") + name + "=\"";
		for (char c : value)
			out += c == '&' ? std::string("&amp;") : c == '<' ? std::string("&lt;") : c == '>' ? std::string("&gt;") : c == '"' ? std::string("&quot;") : std::string(1, c);
		out += '"';
	};
	for (const WalkerTemplate& route : walkerlist) {
		out += "    <walker_template";
		attribute("route_id", route.getRouteId());
		attribute("pool", std::to_string(route.getPool()));
		attribute("formation", xml::enumName(route.getType()));
		// Java's String rowValues is written when bound; saveData writes only FixPath's new templates, whose rowValues is null (setRows sets the
		// transient int[] rows), so the attribute never appears
		attribute("loop_type", xml::enumName(route.getLoopType()));
		if (route.getRouteSteps().empty()) {
			out += "/>\n";
			continue;
		}
		out += ">\n";
		for (const std::unique_ptr<RouteStep>& step : route.getRouteSteps()) {
			out += "        <routestep";
			attribute("x", JavaFloat::toString(step->getX()));
			attribute("y", JavaFloat::toString(step->getY()));
			attribute("z", JavaFloat::toString(step->getZ()));
			attribute("rest_time", std::to_string(step->getRestTime()));
			out += "/>\n";
		}
		out += "    </walker_template>\n";
	}
	out = walkerlist.empty() ? out.substr(0, out.size() - 2) + "/>\n" : out + "</npc_walker>\n"; // JAXB: an element without content is self-closed
	std::ofstream file(xml, std::ios::binary | std::ios::trunc);
	file << out;
	if (!file) {
		// Java: log.error("Error while saving data: " + e.getMessage(), e.getCause()) of the marshaller's JAXBException
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.WalkerData")
			.error("Error while saving data: " + xml.string() + " (cannot be written)");
	}
	walkerlist.clear(); // Java: finally { walkerlist.clear(); walkerlist = null; }
}

std::vector<const WalkerTemplate*> WalkerData::getTemplates() const {
	std::vector<const WalkerTemplate*> templates;
	templates.reserve(walkerlistData.size());
	for (const auto& [routeId, route] : walkerlistData)
		templates.push_back(route);
	return templates;
}

} // namespace aion::gameserver::dataholders
