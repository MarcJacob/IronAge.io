# MILITARY DESIGN DOCUMENT

# Intro : GOAL

The goal of the military mechanics is to immerse the player somewhat in the difficulties of medieval warfare: decentralization / difficulty to control large armies, and the logistics challenge of keeping an army fed, with strategic concerns based on sieges and relatively slow maneuvering mixed in with smaller mobile warbands able to live off the land and damage economies, sometimes bringing back a profit home.

The shape of fighting over the course of the game is this:
- Early game dominated by small mobile groups denying colonization of strategic areas, and conducting raids to kickstart economies.
- Middle game dominated by medium-size forces taking part in small-scale warfare to take out weak players and establish regional powers, as well as the first vassalization chains. Larger raids from some players relying on this as an economic strategy, sometimes razing settlements completely.
- Late game dominated by large "realm wars" working around the Campaign mechanics involving many players on either side and influencing the rise and fall of trading centers based on the supply lines that form naturally.

# DESIGN CONCEPTS:

#Projection: The military system seeks to make it difficult to send armies far away from owned / friendly territory past a certain scale. Small armies can live off the land or through raiding, while larger armies cannot only sustain themselves that way and must resupply through other means, usually by spending some of their carried gold (or a lot of raiding).

#Decentralization: While it is possible to control armies directly, doing so is made intentionally tedious and encourages the use of some powerful mechanics like the Campaign system. Furthermore, groups of players are advantaged against individual players if both are of equal overall power, through hard limitations like one player = one Campaign, and one settlement = one campaign participation.

#Indirectness: Related to #Decentralization, the precise control of armies is made intentionally difficult or even impossible for some specific operations. The point is for players to very quickly understand that micro management is not the point, but rather general positioning and playing around all the other aspects of war. When two opposing armies approach eachother, both players can be relatively certain a battle will happen unless *both* players decide to stand their ground or retreat. Furthermore, army groups / Campaigns are voluntarily made non-player-controlled in their low-level decision-making, as to give them somewhat predictable behavior both for the target players, as well as any counter-campaign(s) started in reaction.

#Economic_ties: The military system is intrinsically linked to the economy and vice versa. The military is an overall weight on the world's economy, but can be used to enrich local economies through the ability to bring back loot both in the owner's treasury and the army's origin settlement goods pool.

#Scales: The design tries pretty intentionally to split warfare between two kinds that use the same mechanics but with different emerging playstyles due to how they interact: Small scale and Large scale. Small scale allows much greater maneuveurability, moving across the map quickly and not reliant / as reliant on supply lines, but not able to achieve major military objectives like conquering a settlement. Large scale is the contrary: it is slow, intentional, and relies on methodical advancement and the execution of slow maneuvers and sieges. Positioning and picking battles careful is key.

# Armies

Armies are the fundamental military entity in the world. An army is made of men from a single settlement: armies can't be comprised of men from different settlements. Campaign armies stay tied to their origin settlement; retinues are independent from it once raised (see [## Two kinds of army]).

They move around using the standard movement mechanics, benefiting from road and struggling off roads depending on terrain.

Armies can attack pretty much any other kind of entity they can reach: other armies ([# Battles]), Caravans ([# Raiding & Looting]), Settlements ([# Sieges]) and Workers ([# Raiding & Looting]).

Armies are controlled through a Standing Order which can either be specifically targeted (Station at X position, attack Y entity) or be more abstract objectives (Defend region, attack / raid region, follow other army...).

The Standing Order and its parameters then determine / influence the Army's behavior.

## Two kinds of army

- **Retinues**: armies created directly by the player. They are "lone operators" directed by the player, and the only armies that take direct orders. Envisioned common practice: a militia that defends the settlement, and small armies for specific missions like small scale raids or quick intervention forces. A retinue can also be sent to join a campaign by order; if the campaign ends, it stays where it is.
- **Campaign armies**: armies raised by a settlement for a campaign (see [# Campaigns]). No direct orders: only the campaign logic controls them. They are the armies that go home and disband automatically.

## Retinues

Any settlement can raise any number of retinues ("Raise Retinue", the `CREATE_RETINUE` command). The player chooses the composition ([## Troop Types]); levies are not allowed, only professionals (men at arms, horsemen, knights) and wagons. The troops are taken from the garrison.

The retinue is raised inside the settlement and stays still until all troops are in. Its destination is then the chosen location, or, if none was chosen, it defends its settlement. Its raising cost is a day of its consumption (at most what it can carry); the rest of its supplies it accumulates through the normal supply mechanics while raising. A settlement under siege cannot raise one.

A retinue is independent from its origin settlement: its men no longer count in its population, casualties and deserters do not lower it, and conquest of the origin has no effect on it. It can disband anywhere in territory the owner owns: its men join the local garrison (and population; a full pool is simply exceeded for now), its gold goes to the owner's treasury and its supplies to the local settlement's market. The Go Home order sends it to its origin settlement, where it stops (it does not disband).

## Garrison

By default, every settlement has a single army within it that has special treatment: the Garrison. The Garrison is not a normal army, in the sense that it is not a separate entity from the settlement itself.

When the garrison lacks troops, it uses the settlement's facilities to train more. Those facilities also determine how many can be trained at the maximum, counting existing troops both in the garrison still and raised in an army.

Levies in garrison remain part of the working population.

A settlement does not train new soldiers while its siege or clash is actively fought (assault, sally, outside clash). An idle siege still allows training.

# Battles

When two armies hostile to one another meet on the field, they start a Battle.

Battles "capture" all involveable armies in the area, with the size of the battle increasing the more men are involved.

While inside a battle, an army's normal supply mechanics are frozen (*intention: I don't want supplies to matter as a mechanic when battle is engaged, nor should battles last long enough for it to matter, unless the battle lasts so long that the army will probably be out of action anyway*).

Battles end when an entire side has died or withdrawn.

## Battle Rounds

Every N ticks once a battle has begun, a "battle round" runs during which armies on either side inflict damage on the other.

The damage is spread uniformly between the armies according to their troop count (IE if two armies are on one side of a battle that takes 10 damage, one army with 80 men, the other with 20, then the former will take 8 damage while the other takes the other 2).

Damage is then spread between each troop type, also by their numbers in the army. The formula goes like this:

    Casualties = (Incoming damage - rand(0, Incoming damage / troop type defense skill)) / rand(1, troop type armor). Both defense values are always at least 1.

    - Defense skill represents the troop's ability to deflect damage actively and "make it harmless". At high values, the effective damage inflicted is often low.
    - Armor represents the troop's resistance to damage that has connected with it, effectively the number of damage required per casualty. At high values, few troops will die even with high inflicted damage.

Various bonuses / maluses can apply as additive percentages.
Examples of bonuses include:
- Defending in a siege on a settlement that has fortification buildings.
- Bonus damage against specific troop types (modelled as more total damage inflicted upon the enemy).
- Malus to damage for cavalry in hills / mountains.
- ect...

The intention behind this system is to keep it relatively simple - sheer mass of units will work for winning a battle reliably, but high quality troops in a good defense location can be deceptively difficult to kill, not to mention that fewer troops means higher speed. Higher quality troops thus get much greater survivability unless they get overwhelmed.

The combat stats of various unit types also combine with their economic / out-of-battle characteristics: speed, carry capacity and so on.

## Troop types

- Levies: Weak against everything. Good carry capacity. Medium speed.
    - Damage: 2
    - Defense skill: 1
    - Armor: 1
    - Upkeep: 1 good / day
    - Carry capacity: 10 goods
- Men at arms: Good all-rounder. +50% Bonus damage against horsemen. Medium speed.
    - Damage: 6
    - Defense skill: 4
    - Armor: 4
    - Upkeep: 3 goods / day
    - Carry capacity: 10 goods
- Horsemen: Particularly effective on their own for maneuvering. High speed. +50% bonus against levies.
    - Damage: 4
    - Defense skill: 4
    - Armor: 2
    - Upkeep: 4 goods / day.
    - Carry capacity: 16 goods
- Knights: Good against everything. Expensive. High speed. Reliant on other units to carry supplies.
    - Damage: 12
    - Defense skill: 6
    - Armor: 10
    - Upkeep: 15 goods / day
    - Carry capacity: 15 goods
- Wagons: Ineffective in combat but difficult to kill. Slow speed. Necessary for larger armies to carry enough goods for prolonged sieges / campaigns.
    - Damage: 0
    - Defense skill: 0
    - Armor: 30
    - Upkeep: 1 good / day
    - Carry capacity: 100 goods

# Campaigns

A player can call a campaign at any time, involving at least one army or settlement.

Involved armies immediately get the standing order to join the campaign, then get controlled by the Campaign AI once they are close enough.

Involved settlements turn 75% of their garrison into a campaign army that is then sent to join the campaign. Such armies are known as *campaign armies* and have some special properties:
- A settlement can only have one campaign army at a time. If it is destroyed while the campaign lives on, the settlement notices on the next tick and raises a replacement (if affordable and not under siege).
- They go home automatically when the campaign ends (a raising one disbands at once), and disband at home.
- They never outlive the campaign itself. If a settlement joins a new campaign while its previous campaign army is still walking home, that army is taken back into the new campaign instead of raising a new one.
- They cannot be controlled manually, the Campaign AI is in full control of their standing order. They still count in their origin settlement's population.
- A campaign ends when it has no members left or none within range; armies still raising or joining then go home.

Allied / Vassal players can join the campaign as well with their own armies and settlements. Overlords can force this process on their vassal's settlements (but not armies), including making them join someone else's campaign.

Campaigns exist as their own entity on the map, centered on the "leader army", the army the campaign was started from. When member armies are close enough, they are "joined" in the campaign, and the campaign AI can use them to accomplish its objective(s):
- Conquering / Raizing a set of regions.
- Defending a set of regions.
- Countering a specific enemy campaign / army.

Campaigns are beneficial in multiple ways:
- Streamline the process of raising armies throughout the realm and having them collaborate.
- Easier collaboration between players.
- Limit how messy wars can get with many players by organizing pushes / defenses.
- Campaigns are valid trading targets, allowing caravans to resupply a common supply pool every army joined with the campaign can draw from.

The intention behind the campaign mechanic is to reward multi-player collaboration (since players are limited to one campaign at a time, and each settlement can also only join one at a time), streamline the war process for large players so micro isn't too intensive, and pace wars so that they alternate between moments of quiet and major battles / sieges as campaigns start / stop.

When an army joins a campaign, it is set to JOIN CAMPAIGN standing order, which has it travel as fast as possible to the campaign / leader army. Once within a radius made of a constant + extra with more armies having joined, it is considered to have joined the campaign. For simplicity, from there an army is never considered out of the campaign if it gets too far away. The campaign is aware of armies that are joining: they are already part of the campaign, but in the "joining" state, so the Campaign AI may elect to wait for armies to arrive.

## Campaign AI

Campaigns are created with a specific objective which cannot be changed.

Possible objectives:
- Conquer Regions, with up to 16 regions selected.
    - The Campaign AI will move to the area, choosing a start point in friendly territory, gathering enough supplies, and move onto the closest target, sieging them down one by own. It may dispatch armies to conquer other targets in parallel if it believes it is possible.
- Defend regions, with up to 16 regions selected.
    - The Campaign AI will move to the area, choosing a base of operation to start with that is as close as possible to all defense targets, then dispatch armies as needed (or the entire campaign) to counter attackers.
- Defense Campaign, selecting an opposing offensive campaign (or multiple) ?
    - Similar to defend regions, except the focus is specifically to stop one or more enemy campaigns from making progress by opposing them in the field and breaking sieges.

It does so by assigning standing orders to armies participating in the campaign.

When its armies besiege the objective settlement, the Campaign AI also decides when to launch the assault: when the besiegers outnumber the garrison 3 to 1, or when the besieging armies are about to run out of supplies (under 2 days). This applies to every campaign; the manual Begin Assault command stays available.

A campaign is founded together with its objective (the player flow: found it on a settlement, then set the objective and join the other settlements in one go; the player AI does the same).

# Raiding & Looting

Raiding & Looting is one of the main ways the military directly influences the economy.

In order to establish economic dominance in an area, or simply kick-start one's own economy further away, wealth can be stolen from other players, damaging them in the process and slowing down their growth.

Further, any hostile activity will add a malus to the attractivity of the area for trade.

In the early game, some players may wish to attempt early raiding as a source of income and as a way to slow down the growth of their immediate neighbours, while later longer-reaching expeditions can be sent to raid distant places by boat.

Larger armies may also use those mechanics as a way to decrease their reliance on supply lines.

## Raids

Armies can be sent to Raid specific territories as a standing order. They will travel to the target, and enter a Raiding state during which they will both steal and destroy goods & gold from the local territory, as well as kill some of its peasant population, proportionate to the number of raiders and how much the surrounding tiles have been raided already (and how many are in range + their productivity). Once their carrying capacity is reached for gold, they will automatically journey home and disband, putting the goods and gold back into the economy.

## Banditry

Armies will, when possible and if their standing order allows it, attack enemy caravans and worker groups. Doing so they will destroy the caravan or worker entity, and get some gold and supply from it. Caravans are considered valid targets according to their origin, meaning an army might attack and despoil a caravan that was heading for the player's own territory.

Caravans that end up in range of an army are intercepted even if the army is not actively chasing them. Active chasing depends on the standing order (Station within leash, Raid, Defend). Loot is capped by the army's carrying capacity. Workers are not targets for now.

## Looting

Every engagement has a Loot Pool (gold + supplies reserve), shared among the victorious armies when the engagement ends.

Pool sources:
- Every army destroyed in the engagement drops all of its gold and supplies into the pool.
- A conquered settlement adds to the pool: 20% of its peasants and 30% of its urban population are killed, each yielding one gold. Buildings are not destroyed for now.

Distribution: among the victorious armies, by manpower and limited by each army's carrying capacity. Leftover loot becomes goods / trade volume for the settlement where the engagement is located (the local population loots the battlefield).

# Supply system

Armies require a certain amount of goods to maintain themselves. They get goods directly by raiding / looting, and indirectly through caravans reaching them to sell.

Armies never hold more gold or supplies than their carrying capacity: anything above it is dropped (into the loot pool when engaged).

**Known weakness, redesign planned** (see `management/work_units/army_supply_redesign_2026-11-07.md`): it is too easy for an army to walk off owned territory, run out of gold and stop getting resupplied.

Armies carry gold. That gold is obtained either through standard pay (when standing in friendly territory and set to be paid, and of course when the owner has the required funds) daily (the army's daily consumption worth of it) or though raiding and looting.

Gold carried by an army can be used in two ways: bringing it back home in the form of goods upon disbanding (some of it going directly to the owner's treasury), or buying supplies from incoming caravans, or the settlement the army is stationned in.

In both cases, goods are taken from the settlement, adding to its trade volume, and added to the army. In exchange, each good is paid for for a specific amount of gold per good, some proportion of it going straight to the settlement's owner's treasury. This is a way a local economy can benefit or suffer from the presence of an army: selling goods to armies creates less profit than selling to settlements, but it still does create some. However, if a settlement is already struggling for goods, feeding an army may put it over the edge: sending a caravan to an army gets a much less severe malus from local scarcity, meaning a settlement close to starvation may still send a caravan out.

Otherwise, armies work as trade targets much the same way settlements do, with the exception that the profit to be gotten from them does not increase with distance to origin, meaning faraway settlements will pretty much never send a caravan towards an army directly. However, consuming goods in a specific area will increase how attractive that area is to trade, creating dynamic supply lines.
