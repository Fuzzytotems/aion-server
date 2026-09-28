"""M5c trade oracle (m5c/trade.py, m5c/trade_config.py, m5c-plan.md G-01): the price arithmetic alone, the Java properties reader, the Java
text reader and the member fingerprints, the Java tables and members as they stand today and on an edited copy, the whole report on a small
static_data tree, and the Poeta and Ishalgen merchants on the real data.

Expected values are derived by hand from the Java sources named in m5c/trade.py and repeated per case. Everything that reads the Java source
tree is skipped without it, like M5bFixtureReportTest.
"""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from m5a.data import StaticData
from m5a.javafloat import f32
from m5c.trade import (AUDIT_NO_PACKET, JAVA_SOURCES, DialogAi, JavaTradeRules, TradeData, ap_sale, buy_list_price, buy_price,
                       check_sales_time, fresh_sell_limit, global_prices, influence_rate, java_member, limited_sale, normalize_java, purchase_reward,
                       required_ap, sell_reward, taxes, trade_report)
from m5c.trade_config import (java_config_boolean, java_decode_int, java_float, load_config, parse_properties, property_defaults,
                              split_and_trim_values)
from staticdata_oracle import OracleError
from staticdata_oracle import run as runner

import oracle

from .support import Tree, xml

JAVA_SRC = runner.TOOL_DIR.parents[2] / "game-server" / "src"
CONFIG_DIR = runner.TOOL_DIR.parents[2] / "game-server" / "config"
HANDLERS = JAVA_SRC.parent / "data" / "handlers"
HAVE_JAVA_TREE = ((JAVA_SRC / "com" / "aionemu" / "gameserver").is_dir() and (HANDLERS / "ai").is_dir()
                  and runner.DEFAULT_STATIC_DATA.is_dir())
BASE = ("com", "aionemu", "gameserver")
# the keys of the gate profiles (m5a/m5b.properties.example): sieges and sell limits off
GATE_PROFILE = ["gameserver.siege.enable=false", "gameserver.limits.enable=false"]
DEFAULT_VALUES = {
	"gameserver.prices.default.prices": 100, "gameserver.prices.default.modifier": 100, "gameserver.prices.default.taxes": 100,
	"gameserver.prices.vendor.buymod": 100, "gameserver.prices.vendor.sellmod": 20, "gameserver.siege.enable": True,
	"gameserver.limits.enable": True, "gameserver.limits.enable_dynamic_cap": False, "gameserver.selling.apitems.enabled": True,
	"gameserver.rates.sell_limit": [1.0, 2.0], "gameserver.country.code": 99}


class M5cTradeFormulaTest(unittest.TestCase):
	"""The arithmetic of PricesService, TradeList, TradeService and PlayerLimitService, without any Java source or static data."""

	def test_global_prices_and_taxes_follow_the_influence(self):
		# sieges off: influence 0 for both races, 100 + (0.5f / 2) * 100 = 125 and Math.round(100 + (0.5f / 4) * 100) = Math.round(112.5f) = 113
		self.assertEqual((global_prices(100, influence_rate(0)), taxes(100, influence_rate(0))), (125, 113))
		self.assertEqual((global_prices(100, influence_rate(50)), taxes(100, influence_rate(50))), (100, 100), "0.5f exactly: the defaults")
		# 0.4f: diff/2*100 = 4.9999995f, 104.99999952 rounds to the float 105; diff/4*100 = 2.4999998f and 102.49999976 is the float 102.5,
		# which Math.round takes up
		self.assertEqual((global_prices(100, influence_rate(40)), taxes(100, influence_rate(40))), (105, 103))
		# above 0.5f the prices fall and the taxes stay: 0.6f - 0.5f = 0.10000002f, * 50 = 5.000001f (a float tie, to even), 94.999999 -> 95
		self.assertEqual((global_prices(100, influence_rate(60)), taxes(100, influence_rate(60))), (95, 100))
		self.assertEqual((global_prices(100, influence_rate(100)), taxes(100, influence_rate(100))), (75, 100))
		self.assertEqual(global_prices(150, influence_rate(0)), 175, "DEFAULT_PRICES is added to, not multiplied")
		self.assertEqual(influence_rate(40), f32(0.4))

	def test_buy_price_truncates_four_times(self):
		# PricesService.getBuyPrice: 250 -> 250 -> 312.5 = 312 -> 312 -> 352.56 = 352 (the Minor Life Elixir)
		self.assertEqual(buy_price(250, 100, 125, 100, 113), 352)
		self.assertEqual(buy_price(50, 100, 125, 100, 113), 70, "62.5 -> 62, 70.06 -> 70")
		self.assertEqual(buy_price(5, 100, 125, 100, 113), 6, "6.25 -> 6, 6.78 -> 6")
		self.assertEqual(buy_price(1000, 100, 125, 100, 113), 1412, "1412.5 truncates")
		self.assertEqual(buy_price(3, 100, 125, 100, 113), 3, "3 * 1.25 * 1.13 = 4.24, but 3.75 is truncated to 3 before the taxes")
		self.assertEqual(buy_price(60000000, 100, 125, 100, 113), 84750000, "the double arithmetic of 2mil+ items")
		self.assertEqual(buy_price(250, 150, 125, 100, 113), 528, "VENDOR_BUY_MODIFIER 150: 375 -> 468.75 = 468 -> 468 -> 528.84 = 528")

	def test_buy_list_price_is_long_arithmetic_per_item(self):
		# TradeList.java:53: getBuyPrice * count * modifier / 100
		self.assertEqual(buy_list_price(352, 2, 100), 704)
		self.assertEqual(buy_list_price(352, 3, 150), 1584)
		self.assertEqual(buy_list_price(6, 3, 33), 5, "594 / 100 truncates")
		self.assertEqual(buy_list_price(6, 1, 200), 12)

	def test_required_ap_truncates_before_the_division(self):
		# TradeList.java:71: (int) ((ap * count * modifier / 100.0D) * VENDOR_BUY_MODIFIER) / 100
		self.assertEqual(required_ap(105500, 1, 100, 100), 105500)
		self.assertEqual(required_ap(3, 1, 50, 100), 1, "1.5 * 100 = 150, 150 / 100 = 1")
		self.assertEqual(required_ap(7, 4, 100, 100), 28)
		self.assertEqual(required_ap(1, 1, 100, 99), 0, "99 / 100 in int division")

	def test_rewards(self):
		self.assertEqual(sell_reward(250, 20), 50, "getSellReward(250, VENDOR_SELL_MODIFIER 20)")
		self.assertEqual(sell_reward(4, 20), 0, "0.8 truncates to 0")
		self.assertEqual(purchase_reward(250, 40), 100)
		self.assertEqual(purchase_reward(5, 40), 2)
		# performSellForAPToShop: Math.round((ap * rate) / 100F) * (int) count
		self.assertEqual(ap_sale(6875, 20, 1), 1375)
		self.assertEqual(ap_sale(3, 20, 3), 3, "0.6f rounds to 1, times 3")
		self.assertEqual(ap_sale(5, 10, 1), 1, "Math.round(0.5f) is 1")
		self.assertEqual(ap_sale(-5, 10, 1), 0, "Math.round(-0.5f) is 0: half up, not away from zero")

	def test_sell_limit(self):
		# Rates.SELL_LIMIT: (long) (limit * rate), a long times a float: 17150047 is not a float, the nearest (ties to even) is 17150048
		self.assertEqual(fresh_sell_limit(5300047, f32(1.0)), 5300047)
		self.assertEqual(fresh_sell_limit(17150047, f32(1.0)), 17150048)
		self.assertEqual(fresh_sell_limit(5300047, f32(2.0)), 10600094)
		self.assertEqual(fresh_sell_limit(5300047, f32(0.00001)), 53, "53.000469f truncates")
		self.assertEqual(limited_sale(5300047, 200, 20000, False), {"soldCount": 20000, "possibleCount": 26500, "remainingLimit": 1300047,
		                                                           "message": None})
		self.assertEqual(limited_sale(100, 200, 5, False)["soldCount"], 0)
		self.assertEqual(limited_sale(100, 200, 5, False)["message"], "STR_MSG_DAY_CANNOT_SELL_NPC")
		self.assertEqual(limited_sale(100, 200, 5, True), {"soldCount": 1, "possibleCount": 1, "remainingLimit": 0, "message": None},
		                 "the dynamic cap sells one item past the limit and the limit ends at 0")
		self.assertEqual(limited_sale(0, 50, 1, True)["soldCount"], 0, "a spent limit sells nothing even with the dynamic cap")
		self.assertEqual(limited_sale(100, 50, 2, True), {"soldCount": 2, "possibleCount": 2, "remainingLimit": 0, "message": None},
		                 "possibleCount < itemCount is false when the limit covers the count exactly: no extra item")

	def test_sales_time_shapes(self):
		# LimitedItemTradeService.start schedules every limited item's sales time at startup; the shapes of the real data are accepted
		for text in ("0 0 0 ? * *", "0 0 09-18 ? * FRI", "0 0 0,10,12,14,18,22 ? * *", "0 0 12 ? * MON", "0 0 0 ? * wed", "0 0 10 ? * MON,TUE"):
			with self.subTest(text=text):
				check_sales_time(text, "t")
		# null (NullPointerException), empty, both day fields set (Quartz refuses it), out of range, a year field or forms not modelled
		for text in (None, "", "0 0 0 * * *", "0 0 24 ? * *", "0 0 18-09 ? * *", "0 0 0 ? * * 2030", "0 0/5 0 ? * *", "0 0 0 ? * MON-FRI"):
			with self.subTest(text=text), self.assertRaises(OracleError):
				check_sales_time(text, "t")


class M5cTradeJavaTextTest(unittest.TestCase):
	"""The Java reader the member fingerprints stand on: comments outside literals, members found by kind, name and parameters."""

	SOURCE = (
		'class A {\n'
		'\tprivate static final String S = "} // not a comment { /* nor this */";\n'
		'\t/* a { comment */\n'
		'\tpublic int f(int a) { return g(a) + 1; } // trailing }\n'
		'\tpublic int f(long a, String b) throws Exception { if (b.equals("{")) { return 2; } return 3; }\n'
		'\tint g(int a) { return new A() { }.hashCode(); }\n'
		'\tenum E { X { int v() { return \'}\'; } }, Y }\n'
		'\tvoid s(int d) { switch (d) { case 1: { h(); break; } case 2: case 3: k(); break; default: m(); } }\n'
		'}\n')

	def member(self, member: str) -> str:
		return normalize_java(java_member(self.SOURCE, member, "A.java"))

	def test_members(self):
		self.assertEqual(self.member("method f(int a)"), "publicintf(inta){returng(a)+1;}")
		self.assertEqual(self.member("method f(long a, String b)"), 'publicintf(longa,Stringb)throwsException{if(b.equals("{")){return2;}return3;}')
		self.assertEqual(self.member("method g"), "intg(inta){returnnewA(){}.hashCode();}", "a call of g and `new A() {` are no declarations")
		self.assertEqual(self.member("class E"), "enumE{X{intv(){return'}';}},Y}")
		self.assertEqual(self.member("block X"), "X{intv(){return'}';}}")
		self.assertEqual(self.member("case 1"), "case1:{h();break;}")
		self.assertEqual(self.member("case 2"), "case2:case3:k();break;", "the labels of one arm, up to the next label")
		for member in ("method f", "method h", "class B", "method f(int b)"):
			with self.subTest(member=member), self.assertRaises(OracleError):  # two declarations, a call only, none, other parameters
				java_member(self.SOURCE, member, "A.java")

	def test_comments_end_outside_literals(self):
		self.assertEqual(normalize_java('String s = "a // b /* c */"; // d\nint x; /* e */ int y;'), 'Strings="a//b/*c*/";intx;inty;')
		self.assertEqual(normalize_java("char c = '\"'; String t = \"//\"; // x"), "charc='\"';Stringt=\"//\";")


class M5cTradeConfigReaderTest(unittest.TestCase):
	"""java.util.Properties.load and the commons transformers, without any Java source."""

	def test_properties_syntax(self):
		text = ("# comment\n! comment too\n\n  a = 1\nb=2\nc:3\nd 4\ne\t=  5  \nf = x\\\n    y\ng = \\u0041\\t\nh\\ key = v\n"
		        "i = ends with a backslash \\\\\n# a comment line does not continue \\\nj = 7\nk\n")
		self.assertEqual(parse_properties(text, "t"), [("a", "1"), ("b", "2"), ("c", "3"), ("d", "4"), ("e", "5  "), ("f", "xy"), ("g", "A\t"),
		                                              ("h key", "v"), ("i", "ends with a backslash \\"), ("j", "7"), ("k", "")])
		self.assertEqual(parse_properties("a = b\\\n\nc = d", "t"), [("a", "b"), ("c", "d")], "a continuation into an empty line ends the line")
		with self.assertRaises(OracleError):
			parse_properties("a = \\u12", "t")

	def test_transformers(self):
		self.assertEqual(java_decode_int("20", "k"), 20)
		self.assertEqual(java_decode_int("+5", "k"), 5)
		self.assertEqual(java_decode_int("-5", "k"), -5)
		for text in ("0x14", "#14", "024", " 20", "20 ", "", "2147483648", "1.0"):
			with self.subTest(text=text), self.assertRaises(OracleError):
				java_decode_int(text, "k")
		self.assertEqual([java_config_boolean(t, "k") for t in ("true", "TRUE", "1", "false", "False", "0")], [True, True, True, False, False, False])
		with self.assertRaises(OracleError):
			java_config_boolean("yes", "k")
		self.assertEqual(split_and_trim_values("1.0, 2.0"), ["1.0", "2.0"])
		self.assertEqual(split_and_trim_values('"a,b", c'), ["a,b", "c"])
		self.assertEqual(split_and_trim_values("1.0,"), ["1.0"], "a trailing empty token is dropped")
		self.assertEqual(split_and_trim_values(",1"), ["", "1"])
		# String.trim removes the chars <= ' ' only: a no-break space stays, and Float.valueOf then rejects the token
		self.assertEqual(split_and_trim_values("1.0 ,\t2.0\x01"), ["1.0 ", "2.0"])

	def test_java_float_rounds_the_decimal_once(self):
		self.assertEqual(java_float("1.0", "k"), 1.0)
		self.assertEqual(java_float("0.1", "k"), f32(0.1))
		# just below the midpoint of 1 + 2^-23 and 1 + 2^-22: through a double it becomes the midpoint and then rounds to even (1 + 2^-22);
		# Float.valueOf rounds the decimal itself, to 1 + 2^-23
		self.assertEqual(java_float("1.000000178813934326171874999", "k"), 1 + 2**-23)
		for text in ("1.0f", "0x1p3", "NaN", "Infinity", "1e39", "1.0 "):
			with self.subTest(text=text), self.assertRaises(OracleError):
				java_float(text, "k")


def copy_java_tree(root: Path) -> Path:
	"""The Java files the trade rules read, copied below root."""
	source, target = JAVA_SRC.joinpath(*BASE), Path(root).joinpath(*BASE)
	for relative in JAVA_SOURCES:
		(target / relative).parent.mkdir(parents=True, exist_ok=True)
		shutil.copyfile(source / relative, target / relative)
	return Path(root)


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cTradeJavaRulesTest(unittest.TestCase):
	"""The tables the report reads from the Java sources, as they stand today, and the config defaults of the Config classes."""

	def test_tables(self):
		rules = JavaTradeRules.read(JAVA_SRC)
		self.assertEqual(rules.dialog_actions, {"BUY": 2, "SELL": 3, "TRADE_IN": 78, "TRADE_SELL_LIST": 103})
		self.assertEqual(rules.npc_type_index, {"NORMAL": 1, "ABYSS": 2, "LEGION_COIN": 3, "REWARD": 4, "ABYSS_KINAH": 5},
		                 "SM_TRADELIST writes 1 for a NORMAL vendor, not 0")
		self.assertEqual(rules.acquisition_types, ("AP", "ABYSS", "REWARD", "COUPON"))
		self.assertEqual((rules.item_masks["TRADEABLE"], rules.item_masks["SELLABLE"]), (2, 4))
		self.assertEqual(rules.sell_limits, ((1, 30, 5300047), (31, 40, 7100047), (41, 55, 12050047), (56, 60, 14600047), (61, 65, 17150047)))
		self.assertEqual(rules.tradelist_defaults, {"npc_type": "NORMAL", "sell_price_rate": 100, "sell_price_rate2": 100, "ap_sell_price_rate2": 100,
		                                            "buy_price_rate": 0})
		self.assertEqual(rules.max_count, 20000)

	def test_config_class_defaults(self):
		self.assertEqual(property_defaults(JAVA_SRC), {key: str(value).lower() if isinstance(value, bool) else "1.0, 2.0" if isinstance(value, list)
		                                               else str(value) for key, value in DEFAULT_VALUES.items()})


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cTradeJavaShapeTest(unittest.TestCase):
	"""JavaTradeRules.read on a copy of the Java files, one of them changed: the modelled members are fingerprinted, the literals are read."""

	@classmethod
	def setUpClass(cls):
		cls.root = tempfile.TemporaryDirectory()
		cls.java = copy_java_tree(Path(cls.root.name))

	@classmethod
	def tearDownClass(cls):
		cls.root.cleanup()

	def changed(self, relative: str, old: str, new: str):
		"""The copied tree with one edit, which the next call (or the end of the test) takes back."""
		self.doCleanups()
		path = self.java.joinpath(*BASE) / relative
		original = path.read_bytes()
		text = original.decode("utf-8").replace("\r\n", "\n")
		self.assertIn(old, text, f"{relative} no longer contains the text this case changes")
		path.write_bytes(text.replace(old, new, 1).encode("utf-8"))
		self.addCleanup(path.write_bytes, original)
		return self.java

	def test_a_changed_formula_is_refused(self):
		cases = [
			("services/trade/PricesService.java", "return (long) (kinahValue * sellModifier / 100D);", "return (long) (kinahValue * sellModifier / 50D);"),
			("services/trade/PricesService.java", "return Math.round(defaultTax + ((diff / 4) * 100));", "return Math.round(defaultTax + ((diff / 2) * 100));"),
			("services/trade/PricesService.java", "(requiredKinah * getVendorBuyModifier() / 100D) * getGlobalPrices(playerRace) / 100D)",
			 "(requiredKinah * getVendorBuyModifier() / 100D) * getTaxes(playerRace) / 100D)"),
			("model/trade/TradeList.java", "* tradeItem.getCount() * modifier / 100;", "* tradeItem.getCount() * modifier / 101;"),
			("services/TradeService.java", "? template.getSellPriceRate2() : template.getSellPriceRate();",
			 "? template.getSellPriceRate() : template.getSellPriceRate2();"),
			("services/player/PlayerLimitService.java", "possibleCount += 1;", "possibleCount += 2;"),
			("network/aion/serverpackets/SM_TRADELIST.java", "writeD(100);", "writeD(0);"),
			("model/gameobjects/Npc.java", "supportsAction(DialogAction.SELL) || canSell()", "supportsAction(DialogAction.SELL) && canSell()"),
			("services/SiegeService.java", "locations = Collections.emptyMap();", "locations = DataManager.SIEGE_LOCATION_DATA.getSiegeLocations();"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				JavaTradeRules.read(self.changed(relative, old, new))

	def test_an_edit_anywhere_in_a_modelled_member_is_refused(self):
		# statements inserted before, after or between the modelled statements, which a statement check alone does not see (the review's edits)
		cases = [
			("services/TradeService.java", "count = PlayerLimitService.updateSellLimit(player, sellReward, count);",
			 "sellReward = sellReward * 2; count = PlayerLimitService.updateSellLimit(player, sellReward, count);"),
			("model/trade/TradeList.java", "return availableKinah >= requiredKinah;", "requiredKinah = requiredKinah * 2; return availableKinah >= requiredKinah;"),
			("services/TradeService.java", "long tradeListPrice = tradeList.getRequiredKinah();", "long tradeListPrice = tradeList.getRequiredKinah() + 100;"),
			("services/TradeService.java", "GoodsList goodsList = goodsListData.getGoodsListById(tradeTab.getId());",
			 "GoodsList goodsList = goodsListData.getGoodsListById(tradeTab.getId()); if (goodsList != null && goodsList.getLegionLevel() > 0) continue;"),
			("services/TradeService.java", "allowedItems.addAll(goodsList.getItemIdList());", "allowedItems.addAll(goodsList.getItemIdList()); break;"),
			("services/DialogService.java", "PacketSendUtility.sendPacket(player, new SM_SELL_ITEM(npc));",
			 "if (npc.canBuy()) PacketSendUtility.sendPacket(player, new SM_SELL_ITEM(npc));"),
			("controllers/NpcController.java", "if (!PositionUtil.isInTalkRange(player, getOwner()))\n\t\t\treturn;",
			 "if (!PositionUtil.isInTalkRange(player, getOwner()))\n\t\t\treturn;\n\t\tif (player.isInTeam())\n\t\t\treturn;"),
		]
		for relative, old, new in cases:
			with self.subTest(file=relative, new=new), self.assertRaises(OracleError):
				JavaTradeRules.read(self.changed(relative, old, new))

	def test_a_comment_or_a_line_break_is_no_change(self):
		java = self.changed("services/trade/PricesService.java", "return (long) (kinahValue * sellModifier / 100D);",
		                    "return (long) (kinahValue // the value\n\t\t\t* sellModifier /* the rate */ / 100D);")
		self.assertEqual(JavaTradeRules.read(java).max_count, 20000)

	def test_literals_are_read(self):
		rules = JavaTradeRules.read(self.changed("model/templates/tradelist/TradeListTemplate.java", "private int sellPriceRate = 100;",
		                                         "private int sellPriceRate = 90;"))
		self.assertEqual(rules.tradelist_defaults["sell_price_rate"], 90)
		rules = JavaTradeRules.read(self.changed("model/SellLimit.java", "LIMIT_1_30(1, 30, 5300047)", "LIMIT_1_30(1, 30, 6000000)"))
		self.assertEqual(rules.sell_limits[0], (1, 30, 6000000))
		java = self.changed("configs/main/PricesConfig.java", 'key = "gameserver.prices.vendor.sellmod", defaultValue = "20"',
		                    'key = "gameserver.prices.vendor.sellmod", defaultValue = "25"')
		self.assertEqual(property_defaults(java)["gameserver.prices.vendor.sellmod"], "25")
		java = self.changed("configs/main/PricesConfig.java", "public static int VENDOR_SELL_MODIFIER;", "public static long VENDOR_SELL_MODIFIER;")
		with self.assertRaises(OracleError):
			property_defaults(java)


NPCS = (
	'<npc_template npc_id="900001" level="9" name="fixture grocer" race="ELYOS" type="GENERAL"><stats maxHp="10"/>'
	'<talk_info distance="5" func_dialogs="2 3"/></npc_template>'
	'<npc_template npc_id="900002" level="9" name="fixture buyer"><stats maxHp="10"/><talk_info func_dialogs="3"/></npc_template>'
	'<npc_template npc_id="900003" level="9" name="fixture silent"><stats maxHp="10"/><talk_info distance="5"/></npc_template>'
	'<npc_template npc_id="900004" level="9" name="fixture purchaser"><stats maxHp="10"/><talk_info func_dialogs="103"/></npc_template>'
	'<npc_template npc_id="900005" level="9" name="fixture ap purchaser"><stats maxHp="10"/><talk_info func_dialogs="103"/></npc_template>'
	'<npc_template npc_id="900006" level="9" name="fixture abyss kinah"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900007" level="9" name="fixture legion coin"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900008" level="9" name="fixture reward"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900009" level="9" name="fixture broken"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900010" level="9" name="fixture guard"><stats maxHp="10"/></npc_template>'
	'<npc_template npc_id="900011" level="9" name="fixture sell purchaser"><stats maxHp="10"/><talk_info func_dialogs="3"/></npc_template>'
	'<npc_template npc_id="900012" level="9" name="fixture legion shop"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
	'<npc_template npc_id="900013" level="9" name="fixture twice"><stats maxHp="10"/><talk_info func_dialogs="2 3"/></npc_template>'
	'<npc_template npc_id="900014" level="9" name="fixture butler" ai="butler"><stats maxHp="10"/><talk_info func_dialogs="2 3"/></npc_template>'
	'<npc_template npc_id="900015" level="9" name="fixture ap purchaser 2"><stats maxHp="10"/><talk_info func_dialogs="103"/></npc_template>'
	'<npc_template npc_id="900016" level="9" name="fixture empty shop"><stats maxHp="10"/><talk_info func_dialogs="2"/></npc_template>'
)

# masks: 12414 has TRADEABLE (2) and SELLABLE (4), 12360 has neither (the starter juice)
ITEMS = (
	'<item_template id="100" name="fixture elixir" price="250" mask="12414"/>'
	'<item_template id="101" name="fixture juice" price="5" mask="12360"/>'
	'<item_template id="102" name="fixture ap blade" price="1000" mask="12414"><acquisition type="AP" ap="3"/></item_template>'
	'<item_template id="103" name="fixture medal armor" price="40" mask="12414"><acquisition type="REWARD" item="104" count="0"/></item_template>'
	'<item_template id="104" name="fixture medal" price="3" mask="12414"/>'
	'<item_template id="105" name="fixture relic" price="7" mask="12360"><acquisition type="ABYSS" ap="7" item="104" count="2"/></item_template>'
	'<item_template id="106" name="fixture cleaned" price="3" mask="12414"/>'
	'<item_template id="107" name="fixture untyped" price="3" mask="12414"><acquisition ap="3"/></item_template>'
	'<item_template id="108" name="fixture limited tonic" price="10" mask="12414"/>'
	'<item_template id="109" name="fixture negative ap" price="10" mask="12414"><acquisition type="AP" ap="-5"/></item_template>'
	'<item_template id="110" name="fixture reward coat" price="10" mask="12414"><acquisition type="REWARD" ap="9" item="104" count="1"/></item_template>'
)

GOODS = (
	'<list id="1"><item id="100"/><item id="101"/><item id="106"/></list>'
	'<list id="2" legion_lvl="2"><item id="102"/></list>'
	'<list id="3"><salestime>0 0 10 ? * *</salestime><item id="100"/><item id="103" buy_limit="0" sell_limit="5"/>'
	'<item id="105" buy_limit="3" sell_limit="10"/><item id="104" sell_limit="7"/></list>'
	'<list id="4"><salestime>0 0 0 ? * *</salestime><item id="102" buy_limit="2" sell_limit="0"/></list>'
	'<list id="5"><item id="107"/></list>'
	'<list id="6" legion_lvl="3"><salestime>0 0 12 ? * MON</salestime><item id="108" buy_limit="0" sell_limit="4"/></list>'
	'<list id="7"><item id="100"/></list>'
	'<list id="7"><item id="104"/></list>'
	'<list id="8"><item id="109"/><item id="110"/></list>'
	'<in_list id="1"/>'
	'<purchase_list id="1"><item id="100"/><item id="101"/></purchase_list>'
	'<purchase_list id="2"><item id="105"/><item id="102"/><item id="104"/><item id="107"/></purchase_list>'
)
# goodslists_europe.xml: what gameserver.country.code = 2 loads instead (XmlMerger.applyCountryOverride): list 1 without the juice
GOODS_EUROPE = GOODS.replace('<list id="1"><item id="100"/><item id="101"/><item id="106"/></list>', '<list id="1"><item id="100"/><item id="106"/></list>')

TRADE_LISTS = (
	'<tradelist_template npc_id="900001" buy_price_rate="200"><tradelist id="1"/><tradelist id="2"/><tradelist id="99"/><tradelist id="3"/>'
	'</tradelist_template>'
	'<tradelist_template npc_id="900002"><tradelist id="1"/></tradelist_template>'
	'<tradelist_template npc_id="900003"><tradelist id="1"/></tradelist_template>'
	'<tradelist_template npc_id="900006" npc_type="ABYSS_KINAH" sell_price_rate="150" sell_price_rate2="200" ap_sell_price_rate2="50">'
	'<tradelist id="1"/><tradelist id="2"/></tradelist_template>'
	'<tradelist_template npc_id="900007" npc_type="LEGION_COIN"><tradelist id="1"/></tradelist_template>'
	'<tradelist_template npc_id="900008" npc_type="REWARD" sell_price_rate="50"><tradelist id="3"/><tradelist id="4"/></tradelist_template>'
	'<tradelist_template npc_id="900009"><tradelist id="5"/></tradelist_template>'
	'<tradelist_template npc_id="900012"><tradelist id="6"/></tradelist_template>'
	'<tradelist_template npc_id="900013"><tradelist id="1"/></tradelist_template>'
	'<tradelist_template npc_id="900013"><tradelist id="7"/><tradelist id="8"/></tradelist_template>'
	'<tradelist_template npc_id="900014"><tradelist id="1"/></tradelist_template>'
	'<trade_in_list_template npc_id="900001"><tradelist id="1"/></trade_in_list_template>'
	'<purchase_template npc_id="900004" buy_price_rate="40"><tradelist id="1"/></purchase_template>'
	'<purchase_template npc_id="900005" buy_price_rate="20" npc_type="ABYSS"><tradelist id="2"/></purchase_template>'
	'<purchase_template npc_id="900011" buy_price_rate="30"><tradelist id="1"/></purchase_template>'
	'<purchase_template npc_id="900015" buy_price_rate="20" npc_type="ABYSS"><tradelist id="98"/><tradelist id="2"/></purchase_template>'
)

# 106: sell="0" clears SELLABLE, trade="2" leaves TRADEABLE set; 101: trade="2" leaves TRADEABLE cleared; 999 has no template, but no result
# 0 or 1 either, which ItemData.applyCleanup ignores
CLEANUPS = '<cleanup id="106" sell="0" trade="2"/><cleanup id="101" trade="2"/><cleanup id="999" wh="5" awh="-1"/>'

SPAWNS = """
<spawn_map map_id="1">
	<spawn npc_id="900001"><spot x="10" y="20" z="30" h="1"/></spawn>
	<spawn npc_id="900002" pool="1"><spot x="11" y="20" z="30" h="1"/><spot x="12" y="20" z="30" h="1"/></spawn>
	<spawn npc_id="900003"><spot x="13" y="20" z="30" h="1" static_id="7"/></spawn>
	<spawn npc_id="900004" difficult_id="1"><spot x="14" y="20" z="30" h="1"/></spawn>
	<spawn npc_id="900010"><spot x="15" y="20" z="30" h="1"/></spawn>
	<spawn npc_id="900014"><spot x="16" y="20" z="30" h="1" ai="__NO_AI__"/><spot x="17" y="20" z="30" h="1"/></spawn>
</spawn_map>
"""

# a data set in which no npc lists BUY (2) among its func_dialogs: NpcData.isFunctionDialog(2) is false and CM_DIALOG_SELECT lets BUY through
NPCS_WITHOUT_BUY = (
	'<npc_template npc_id="900001" level="9" name="fixture seller"><stats maxHp="10"/><talk_info func_dialogs="3"/></npc_template>'
	'<npc_template npc_id="900004" level="9" name="fixture purchaser"><stats maxHp="10"/><talk_info func_dialogs="103"/></npc_template>'
)
TRADE_LISTS_WITHOUT_BUY = (
	'<tradelist_template npc_id="900001"><tradelist id="1"/></tradelist_template>'
	'<tradelist_template npc_id="900004"><tradelist id="1"/></tradelist_template>'
	'<trade_in_list_template npc_id="900001"><tradelist id="1"/></trade_in_list_template>'
	'<purchase_template npc_id="900004" buy_price_rate="40"><tradelist id="1"/></purchase_template>'
)


def fixture_tree(events="", **holders) -> Tree:
	tree = Tree()
	tree.minimal({
		"npc_templates": NPCS,
		"item_templates": ITEMS,
		"goodslists": GOODS,
		"npc_trade_list": TRADE_LISTS,
		"item_restriction_cleanups": CLEANUPS,
		"spawns": SPAWNS,
		"timed_events": events,
		**holders,
	})
	tree.write("goodslists_europe.xml", xml("goodslists", GOODS_EUROPE))
	return tree


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cTradeConfigTest(unittest.TestCase):
	"""load_config over small config directories: Config.loadProperties' order, the @Property defaults, the refusals."""

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.addCleanup(self.tmp.cleanup)
		self.config = Path(self.tmp.name) / "config"
		for folder in ("administration", "main", "network"):
			(self.config / folder).mkdir(parents=True)

	def write(self, relative: str, text: str) -> Path:
		path = self.config / relative
		path.write_bytes(text.encode("iso-8859-1"))
		return path

	def test_defaults_and_their_sources(self):
		config = load_config(JAVA_SRC, self.config, self.config / "mygs.properties")
		self.assertEqual({k: v.value for k, v in config.items()}, DEFAULT_VALUES)
		self.assertEqual(config["gameserver.prices.vendor.sellmod"].source, "PricesConfig.java @Property defaultValue")
		self.assertEqual(config["gameserver.country.code"].source, "GSConfig.java @Property defaultValue")

	def test_default_folder_then_profile_then_set(self):
		self.write("main/prices.properties", "gameserver.prices.vendor.sellmod = 25\n")
		config = load_config(JAVA_SRC, self.config, self.config / "mygs.properties")
		self.assertEqual((config["gameserver.prices.vendor.sellmod"].value, config["gameserver.prices.vendor.sellmod"].source),
		                 (25, "config/main/prices.properties"))
		profile = self.write("mygs.properties", "gameserver.prices.vendor.sellmod = 30\ngameserver.siege.enable = FALSE\n")
		config = load_config(JAVA_SRC, self.config, profile)
		self.assertEqual((config["gameserver.prices.vendor.sellmod"].value, config["gameserver.siege.enable"].value), (30, False))
		config = load_config(JAVA_SRC, self.config, profile, ["gameserver.prices.vendor.sellmod=35"])
		self.assertEqual((config["gameserver.prices.vendor.sellmod"].value, config["gameserver.prices.vendor.sellmod"].source), (35, "--set"))
		config = load_config(JAVA_SRC, self.config, None)
		self.assertEqual(config["gameserver.prices.vendor.sellmod"].value, 25, "without a profile the default folders still count")

	def test_set_is_read_as_a_profile_line(self):
		for text, value in (("gameserver.prices.vendor.sellmod = 25", 25), ("gameserver.prices.vendor.sellmod:30", 30),
		                    ("gameserver.prices.vendor.sellmod 31", 31), ("  gameserver.prices.vendor.sellmod=\\u0033\\u0032", 32)):
			with self.subTest(text=text):
				self.assertEqual(load_config(JAVA_SRC, None, None, [text])["gameserver.prices.vendor.sellmod"].value, value)
		# a profile line keeps its trailing white space, which Integer.decode rejects; a comment is no property
		for text in ("gameserver.prices.vendor.sellmod=25 ", "gameserver.prices.vendor.sellmod=25\t", "# gameserver.prices.vendor.sellmod=25", "=25"):
			with self.subTest(text=text), self.assertRaises(OracleError):
				load_config(JAVA_SRC, None, None, [text])

	def test_an_explicit_profile_must_exist(self):
		missing = self.config / "gate.properties"
		self.assertEqual(load_config(JAVA_SRC, self.config, missing)["gameserver.siege.enable"].value, True, "the default mygs.properties may be missing")
		with self.assertRaises(OracleError):
			load_config(JAVA_SRC, self.config, missing, require_profile=True)
		self.write("gate.properties", "gameserver.siege.enable = false\n")
		self.assertEqual(load_config(JAVA_SRC, self.config, missing, require_profile=True)["gameserver.siege.enable"].value, False)

	def test_placeholders_and_the_empty_string(self):
		self.write("main/prices.properties", "x = 40\ngameserver.prices.vendor.sellmod = ${x}\ngameserver.rates.sell_limit = \"\" \n")
		config = load_config(JAVA_SRC, self.config, None)
		self.assertEqual(config["gameserver.prices.vendor.sellmod"].value, 40)
		self.assertEqual(config["gameserver.rates.sell_limit"].value, [],
		                 '"" (trimmed: the trailing space of the line stays in the value) is the empty string: a float[] of length 0 (Rates.get answers 1)')

	def test_refusals(self):
		self.write("main/a.properties", "gameserver.prices.vendor.sellmod = 25\n")
		self.write("main/b.properties", "gameserver.prices.vendor.sellmod = 26\n")
		with self.assertRaises(OracleError):  # which file wins is the file system order of Files.find
			load_config(JAVA_SRC, self.config, None)
		self.write("main/b.properties", "gameserver.prices.vendor.sellmod = 25\n")
		self.assertEqual(load_config(JAVA_SRC, self.config, None)["gameserver.prices.vendor.sellmod"].value, 25, "the same value twice is no conflict")
		(self.config / "main" / "a.properties").unlink()
		for text in ("gameserver.prices.vendor.sellmod = 25 \n", "gameserver.prices.vendor.sellmod = 0x19\n", "gameserver.siege.enable = yes\n",
		             "gameserver.rates.sell_limit = 1.0 \n"):
			self.write("main/b.properties", text)
			with self.subTest(text=text), self.assertRaises(OracleError):
				load_config(JAVA_SRC, self.config, None)
		(self.config / "main" / "b.properties").unlink()
		(self.config / "network").rmdir()
		with self.assertRaises(OracleError):  # Config.loadProperties: Files.find on a missing default folder throws
			load_config(JAVA_SRC, self.config, None)

	def test_an_event_that_sets_a_key_is_refused(self):
		event = ('<event name="fixture sale"><config_properties><property>gameserver.prices.vendor.sellmod = 50</property></config_properties></event>'
		         '<event name="fixture gather"><config_properties><property>gameserver.rates.gathering.count = 4</property></config_properties></event>')
		with fixture_tree(event) as tree:
			with self.assertRaises(OracleError):
				load_config(JAVA_SRC, self.config, None, GATE_PROFILE, StaticData(tree.root))
		with fixture_tree(event.split("</event>")[1] + "</event>") as tree:
			self.assertEqual(load_config(JAVA_SRC, self.config, None, GATE_PROFILE, StaticData(tree.root))["gameserver.prices.vendor.sellmod"].value, 20,
			                 "an event that sets another key does not matter")


def goods(report: dict) -> dict[int, dict]:
	return {g["itemId"]: g for g in report["buy"]["goods"]}


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cTradeFixtureReportTest(unittest.TestCase):
	"""The whole report on a small static_data tree (the Java tables and the AI classes still come from the Java sources)."""

	@classmethod
	def setUpClass(cls):
		cls.tree = fixture_tree()
		cls.data = StaticData(cls.tree.root)
		cls.rules = JavaTradeRules.read(JAVA_SRC)
		cls.trade = TradeData(cls.data, cls.rules)
		cls.config = load_config(JAVA_SRC, None, None, GATE_PROFILE, cls.data)
		cls.ai = DialogAi(JAVA_SRC, HANDLERS)

	@classmethod
	def tearDownClass(cls):
		cls.tree.close()

	def report(self, config=None, **kwargs):
		return trade_report(self.data, JAVA_SRC, config or self.config, trade=self.trade, dialog_ai=self.ai, **kwargs)

	def config_with(self, *keys):
		return load_config(JAVA_SRC, None, None, GATE_PROFILE + list(keys), self.data)

	def test_prices_and_the_normal_vendor(self):
		report = self.report(npc_id=900001)
		self.assertEqual(report["prices"]["ELYOS"]["smPrices"], [125, 100, 113])
		self.assertEqual(report["npc"]["canSell"], True)
		self.assertEqual((report["npc"]["canBuy"], report["npc"]["canPurchase"], report["npc"]["canTradeIn"]), (True, False, False),
		                 "a trade-in list without the TRADE_IN function is no trade-in")
		self.assertEqual((report["npc"]["talkDistance"], report["npc"]["talkRange"]), (5, 6), "PositionUtil.isInTalkRange: getTalkDistance() + 1")
		self.assertEqual(report["tradeIn"]["tabs"], [1])
		by_id = goods(report)
		self.assertEqual(list(by_id), [100, 101, 106, 102, 103, 105, 104], "tab order, each item once; the missing list 99 adds nothing")
		self.assertEqual({i: g["kinah"]["ELYOS"] for i, g in by_id.items()}, {100: 352, 101: 6, 106: 3, 102: 1412, 103: 56, 105: 9, 104: 3})
		self.assertEqual(by_id[100]["tabs"], [1, 3])
		self.assertEqual((by_id[102]["shown"], by_id[102]["buyable"], by_id[102]["requiredAp"]), (False, True, 3),
		                 "legion level 2 hides the tab, but validateBuyItems takes every tab; 3 AP * 100 / 100.0 * 100 / 100")
		self.assertEqual(by_id[103]["failure"]["message"], "STR_MSG_NOT_ENOUGH_ABYSSPOINT", "an acquisition count of 0 needs 0 medals: < 1 fails")
		self.assertEqual((by_id[105]["requiredAp"], by_id[105]["requiredItems"]), (7, [{"itemId": 104, "count": 2}]))
		self.assertIsNone(by_id[104]["limited"], "only sell_limit: not a limited item")

	def test_the_windows(self):
		report = self.report(npc_id=900001)
		self.assertEqual((report["buy"]["dialog"], report["buy"]["dialogPath"]["outcome"]), ("SM_TRADELIST", "DialogService"))
		self.assertEqual(report["buy"]["smTradeList"], {
			"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceModifier": 100, "constant": 100, "showBuyTab": True, "showSellTab": True, "tabs": [1, 3],
			"limitedItems": [{"itemId": 103, "buyCount": 0, "sellLimit": 5}, {"itemId": 105, "buyCount": 0, "sellLimit": 10}]})
		self.assertEqual(self.report(npc_id=900001, legion_level=2)["buy"]["smTradeList"]["tabs"], [1, 2, 3])
		self.assertEqual(report["sell"]["smSellItem"], {"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceRate": 20, "showBuyTab": True,
		                                                "showSellTab": True, "tabs": []})
		self.assertEqual(report["sell"]["dialogs"], {"SELL": "SM_SELL_ITEM", "TRADE_SELL_LIST": AUDIT_NO_PACKET})
		# CM_DIALOG_SELECT.java:112-116: BUY (2) is a function dialog of the data and 900002 does not support it - an audit, no packet
		buyer = self.report(npc_id=900002)
		self.assertEqual((buyer["buy"]["dialog"], buyer["buy"]["smTradeList"], buyer["buy"]["dialogPath"]["functionDialog"]),
		                 (AUDIT_NO_PACKET, None, True))
		self.assertEqual(goods(buyer)[100]["failure"]["reason"], "npc.canSell() is false: CM_BUY_ITEM.java:130 ignores the buy")
		self.assertEqual(buyer["sell"]["reachableBy"], ["SELL"])
		self.assertEqual(self.report(npc_id=900010)["buy"]["dialog"], AUDIT_NO_PACKET, "no func_dialogs at all")
		self.assertEqual(self.report(npc_id=900016)["buy"]["dialog"], "STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM", "BUY supported, no trade list")

	def test_a_sale_without_a_sell_window(self):
		# 900006 supports BUY only: SELL (3) and TRADE_SELL_LIST (103) are function dialogs it does not support, so SM_SELL_ITEM never opens,
		# but canBuy() is canSell() too and a CM_BUY_ITEM sale (from SM_TRADELIST's sell tab) is paid
		report = self.report(npc_id=900006, item_id=100)
		self.assertEqual((report["sell"]["reachable"], report["sell"]["smSellItem"]), (False, None))
		self.assertEqual(report["sell"]["dialogs"], {"SELL": AUDIT_NO_PACKET, "TRADE_SELL_LIST": AUDIT_NO_PACKET})
		self.assertEqual((report["npc"]["canBuy"], report["buy"]["smTradeList"]["showSellTab"]), (True, True))
		sale = report["item"]["sell"]
		self.assertEqual((sale["accepted"], sale["arm"], sale["unitReward"]), (True, "VENDOR", 50))

	def test_an_ai_that_answers_the_dialog(self):
		# ButlerAI overrides onDialogSelect: NpcController asks it before DialogService, which the oracle does not model
		report = self.report(npc_id=900014)
		self.assertEqual((report["buy"]["dialog"], report["buy"]["smTradeList"]), (None, None))
		self.assertIn("ButlerAI", report["buy"]["dialogPath"]["notModelled"])
		self.assertEqual(report["buy"]["dialogPath"]["ai"]["classChain"], ["ButlerAI", "GeneralNpcAI", "NpcAI", "AITemplate"])
		self.assertEqual((report["sell"]["reachable"], report["sell"]["smSellItem"]), (None, None))
		self.assertTrue(goods(report)[100]["buyable"], "CM_BUY_ITEM does not ask the AI")
		plain = self.report(npc_id=900001)["buy"]["dialogPath"]["ai"]
		self.assertEqual((plain["classChain"], plain["overridesOnDialogSelect"]), (["DummyAI", "AITemplate"], []), "no ai: AIEngine.newAI(null)")

	def test_a_dialog_no_npc_lists_as_a_function_passes_the_gate(self):
		with fixture_tree(npc_templates=NPCS_WITHOUT_BUY, npc_trade_list=TRADE_LISTS_WITHOUT_BUY) as tree:
			data = StaticData(tree.root)
			seller = trade_report(data, JAVA_SRC, self.config, npc_id=900001, dialog_ai=self.ai)
			self.assertEqual((seller["buy"]["dialogPath"]["functionDialog"], seller["buy"]["dialog"]), (False, "SM_TRADELIST"))
			self.assertEqual((seller["buy"]["smTradeList"]["showBuyTab"], seller["buy"]["smTradeList"]["showSellTab"]), (False, True),
			                 "canSell() needs BUY; canBuy() is SELL")
			purchaser = trade_report(data, JAVA_SRC, self.config, npc_id=900004, dialog_ai=self.ai)
			self.assertEqual((purchaser["npc"]["canPurchase"], purchaser["buy"]["smTradeList"]["showSellTab"]), (True, False),
			                 "SM_TRADELIST's sell tab is canBuy() alone")
			self.assertEqual((purchaser["sell"]["dialogs"], purchaser["sell"]["smSellItem"]["showSellTab"]),
			                 ({"SELL": AUDIT_NO_PACKET, "TRADE_SELL_LIST": "SM_SELL_ITEM"}, True), "SM_SELL_ITEM's is canBuy() || canPurchase()")

	def test_a_window_whose_tabs_the_legion_level_hides(self):
		shop = self.report(npc_id=900012)
		self.assertEqual((shop["buy"]["dialog"], shop["buy"]["smTradeList"]), ("STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM", None),
		                 "hasAnythingToSell is false: list 6 needs legion level 3")
		self.assertEqual(goods(shop)[108]["limited"]["sellLimit"], 4, "LimitedItemTradeService.start takes a legion level list's limited items too")
		legion = self.report(npc_id=900012, legion_level=3)["buy"]
		self.assertEqual((legion["dialog"], legion["smTradeList"]["tabs"], legion["smTradeList"]["limitedItems"]),
		                 ("SM_TRADELIST", [6], [{"itemId": 108, "buyCount": 0, "sellLimit": 4}]))

	def test_limited_item_boundaries(self):
		# canBuyLimitItem on a fresh server: sellLimit - count < 0 fails, 0 + count > buyLimit fails, and a limit of 0 limits nothing
		self.assertTrue(goods(self.report(npc_id=900012, count=4))[108]["buyable"], "4 of a sell limit of 4 leaves 0")
		self.assertEqual(goods(self.report(npc_id=900012, count=5))[108]["failure"]["message"], "STR_MSG_LIMITED_BUYING_CANT_SELECT_NO_ITEMS")
		self.assertTrue(goods(self.report(npc_id=900001, count=3))[105]["buyable"], "3 of a buy limit of 3")
		self.assertTrue(goods(self.report(npc_id=900008, count=1))[102]["buyable"], "sell_limit 0: getDefaultSellLimit() > 0 is false")
		self.assertTrue(goods(self.report(npc_id=900008, count=2))[102]["buyable"], "2 of a buy limit of 2")

	def test_duplicates_and_odd_acquisitions(self):
		report = self.report(npc_id=900013)
		self.assertEqual([t["id"] for t in report["buy"]["tradeList"]["tabs"]], [7, 8], "the later tradelist_template of the npc replaces the earlier")
		self.assertEqual(list(goods(report)), [104, 109, 110], "the later <list id=7> replaces the earlier")
		negative = goods(report)[109]
		self.assertEqual((negative["requiredAp"], negative["failure"]), (-5, {"reason": "TradeService.java:111: getRequiredAp() < 0",
		                                                                      "message": "STR_MSG_NOT_ENOUGH_ABYSSPOINT"}))
		reward = goods(report)[110]
		self.assertEqual((reward["requiredAp"], reward["requiredItems"], reward["buyable"]), (0, [{"itemId": 104, "count": 1}], True),
		                 "a REWARD acquisition's ap is not charged: only AP and ABYSS are")

	def test_selling_to_a_vendor(self):
		sell = {i: g["sellBack"] for i, g in goods(self.report(npc_id=900001, count=3)).items()}
		self.assertEqual((sell[100]["unitReward"], sell[100]["kinah"], sell[100]["repurchasePrice"]), (50, 150, 150))
		self.assertEqual(sell[101]["failure"]["message"], "STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC")
		self.assertEqual(sell[106]["failure"]["message"], "STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC", "the cleanup's sell=\"0\" clears SELLABLE")
		self.assertEqual((sell[104]["accepted"], sell[104]["unitReward"], sell[104]["kinah"]), (True, 0, 0), "3 * 20 / 100 truncates to 0")
		item = self.report(npc_id=900001, item_id=106)["item"]
		self.assertEqual((item["templateMask"], item["mask"], item["sellable"]), (12414, 12410, False), "trade=\"2\" changes nothing")
		juice = self.report(npc_id=900001, item_id=101)["item"]
		self.assertEqual((juice["templateMask"], juice["mask"], juice["cleanup"]["trade"]), (12360, 12360, 2), "trade=\"2\" does not set a cleared bit")
		silent = self.report(npc_id=900003, item_id=100)["item"]["sell"]
		self.assertEqual((silent["accepted"], silent["arm"]), (False, None), "no SELL function and no BUY: CM_BUY_ITEM does nothing")

	def test_selling_to_a_purchase_template(self):
		sale = self.report(npc_id=900004, item_id=100, count=3)["item"]["sell"]
		self.assertEqual((sale["arm"], sale["unitReward"], sale["kinah"]), ("PURCHASE", 100, 300), "250 * 40 / 100")
		sale = self.report(npc_id=900004, item_id=101)["item"]["sell"]
		self.assertEqual((sale["accepted"], sale["sellable"], sale["unitReward"]), (True, False, 2), "the purchase arm does not ask isSellable")
		self.assertEqual(self.report(npc_id=900004, item_id=102)["item"]["sell"]["failure"]["message"], None, "not in the lists: no message")
		report = self.report(npc_id=900004)
		self.assertEqual(report["sell"]["smSellItem"], {"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceRate": 40, "showBuyTab": False,
		                                                "showSellTab": True, "tabs": [1]})
		self.assertEqual(self.report(npc_id=900005, item_id=102, count=3)["item"]["sell"]["ap"], 3, "Math.round(60 / 100F) = 1, times 3")
		self.assertEqual(self.report(npc_id=900005, item_id=105)["item"]["sell"]["ap"], 1, "Math.round(1.4f)")
		with self.assertRaises(OracleError):  # 104 has no <acquisition>: NullPointerException in performSellForAPToShop
			self.report(npc_id=900005, item_id=104)
		disabled = self.config_with("gameserver.selling.apitems.enabled=false")
		self.assertFalse(self.report(disabled, npc_id=900005, item_id=102)["item"]["sell"]["accepted"])

	def test_a_purchase_template_without_its_function(self):
		# 900011 supports SELL but not TRADE_SELL_LIST: canPurchase() is false, canBuy() is true, and CM_BUY_ITEM still passes the npc's
		# purchase template to performSellToShop (CM_BUY_ITEM.java:115-119)
		report = self.report(npc_id=900011, item_id=100, count=2)
		self.assertEqual((report["npc"]["canPurchase"], report["npc"]["canBuy"]), (False, True))
		self.assertEqual((report["item"]["sell"]["arm"], report["item"]["sell"]["unitReward"], report["item"]["sell"]["kinah"]), ("PURCHASE", 75, 150))
		self.assertEqual(report["sell"]["smSellItem"], {"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceRate": 30, "showBuyTab": False,
		                                                "showSellTab": True, "tabs": [1]})

	def test_an_ap_purchase_template(self):
		with self.assertRaises(OracleError):  # list 98 does not exist and is read before list 2: NullPointerException in performSellForAPToShop
			self.report(npc_id=900015, item_id=102)
		# performSellForAPToShop asks gameserver.selling.apitems.enabled before it reads a list (TradeService.java:252-255)
		sale = self.report(self.config_with("gameserver.selling.apitems.enabled=false"), npc_id=900015, item_id=102)["item"]["sell"]
		self.assertEqual((sale["accepted"], sale["failure"]["reason"]), (False, 'gameserver.selling.apitems.enabled is false: "This feature is disabled"'))
		self.assertEqual(self.report(npc_id=900005, item_id=107)["item"]["sell"]["ap"], 1,
		                 "an <acquisition> without a type: the AP sale reads only getRequiredAp(), Math.round(0.6f) = 1")

	def test_the_other_vendor_types(self):
		abyss_kinah = self.report(npc_id=900006)
		self.assertEqual(abyss_kinah["buy"]["smTradeList"]["buyPriceModifier"], 150, "the window shows sell_price_rate ...")
		self.assertEqual({i: g["kinah"]["ELYOS"] for i, g in goods(abyss_kinah).items()}, {100: 704, 101: 12, 106: 6, 102: 2824},
		                 "... but the purchase charges sell_price_rate2 (200)")
		self.assertEqual(goods(abyss_kinah)[102]["requiredAp"], 1, "ap_sell_price_rate2 50: 1.5 * 100 = 150, / 100 = 1")
		self.assertEqual(self.report(npc_id=900006, item_id=101, count=3)["item"]["buy"]["kinah"]["ELYOS"], 36)
		legion = self.report(npc_id=900007)
		self.assertEqual((legion["buy"]["smTradeList"]["npcTypeIndex"], goods(legion)[100]["failure"]["reason"]),
		                 (3, "TradeService.performBuyFromShop: unhandled TradeNpcType LEGION_COIN"))
		reward = self.report(npc_id=900008)
		by_id = goods(reward)
		self.assertEqual({i: g["kinah"]["ELYOS"] for i, g in by_id.items()}, {100: 0, 103: 0, 105: 0, 104: 0, 102: 0}, "REWARD pays no kinah")
		self.assertEqual((by_id[105]["requiredAp"], by_id[102]["requiredAp"]), (3, 1), "the AP modifier is sell_price_rate (50)")
		self.assertEqual(by_id[102]["limited"]["salesTime"], "0 0 0 ? * *")
		self.assertEqual(reward["buy"]["smTradeList"]["limitedItems"], [{"itemId": 103, "buyCount": 0, "sellLimit": 5},
		                                                                {"itemId": 105, "buyCount": 0, "sellLimit": 10},
		                                                                {"itemId": 102, "buyCount": 0, "sellLimit": 0}])
		limited = goods(self.report(npc_id=900008, count=3))[102]["failure"]
		self.assertEqual(limited["message"], "STR_MSG_LIMITED_BUYING_CANT_SELECT_NO_ITEMS", "buy_limit 2, count 3")
		limited = goods(self.report(npc_id=900001, count=4))[105]
		self.assertEqual((limited["failure"]["message"], limited["requiredAp"], limited["requiredItems"]),
		                 ("STR_MSG_LIMITED_BUYING_CANT_SELECT_NO_ITEMS", 28, [{"itemId": 104, "count": 8}]))
		with self.assertRaises(OracleError):  # <acquisition> without a type: NullPointerException in calculateAbyssRewardBuyList
			self.report(npc_id=900009)

	def test_the_vendor_modifiers(self):
		config = self.config_with("gameserver.prices.vendor.sellmod=25", "gameserver.prices.vendor.buymod=150")
		report = self.report(config, npc_id=900001, item_id=100)
		self.assertEqual((report["item"]["unitPrice"]["ELYOS"], report["buy"]["smTradeList"]["buyPriceModifier"]), (528, 150))
		self.assertEqual((report["item"]["sell"]["unitReward"], report["sell"]["smSellItem"]["buyPriceRate"]), (62, 25), "250 * 25 / 100 = 62.5")
		self.assertEqual(goods(report)[102]["requiredAp"], 4, "(int) (3.0 * 150) / 100")
		self.assertEqual(self.report(config, npc_id=900006)["buy"]["smTradeList"]["buyPriceModifier"], 225, "150 * sell_price_rate 150 / 100")
		prices = self.report(self.config_with("gameserver.prices.default.prices=300"), npc_id=900001)["prices"]["ELYOS"]
		self.assertEqual((prices["smPrices"], prices["smPricesBytes"]), ([325, 100, 113], [69, 100, 113]), "SM_PRICES' writeC keeps the low byte")

	def test_sell_limits(self):
		limits = load_config(JAVA_SRC, None, None, ["gameserver.siege.enable=false"], self.data)
		sale = self.report(limits, npc_id=900001, item_id=100, count=20000)["item"]["sell"]
		self.assertEqual((sale["soldCount"], sale["kinah"], sale["sellLimit"]["freshAccountLimit"], sale["sellLimit"]["remainingLimit"]),
		                 (20000, 1000000, 5300047, 4300047))
		fresh = lambda **kwargs: self.report(limits, npc_id=900001, item_id=100, **kwargs)["item"]["sell"]["sellLimit"]["freshAccountLimit"]  # noqa: E731
		self.assertEqual((fresh(account_max_level=30), fresh(account_max_level=31), fresh(account_max_level=61)), (5300047, 7100047, 17150048),
		                 "SellLimit's bands include both ends")
		self.assertEqual(fresh(membership=1), 10600094)
		self.assertIsNone(self.report(limits, npc_id=900001, item_id=104)["item"]["sell"]["sellLimit"], "a reward of 0 is never limited")
		tight = load_config(JAVA_SRC, None, None, ["gameserver.siege.enable=false", "gameserver.rates.sell_limit=0.00001",
		                                           "gameserver.limits.enable_dynamic_cap=true"], self.data)
		sale = self.report(tight, npc_id=900001, item_id=100, count=5)["item"]["sell"]
		self.assertEqual((sale["sellLimit"]["freshAccountLimit"], sale["soldCount"], sale["kinah"]), (53, 2, 100),
		                 "53 / 50 = 1, plus the dynamic cap's one")
		with self.assertRaises(OracleError):  # SellLimit has no band for level 66: NoSuchElementException
			self.report(limits, npc_id=900001, item_id=100, account_max_level=66)

	def test_the_country_code_picks_the_goods_lists(self):
		europe = self.config_with("gameserver.country.code=2")
		with self.assertRaises(OracleError):  # the static data was read with the default code 99: goodslists.xml, not goodslists_europe.xml
			self.report(europe, npc_id=900001)
		report = trade_report(StaticData(self.tree.root, 2), JAVA_SRC, europe, npc_id=900001, dialog_ai=self.ai)
		self.assertEqual(list(goods(report)), [100, 106, 102, 103, 105, 104], "goodslists_europe.xml: list 1 without the juice")
		self.assertEqual(report["config"]["gameserver.country.code"]["value"], 2)

	def test_one_item_everywhere(self):
		report = self.report(item_id=100, count=2)
		self.assertEqual(report["mode"], "item")
		self.assertEqual((report["item"]["unitPrice"], report["vendorSale"]["kinah"]), ({"ELYOS": 352, "ASMODIANS": 352}, 100))
		self.assertEqual([(s["npcId"], s["kinah"]["ELYOS"], s["buyable"]) for s in report["soldBy"]],
		                 [(900001, 704, True), (900002, 704, False), (900003, 704, False), (900006, 1408, True), (900007, 0, False), (900008, 0, True),
		                  (900014, 704, True)],
		                 "a LEGION_COIN vendor charges no kinah because performBuyFromShop never gets to charge it; 900013's first template is replaced")
		self.assertEqual([(p["npcId"], p["sale"]["kinah"]) for p in report["purchasedBy"]], [(900004, 200), (900011, 150)])

	def test_the_merchants_of_a_map(self):
		report = self.report(map_id=1)
		self.assertEqual([m["npcId"] for m in report["merchants"]], [900001, 900002, 900003, 900014],
		                 "900004 spawns only at a difficulty (not in the open world), 900010 has no trade function")
		self.assertEqual((report["sellers"], report["buyers"]), ([900001, 900014], [900001, 900002, 900014]))
		self.assertEqual([s["spawned"] for s in report["merchants"][1]["spots"]], [None, None], "a pool of 1 out of 2 spots")
		self.assertEqual(report["merchants"][2]["spots"][0]["staticId"], 7)
		butler = report["merchants"][3]["spots"]
		self.assertEqual([(s["ai"], s["aiOverridesOnDialogSelect"]) for s in butler], [(None, False), ("butler", True)],
		                 "a spot's ai replaces the template's; __NO_AI__ means none (Creature.java:64-67)")

	def test_sieges_and_races(self):
		sieges = load_config(JAVA_SRC, None, None, ["gameserver.limits.enable=false"], self.data)
		with self.assertRaises(OracleError):  # the influence is database state
			self.report(sieges, npc_id=900001)
		report = self.report(sieges, npc_id=900001, races=("ELYOS",), influences={"ELYOS": 40})
		self.assertEqual((list(report["prices"]), report["prices"]["ELYOS"]["smPrices"]), (["ELYOS"], [105, 100, 103]))
		self.assertEqual(goods(report)[100]["kinah"], {"ELYOS": 269}, "250 -> 262.5 = 262 -> 269.86 = 269")
		with self.assertRaises(OracleError):  # without sieges there is no influence to give
			self.report(npc_id=900001, influences={"ELYOS": 40})

	def test_arguments_the_oracle_refuses(self):
		for kwargs in ({"npc_id": 900001, "count": 0}, {"npc_id": 900001, "count": 20001}, {"map_id": 1, "npc_id": 900001}, {},
		               {"npc_id": 999999}, {"item_id": 999999}, {"npc_id": 900001, "membership": -1}):
			with self.subTest(kwargs=kwargs), self.assertRaises(OracleError):
				self.report(**kwargs)

	def test_data_java_cannot_start_with(self):
		cases = {
			"a limited item without <salestime>": {"goodslists": GOODS.replace("<salestime>0 0 10 ? * *</salestime>", "")},
			"a limited item with an empty <salestime/>": {"goodslists": GOODS.replace("<salestime>0 0 10 ? * *</salestime>", "<salestime/>")},
			"a cleanup of an item without a template that sets a bit": {"item_restriction_cleanups": CLEANUPS + '<cleanup id="998" sell="0"/>'},
			"no <trade_in_list_template>": {"npc_trade_list": TRADE_LISTS.replace(
				'<trade_in_list_template npc_id="900001"><tradelist id="1"/></trade_in_list_template>', "")},
			"no <purchase_list>": {"goodslists": GOODS.split("<purchase_list")[0]},
		}
		for what, holders in cases.items():
			with self.subTest(what=what), fixture_tree(**holders) as tree, self.assertRaises(OracleError):
				TradeData(StaticData(tree.root), self.rules)
		# AIEngine.validateScripts: an npc_template ai that no AI class carries
		with fixture_tree(npc_templates=NPCS + '<npc_template npc_id="900020" name="x" ai="no_such_ai"><stats/></npc_template>') as tree:
			with self.assertRaises(OracleError):
				trade_report(StaticData(tree.root), JAVA_SRC, self.config, npc_id=900001, dialog_ai=self.ai)

	def test_the_command_line(self):
		base = ["m5c-trade", "--static-data", str(self.tree.root), "--java-src", str(JAVA_SRC), "--config", str(CONFIG_DIR)]
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(base + ["--no-profile", "--set", "gameserver.siege.enable=false", "--set", "gameserver.limits.enable=false",
			                           "--npc", "900001", "--item", "100", "--count", "2", "--race", "ELYOS"])
		self.assertEqual(code, 0)
		answer = json.loads(out.getvalue())
		self.assertEqual((answer["format"], answer["item"]["buy"]["kinah"], answer["item"]["sell"]["kinah"]), ("aion-m5c-trade", {"ELYOS": 704}, 100))
		self.assertEqual(answer["config"]["gameserver.prices.vendor.sellmod"]["source"], "config/main/prices.properties")
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			code = oracle.main(base + ["--no-profile", "--set", "gameserver.siege.enable=false", "--set", "gameserver.country.code=2",
			                           "--npc", "900001", "--item", "101", "--race", "ELYOS"])
		self.assertEqual((code, json.loads(out.getvalue())["item"]["buy"]["buyable"]), (0, False), "the CLI reads goodslists_europe.xml for code 2")
		for args, message in ((["--no-profile", "--npc", "900001"], "gameserver.siege.enable is true"),
		                      (["--profile", str(self.tree.root / "gate.properties"), "--npc", "900001"], "no such file")):
			err = io.StringIO()
			with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
				code = oracle.main(base + args)
			with self.subTest(args=args):
				self.assertEqual(code, 2, "sieges on without --influence; a --profile that does not exist")
				self.assertIn(message, err.getvalue())


@unittest.skipUnless(HAVE_JAVA_TREE, "Java tree not present")
class M5cTradeRealDataTest(unittest.TestCase):
	"""The Poeta and Ishalgen merchants under the gate profile (m5c-plan.md §2.10, X1, X4-X7)."""

	@classmethod
	def setUpClass(cls):
		cls.data = StaticData(runner.DEFAULT_STATIC_DATA)
		cls.trade = TradeData(cls.data, JavaTradeRules.read(JAVA_SRC))
		cls.config = load_config(JAVA_SRC, CONFIG_DIR, None, GATE_PROFILE, cls.data)
		cls.ai = DialogAi(JAVA_SRC, HANDLERS)

	def report(self, **kwargs):
		return trade_report(self.data, JAVA_SRC, self.config, trade=self.trade, dialog_ai=self.ai, **kwargs)

	def test_the_shipped_config_defaults(self):
		shipped = load_config(JAVA_SRC, CONFIG_DIR, None, [], self.data)
		self.assertEqual({k: v.value for k, v in shipped.items()}, DEFAULT_VALUES)
		self.assertEqual(shipped["gameserver.country.code"].source, "config/main/gameserver.properties")

	def test_the_merchants_of_poeta_and_ishalgen(self):
		poeta = self.report(map_id=210010000)
		self.assertEqual((poeta["sellers"], poeta["buyers"]), ([203060, 203061, 203063, 203080, 798007], [203060, 203061, 203063, 203080, 798007]))
		self.assertEqual({m["npcId"]: m["tabs"] for m in poeta["merchants"]}, {
			203060: [129, 130, 131, 450], 203061: [133], 203063: [127, 128], 203080: [132, 720], 203081: [132], 203082: [132], 798007: [132, 720],
			798008: [132]}, "oz, tula and the cube expander baevrunerk have a trade list but no BUY or SELL function")
		ishalgen = self.report(map_id=220010000)
		self.assertEqual(ishalgen["sellers"], [203514, 203515, 203526, 203542, 798038])
		self.assertEqual({m["npcId"]: m["tabs"] for m in ishalgen["merchants"]}, {
			203514: [259, 260], 203515: [261, 262, 263, 454], 203526: [265], 203542: [264, 721], 798037: [274, 275], 798038: [264, 721]})
		self.assertTrue(all(s["spawned"] is True for m in poeta["merchants"] + ishalgen["merchants"] for s in m["spots"]))
		self.assertFalse(any(s["aiOverridesOnDialogSelect"] for m in poeta["merchants"] + ishalgen["merchants"] for s in m["spots"]))

	def test_minalinerk(self):
		report = self.report(npc_id=798007)
		self.assertEqual(report["prices"]["ELYOS"]["smPrices"], [125, 100, 113], "X1")
		self.assertEqual((report["npc"]["ai"], report["npc"]["talkRange"], report["buy"]["dialog"]), ("general", 6, "SM_TRADELIST"))
		self.assertEqual(report["buy"]["smTradeList"], {"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceModifier": 100, "constant": 100,
		                                                "showBuyTab": True, "showSellTab": True, "tabs": [132, 720], "limitedItems": []}, "X4")
		self.assertEqual(report["sell"]["dialogs"], {"SELL": "SM_SELL_ITEM", "TRADE_SELL_LIST": AUDIT_NO_PACKET})
		self.assertEqual(report["sell"]["smSellItem"], {"npcType": "NORMAL", "npcTypeIndex": 1, "buyPriceRate": 20, "showBuyTab": True,
		                                                "showSellTab": True, "tabs": []}, "X4")
		self.assertEqual({i: (g["name"], g["kinah"]["ELYOS"], g["sellBack"]["unitReward"]) for i, g in goods(report).items()}, {
			169000003: ("Minor Power Shard", 6, 1), 165000001: ("Extraction Tools", 1412, 200), 169300002: ("Bandage", 6, 1),
			162000052: ("Minor Life Elixir", 352, 50), 162000057: ("Minor Mana Elixir", 352, 50)})
		prices = lambda r: {i: (g["kinah"], g["sellBack"]["kinah"]) for i, g in goods(r).items()}  # noqa: E731
		self.assertEqual(prices(self.report(npc_id=798038)), prices(report), "crizpinerk in Ishalgen sells the same at the same prices")

	def test_trade_lists_without_a_window(self):
		# oz, tula, baevrunerk and the Ishalgen cube expander have a trade list but do not support BUY: CM_DIALOG_SELECT audits the dialog
		for npc_id in (203081, 203082, 798008, 798037):
			with self.subTest(npc=npc_id):
				report = self.report(npc_id=npc_id)
				self.assertEqual((report["buy"]["dialog"], report["buy"]["smTradeList"], report["sell"]["reachable"]), (AUDIT_NO_PACKET, None, False))
		limones = self.report(npc_id=800591)["sell"]
		self.assertEqual((limones["reachable"], limones["smSellItem"]), (False, None), "func_dialogs [2]: no SM_SELL_ITEM")

	def test_the_gate_purchases_and_sales(self):
		elixirs = self.report(npc_id=798007, item_id=162000052, count=2)["item"]["buy"]
		self.assertEqual((elixirs["kinah"], elixirs["buyable"]), ({"ELYOS": 704, "ASMODIANS": 704}, True), "X5: 1000 - 704 = 296 left")
		potions = self.report(npc_id=798007, item_id=162000002, count=10)["item"]
		self.assertEqual(potions["buy"]["failure"]["message"], "STR_BUY_SELL_USER_BUY_FAILED", "X6: the potion is not on the list")
		self.assertEqual((potions["sell"]["arm"], potions["sell"]["kinah"], potions["sell"]["repurchasePrice"]), ("VENDOR", 500, 500), "X7, X8")
		juice = self.report(npc_id=798007, item_id=160000001)["item"]
		self.assertEqual((juice["mask"], juice["sell"]["failure"]["message"]), (12360, "STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC"), "X7")


if __name__ == "__main__":
	unittest.main()
