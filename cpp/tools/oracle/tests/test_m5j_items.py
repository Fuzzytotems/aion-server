"""M5j items oracle (m5j/items.py, m5j-plan.md §10.4 Z9-Z11, §18.3 H-21): the use checks, the picks and the zone rules on hand-made data, and
on the real tree the gate's ride, kisk and pet.

Expected values are derived by hand from PlayerRestrictions.canUseItem, ItemTemplate.isClassSpecific / getRequiredLevel, ZoneInstance.
canPutKisk / canRide, KiskStatsTemplate's defaults, PetCommonData's constructor and SM_PET.writePetData, and repeated per case.
"""

import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from types import SimpleNamespace

from m5j import items as it
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

GAME_SERVER = runner.DEFAULT_STATIC_DATA.parent.parent

# two classes are enough for the rules: WARRIOR (starting) at ordinal 0 and GLADIATOR (advanced, from WARRIOR) at ordinal 1
ENUMS = SimpleNamespace(classes={"WARRIOR": None, "GLADIATOR": None}, starting_classes={"WARRIOR": "WARRIOR", "GLADIATOR": "WARRIOR"})


class FakeData:
	def __init__(self, **roots: str):
		self.roots = {root: ET.fromstring(text) for root, text in roots.items()}

	def stream(self, root_tag: str, child_tag: str):
		root = self.roots.get(root_tag)
		return iter([] if root is None else root.findall(child_tag))


def item(attrs: str, actions: str = "", limits: str = "") -> ET.Element:
	return ET.fromstring(f'<item_template {attrs}><actions>{actions}</actions>{limits}</item_template>')


class UsableTest(unittest.TestCase):
	def test_the_required_level_and_the_class(self):
		self.assertEqual(it.usable(item('id="1"'), ENUMS, "WARRIOR", "ELYOS", 1), 1)  # no restrict: every class at level 1
		self.assertEqual(it.usable(item('id="1" restrict="5 5"'), ENUMS, "WARRIOR", "ELYOS", 5), 5)
		self.assertIsNone(it.usable(item('id="1" restrict="5 5"'), ENUMS, "WARRIOR", "ELYOS", 4))
		self.assertIsNone(it.usable(item('id="1" restrict="0 5"'), ENUMS, "WARRIOR", "ELYOS", 60), "restrict 0: not class specific")
		# an advanced class without its own level takes the starting class's for isClassSpecific, and getRequiredLevel answers -1
		self.assertEqual(it.usable(item('id="1" restrict="5 0"'), ENUMS, "GLADIATOR", "ELYOS", 1), -1)

	def test_race_gender_area_activation_and_max_level(self):
		self.assertIsNone(it.usable(item('id="1" race="ASMODIANS"'), ENUMS, "WARRIOR", "ELYOS", 1))
		self.assertEqual(it.usable(item('id="1" race="PC_ALL"'), ENUMS, "WARRIOR", "ELYOS", 1), 1)
		self.assertIsNone(it.usable(item('id="1"', limits='<uselimits gender="MALE"/>'), ENUMS, "WARRIOR", "ELYOS", 1))
		self.assertIsNone(it.usable(item('id="1"', limits='<uselimits usearea="X"/>'), ENUMS, "WARRIOR", "ELYOS", 1))
		self.assertIsNone(it.usable(item('id="1" activate_target="KRALL"'), ENUMS, "WARRIOR", "ELYOS", 1, {"KRALL"}))
		self.assertEqual(it.usable(item('id="1" activate_target="STANDALONE"'), ENUMS, "WARRIOR", "ELYOS", 1, {"KRALL"}), 1)
		self.assertIsNone(it.usable(item('id="1" restrict_max="9 9"'), ENUMS, "WARRIOR", "ELYOS", 10))
		self.assertEqual(it.usable(item('id="1" restrict_max="9 9"'), ENUMS, "WARRIOR", "ELYOS", 9), 1)


ITEMS = """<item_templates>
	<item_template id="30" restrict="30 30" casting_delay="3000"><actions><ride npc_id="2"/></actions></item_template>
	<item_template id="20" restrict="1 1" casting_delay="1000" desc="7"><actions><ride npc_id="2"/></actions></item_template>
	<item_template id="10"><actions><ride npc_id="99"/></actions></item_template>
	<item_template id="21" restrict="1 1"><actions><ride npc_id="2"/></actions></item_template>
	<item_template id="40"><actions><toypetspawn npcid="500"/></actions></item_template>
	<item_template id="41"><actions><toypetspawn npcid="501"/></actions></item_template>
	<item_template id="42" casting_delay="10000"><actions><toypetspawn npcid="502"/></actions></item_template>
	<item_template id="50"><actions><adoptpet petId="900"/></actions></item_template>
	<item_template id="51"><actions><adoptpet petId="901" minutes="60"/></actions></item_template>
	<item_template id="52"><actions><adoptpet petId="902"/></actions></item_template>
	<item_template id="53"><actions><adoptpet petId="903"/></actions></item_template>
</item_templates>"""


class PickTest(unittest.TestCase):
	def data(self):
		return FakeData(item_templates=ITEMS,
		                rides='<rides><ride_info id="2" type="1" move_speed="14.0" fly_speed="16.0"/></rides>',
		                npc_templates='<npc_templates><npc_template npc_id="500"><kisk_stats resurrects="0" members="30"/></npc_template>'
		                              '<npc_template npc_id="501"><kisk_stats members="1"/></npc_template>'
		                              '<npc_template npc_id="502" name_id="9"><kisk_stats usemask="0"/></npc_template></npc_templates>',
		                pets='<pets><pet id="900"><petfunction id="6" type="FOOD"/></pet><pet id="901"><petfunction id="7" type="FOOD"/></pet>'
		                     '<pet id="902"><petfunction type="LOOT"/><petfunction type="DOPING"/><petfunction id="7" type="FOOD"/></pet>'
		                     '<pet id="903"><petfunction id="7" type="FOOD"/><petfunction type="WING"/><petfunction type="LOOT"/></pet></pets>',
		                pet_feed='<pet_feed><flavour id="7" full_count="100" cd="10"><food group="FLUIDS"/></flavour></pet_feed>')

	def test_the_ride_with_the_lowest_level_then_id(self):
		ride = it.ride_block(self.data(), ENUMS, "WARRIOR", "ELYOS", 1, set())
		# 10 has no ride_info (getRideInfo() null), 30 needs level 30: 20 and 21 need 1, 20 is the lower id
		self.assertEqual((ride["itemId"], ride["requiredLevel"], ride["castingDelay"], ride["npcId"], ride["nameId"]), (20, 1, 1000, 2, 7))
		self.assertEqual(ride["rideInfo"], {"type": 1, "moveSpeed": 14.0, "flySpeed": 16.0})

	def test_the_kisk_needs_resurrections_and_the_dialog(self):
		kisk = it.kisk_block(self.data(), ENUMS, "WARRIOR", "ELYOS", 1, set())
		# 500: no resurrection; 501: one member (onBind at once); 502 takes KiskStatsTemplate's defaults members 6, resurrects 18
		self.assertEqual((kisk["itemId"], kisk["npcId"], kisk["maxMembers"], kisk["maxResurrects"], kisk["useMask"]), (42, 502, 6, 18, 0))
		self.assertEqual(kisk["lifetimeSeconds"], 7200)
		self.assertEqual(kisk["castingDelay"], 10000)

	def test_the_pet_needs_a_fed_flavour_and_two_specialties(self):
		pet = it.pet_block(self.data())
		# 900: flavour 6 is not in pet_feed (PetCommonData NPE); 901 expires; 902 writes three specialties; 903 writes LOOT, FOOD
		self.assertEqual((pet["eggItemId"], pet["petId"]), (53, 903))
		self.assertEqual(pet["writtenSpecialties"], ["LOOT", "FOOD"])
		self.assertEqual(pet["flavour"]["id"], 7)

	def test_no_pet(self):
		with self.assertRaises(OracleError):
			it.pet_block(FakeData(item_templates="<item_templates/>", pets="<pets/>", pet_feed="<pet_feed/>"))


class ZoneTest(unittest.TestCase):
	def test_flagless_zones_take_the_map_and_flagged_ones_their_bits(self):
		data = FakeData(world_maps='<world_maps><map id="1" flags="BIND RECALL GLIDE"/></world_maps>',
		                zones='<zones><zone mapid="1" name="A" flags="7"/><zone mapid="1" name="B"/><zone mapid="1" name="C" flags="0"/>'
		                      '<zone mapid="2" name="D" flags="2"/></zones>')
		zones = it.map_zones(data, 1)
		self.assertEqual([z["name"] for z in zones["zones"]], ["A", "B", "C"])
		self.assertTrue(zones["kiskEverywhere"])
		self.assertFalse(zones["rideEverywhere"], "no RIDE on the map nor in A")
		refusing = FakeData(world_maps='<world_maps><map id="1" flags="BIND RIDE"/></world_maps>',
		                    zones='<zones><zone mapid="1" name="A" flags="2"/><zone mapid="1" name="B" flags="16"/></zones>')
		zones = it.map_zones(refusing, 1)
		self.assertEqual([z["canPutKisk"] for z in zones["zones"]], [False, False])
		self.assertEqual([z["canRide"] for z in zones["zones"]], [False, True])
		with self.assertRaises(OracleError):
			it.map_zones(refusing, 3)


@unittest.skipUnless((GAME_SERVER / "src").is_dir(), "the Java tree is not beside the static data")
class RealTreeTest(unittest.TestCase):
	def test_the_gate_items(self):
		from m5a.data import StaticData
		report = it.items_report(StaticData(runner.DEFAULT_STATIC_DATA), GAME_SERVER / "src", GAME_SERVER / "config", None,
		                         ["gameserver.ride.restriction.enable=false"], "WARRIOR", "ELYOS", 1, 210010000, ["STR_USE_ITEM"],
		                         ["STR_ASK_REGISTER_BINDSTONE"])
		self.assertEqual(report["emotions"], {"RIDE": 15, "RIDE_END": 16, "CHANGE_SPEED": 35})
		self.assertEqual(report["ride"]["itemId"], 190100042)  # Legion Pagati: level 1, casting_delay 1000
		self.assertEqual(report["ride"]["npcId"], 2000025)
		self.assertEqual(report["kisk"]["itemId"], 184000005)  # (E) Small Kisk: npc 700273, members 6, resurrects 18
		self.assertEqual(report["kisk"]["npcId"], 700273)
		self.assertEqual(report["pet"]["eggItemId"], 190000020)
		self.assertEqual(report["pet"]["writtenSpecialtyIds"], [1])
		self.assertTrue(report["map"]["kiskEverywhere"])
		self.assertFalse(report["map"]["rideEverywhere"])
		self.assertEqual(report["messages"]["STR_USE_ITEM"], 1300423)
		self.assertEqual(report["questions"]["STR_ASK_REGISTER_BINDSTONE"], 160018)

	def test_the_ride_restriction_is_refused_on_poeta(self):
		from m5a.data import StaticData
		with self.assertRaises(OracleError):
			it.items_report(StaticData(runner.DEFAULT_STATIC_DATA), GAME_SERVER / "src", GAME_SERVER / "config", None,
			                ["gameserver.ride.restriction.enable=true"], "WARRIOR", "ELYOS", 1, 210010000, [], [])


if __name__ == "__main__":
	unittest.main()
