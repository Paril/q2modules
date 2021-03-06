#pragma once

#include "../lib/types.h"
#include "game.h"

constexpr vector MOVEDIR_UP		= { 0, 0, 1 };
constexpr vector MOVEDIR_DOWN	= { 0, 0, -1 };

constexpr size_t BODY_QUEUE_SIZE = 8;

/*
=================
G_InitEdict

Marks an entity as active, and sets up some default parameters.
=================
*/
void G_InitEdict(entity &e);

/*
=================
G_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
entity &G_Spawn();

/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict(entity &e);

constexpr vector G_ProjectSource(const vector &point, const vector &distance, const vector &forward, const vector &right)
{
	return point + (forward * distance.x) + (right * distance.y) + vector(0, 0, distance.z);
}

/*
=============
G_Find

Searches all active entities for the next one that holds
the matching member in the structure.

Searches beginning at the edict after from, or the beginning (after clients) if
from is null/world/a client. null_entity will be returned if the end of the list is reached.
=============
*/
template<typename T>
inline entityref G_Find(entityref from, const T &match, bool (*matcher)(const entity &e, const T &match))
{
	if (!from.has_value() || etoi(from) <= game.maxclients)
		from = itoe(game.maxclients + 1);
	else
		from = next_ent(from);

	for (; etoi(from) < num_entities; from = next_ent(from))
		if (from->inuse && matcher(from, match))
			return from;

	return null_entity;
}

// G_Find using the specified function to match the specified member
#define G_FindFunc(from, member, match, func) \
	G_Find<decltype(match)>((from), match, [](const entity &e, const decltype(match) &m) -> bool { return func(e.member, m); })

// G_Find using a simple wrapper to the == operator of the specified member
#define G_FindEquals(from, member, match) \
	G_Find<decltype(match)>((from), match, [](const entity &e, const decltype(match) &m) -> bool { return e.member == m; })

/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
entityref findradius(entityref from, vector org, float rad);

/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if world.
null_entity will be returned if the end of the list is reached.
=============
*/
entityref G_PickTarget(const stringref &stargetname);

/*
==============================
G_UseTargets

"cactivator" should be set to the entity that initiated the firing.

If ent.delay is set, a DelayedUse entity will be created that will actually
do the G_UseTargets after that many seconds have passed.

Centerprints any ent.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function
==============================
*/
void G_UseTargets(entity &ent, entity &cactivator);

void G_SetMovedir(vector &angles, vector &movedir);

void BecomeExplosion1(entity &self);

void BecomeExplosion2(entity &self);

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
bool KillBox(entity &ent);

/*
=============
visible

returns 1 if the entity is visible to self, even if not infront ()
=============
*/
bool visible(const entity &self, const entity &other);

/*
=============
infront

returns 1 if the entity is in front (in sight) of self
=============
*/
bool infront(const entity &self, const entity &other);

void G_TouchTriggers(entity &ent);