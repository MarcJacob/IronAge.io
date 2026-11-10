# MILITARY DESIGN DOCUMENT

# Intro : GOAL

The goal of the military mechanics is to immerse the player somewhat in the difficulties of medieval warfare: decentralization / difficulty to control large armies, and the logistics challenge of keeping an army fed, with strategic concerns based on sieges and relatively slow maneuvering mixed in with smaller mobile warbands able to live off the land and damage economies, sometimes bringing back a profit home.

The shape of fighting over the course of the game is this:
- Early game dominated by small mobile groups denying colonization of strategic areas, and conducting raids to kickstart economies.
- Middle game dominated by medium-size forces taking part in small-scale warfare to take out weak players and establish regional powers, as well as the first vassalization chains. Larger raids from some players relying on this as an economic strategy, sometimes razing settlements completely.
- Late game dominated by large "realm wars" working around the Campaign mechanics involving many players on either side and influencing the rise and fall of trading centers based on the supply lines that form naturally.

# DESIGN CONCEPTS:

#Projection: The military system seeks to make it difficult to send armies far away from owned / friendly territory past a certain scale. Small armies can live off the land or through raiding, while larger armies cannot only sustain themselves that way and must resupply through other means, usually through campaign supply caravans (or a lot of raiding).

#Decentralization: While it is possible to control armies directly, doing so is made intentionally tedious and encourages the use of some powerful mechanics like the Campaign system. Furthermore, groups of players are advantaged against individual players if both are of equal overall power, through hard limitations like one player = one Campaign, and one settlement = one campaign participation.

#Indirectness: Related to #Decentralization, the precise control of armies is made intentionally difficult or even impossible for some specific operations. The point is for players to very quickly understand that micro management is not the point, but rather general positioning and playing around all the other aspects of war. When two opposing armies approach eachother, both players can be relatively certain a battle will happen unless *both* players decide to stand their ground or retreat. Furthermore, army groups / Campaigns are voluntarily made non-player-controlled in their low-level decision-making, as to give them somewhat predictable behavior both for the target players, as well as any counter-campaign(s) started in reaction.

#Economic_ties: The military system is intrinsically linked to the economy and vice versa. The military is an overall weight on the world's economy, but can be used to enrich local economies through the ability to bring back loot both in the owner's treasury and the army's origin settlement goods pool.

#Scales: The design tries pretty intentionally to split warfare between two kinds that use the same mechanics but with different emerging playstyles due to how they interact: Small scale and Large scale. Small scale allows much greater maneuveurability, moving across the map quickly and not reliant / as reliant on supply lines, but not able to achieve major military objectives like conquering a settlement. Large scale is the contrary: it is slow, intentional, and relies on methodical advancement and the execution of slow maneuvers and sieges. Positioning and picking battles careful is key.

# Armies

Armies are the fundamental military entity in the world. An army is made of troops of various types (see [# Troop Types]).

They move around using the standard movement mechanics, benefiting from road and struggling off roads depending on terrain.

Armies can attack pretty much any other kind of entity they can reach: other armies ([# Battles]), Caravans ([# Raiding & Looting]), Settlements ([# Sieges]) and Workers ([# Raiding & Looting]).

Armies are controlled through a *Standing Order*, which defines that army's current objective. Based on this, the lowest-level control of armies is an automated decision-making systems that seeks to accomlish the given objective while letting the army perform basic self-preservation actions like gathering supplies, and fleeing from overly dangerous forces (unless explicitly told to engage it).

There are two types of armies:

- **Retinues**: armies created directly by the player. They are "lone operators" directed by the player, and the only armies that take direct orders. Envisioned common practice: Small armies for specific missions like small scale raids or quick intervention forces. A retinue can also be sent to join a campaign by order; if the campaign ends, it stays where it is. Retinues are only loosely tied to their home settlement, and can reinforce from anywhere. They are only made up "professional" troops, which are all troop types other than levies.
- **Campaign armies**: armies raised by a settlement for a campaign (see [# Campaigns]). No direct orders: only the campaign logic controls them. They are the armies that go home and disband automatically, mostly made up of levies. They are the backbone of major military operations, used for direct conquest / frontal defense against opposing campaigns.

## Garrison

By default, every settlement has a single army within it that has special treatment: the Garrison. The Garrison is not a normal army, in the sense that it is not a separate entity from the settlement itself.

When the garrison lacks troops, it uses the settlement's facilities to train more. Those facilities also determine how many can be trained at the maximum, counting existing troops both in the garrison still and raised in an army.

Levies in garrison are not actual standing troops but an indication of how many levies will oppose assaults on the settlement, as a function of the non-soldier home population.

# Battles

When two armies hostile to one another meet on the field, they start a Battle.

Battles "capture" all involveable armies in the area, with the size of the battle increasing the more men are involved.

While inside a battle, an army's normal supply mechanics are frozen (*intention: I don't want supplies to matter as a mechanic when battle is engaged, nor should battles last long enough for it to matter, unless the battle lasts so long that the army will probably be out of action anyway*).

Battles end when an entire side has died or withdrawn.

Within a battle, armies keep maneuvering. @TODO(Marc): Positional system for inflicting damage & retreating. Would make ranged units meaningful, and make some systems like bonus damage more natural to implement. Maybe the entire damage system could be army-to-army instead of pooled and applied to entire sides ? A "physicallity" to battles could be neat.

## Battle Rounds

Every N ticks once a battle has begun, a "battle round" runs during which armies on either side inflict damage on the other.

The damage is spread uniformly between the armies according to their troop count (IE if two armies are on one side of a battle that takes 10 damage, one army with 80 men, the other with 20, then the former will take 8 damage while the other takes the other 2).

Damage is then spread between each troop type, also by their numbers in the army. The formula goes like this:

    Casualties = (Incoming damage / rand(1, troop type defense skill)) / rand(1, troop type armor). Both defense values are always at least 1.

    - Defense skill represents the troop's ability to deflect damage actively and "make it harmless". At high values, the effective damage inflicted is often low.
    - Armor represents the troop's resistance to damage that has connected with it, effectively the amount of damage required per casualty. At high values, few troops will die even with high inflicted damage.

Various bonuses / maluses can apply as additive percentages.
Examples of bonuses include:
- Defending in a siege on a settlement that has fortification buildings.
- Bonus damage against specific troop types (modelled as more total damage inflicted upon the enemy).
- Malus to damage for cavalry in hills / mountains.
- ect...

The intention behind this system is to keep it relatively simple - sheer mass of units will work for winning a battle reliably, but high quality troops in a good defense location can be deceptively difficult to kill, not to mention that fewer troops means higher speed. Higher quality troops thus get much greater survivability unless they get overwhelmed.

Further, the types of bonuses an army can get as it fights interacts with the damage, defense and armor stats differently, making different troop types emergently weaker / stronger in specific circumstances.
    - Example: Siege defense slightly lowers defense skill but doubles armor. In this case, high-armor troops like men at arms and knights are far better than high-skill troops like horsemen.
    - Example: A higher total troop count will add a damage bonus due to overwhelming the defenders. That bonus matters less if the defending troops have a high defense skill, while extra armors just "delays the inevitable" linearly.

The combat stats of various unit types also combine with their economic / out-of-battle characteristics: speed, carry capacity and so on.

## Troop types (WIP)

- Levies: Cheap. Weak against everything. Good carry capacity. Medium speed.
    - Damage: 2
    - Defense skill: 2
    - Armor: 2
    - Upkeep: 1 good / day
    - Carry capacity: 10 goods
- Men at arms: Good all-rounder. +50% Bonus damage against horsemen. Medium speed.
    - Damage: 6
    - Defense skill: 4
    - Armor: 8
    - Upkeep: 5 goods / day
    - Carry capacity: 10 goods
- Horsemen: Particularly effective on their own for maneuvering. High speed. +50% bonus against levies.
    - Damage: 8
    - Defense skill: 5
    - Armor: 2
    - Upkeep: 4 goods / day.
    - Carry capacity: 16 goods
- Knights: Good against everything. Expensive. High speed. Reliant on other units to carry supplies.
    - Damage: 16
    - Defense skill: 6
    - Armor: 12
    - Upkeep: 15 goods / day
    - Carry capacity: 15 goods
- Wagons: Ineffective in combat but difficult to kill. Slow speed. Necessary for larger armies to carry enough goods for prolonged sieges / campaigns.
    - Damage: 1
    - Defense skill: 0
    - Armor: 30
    - Upkeep: 1 good / day
    - Carry capacity: 500 goods

# Campaigns

A player can call a campaign at any time, involving at least one army or settlement.

Involved armies immediately get the standing order to join the campaign, then get controlled by the Campaign AI once they are close enough.

Involved settlements turn a certain percentage of their garrison into a campaign army that is then sent to join the campaign. Such armies are known as *campaign armies* and have some special properties:
- A settlement can only have one campaign army at a time. If it is destroyed while the campaign lives on, the settlement notices on the next tick and raises a replacement (if affordable and not under siege).
- They go home automatically when the campaign ends, and disband at home.
- They never outlive the campaign itself unless assigned to a new one.
- They cannot be controlled manually, the Campaign AI is in full control of their standing order.
- They still count in their origin settlement's population.

Allied / Vassal players can join the campaign as well with their own retinues and settlements. Overlords can force this process on their vassal's settlements (but not retinues), including making them join someone else's campaign.

Campaigns exist as their own entity on the map, centered on the "leader army", the army the campaign was started from. When member armies are close enough, they are "joined" in the campaign, and the campaign AI can use them to accomplish its objective(s):
- Conquering / Raizing a set of regions.
- Defending a set of regions.
- Countering a specific enemy campaign / army.

Campaigns are beneficial in multiple ways:
- Streamline the process of raising armies throughout the realm and having them collaborate.
- Easier collaboration between players.
- Limit how messy wars can get with many players by organizing pushes / defenses.
- Shared supply pool that even retinues can take advantage of.

The intention behind the campaign mechanic is to reward multi-player collaboration (@TODO(Marc): Some cost to discourage one player having many campaigns at a time), streamline the war process for large players so micro isn't too intensive, and pace wars so that they alternate between moments of quiet and major battles / sieges as campaigns start / stop.

When an army joins a campaign, it is set to JOIN CAMPAIGN standing order, which has it travel as fast as possible to the campaign / leader army. Once within a radius made of a constant + extra with more armies having joined, it is considered to have joined the campaign. For simplicity, from there an army is never considered out of the campaign if it gets too far away. The campaign is aware of armies that are joining: they are already part of the campaign, but in the "joining" state, so the Campaign AI may elect to wait for armies to arrive.

Campaigns are loosely controllable by the commander (the player who started the campaign), changing its objectives / ending it early.

## Campaign AI

Campaigns are created with a specific set of objectives. They can be changed as the campaign goes on.

Possible objectives:
- Conquer Regions
    - The Campaign AI will move to the area, choosing a start point in friendly territory, gathering enough supplies, and move onto the closest target, sieging them down one by own. It may dispatch armies to conquer other targets in parallel if it believes it is possible.
- Defend regions
    - The Campaign AI will move to the area, choosing a base of operation to start with that is as close as possible to all defense targets, then dispatch armies as needed (or the entire campaign) to counter attackers.
- Defense Campaign, selecting an opposing offensive campaign (or multiple) ?
    - Similar to defend regions, except the focus is specifically to stop one or more enemy campaigns from making progress by opposing them in the field and breaking sieges.
- Raid Regions
    - The Campaign AI will act similarly to the Conquer objective but will only seek to raid and fill the participating armies' gold reserves.

It does so by assigning standing orders to armies participating in the campaign, and controlling the state of active engagements.

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

Armies will, when possible and if their standing order allows it, attack enemy caravans and worker groups. Doing so they will destroy the caravan or interrupt the worker entity (killing the population working on it), and get some gold and supply from it. Caravans are considered valid targets according to their origin, meaning an army might attack and despoil a caravan that was heading for the player's own territory.

Caravans that end up in range of an army are intercepted even if the army is not actively chasing them. Active chasing depends on the standing order (Station within leash, Raid, Defend). Loot is capped by the army's carrying capacity. Workers are not targets for now.

## Looting

Every engagement has loot pool (gold + supplies reserve), shared among the victorious armies when the engagement ends.

Pool sources:
- Every army destroyed in the engagement drops all of its gold and supplies into the pool.
- Dying soldiers add to the pool.
- A conquered settlement adds to the pool: 20% of its peasants and 30% of its urban population are killed, each yielding one gold. Buildings are not destroyed for now.

Distribution: among the victorious armies, by manpower and limited by each army's carrying capacity. Leftover loot becomes goods / trade volume for the settlement where the engagement is located (the local population loots the battlefield).

# Supply system

Armies require a certain amount of goods to maintain themselves. They get goods directly by raiding / looting, and indirectly through caravans reaching them to deliver.

Armies never hold more gold or supplies than their carrying capacity: anything above it is dropped (into the loot pool when engaged).

Armies never spend their carried gold on supplies (carried gold is loot only). All supply costs are paid by the owner's treasury.

In order to get goods, armies have two options: directly buying from the settlement they're stationed in (any army, own-owner settlement, exact tile), or through caravan deliveries (campaign members only). Either way the treasury payment is a pure gold sink: no income to any settlement owner and no trade tax, the settlement only gains trade volume (size and profit).

## Supply caravans

Caravan resupply is a campaign feature: an army is recognized as a trading target by settlements only if it is a campaign member (or is joining one). The campaign coordinates the supply line. Armies outside a campaign that are away from settlements get no supply.

Delivering goods to an army is an altermative mean for a settlement to increase its trade volume, with a slightly different profit margin formula which does not reward distance. This means that armies tend to be supplied by nearby settlements. When a caravan departs to deliver supplies, the supplies are paid for immediately from the army owner's treasury. Settlements send exactly enough caravans for what the army needs, counting caravans already in flight; goods in flight count at 50% in the origin's scarcity. A caravan that loses its army target goes home (it never retargets to another army), and a delivery consumes the caravan.

## Out of supplies

An army with no supplies loses troops to desertion each day, per troop type, in proportion to how unfunded its consumption is. The full rate (10% of the army's maximum troops of that type per day) applies only at zero supplies. This allows armies to be resupplied through a naturally occuring supply lines system, which can of course be cut by enterprising opposing players and well-positioned retinues. It is also why it is a poor idea to bypass enemy settlements on the way to conquering the main target, as they may harbor enemy retinues that will prevent supply caravans from reaching the armies.
