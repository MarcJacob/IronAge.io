# ECONOMY DESIGN DOCUMENT

# Intro: GOAL

The goal of the economy is to drive the game towards an overall increase in production and trade over time, forming hierarchical networks of larger and larger population centers from the interaction between unfair advantages for larger settlements (but whose existence relies on smaller settlement), and the cost of distance.

This leads to the rise of trade centers which become strategic holdings to fight over.

# DESIGN CONCEPTS:

#Distance_Cost = The economic system seeks to allow distance to be a major factor in offsetting the economic advantage of a specific place / player. Distance is effectively a barrier that must be broken through through scaling up key locations, at which point economic interaction / competion can "physically" become profitable and thus starts to happen. This is achieved through taking Distance into account both positively and negatively in various calculations, sometimes linearly, sometimes super-linearly.

#Goods_Sources = Whether in Gold or Goods form, Wealth is modeled using only two resources. Goods are used to satisfy various sinks which are required by some systems, while Gold is accumulated wealth that can be turned into goods directly or indirectly through specific mechanics. Gold is the arbitrary translocation of Wealth while Goods take part of the standard systemic flows.

#Goods_Sinks = Some systems depend on the ability to consume (and destroy) goods, usually allowing the growth or continued existence of an entity if able.
                Goods gone through sinks disappear from the game entirely. The balance between sources and sinks at various levels is something the game intentionally tries to play with.

#Economic_Tick = Arbitrary tick interval at which many economic calculations happen, as doing those calculations on every tick would be too costly computationally and make visibility harder for players. Currently set to a "day", meaning 120 ticks.

# Settlements

The game's economy is centered around Settlements. Settlements are centers of population, which mostly uses the land held by the settlement to produce goods.

## Land / Territory

Settlements claim tiles on the map in order to provide work for the peasant population. The effective number of peasant per tile ("Land output") depends on the tile's terrain type, and its Priximity to the settlement which is the travel cost from itself to the settlement. It can be reduced by roads, making them an essential component of increasing the output of a settlement. Peasants above the land output value still produce, but less and less per peasant, meaning they eventually consume more than they produce, capping the population growth. This is one way #Distance_Cost impacts the game by making it so building multiple settlements to exploit land is better than trying to grow one large settlement.

## Population

Population represents the sum of all human beings living in the settlement, its territory, or with ties to the settlement (while out on the map in an army / a worker group).

Population is divided by strata: Peasants are the default and contribute to Production from land, Workers are people hired to work on construction projects, Soldiers are part of armies raised from this settlement, whether those armies are stationned in the settlement or out on the map.

### Soldiers

While workers can be out on the map but never too far from the settlement, soldiers can end up anywhere.
To properly model the fact soldiers need to be replaced from parts of the population being born, armies keep their exact origin in memory, meaning they keep counting in some of the population-related properties of the settlement, mostly the population capacity (while local consumption is only increased by armies actually stationned inside).

## Production

The goods are pooled in the settlement, and every economic step, a portion of that pool can be sold to the local population. How many goods are sold to the population compared to its size / total demand determines growth / shrinkage.

Settlements are the only "pure" #Goods_Sources, meaning the goods produced are produced at no expense to anything. From there, the goods are accumulated into the settlement's market goods pool, from which they can be taken away by a caravan to be sold at a profit to another settlement (See [### Trade]).

Production is taxable. One good -> One gold.

## Consumption

The market goods pool, on every economic tick, leaves a certain % of its contents for sales to the local population. The population, from their numbers and the "strata" (peasants, workers, soldiers...) computes a total consumption demand. If there are enough goods for sales to fully meet the demand, population grows, with a bonus the greater the goods surplus. Otherwise, the population stagnates or can even shrink.

Sales are taxable. One good = One gold.

The population taking part in consumption is only the settlement's non-military population, since armies use their own supply system (meaning they indirectly participate by buying from the settlement).

By default, a settlement sets half of its goods pool to local sales.

## Trade

Trade in this design is tricky because there is no difference in nature between a good produced on one corner of the map, and one produced on the other corner.

The trading model seeks to model three things:
- The overall bilateral (but not necessarily symmetrical) profitability of trading as an activity, with greater profits when exchanging goods of greater difference / variety, which is modeled by distance between seller and buyer.

- Small transactions are better conducted over short distances while larger transactions can scale up in distance until they stretch across the map between major trading centers.

- Large trade centers are more attractive than small settlements, through the ability to absorb more goods at a stable price, greater demand, and other "unfair" advantages.

This is the main aspect where #Distance_Cost happens. At transaction time, distance between seller and buyer is a *positive* element: the sold products are worth more because they come from far away and are not easy to find locally, and vice versa. However, travelling takes time and the trading entities (Caravans / ships) take that into account seeking to increase their profit per travel time.

Since small transactions benefit less *in absolute value* from Distance, going further for more value per good will quickly be outweighed by the unattractiveness of the time it takes to get there. Ergo, to perform longer-distance trades that are more profitable per good, the caravan / ship must be larger in volume, which can only happen from larger settlements / good pools. It also eventually becomes necessary to perform the trade by sea, where volumes can be much larger and travel times overall lower.

### Trade Flow

Trade does not create goods. A settlement invests goods above its reserve (a few days of local need) into a caravan, which delivers them to the destination's pool.
Both ends of a delivery gain Trade Flow, scaled by caravan size and distance (short trades gain little, trading towards a smaller settlement is penalized).

Trade Flow decays quickly and is capped by a Max Trade Flow, seeded by urban population and mostly raised by buildings (Trade Depots, which also raise pool size).

Trade Flow converts into a percentage Trade Bonus (relative to population) on the settlement's production, peasants included, and makes the settlement a more attractive destination.

This snowballs by design: invested trade centers keep attracting trade and tend to become production centers, while small villages send caravans to nearby or regional hubs to earn a little more.
Trade is inter-settlement AND inter-player. Embargoes, closed routes and blocking or despoiling caravans with armies come later.

### Goods Export

Caravans get their value from taking goods from their origin settlement's goods pool, and transfer those goods to the destination.

The goods can be taken at any time when the system sees that a good deal is currently awaiting a caravan if it would spawn. To discourage caravans starving settlements, a heavy penalty is imposed to the decision making (but NOT final profit) when goods are scarce in the origin settlement. This means that to get trade started somewhere, a good opportunity needs to exist. The trade volume bonus then ensures that the activity is worthwhile through increased production.

In order to create exportable goods, a settlement sets them aside from production and imports at a choosable rate, so some settlements can be made export-oriented and others import-oriented. The intention is for small settlements to HAVE to export to get trade volume while larger settlements can export less and maintain larger populations which allows them to have available population for industries and armies.

For extra control, settlements can be allowed or disallowed from exporting abroad (meaning players that do not have a free trade agreement with the settlement's owner). 

### Industries

Trade Volume accumulates into a spendable resource. Cities that have strong, sustained trade volume are able to build industry buildings which create more goods without the usage of land, furthering their economic value and making them even more attractive to further trade.

### Comments

*This new design based on the Trade Flow property instead of generating goods from thin air works MUCH better, but we definitely have a problem with being able to accumulate resources. Two possible solutions: limit non-peasant population and make peasants more productive, or give the player a slider and allow goods to pool into an export pool that is never sold.*

## Buildings & Construction

Buildings are built using gold from the treasury through the construction project system. Each project hires the workers it needs from the peasant population, attempts to withdraw the required gold from treasury. If both are successful, the peasants are actually transfered to the workers population (taking them out of the peasants pool, meaning less production). This starts again on every tick, so workers go back to being peasants if the project isn't / can't be financed.

Buildings are either on the map or inside the settlement, and modify some part of the world state, usually related to the tile they're on or the settlement they're in.

Some examples include:
- Housing to increase population capacity.
- Trade buildings to increase pool size and number of active caravans.
- Military buildings to increase local defense or troop training speed.
- Roads on the map to increase movement speed and Proximity.
