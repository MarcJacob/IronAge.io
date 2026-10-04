# IRONAGE.IO DESIGN DOCUMENT - Intro

Openfront-like lightweight game themed on medieval / feudal strategy & diplomacy and subjugating other players as an alternative to conquest,
with multiple paths to any sort of victory the player is aiming for.

The map is built in an Openfront-like way with the use of texelized terrain that is freely conquerable.

# Game Elements

## Settlements (Villages -> Towns -> Cities)

Empty land is colonized by building settlements. First settlement a player builds is their capital. Capital can be changed at a cost.
Settlements are subjected to a player, first to whoever built them, then change hands by conquest or diplomatic gift.
Settlements generate taxes as a function of travel distance to capital (square or linear ?) and their population.
Settlements grow over time (linearly ? Logarithmically according to a capacity ?).

Roads can be built between settlements at a cost. Roads connect two settlements for trade and reduces the travel distance between them.
settlements can be upgraded for growth, capacity, defense, caravan / trade fleet generation...

### Land exploitation / colonization

Settlements have a physical circular size. After that they expand on the surrounding land to "exploit" it.
Higher population increases the settlement's "influence radius" logarithmically (with a penalty when the settlement increases in tiers)

Influence radius determines how "strongly" a settlement exploits a tile, and whether the tile is wild or colonized. Once a tile is colonized, it is linked to up to 3 closest settlements (assuming the 3 form a triangle around it) and from there on the % of control between the 1 to 3 settlements is determined by their respective influences on it. The more of an even split it is, the more contested the tile becomes until its productivity is penalized (reduced to 0 if the settlements are enemies, 25% if neutral, 50% if common liege, 75% if friendly, combined influences if the settlements have the same owner).

The more land exploited by a settlement, the higher its base population capacity.

Goal: settlements colonize as much land as possible around themselves but eventually reach a limit where it becomes necessary to build more settlements to more efficiently exploit the land.
Too many settlements in a small area on the other hand decreases the area's overall exploitation indirectly decreasing everything else (trade, population...),
encouraging border conflicts for small players who have nowhere else to focus their means.

### Local Wealth & Treasury

Settlements have *local wealth* which can be periodically taxed into a separate money pool called the *treasury*.
The treasury is the money put away by the owner of the settlement for actions other than local upgrades.

Treasury can be used to:
- Recruit troops (raise levies / recruit men at arms / knights).
- Build defensive buildings.
- Pay troop upkeep.
- Go back to local wealth.

The treasury can be moved to another location / to an army either automatically or manually. Moving costs a percentage of the gold linear to travel time and how far away from the capital the start and end point are.
The treasury wealth spent that way is gone from the game completely.

Local wealth being accumulated in a settlement has positive effects, capped at a certain fraction of the population:
- Increased population capacity
- Increased attractivity for trade
- Increased base trade income per caravan

When local wealth is too high compared to local population, it slowly disappears.

To grow a settlement it is wise to strike a balance between accumulating local wealth, limiting taxation into treasury and building upgrades made to attract / emit trade and increase population cap.
### Trade

Caravans / trading fleets are generated randomly as a function of population, local wealth, upgrades... They target another village at random using the trade network as an itinerary (roads on land, established sea trade lanes on water).
On every stop until reaching the target, they generate local wealth for both their home village and the village they stopped at. The total amount of trade generated depends on distance and the amount of local wealth stored at the stop.
When they reach their target, they disappear (or return home along the same path ?)
#### Target selection

Which target village a newly spawned caravan / trade fleet will choose is based on:
- Travel time from home village (so fleets will usually be much better at going further away)
- Local wealth in target
- Upgrades in target
- Tier of target (Towns and Cities get a nonlinear boost)
- Diplomatic status between home and target

## War & conquest

Armies can be created at any owned settlement from stationned / raised troops, with three troop types:
- Levies, immediately raised from the local population at no immediate cost. Low upkeep.
- Men-at-arms, medium strength and upkeep. Bonus damage against horsemen.
- Archers, same as men-at-arms but instead of contributing to melee damage instead fire arrows periodically at nearby enemy armies including while in battle.
- Horsemen, medium strength and upkeep. Bonus damage against archers. High speed if the army is only made up of horsemen / knights.
- Knights, very high strength and upkeep. Bonus damage against everything other than other knights. High speed if the army is only made up of horsemen / knights.

Armies move around the map freely, go faster on roads. Their total upkeep cost is slightly superlinear with total troop count, encouraging quality over quantity when possible for longer use but not to the point it's better to field one knight over 25 levies.

Hostile armies fight one another automatically when coming into contact. Fighting is very simple: a certain percentage of damage is inflicted on both sides per tick until one side is completely wiped out or is told to retreat.
Neutral armies can be told to engage one another manually. The % of damage inflicted on either side depends on average quality and troop count difference (so to efficiently kill enemy armies you need to highly outnumber them or to have much better troops on average).

Armies can be attached to other armies so they follow and join battle automatically.
Battles can also be joined manually.

When an army reaches a settlement, they can attack it with multiple objectives:
- Conquer, which only requires the defeat of the local troops. Conquest transfers control of the settlement to the attacker.
- Raid, which starts a battle with the local troops. While the battle lasts local wealth is pillaged by the attacker army, up until the attacking army's carrying capacity is reached.
- Sack, which starts a battle with the local troops and 25% of the local population as resistance. The process is faster than raiding and damages local upgrades.
- Raze, which starts a battle with the local troops and 50% of the local population as resistance. Acts like a sack but doesn't stop until the settlement is entirely destroyed.

It wouldn't be rare to see a raid or sack followed by conquest in case the attacker wants to get wealth back to their heartland while keeping control of the settlement.

Armies can station themselves at a neutral, friendly or owned settlement, or disband. When disbanded troops return to their home villages (levies automatically go back to normal population).
When stationed, the owner of the village can use the local treasury to take care of their upkeep at some %. Probably something you want to do for long-term stationed troops and allied armies that are there temporarily.

Armies can cross water freely but have a very reduced speed and increased upkeep while doing so.
By default armies will use pathfinding using roads unless "direct movement" is used in which case they will go in a straight line.

Armies coming across a caravan can plunder it, killing the caravan and increasing the army's gold reserve.

Goal: Relative simplicity emphasising raw strength, having the support of other players and it being difficult to move an army far from home.

### Troop recruitment

A settlement can build upgrades allowing it to take from local wealth and / or treasury to periodically recruit soldiers into a pool of raisable troops.
Troops in the pool cost upkeep and do not contribute to land exploitation.
By default only levies can be recruited, at a very fast rate, limited only by money and population.
Barracks upgrade allow recruitment of men at arms, archers and horsemen.
Keep allows recruitment of knights.

All available troop types can be set to a ceiling where recruitment continues until reaching that ceiling.
Troop count for recruitment does NOT


Goal: Simplicity, center recruitment of high quality troops in specific places. Not every settlement should have recruitment buildings.
### Upkeep & Economy

Armies have to be paid for periodically by the gold they're carrying.
If they run out of gold, the army becomes indebted. The more in debt, the more troops of higher quality start deserting (levies start deserting as soon as debt starts, others need a higher percentage of debt compared to total army upkeep). Deserting troops take a % of the debt away with them so there's no runaway "army melting" phenomenon.

Goal: Create tension when an large army is raised - it has to get gold from somewhere unless the owner stocked its gold inventory. It is likely armies will NEED to pillage for upkeep.

## Diplomacy

V1 = Vassal is under complete diplomatic control, no independent relationships.

Diplomacy is managed on an inter-player level. They have a *base relationship* which is neutral unless overruled by vassal / liege rules, and a *personal relationship* which is set freely by players.
The fundamental stances between two players are:
- Neutral: All interactions are allowed with no automatic hostilities. Battles and other hostile actions do not change this state automatically.
- Friendly: Same as neutral, with a small bonus to trade attractivity and less malus from land exploitation being contested. Automatically reverts to neutral if one of the players takes hostile action.
- Enemy: Armies attack one another automatically. Trade attraction goes down to 0. Enemy armies stationed in own settlement are kicked out.

V2 = Separation between personal and base.
The personal relationship between two players is set by the lowest value set by either player (with it being initially neutral, so friendship or peace can't be unilateraly declared).

On top of this, players can be vassal or liege of other players, forming *vassal chains*.
- Liege / vassals work are forced to Friendly relationship with one another.
- Lieges can tax vassals at a certain % of their income.
- Lieges can force a vassal settlement to spend its local wealth to pay for a liege army upkeep.
- Lieges can load the treasury of a vassal settlement onto their army the same way the direct owner can.
- The *personal relationship* (or lacking one, *base relationship*) of the liege to another player is the *base relationship* of the vassal to that same player and vice versa.

This leads to various possible *diplomatic statuses* between two players with cosmetic names depending on the situation:
(Personal relationship X Base relationship) 
- Neutral X Neutral = Neutral
- Neutral X Friendly = Friendly
- Neutral X Enemy = Foe
- Friendly X Neutral = Friendly
- Friendly X Friendly = Allied
- Friendly X Enemy = At Truce 
- Enemy X Neutral = Feuding
- Enemy X Friendly = Rival
- Enemy X Enemy = Enemy

When player is independent, then base relationship is the same as personal relationship (since they fully control their own diplomacy).

## Winning the game

The goal of the game is to reach one of the victory conditions:
- Sovereign victory: Have at least 75% of the land controlled by you or your vassals.
- Development victory: Have at least 50% of the world population in owned or vassal settlements.
	- Note: This is a "bottom up" victory, possible to get without being independent if your own sub-vassal-chain has 50% of the world population in it.
- Merchant victory: Have at least 50% of all world wealth in directly owned settlements.
- Scourge victory: Have a kill count equal at least 50% of the number of people who ever lived.

When any of those victory conditions, the victory screen shows the victor and the players who were closest to getting the other victory conditions, and the second + third in place for the primary victory condition (called "runner ups").
Furthermore, other players are shown as "secondary victors" based on other stats like:

Loyalty: how long a player stayed a vassal to the same other player, weighed by their strength.
Personal domain: how much land / population a player directly owns.
Personal wealth: how much gold a player has in their treasury.
...

And finally, *overall score* which is calculated by those same metrics all put together.

Primary and secondary win conditions provide progress rewards, emphasizing the fact that being the primary victor in the game is not necessary, and can even be detrimental to getting a better score.