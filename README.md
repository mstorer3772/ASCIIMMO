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

# RPG Design

Everything with a value is a floating point number.  

## Stats 
Several stat categories (physical, mental, magical).  Within each category, there will be a power, endurance, speed, and control attribute.  Debating having "social" and "spiritual" categories.

Hidden category?  Luck, others (belief, charisma?)

Each point in any given stat is a multiplicative 5% increase over the base/racial value.  A super-brain-critter with 10 int is going to be much smarter than a human with 15 int.

- Physical
  - power: strength
  - speed: agility
  - endurance: durability (HP)
  - control: dexterity
- Mental
  - power: intelligence
  - speed: wit
  - endurance: memory
  - control: focus (or concentration)
- Magical
  - power: force
  - speed: weave (better name?)
  - endurance: will
  - control: finese (better name?)

Each category will have some base value, and let the players modify the individual stats up or down.  During character creation, each up must be paid for with a down.  Max +2 for starting players, more later.

Each category will have several stats that are determined based on the values in that category:
Passive defense: endurance + strength
Active defense: speed + control
Offense Base: strength + speed

Resource: endurance plus a little of everything else
- Physical Resource: Health
- Mental Resource: Attention
- Magical Resource: Mana

Resource gain rate(control + endurance)

Species determines base values, not stats, for all the individal stats.

## Magic

Magical energy, "mana" is fairly pervasive, though the degree varies with proximity to ley lines.  Mana has aspects (earth air fire water metal wood lightening, lava, others) and tiers (combine lower tier manas in the right ratio to form higher tier mana, with some loss).  A given source (line, dungeon) has a set aspect.  Intersections of ley lines have an additive (or multiplicative?) effect on background mana level.

Mana level directly/linearly affects local monster/beast levels.  It's uncomfortable or even damaging to exist in a mana level too far below your own level (times some factor)

### Manas

Like many things in ASCIIMMO, mana has categories and subcategories.
- Life (health, growth)
  - Plant
    - specise 
  - Animal
    - specise 
  - Fungus
    - specise 
- Mind
  - Decision
    - Control
  - information (scrying)
  - Communication
- Sense
  - Sight
    - Enhance
    - Decieve
  - Sound
    - Enhance
    - Decieve
  - Taste
    - Enhance
    - Decieve
  - Touch
    - Enhance
    - Decieve
  - Smell
    - Enhance
    - Decieve
  - Balance/Body position
    - Enhance
    - Decieve
- Death (drain, wither)
  - Unlife
  - Soul (resurrect, imprison, enhance, enslave)
  - Poison
  - Disease
  - Curse
- Heat (add energy to something)
  - Fire
  - Plasma
  - Light
- Cold (ake energy from something)
  - Ice
  - Deceleration/weaken (debuffs galore)
- Earth (solids in general)
  - Metal
  - Stone
  - Crystal
- Air (gases in general)
  - Weather
    - Storm
  - oxygen, hydrogen, etc
- Water (liquids in general?  Liquid stone?)
  - Fresh
  - Salt
- Continuum
  - Space
  - Time
- Raw (not really a type?)
  - physical force (shields, attacks)

### Mana Affinities
Players have different base affinities for different manas.  Affinities can be improved via immersion in the substance associated with that mana.  Initial improvements can come from skin contact, while at some point one must be completely immersed/surrounded, and finally must transform into that substance.  Longer works better.

That means it's trivially easy to get good at life/animal mana.  We're already transformed into it.  Basic self healing is nigh-universal.  Also easy to get decent at air/water/earth.

Some manas are just hard to learn.  Continuum/space/time.

## Crafting

When crafting something, the ability of the crafter is determined by the intersection of their skill with the material and their output skill (ex: bone bow, cow hide & wood shield).

Mana infused living things (monsters/beasts/people) will have mana concentrated in different parts of their anatomy, which can be harvested for crafting purposes.  Harvesting people is DEEPLY illegal, and not hard to detect.

If a monster/beast has a magical bite ability, their teeth will be part of their anatomy that is infused.  Magically improved durability could settle into their hide/muscles/bones (or all of them).  Improved vitality generally settles into the heart and blood.  Sense skills go into that organ.

The ability to identify and harvest viable materials is critical, as is the ability to use those materials in crafting.

Each material will have a set of tags and a mass.  Tags exist in a heirarchy

For typical animals/beasts/monsters
- structure
  - bone
  - muscle
  - skin
    - hide
    - scale
    - hair
  - limb
    - tail
    - wing
- natural weapon
  - claw
  - tooth
  - horn
  - hoof
- viscera
  - blood
  - organ
    - heart/liver/stomach/lungs
- sense
  - eye
  - ear
  - nose/snout
  - tongue
- mana aspect
  (specific to species)

For plants:
- wood
  - limb
    - branch
- stem (bush/fern/)
- root
- seed
- pulp (suclents/cacti) 

Debating about having "mana cores" in living things, which would mostly just be tagged with a mana aspect (and "core")


## Skills
Skills for everything.  Skills are also a hierarchy, like 1st edition Paranoia.  Skills higher up on the tree are slower/more expensive to level, but help across a broader set of applications.  All the levels from application point (the specific skill that applies in the given situation) all the way to the base stat add up to determine how good someone is at a given task.

Skills are based on a given attribute (possibly affected by a weighted average with others), and specialize from there.  

RNG(skill ID & player seed) == skill affinity (statistical bell curve distribution).  Represents how fast you train in that skill.  1 deviation +-50%, 2: 100%, 3: 200% (cap at 3 diviations)

### 

- Physical Power
  - growth (how good you are at unguided training)
  - strike
    - armed
      - weapon category
        - specific weapon type
          - named weapon
    - unarmed
  - lift
  - leap (w/agility)
- Pysical Speed
  - growth (how good you are at unguided training)
  - run
    - flee
      - biome (cave, woods, urban)
    - pursue
      - biome
  - ride
    - by animal/vehicle
      - by biome
- Physical Endurance
  - growth (how good you are at unguided training)
  - Resist physical debilitation (debuf resistance)
    - by stat
  (treat long term exhastion as a debuf?)
- Physical Control
  - manual dexterity
  - balance
  - footwork
  - acrobatics
  - stealth
- Determined Passive defense:
  - Resist Physical damage
    - by damage type (cut, pierce, crush, burn, acid)
- Dermined physical active defense:
  - Dodge (+ phys speed)
  - Parry (+ phys control)
  - Block (+ phys power)
- Mental
  - Mental Power: Intelligence
    - Growth
    - Teach
      - by skill... basically clone the entire rest of the skill tree here.  Combines with own ability at that skill.  Hard to teach what you don't know, but not impossible.
    - read/write
    - math
    - experiment (for discovering/developing crafting recipes and spells)
      - by mana skill
      - by spell effect skill
      - by craft skill
      - by craft material skill
  - Mental Edurance:
    - Growth
    - Meditation
      - Mana affinity improvement
      - Mana gathering
    - Resist Mental Debufs (pain is mental)
      - by type (including mental exhaustion/sleep deprivation)
    - Craft
      - Item produced (category)
        - item produced (type)
          - specific recipe
      - Materials (dupe material tag tree)
  - Mental Speed:
    - Growth
    - Notice/search?
    - lie
      - act
    - Split attention (improves mental resource)
      - combat awareness (keeping track of multiple foes)


### Crafting materials
- wood
  - by tree
- stem
- leaf
  - by plant
- fruit
  - type
- vegetable
  - type
- seed
  - by plant
- stone
  - type
- crystal
  - type (quartz, rhinestone)
- metal
  - type (iron copper bronze tin etc)
- [mirror animal tags]

## Combat

Active defense skill vs offense skill. Base accuracy vs flat footed/unaware: 75 %

Penalty for aiming at specific targets (later!)
If a blow lands, how-much-it-lands-by affects damage 
  Lands by X starts to add critical effects (bleed damage over time, broken bone debufs, depends on damage type)
