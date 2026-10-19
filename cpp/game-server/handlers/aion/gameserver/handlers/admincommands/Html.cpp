#include "aion/gameserver/handlers/admincommands/Html.h"

#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/services/HTMLService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Html);

// Java Html.java:14-16
Html::Html() : AdminCommand("html") {
}

// Java Html.java:18-33
void Html::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params == null || params.length < 1) {
		PacketSendUtility::sendMessage(admin, "Usage: //html <reload|show>");
		return;
	}

	if (params[0] == "reload") { // parity= if (params[0].equals("reload")) {
		cache::HTMLCache::getInstance().reload(true);
		PacketSendUtility::sendMessage(admin, cache::HTMLCache::getInstance().toString());
	} else if (params[0] == "show") // parity= } else if (params[0].equals("show"))
		if (params.size() >= 2) { // parity= if (params.length >= 2)
			std::optional<std::string> html = cache::HTMLCache::getInstance().getHTML(params[1] + ".xhtml"); // parity= HTMLService.showHTML(player, HTMLCache.getInstance().getHTML(params[1] + ".xhtml"));
			// Java: HTMLService.showHTML(player, null) throws in html.length() for a page the cache has not got
			services::HTMLService::showHTML(admin, html.has_value() ? *html : throw runtime::NullPointerException("HTMLCache.getHTML")); // parity: (the same, Java's NPE explicit)
		} else
			PacketSendUtility::sendMessage(admin, "Usage: //html show <filename>");
}

// Java Html.java:35-38
void Html::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Usage: //html <reload|show>");
}

} // namespace aion::gameserver::handlers::admincommands
