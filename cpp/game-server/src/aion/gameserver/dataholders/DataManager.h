#pragma once

namespace aion::gameserver::dataholders {

/**
 * Holds the loaded static data (the holders of data/static_data). Spine step S0a stub: only the entry point exists, so the server links and
 * main.cpp reaches AION_UNPORTED here. S0b replaces it with the holder accessors (docs/design/static-data.md §3.3: HolderRef members, init()).
 * <p>
 * Java: com.aionemu.gameserver.dataholders.DataManager
 *
 * @author Luno, orz, Wakizashi, Neon
 */
class DataManager final {
public:
	/** Java: getInstance() - the first call loads all static data (the private constructor). Not ported yet. */
	static DataManager& getInstance();

	DataManager(const DataManager&) = delete;
	DataManager& operator=(const DataManager&) = delete;

private:
	DataManager() = default;
};

} // namespace aion::gameserver::dataholders
