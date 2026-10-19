# The broker, lane B (night of 2026-10-18)

The owner un-parked the broker on 2026-10-18 (it had been parked by m5c-plan.md D2 on 2026-09-27). This plan covers m5c-plan.md's B-01 and
B-02. B-03 (an `SM_BROKER_SERVICE` decoder and gate cases in Sanctum) is not in this job, because it would change gate files.

## Measured (census on a5c4826ed)

- **P5-09b:** `BrokerService` has 11 open bodies, 388 Java lines: `showRequestedItems`, `getAveragePrice`, `getItemsByMask`,
  `getRequestedPage`, `buyBrokerItem`, `registerItem`, `showSellWindow`, `showRegisteredItems`, `cancelRegisteredItem`,
  `showSettledItems` and `settleAccount`. The other 19 bodies, and every model the bodies use, are ported: `BrokerItem` with its
  comparators, `BrokerItemMaskInfo.h`, `BrokerPlayerCache`, `SM_BROKER_SERVICE`, `DynamicServerPacketBodySplitList`, `BrokerDAO`.
- **P5-15 and P5-16:** 9 packets with no C++ class yet, 27 bodies (constructor, readImpl, runImpl each). They are the 8 `CM_BROKER_*`
  packets plus `CM_BUY_BROKER_ITEM`, and `CM_REGISTER_BROKER_ITEM`.
- **Gates:** no scenario gate sends a broker packet. Today the server drops them with one "not ported yet" warning per class. So the gates
  are `gs.smoke.startup` and `gs.scenario.m5c`.

## Checkpoints (stacked branches lane-b/broker-cpN from a5c4826ed)

| cp | BrokerService | Packets | Tests |
|---|---|---|---|
| 1 | `showRequestedItems`, `getAveragePrice`, `getItemsByMask`, `getRequestedPage`, `showSellWindow`, `showRegisteredItems` | `CM_BROKER_LIST`, `CM_BROKER_SEARCH`, `CM_BROKER_SELL_WINDOW`, `CM_BROKER_REGISTERED` | browsing, search, paging and sorting, the price window |
| 2 | `registerItem`, `cancelRegisteredItem` | `CM_REGISTER_BROKER_ITEM`, `CM_BROKER_CANCEL_REGISTERED` | registration fees and limits, cancel, the stored rows |
| 3 | `buyBrokerItem` | `CM_BUY_BROKER_ITEM` | whole and split purchases, the seller's settled icon, the stored rows |
| 4 | `showSettledItems`, `settleAccount` | `CM_BROKER_SETTLE_LIST`, `CM_BROKER_SETTLE_ACCOUNT` | the settled pages, collecting kinah and unsold items |

**Header change:** `showRequestedItems` takes `itemList` as `runtime::Ptr<runtime::RcArrayList<int32_t>>` instead of
`const std::vector<int32_t>&`. Java passes null from `CM_BROKER_LIST` and tests `itemList != null`. `buyBrokerItem` passes the cached list
back in, so the type is `BrokerPlayerCache.itemList`'s own.

**Tests** run on the test database, in `tests/economy/P5-09b` (EconomyTestSupport.h). The `BrokerService` singleton loads the broker table
once per process, so each case needs its own process, as `BrokerServiceTest` already does. The new suites carry the realdata label, the
way #381 (6d01a77c6, not on this base) labels database tests. The mutation runs go through ctest, one process per case, under the gate
lock with the test database's environment.

**Gates:** `gs.smoke.startup` and `gs.scenario.m5c` per checkpoint. The window 02:00-03:15 belongs to lane A's soak.

## Progress

- **cp1** (2026-10-18): the six browsing bodies and the four browsing packets. `BrokerPacketsTest.cpp` has 12 cases on the test database.
  J-P5-09b-1 (the average price's division by zero) is recorded in P5-09b.md and not applied.
- **cp2** (2026-10-18): `registerItem`, `cancelRegisteredItem`, `CM_REGISTER_BROKER_ITEM` and `CM_BROKER_CANCEL_REGISTERED`.
  `BrokerRegisterPacketTest.cpp` (cm_lz) has 11 cases and `BrokerCancelPacketTest.cpp` (cm_ak) 5. `BrokerOpSaveTask` takes nullable items.
- **cp3** (2026-10-18): `buyBrokerItem` (and `putToSettled`, ported before, reached for the first time) and `CM_BUY_BROKER_ITEM`.
  `BrokerBuyPacketTest.cpp` has 10 cases. J-P5-09b-2 (an unsplittable offer bought whole for fewer pieces' price) is recorded and not applied.
- **cp4** (2026-10-18): `showSettledItems`, `settleAccount`, `CM_BROKER_SETTLE_LIST` and `CM_BROKER_SETTLE_ACCOUNT`.
  `BrokerSettlePacketsTest.cpp` has 8 cases. With cp4, `BrokerService` has no `AION_UNPORTED` site left, and the broker's allow-list rows
  (11 sites, 9 opcodes) are gone.
