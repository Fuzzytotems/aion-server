// M5j stage 4 CP4 (m5j-plan.md §18.5, L-01): the stage-4 system commands //bookmark (Bookmark.java), the console command Bookmark_add
// (Bookmark_add.java) and //collide (Collide.java), on real Players with real AionConnections (CommandTestSupport.h) in a Poeta map instance.
// The texts are the Java literals.
//
// The bookmark cases that reach BookmarkDAO use the test database of the DAO tests (dao/DaoTestDatabase.h, the players row of the foreign key)
// and skip themselves without AION_TEST_GS_DATABASE_URL.
//
// //collide runs against real geometry: GeoService is initialised with geo data on from generated files (the GeoMovementEffectsTest format: a
// big endian models.mesh and a 210010000.geo of placements, no terrain), holding
//
// - a floor (a horizontal quad 6 x 6 m) at (99, 99, 48): 2 m below a target at (100, 100, 50), the "below the target" arm;
// - a floor at (199, 99, 49.8): 0.2 m below a target at (200, 100, 50), the arm that spawns the marker npc 200000 at the contact point;
// - a wall (a vertical quad in the y-z plane, y +-3, z -5..15) at (305, 101, 45) between a target at x 300 and the GM at x 310 (y 100);
//
// The rays pass off the quads' diagonals: a ray through the edge the two triangles share collides with both.
// - nothing around x 400: no collisions.
//
// The expected collision lists are read from GeoService.getCollisions with the rays of Collide.java:46-82 (the oracle of the geometry; the case
// asserts the command's rays, order, texts and spawns). The fixture restores the empty GeoMaps (GeoService.init with geo data off) after each
// //collide case, and the working directory right after the load.

#include "CommandTestSupport.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "../../dao/DaoTestDatabase.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/handlers/admincommands/Bookmark.h"
#include "aion/gameserver/handlers/admincommands/Collide.h"
#include "aion/gameserver/handlers/admincommands/Debug.h"
#include "aion/gameserver/handlers/admincommands/Reload.h"
#include "aion/gameserver/handlers/consolecommands/Bookmark_add.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_BOOKMARK_ADD.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using geoEngine::math::JavaFloat;
using model::gameobjects::Npc;
using serverpackets::SM_SYSTEM_MESSAGE;

/** ChatCommand.getAliasForLevel (protected) */
struct BookmarkPeer : handlers::admincommands::Bookmark {
	using Bookmark::getAliasForLevel;
};

/** The marker npc of Collide.java:91-100, its row of npc_templates.xml verbatim */
constexpr std::string_view MARKER_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="200000" level="1" name=" " name_id="350000" height="0.3" group_drop="WORM" rank="DISCIPLINED" rating="NORMAL" tribe="GENERAL" ai="skillarea" srange="15" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="2522" />
		<bound_radius front="0.06" side="0.1728" upper="0.3" />
	</npc_template>
</npc_templates>)xml";

// ---- geo files (GeoWorldLoader.java's formats, as tests/effects_mz/GeoMovementEffectsTest.cpp writes them) --------------------------------

class BigEndianWriter {
public:
	std::vector<uint8_t> bytes;

	BigEndianWriter& u8(uint8_t value) {
		bytes.push_back(value);
		return *this;
	}
	BigEndianWriter& i16(int16_t value) {
		bytes.push_back(static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8));
		bytes.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	BigEndianWriter& u32(uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8)
			bytes.push_back(static_cast<uint8_t>(value >> shift));
		return *this;
	}
	BigEndianWriter& f32(float value) { return u32(std::bit_cast<uint32_t>(value)); }
	BigEndianWriter& name(std::string_view text) {
		i16(static_cast<int16_t>(text.size()));
		bytes.insert(bytes.end(), text.begin(), text.end());
		return *this;
	}
};

constexpr std::string_view FLOOR = "levels/test/floor.cgf";
constexpr std::string_view WALL = "levels/test/wall.cgf";

/** A models.mesh entry of one PHYSICAL model of two triangles with the four vertices */
void writeQuad(BigEndianWriter& out, std::string_view name, const float (&vertices)[12]) {
	const int8_t indices[] = {0, 1, 2, 1, 3, 2};
	out.name(name).u8(1); // one model
	out.i16(4);
	for (float value : vertices)
		out.f32(value);
	out.i16(2).u8(1); // two triangles, byte indices
	for (int8_t index : indices)
		out.u8(static_cast<uint8_t>(index));
	out.u8(0).u8(1); // material 0 (no material template), PHYSICAL
}

/** A .geo placement: the model's name, its location, the identity rotation, scale 1, no despawnable type */
void writePlacement(BigEndianWriter& out, std::string_view name, float x, float y, float z) {
	out.name(name).f32(x).f32(y).f32(z);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out.f32(i == j ? 1.0f : 0.0f);
	out.f32(1).f32(1).f32(1).u8(0).i16(0).u8(0);
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

class SystemCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["Bookmark_add"] = 3; // commands.properties
		levels["collide"] = 3;
		levels["debug"] = 3;
		levels["reload"] = 3;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	void TearDown() override {
		for (const runtime::Ptr<Npc>& npc : markers) {
			if (npc->isSpawned())
				npc->getController().delete_();
		}
		markers.clear();
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		for (PlayerFixture& fixture : players)
			fixture.player->setTarget(nullptr);
		stored.clear();
		if (geoLoaded) { // inside the fixture's TaskScope: the replaced GeoMaps are released here
			configs::main::GeoDataConfig::GEO_ENABLE.store(false);
			world::geo::GeoService::getInstance().init(); // empty GeoMaps: what a server with geo data off has
			configs::main::GeoDataConfig::GEO_ENABLE.store(geoEnabled);
			std::error_code ignored;
			std::filesystem::remove_all(root, ignored);
		}
		CommandTest::TearDown();
		if (dataPublished) {
			dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
			dataholders::DataManager::SPAWNS_DATA.resetForTests();
			dataholders::DataManager::GATHERABLE_DATA.resetForTests();
			dataholders::DataManager::NPC_DATA.resetForTests();
			if (savedMissingAiHandlers)
				configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		}
	}

	/** The //collide fixture: the marker npc's data and the geometry of the file comment, loaded by GeoService.init with geo data on */
	void loadGeo() {
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, MARKER_NPC_TEMPLATES_XML));
		dataholders::DataManager::GATHERABLE_DATA.publish(xml::bindString<dataholders::GatherableData>(context, "<gatherable_templates/>"));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, "<spawns/>"));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, R"(<tribe_relations><tribe name="PC"/><tribe name="GENERAL"/></tribe_relations>)"));
		geoEnabled = configs::main::GeoDataConfig::GEO_ENABLE.load();
		geoLoaded = true;
		dataPublished = true;

		root = std::filesystem::temp_directory_path() / ("aion_gs_collide_geo_" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(root / "data" / "geo");
		BigEndianWriter meshes;
		const float floor[] = {-3, -3, 0, 3, -3, 0, -3, 3, 0, 3, 3, 0};
		const float wall[] = {0, -3, -5, 0, 3, -5, 0, -3, 15, 0, 3, 15};
		writeQuad(meshes, FLOOR, floor);
		writeQuad(meshes, WALL, wall);
		writeFile(root / "data" / "geo" / "models.mesh", meshes.bytes);
		BigEndianWriter geo;
		writePlacement(geo, FLOOR, 99, 99, 48);
		writePlacement(geo, FLOOR, 199, 99, 49.8f);
		writePlacement(geo, WALL, 305, 101, 45);
		writeFile(root / "data" / "geo" / "210010000.geo", geo.bytes);

		const std::filesystem::path workingDirectory = std::filesystem::current_path();
		std::filesystem::current_path(root);
		configs::main::GeoDataConfig::GEO_ENABLE.store(true);
		try {
			world::geo::GeoService::getInstance().init();
		} catch (...) {
			std::filesystem::current_path(workingDirectory);
			throw;
		}
		std::filesystem::current_path(workingDirectory);
		geoEngine::GeoCallbacks::setMaterialZoneSink(nullptr); // set by init with geo data on; the quads have no material
	}

	Player& gm(int32_t objectId, std::string_view name = "Warden") {
		Player& player = connected(objectId, name, 9);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	void place(Player& player, float x, float y, float z) {
		player.setPosition(world::WorldPosition::create(210010000, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		player.getPosition()->setIsSpawned(true);
	}

	/** The packets of sendInfo(admin, text) for each text, in order */
	std::vector<std::vector<uint8_t>> infos(std::initializer_list<std::string> texts) {
		std::vector<std::vector<uint8_t>> packets;
		for (const std::string& text : texts)
			for (std::vector<uint8_t>& packet : info(text))
				packets.push_back(std::move(packet));
		return packets;
	}

	/** Collide.listCollisions' text (Collide.java:106-125) of `results`, from the geometry oracle */
	static std::string listing(geoEngine::collision::CollisionResults& results) {
		std::optional<geoEngine::collision::CollisionResult> closest = results.getClosestCollision();
		std::string text;
		int32_t count = 1;
		int32_t closestId = 0;
		for (const geoEngine::collision::CollisionResult& result : results) {
			if (result.equals(*closest))
				closestId = count;
			text += std::to_string(count) + ". " + result.getContactPoint().toString() + "; name=" + result.getGeometry()->getName() + "\n";
			count++;
		}
		return text + "-----------------------\nClosest: " + std::to_string(closestId) + ". Distance: " + JavaFloat::toString(closest->getDistance());
	}

	static int8_t physical() { return geoEngine::collision::getId(geoEngine::collision::CollisionIntention::PHYSICAL); }

	/** The npcs 200000 of the World's Poeta instance 1 (SpawnEngine.spawnObject's map instance), kept for the TearDown */
	std::vector<runtime::Ptr<Npc>> spawnedMarkers() {
		std::vector<runtime::Ptr<Npc>> found = world::World::getInstance().getWorldMap(210010000)->getWorldMapInstance(1)->getNpcs({200000});
		for (const runtime::Ptr<Npc>& npc : found)
			markers.push_back(npc);
		return found;
	}

	std::vector<Player*> stored;
	std::vector<runtime::Ptr<Npc>> markers;
	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	bool geoEnabled = false;
	bool geoLoaded = false;
	bool dataPublished = false; // the npc, gatherable, spawn and tribe holders and MISSING_AI_HANDLERS, restored by TearDown
	std::filesystem::path root;
};

// ---- //bookmark (Bookmark.java:11-40) ------------------------------------------------------------------------------------------------------

/** The syntax arms, and the access level alias of the console command (Bookmark.java:19-22) */
TEST_F(SystemCommandsTest, BookmarkSyntaxAndAliasForLevel) {
	Player& admin = gm(731001);
	BookmarkPeer bookmark;
	EXPECT_EQ(bookmark.getAliasForLevel(), "Bookmark_add");
	EXPECT_EQ(bookmark.getAlias(), "bookmark");
	EXPECT_EQ(handlers::consolecommands::Bookmark_add::ALIAS, "Bookmark_add");
	for (std::vector<std::string> params : {args({}), args({"del"}), args({"list"}), args({"deleteAllx"})}) {
		client()->clearSent();
		EXPECT_TRUE(bookmark.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(bookmark.getSyntaxInfo())) << params.size();
	}
}

/** del <name> (the joined parameters after "del "), its not-found answer, and deleteAll */
TEST_F(SystemCommandsTest, BookmarkDeletesByNameAndAll) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: BookmarkDAO reads and writes the database";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	Player& admin = gm(731002);
	dao::test::insertPlayer(admin.getObjectId(), "Warden", 9902);
	dao::test::insertPlayer(731003, "Other", 9903);
	dao::BookmarkDAO::storeBookmark(admin.getObjectId(), *dao::BookmarkDAO::Bookmark::create("My Home", 110010000, 1.0f, 2.0f, 3.0f));
	dao::BookmarkDAO::storeBookmark(admin.getObjectId(), *dao::BookmarkDAO::Bookmark::create("Mine", 210010000, 4.0f, 5.0f, 6.0f));
	dao::BookmarkDAO::storeBookmark(admin.getObjectId(), *dao::BookmarkDAO::Bookmark::create("Third", 210010000, 4.0f, 5.0f, 6.0f));
	dao::BookmarkDAO::storeBookmark(731003, *dao::BookmarkDAO::Bookmark::create("Mine", 210010000, 7.0f, 8.0f, 9.0f));
	handlers::admincommands::Bookmark bookmark;

	EXPECT_TRUE(bookmark.process(admin, args({"DEL", "My", "Home"})));
	EXPECT_EQ(client()->sentBytes(), info("The bookmark has been deleted. The bookmarks list will be updated after relog."));
	EXPECT_EQ(dao::BookmarkDAO::loadBookmarks(admin.getObjectId()).size(), 2u);

	client()->clearSent();
	EXPECT_TRUE(bookmark.process(admin, args({"del", "My", "Home"})));
	EXPECT_EQ(client()->sentBytes(), info("No bookmark with that name was found."));
	client()->clearSent();
	EXPECT_TRUE(bookmark.process(admin, args({"del", "Min"})));
	EXPECT_EQ(client()->sentBytes(), info("No bookmark with that name was found.")) << "the whole name";
	EXPECT_EQ(dao::BookmarkDAO::loadBookmarks(admin.getObjectId()).size(), 2u);

	client()->clearSent();
	EXPECT_TRUE(bookmark.process(admin, args({"deleteall", "ignored"})));
	EXPECT_EQ(client()->sentBytes(), info("All bookmarks have been deleted. The bookmarks list will be updated after relog."));
	EXPECT_TRUE(dao::BookmarkDAO::loadBookmarks(admin.getObjectId()).empty());
	EXPECT_EQ(dao::BookmarkDAO::loadBookmarks(731003).size(), 1u) << "only the GM's own bookmarks";
}

// ---- Bookmark_add (Bookmark_add.java:15-31) ------------------------------------------------------------------------------------------------

/** An empty name stores and sends nothing (before the database) */
TEST_F(SystemCommandsTest, BookmarkAddIgnoresAnEmptyName) {
	Player& admin = gm(731004);
	handlers::consolecommands::Bookmark_add add;
	EXPECT_EQ(add.getAlias(), "Bookmark_add");
	EXPECT_EQ(add.getPrefix(), "");
	EXPECT_TRUE(add.process(admin, args({})));
	EXPECT_TRUE(client()->sentBytes().empty());
}

/** The joined name, cut to 27 UTF-16 units, stored at the GM's position and sent back as SM_GM_BOOKMARK_ADD */
TEST_F(SystemCommandsTest, BookmarkAddStoresTheGmsPositionUnderTheJoinedName) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: BookmarkDAO writes the database";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	Player& admin = gm(731005);
	dao::test::insertPlayer(admin.getObjectId(), "Warden", 9905);
	place(admin, 120.5f, 130.25f, 51.0f);
	handlers::consolecommands::Bookmark_add add;

	EXPECT_TRUE(add.process(admin, args({"Poeta", "Gate"})));
	runtime::Ref<dao::BookmarkDAO::Bookmark> gate = dao::BookmarkDAO::Bookmark::create("Poeta Gate", 210010000, 120.5f, 130.25f, 51.0f);
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{serialized(serverpackets::SM_GM_BOOKMARK_ADD(*gate), client().con())});

	client()->clearSent();
	// 28 UTF-16 units (the cyrillic letters are two UTF-8 bytes each): the first 27 are kept
	EXPECT_TRUE(add.process(admin, args({"\xD0\x90\xD0\x91\xD0\x92\xD0\x93\xD0\x94", "abcdefghijklmnopqrstuvw"})));
	runtime::Ref<dao::BookmarkDAO::Bookmark> cut =
		dao::BookmarkDAO::Bookmark::create("\xD0\x90\xD0\x91\xD0\x92\xD0\x93\xD0\x94 abcdefghijklmnopqrstu", 210010000, 120.5f, 130.25f, 51.0f);
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{serialized(serverpackets::SM_GM_BOOKMARK_ADD(*cut), client().con())});

	client()->clearSent();
	EXPECT_TRUE(add.process(admin, args({"abcdefghijklmnopqrstuvwxyz0"}))); // exactly 27: kept
	std::vector<runtime::Ref<dao::BookmarkDAO::Bookmark>> bookmarks = dao::BookmarkDAO::loadBookmarks(admin.getObjectId());
	ASSERT_EQ(bookmarks.size(), 3u);
	int32_t matched = 0;
	for (const runtime::Ref<dao::BookmarkDAO::Bookmark>& b : bookmarks)
		matched += b->equals(*gate) || b->equals(*cut) ||
			b->equals(*dao::BookmarkDAO::Bookmark::create("abcdefghijklmnopqrstuvwxyz0", 210010000, 120.5f, 130.25f, 51.0f));
	EXPECT_EQ(matched, 3);
}

// ---- //collide (Collide.java:21-125) -------------------------------------------------------------------------------------------------------

/** A parameter other than "me" gives the syntax; no target gives STR_INVALID_TARGET (before the geo data) */
TEST_F(SystemCommandsTest, CollideSyntaxAndTarget) {
	Player& admin = gm(731010);
	handlers::admincommands::Collide collide;
	EXPECT_TRUE(collide.process(admin, args({"you"})));
	EXPECT_EQ(client()->sentBytes(), info(collide.getSyntaxInfo()));
	for (std::vector<std::string> params : {args({}), args({"ME"})}) {
		client()->clearSent();
		EXPECT_TRUE(collide.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{serialized(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET(), client().con())});
	}
}

/** Without "me": the ray from the target down 10 m; a floor 2 m below lists its collision and warns, nothing is spawned */
TEST_F(SystemCommandsTest, CollideBelowTheTargetWarns) {
	loadGeo();
	Player& admin = gm(731011);
	Player& target = gm(731012, "Target");
	place(admin, 110.0f, 100.0f, 50.0f);
	place(target, 100.0f, 100.0f, 50.0f);
	admin.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&target));
	client()->clearSent();
	handlers::admincommands::Collide collide;

	EXPECT_TRUE(collide.process(admin, args({})));
	geoEngine::collision::CollisionResults oracle = world::geo::GeoService::getInstance().getCollisions(target, 100.0f, 100.0f, 40.0f, physical(), nullptr);
	ASSERT_EQ(oracle.size(), 1);
	EXPECT_EQ(oracle.getClosestCollision()->getContactPoint().z, 48.0f);
	EXPECT_EQ(client()->sentBytes(), infos({"Target: X=100.0; Y=100.0; Z=50.0", listing(oracle), "Closest collision is below target's Z coordinate!"}));
	EXPECT_TRUE(spawnedMarkers().empty());
}

/** Without "me": a floor 0.2 m below the target (contact z + 0.5 >= the target's z) spawns the marker npc at the contact point */
TEST_F(SystemCommandsTest, CollideSpawnsAMarkerAtTheClosestCollision) {
	loadGeo();
	Player& admin = gm(731013);
	Player& target = gm(731014, "Target");
	place(admin, 210.0f, 100.0f, 50.0f);
	place(target, 200.0f, 100.0f, 50.0f);
	admin.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&target));
	client()->clearSent();
	handlers::admincommands::Collide collide;

	EXPECT_TRUE(collide.process(admin, args({})));
	geoEngine::collision::CollisionResults oracle = world::geo::GeoService::getInstance().getCollisions(target, 200.0f, 100.0f, 40.0f, physical(), nullptr);
	ASSERT_EQ(oracle.size(), 1);
	geoEngine::math::Vector3f contact = oracle.getClosestCollision()->getContactPoint();
	EXPECT_EQ(client()->sentBytes(), infos({"Target: X=200.0; Y=100.0; Z=50.0", listing(oracle)}));
	std::vector<runtime::Ptr<Npc>> spawned = spawnedMarkers();
	ASSERT_EQ(spawned.size(), 1u);
	EXPECT_EQ(spawned[0]->getX(), contact.x);
	EXPECT_EQ(spawned[0]->getY(), contact.y);
	EXPECT_EQ(spawned[0]->getZ(), contact.z);
	EXPECT_EQ(spawned[0]->getHeading(), 0);
	EXPECT_EQ(spawned[0]->getSpawn()->getRespawnTime(), 0);
}

/** "me": the ray from the target towards the GM (z + upper / 6), then from the GM towards the target (z + upper / 2); a wall between them
 * gives a collision each way and two markers */
TEST_F(SystemCommandsTest, CollideMeListsBothDirectionsAndSpawnsTwoMarkers) {
	loadGeo();
	Player& admin = gm(731015);
	Player& target = gm(731016, "Target");
	place(admin, 310.0f, 100.0f, 50.0f);
	place(target, 300.0f, 100.0f, 50.0f);
	// bound radii with an upper part, so that Java's upper / 6 and upper / 2 move the rays' ends (the test players' radius is 0)
	admin.getCommonData()->setBoundingRadius(model::templates::BoundRadius::intern(0.5f, 0.5f, 3.0f));
	target.getCommonData()->setBoundingRadius(model::templates::BoundRadius::intern(0.5f, 0.5f, 2.4f));
	admin.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&target));
	client()->clearSent();
	handlers::admincommands::Collide collide;

	EXPECT_TRUE(collide.process(admin, args({"Me"})));
	const float adminUpper = admin.getObjectTemplate()->getBoundRadius()->getUpper();
	const float targetUpper = target.getObjectTemplate()->getBoundRadius()->getUpper();
	geoEngine::collision::CollisionResults towardsYou =
		world::geo::GeoService::getInstance().getCollisions(target, 310.0f, 100.0f, 50.0f + adminUpper / 6, physical(), nullptr);
	geoEngine::collision::CollisionResults towardsTarget =
		world::geo::GeoService::getInstance().getCollisions(admin, 300.0f, 100.0f, 50.0f + targetUpper / 2, physical(), nullptr);
	ASSERT_EQ(towardsYou.size(), 1);
	ASSERT_EQ(towardsTarget.size(), 1);
	EXPECT_EQ(client()->sentBytes(), infos({"From target towards you:", "Target: X=300.0; Y=100.0; Z=50.0", listing(towardsYou),
		"From you towards your target:", "Admin: X=310.0; Y=100.0; Z=50.0", listing(towardsTarget)}));
	std::vector<runtime::Ptr<Npc>> spawned = spawnedMarkers();
	ASSERT_EQ(spawned.size(), 2u);
	std::vector<float> zs{spawned[0]->getZ(), spawned[1]->getZ()};
	std::sort(zs.begin(), zs.end());
	std::vector<float> expected{towardsYou.getClosestCollision()->getContactPoint().z, towardsTarget.getClosestCollision()->getContactPoint().z};
	std::sort(expected.begin(), expected.end());
	EXPECT_EQ(zs, expected);
	for (const runtime::Ptr<Npc>& npc : spawned)
		EXPECT_EQ(npc->getX(), 305.0f) << "on the wall";
}

/** No geometry on either ray: "No collisions found." each way, nothing spawned */
TEST_F(SystemCommandsTest, CollideWithoutCollisions) {
	loadGeo();
	Player& admin = gm(731017);
	Player& target = gm(731018, "Target");
	place(admin, 410.0f, 100.0f, 50.0f);
	place(target, 400.0f, 100.0f, 50.0f);
	admin.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&target));
	client()->clearSent();
	handlers::admincommands::Collide collide;

	EXPECT_TRUE(collide.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), infos({"Target: X=400.0; Y=100.0; Z=50.0", "No collisions found."}));
	client()->clearSent();
	EXPECT_TRUE(collide.process(admin, args({"me"})));
	EXPECT_EQ(client()->sentBytes(), infos({"From target towards you:", "Target: X=400.0; Y=100.0; Z=50.0", "No collisions found.",
		"From you towards your target:", "Admin: X=410.0; Y=100.0; Z=50.0", "No collisions found."}));
	EXPECT_TRUE(spawnedMarkers().empty());
}

// ---- //debug (Debug.java:22-94) -----------------------------------------------------------------------------------------------------------

/** The syntax arms; without a running client NioServer every arm ends in Java's NullPointerException (GameServer.nioServer is null). The
 * three arms on a running server are the M5j gate's (Z18). */
TEST_F(SystemCommandsTest, DebugSyntaxAndNoNioServer) {
	Player& admin = gm(731020);
	handlers::admincommands::Debug debug;
	ASSERT_EQ(GameServer::getNioServer(), nullptr);
	for (std::vector<std::string> params : {args({}), args({"connection"}), args({"players"})}) {
		client()->clearSent();
		EXPECT_TRUE(debug.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(debug.getSyntaxInfo())) << params.size();
	}
	for (std::vector<std::string> params : {args({"CONNECTIONS"}), args({"connectedplayers"}), args({"dcBuggedPlayers"})}) {
		client()->clearSent();
		EXPECT_TRUE(debug.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>")) << params[0];
	}
}

// ---- //reload (Reload.java:24-106) --------------------------------------------------------------------------------------------------------

/** The syntax arms, the ai arm (AIEngine.reload), and the eight static-data arms that stay unported (D3: HolderRef publishes once). The
 * config and commands arms read ./config: the M5j gate's (Z18). */
TEST_F(SystemCommandsTest, ReloadSyntaxAiAndTheUnportedStaticDataArms) {
	Player& admin = gm(731021);
	// AIEngine.validateScripts reads the npc templates: none here
	savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
	dataPublished = true;
	dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, "<npc_templates/>"));
	handlers::admincommands::Reload reload;
	for (std::vector<std::string> params : {args({}), args({"quest"}), args({"all"})}) {
		client()->clearSent();
		EXPECT_TRUE(reload.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(reload.getSyntaxInfo())) << params.size();
	}
	client()->clearSent();
	EXPECT_TRUE(reload.process(admin, args({"AI"})));
	EXPECT_EQ(client()->sentBytes(), info("AI successfully reloaded!"));
	for (const char* arm : {"quests", "skills", "npcskills", "items", "customdrops", "events", "arcade", "decomposables"}) {
		const uint64_t before = runtime::unportedHitCount();
		client()->clearSent();
		EXPECT_TRUE(reload.process(admin, args({arm})));
		EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>")) << arm;
		EXPECT_EQ(runtime::unportedHitCount(), before + 1) << arm;
	}
	uint64_t reloadHits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits())
		if (hit.file.ends_with("Reload.cpp"))
			reloadHits += hit.hits;
	EXPECT_EQ(reloadHits, 8u) << "one site per arm, in Reload.cpp";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
