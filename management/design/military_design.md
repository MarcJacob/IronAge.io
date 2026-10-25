# MILITARY DESIGN DOCUMENT

# Intro : GOAL

The goal of the military mechanics is to immerse the player somewhat in the difficulties of medieval warfare: decentralization / difficulty to control large armies, and the logistics challenge of keeping an army fed, with strategic concerns based on sieges and relatively slow maneuvering mixed in with smaller mobile warbands able to live off the land and damage economies, sometimes bringing back a profit home.

The shape of fighting over the course of the game is this:
- Early game dominated by small mobile groups denying colonization of strategic areas, and conducting raids to kickstart economies.
- Middle game dominated by medium-size forces taking part in small-scale warfare to take out weak players and establish regional powers, as well as the first vassalization chains. Larger raids from some players relying on this as an economic strategy, sometimes razing settlements completely.
- Late game dominated by large "realm wars" working around the Campaign mechanics involving many players on either side and influencing the rise and fall of trading centers based on the supply lines that form naturally.

# DESIGN CONCEPTS:

#Projection: The military system seeks to make it difficult to send armies far away from owned / friendly territory past a certain scale. Small armies can live off the land or through raiding, while larger armies cannot only sustain themselves that way and must resupply through other means, usually by spending some of their carried gold (or a lot of raiding).

#Decentralization: While it is possible to control armies directly, doing so is made intentionally tedious and prevents the use of some powerful mechanics like the Campaign system. Furthermore, groups of players are advantaged against individual players if both are of equal overall power, through hard limitations like one player = one Campaign, and one settlement = one campaign participation.

#Economic_ties: The military system is intrinsically linked to the economy and vice versa. The military is an overall weight on the world's economy, but can be used to enrich local economies through the ability to bring back loot both in the owner's treasury and the army's origin settlement goods pool.

#Scales: The design tries pretty intentionally to split warfare between two kinds that use the same mechanics but with different emerging playstyles due to how they interact: Small scale and Large scale. Small scale allows much greater maneuveurability, moving across the map quickly and not reliant / as reliant on supply lines, but not able to achieve major military objectives like conquering a settlement. Large scale is the contrary: it is slow, intentional, and relies on methodical advancement and the execution of slow maneuvers and sieges. Positioning and picking battles careful is key.

# Armies

Armies are the fundamental military entity in the world. An army is tied to a specific settlement it was trained in: Armies can't be comprised of men from different settlements.

They move around using the standard movement mechanics, benefiting from road and struggling off roads depending on terrain.

Armies can attack pretty much any other kind of entity they can reach: other armies ([# Battles]), Caravans ([# Raiding & Looting]), Settlements ([# Sieges]) and Workers ([# Raiding & Looting]).

Armies are controlled through a Standing Order which can either be specifically targeted (Station at X position, attack Y entity) or be more abstract objectives (Defend region, attack / raid region, follow other army...).

The Standing Order and its parameters then determine / influence the Army's behavior.

## Raising an army

Any settlement can raise any number of armies. Those armies are considered to be "lone operators" and must be directed by the player. Envisioned common practice is to use this to raise a militia that defends the settlement, and small armies used for specific missions like small scale raids, or quick intervention forces.

When raising an army, the player is prompted to choose the army's composition ([# Troop Types]), and optionally a start standing order (with the default being to defend the settlement and its territory). The troops are taken from the garrison.

Armies can change their composition at any time while inside the settlement.

## Garrison

By default, every settlement has a single army within it that has special treatment: the Garrison. The Garrison is not a normal army, in the sense that it is not a separate entity from the settlement itself.

When the garrison lacks troops, it uses the settlement's facilities to train more.

Levies remain part of the working population unless the settlement is under siege or *On Campaign*.

# Campaigns

A player can call a campaign at any time, involving at least one army or settlement.

Involved armies immediately get the standing order to join the campaign, then get controlled by the Campaign AI once they are close enough.

Involved settlements turn a portion of their garrison into an army that is then sent to join the campaign. Such armies are known as *campaign armies* and have some special properties:
- They replace losses automatically when in friendly territory.
- They go home automatically when the campaign ends.
- They never outlive the campaign itself - if the campaign ends, they return home and rejoin the garrison. They also cannot be controlled manually, the Campaign AI is in full control of their standing order.

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

