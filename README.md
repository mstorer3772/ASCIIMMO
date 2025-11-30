# ASCIIMMO

ASCIIMMO — an ASCII-art based MMO prototype (sword & sorcery tropes).

This repo contains a very basic set of services to be built out into an MMO.  I'm using GitHub CoPilot to assist me in creating this project.  The goal is to use ProtoBuff in release and JSON in debug for communication between the services and a fairly simple web client.

1. Build the CLI generator:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

2. Run the generator to print a map to stdout:

```bash
./asciimmo-server
```

3. To use the browser client before HTTP is implemented, run the generator and save the map to `client/world.txt`, then serve `client/` with a static server, e.g.: 

```bash
./asciimmo-server client/world.txt
cd client
python3 -m http.server 8000
# open http://localhost:8000 in your browser
```

Development notes:
- The generator is deterministic using `std::mt19937_64` seeded with the provided seed.
- The browser client will attempt to fetch `/world?seed=`; when the server HTTP endpoint is implemented it can be used directly.

If you'd like, I can:
- Implement a basic HTTP endpoint using `cpp-httplib` so the client works immediately.
- Add WebSocket support for realtime updates.
- Create the GitHub repo and push this scaffold.


RPG Design:

Everything with a value is a floating point number.  

= stats = 
Several stat categories (physical, mental, magical).  Within each category, there will be a power, endurance, speed, and control attribute.  Debating having "social" and "spiritual" categories.
Hidden category?  Luck, others (belief, charisma?)

Each point in any given stat is a multiplicative 5% increase over the base/racial value.  A super-brain-critter with 10 int is going to be much smarter than a human with 15 int.

Physical power: strength
Physical speed: agility
Physical endurance: durability (HP)
Physical control: dexterity
Mental power: intelligence
mental speed: wit
mental endurance: memory
mental control: focus
magical power: force
magical speed: weave (better name?)
magical endurance: will
magical control: finese

Each category will have some base value, and let the players modify the individual stats up or down, each up must be paid for with a down.  Max +2 for starting players, more later.

Each category will have several stats that are determined based on the values in that category:
Passive defense: combine endurance and strength
Active defense: combine speed and control
resource: endurance plus a little of everything else

Race/species determines base values, not stats, for all the individal stats.

Magical energy, "mana" is fairly pervasive, though the degree varies with proximity to ley lines.  Mana has aspects (earth air fire water metal wood lightening, lava, others) and tiers (combine lower tier manas in the right ratio to form higher tier mana, with some loss).  A given source (line, dungeon) has a set aspect.  Intersections of ley lines have an additive (or multiplicative?) effect on background mana level.

Mana level directly/linearly affects local monster/beast levels.

Mana infused living things (monsters/beasts/people) will have mana concentrated in different parts of their anatomy, which can be harvested for crafting purposes.  Harvesting people is DEEPLY illegal.

If a monster/beast has a magical bite ability, their teeth will be part of their anatomy that is infused.  Magically improved durability could settle into their hide/muscles/bones (or all of them).  Improved vitality generally settles into the heart and blood.  Sense skills go into that organ.

The ability to identify and harvest viable materials is critical, as is the ability to use those materials in crafting.

Each material will have a set of tags and a mass.  Tags exist in a heirarchy

For typical animals/beasts/monsters
 - structure
 - - bone
 - - muscle
 - - skin
 - - - hide
 - - - scale
 - - - hair
 - - limb
 - - - tail
 - - - wing
 - natural weapon
 - - claw
 - - tooth
 - - horn
 - - hoof
 - viscera
 - - blood
 - - organ
 - - - heart/liver/stomach/lungs
  - sense
 - - eye
 - - ear
 - - nose/snout
 - - tongue
 - mana aspect
 
 For plants:
 - mineral

Debating about having "mana cores" in living things, which would mostly just be tagged with a mana aspect (and "core")


=Skills=
Skills for everything.  Skills are also a hierarchy, like 1st edition Paranoia.  Skills higher up on the tree are slower/more expensive to level, but help across a broader set of skills.

RNG(skill ID & player seed) == skill affinity. 0.5 - 2 (bell curve distribution).  Represents how fast you train in that skill.

===Crafting materials===
- wood
- - by tree
- stem
- leaf
- - by plant
- fruit
- - type
- vegetable
- - type
- seed
- - by plant
- stone
- - type
- crystal
- - type (quartz, rhinestone)
- metal
- - type (iron copper bronze tin etc)
- [mirror animal tags]

Combat
Magic