#include "aion/gameserver/configs/main/HTMLConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void HTMLConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.html.welcome.enable", ENABLE_HTML_WELCOME, "false");
	AION_BIND(p, "gameserver.html.guides.enable", ENABLE_GUIDES, "false");
	AION_BIND(p, "gameserver.html.root", HTML_ROOT, "./data/static_data/HTML/");
	AION_BIND(p, "gameserver.html.cache.file", HTML_CACHE_FILE, "./cache/html.cache");
	AION_BIND(p, "gameserver.html.encoding", HTML_ENCODING, "UTF-8");
}

} // namespace aion::gameserver::configs::main
