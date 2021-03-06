#include "../lib/types.h"

#ifdef HOOK_CODE
#include "../lib/entity.h"
#include "../lib/gi.h"
#include "game.h"
#include "itemlist.h"
#include "pweapon.h"
#include "phys.h"
#include "combat.h"

constexpr int GRAPPLE_SPEED					= 650; // speed of grapple in flight
constexpr int GRAPPLE_PULL_SPEED			= 650;	// speed player is pulled at

/*------------------------------------------------------------------------*/
/* GRAPPLE																  */
/*------------------------------------------------------------------------*/

// self is grapple, not player
static void GrappleReset(entity &self)
{
	entity &cl = self.owner;

	if (!cl.client->g.grapple.has_value())
		return;

	float volume = 1.0;

	if (cl.client->g.silencer_shots)
		volume = 0.2f;

#ifdef HOOK_STANDARD_ASSETS
	gi.sound (cl, CHAN_RELIABLE | CHAN_WEAPON, gi.soundindex("weapons/Sshotr1b.wav"), volume, ATTN_NORM, 0);
#else
	gi.sound (cl, CHAN_RELIABLE | CHAN_WEAPON, gi.soundindex("weapons/grapple/grreset.wav"), volume, ATTN_NORM, 0);
#endif
	cl.client->g.grapple = 0;
	cl.client->g.grapplereleaseframenum = level.framenum;
	cl.client->g.grapplestate = GRAPPLE_STATE_FLY; // we're firing, not on hook
	cl.client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	G_FreeEdict(self);
}

// ent is player
void GrapplePlayerReset(entity &ent)
{
	if (ent.is_client() && ent.client->g.grapple.has_value())
		GrappleReset(ent.client->g.grapple);
}

static void GrappleTouch(entity &self, entity &other, vector plane, const surface &surf)
{
	float volume = 1.0;

	if (other == self.owner)
		return;

	if (self.owner->client->g.grapplestate != GRAPPLE_STATE_FLY)
		return;

	if (surf.flags & SURF_SKY)
	{
		GrappleReset(self);
		return;
	}

	self.g.velocity = vec3_origin;
#if defined(SINGLE_PLAYER)

	PlayerNoise(self.owner, self.s.origin, PNOISE_IMPACT);
#endif

	if (other.g.takedamage)
	{
		T_Damage (other, self, self.owner, self.g.velocity, self.s.origin, plane, self.g.dmg, 1, DAMAGE_NONE, MOD_GRAPPLE);
		GrappleReset(self);
		return;
	}

	self.owner->client->g.grapplestate = GRAPPLE_STATE_PULL; // we're on hook
	self.g.enemy = other;

	self.solid = SOLID_NOT;

	if (self.owner->client->g.silencer_shots)
		volume = 0.2f;

#ifdef HOOK_STANDARD_ASSETS
	gi.sound (self, CHAN_WEAPON, gi.soundindex("flyer/Flyatck1.wav"), volume, ATTN_NORM, 0);
#else
	gi.sound (self.owner, CHAN_RELIABLE+CHAN_WEAPON, gi.soundindex("weapons/grapple/grpull.wav"), volume, ATTN_NORM, 0);
	gi.sound (self, CHAN_WEAPON, gi.soundindex("weapons/grapple/grhit.wav"), volume, ATTN_NORM, 0);
#endif

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPARKS);
	gi.WritePosition (self.s.origin);
	gi.WriteDir (plane);
	gi.multicast (self.s.origin, MULTICAST_PVS);
}

// draw beam between grapple and self
static void GrappleDrawCable(entity &self)
{
	vector	offset, start, end, f, r;
	vector	dir;
	float	distance;

	AngleVectors(self.owner->client->g.v_angle, &f, &r, nullptr);
	offset = { 16.f, self.g.count ? -16.f : 16.f, self.owner->g.viewheight - 8.f };
	start = P_ProjectSource(self.owner, self.owner->s.origin, offset, f, r);

	offset = start - self.owner->s.origin;
	dir = start - self.s.origin;
	distance = VectorLength(dir);

	// don't draw cable if close
	if (distance < 64)
		return;

	end = self.s.origin;

	gi.WriteByte (svc_temp_entity);
#ifdef HOOK_STANDARD_ASSETS
	gi.WriteByte (TE_PARASITE_ATTACK);
	gi.WriteEntity (self);
	gi.WritePosition (start);
	gi.WritePosition (end);
#else
	gi.WriteByte (TE_GRAPPLE_CABLE);
	gi.WriteEntity (self.owner);
	gi.WritePosition (self.owner.s.origin);
	gi.WritePosition (end);
	gi.WritePosition (offset);
#endif
	gi.multicast (self.s.origin, MULTICAST_PVS);
}

// pull the player toward the grapple
void GrapplePull(entity &self)
{
	vector hookdir, v;
	float vlen;

#ifdef GRAPPLE
	if (self.owner->client->g.pers.weapon &&
		self.owner->client->g.pers.weapon->id == ITEM_GRAPPLE &&
		!self.owner->client->g.newweapon &&
		self.owner->client->g.weaponstate != WEAPON_FIRING &&
		self.owner->client->g.weaponstate != WEAPON_ACTIVATING)
	{
		GrappleReset(self);
		return;
	}
#endif

	if (self.g.enemy.has_value())
	{
		if (self.g.enemy->solid == SOLID_NOT || self.g.enemy->g.deadflag)
		{
			GrappleReset(self);
			return;
		}
		else if (self.g.enemy->solid == SOLID_BBOX)
		{
			v = self.g.enemy->size * 0.5f;
			v += self.g.enemy->s.origin;
			self.s.origin = v + self.g.enemy->mins;
			gi.linkentity (self);
		}
		else
			self.g.velocity = self.g.enemy->g.velocity;
	}

	GrappleDrawCable(self);

	if (self.owner->client->g.grapplestate > GRAPPLE_STATE_FLY)
	{
		// pull player toward grapple
		// this causes icky stuff with prediction, we need to extend
		// the prediction layer to include two new fields in the player
		// move stuff: a point and a velocity.  The client should add
		// that velociy in the direction of the point
		vector forward, up;

		AngleVectors (self.owner->client->g.v_angle, &forward, nullptr, &up);
		v = self.owner->s.origin;
		v[2] += self.owner->g.viewheight;
		hookdir = self.s.origin - v;

		vlen = VectorNormalize(hookdir);

		if (self.owner->client->g.grapplestate == GRAPPLE_STATE_PULL)
		{
			float volume = 1.0;

			if (self.owner->client->g.silencer_shots)
				volume = 0.2f;

			if (vlen < 64)
			{
				self.owner->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
#ifndef HOOK_STANDARD_ASSETS
				gi.sound (self.owner, CHAN_RELIABLE+CHAN_WEAPON, gi.soundindex("weapons/grapple/grhang.wav"), volume, ATTN_NORM, 0);
#endif
				self.owner->client->g.grapplestate = GRAPPLE_STATE_HANG;
			}
#ifdef HOOK_STANDARD_ASSETS
			else if (self.g.pain_debounce_framenum < level.framenum)
			{
				gi.sound (self.owner, CHAN_WEAPON, gi.soundindex("world/turbine1.wav"), volume, ATTN_NORM, 0);
				self.g.pain_debounce_framenum = level.framenum + (gtime)(0.5 * BASE_FRAMERATE);
			}
#endif
		}

		hookdir *= GRAPPLE_PULL_SPEED;
		self.owner->g.velocity = hookdir;
		SV_AddGravity(self.owner);
	}
}

static void CTFFireGrapple(entity &self, vector start, vector dir, int damage, int speed, bool offhand)
{
	VectorNormalize(dir);

	entity &grapple = G_Spawn();
	grapple.s.origin = grapple.s.old_origin = start;
	grapple.s.angles = vectoangles(dir);
	grapple.g.velocity = dir * speed;
	grapple.g.movetype = MOVETYPE_FLYMISSILE;
	grapple.clipmask = MASK_SHOT;
	grapple.solid = SOLID_BBOX;
	grapple.g.count = offhand;
#ifdef HOOK_STANDARD_ASSETS
	grapple.s.modelindex = gi.modelindex ("models/objects/debris2/tris.md2");
#else
	grapple.s.modelindex = gi.modelindex ("models/weapons/grapple/hook/tris.md2");
#endif
	grapple.owner = self;
	grapple.g.touch = GrappleTouch;
	grapple.g.dmg = damage;
	self.client->g.grapple = grapple;
	self.client->g.grapplestate = GRAPPLE_STATE_FLY; // we're firing, not on hook
	gi.linkentity (grapple);

	trace tr = gi.traceline (self.s.origin, grapple.s.origin, grapple, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		grapple.s.origin += dir * -10;
		grapple.g.touch (grapple, tr.ent, vec3_origin, null_surface);
	}
}

static void CTFGrappleFire(entity &ent, int damage, bool offhand)
{
	vector forward, right;
	vector start;
	vector offset;
	float volume = 1.0;

	if (ent.client->g.grapplestate > GRAPPLE_STATE_FLY)
		return; // it's already out

	AngleVectors (ent.client->g.v_angle, &forward, &right, nullptr);
	offset = { 24.f, offhand ? -8.f : 8.f, ent.g.viewheight - 8.f + 2.f };
	start = P_ProjectSource (ent, ent.s.origin, offset, forward, right);

	if (ent.client->g.silencer_shots)
		volume = 0.2f;

#ifdef HOOK_STANDARD_ASSETS
	gi.sound (ent, CHAN_WEAPON, gi.soundindex("medic/Medatck2.wav"), volume, ATTN_NORM, 0);
#else
	gi.sound (ent, CHAN_RELIABLE+CHAN_WEAPON, gi.soundindex("weapons/grapple/grfire.wav"), volume, ATTN_NORM, 0);
#endif
	CTFFireGrapple (ent, start, forward, damage, GRAPPLE_SPEED, offhand);
#if defined(SINGLE_PLAYER)

	PlayerNoise(ent, start, PNOISE_WEAPON);
#endif
}

#ifdef GRAPPLE
static void Weapon_Grapple_Fire(entity &ent)
{
	const int damage = 10;
	CTFGrappleFire (ent, damage, false);
	ent.client->ps.gunframe++;
}

void CTFWeapon_Grapple(entity &ent)
{
	// if the the attack button is still down, stay in the firing frame
	if ((ent.client->g.buttons & BUTTON_ATTACK) && 
		ent.client->g.weaponstate == WEAPON_FIRING && ent.client->g.grapple.has_value())
		ent.client->ps.gunframe = 9;

	if (!(ent.client->g.buttons & BUTTON_ATTACK) && ent.client->g.grapple.has_value())
	{
		GrappleReset(ent.client->g.grapple);
		if (ent.client->g.weaponstate == WEAPON_FIRING)
			ent.client->g.weaponstate = WEAPON_READY;
	}

	if (ent.client->g.newweapon && 
		ent.client->g.grapplestate > GRAPPLE_STATE_FLY &&
		ent.client->g.weaponstate == WEAPON_FIRING)
	{
		// he wants to change weapons while grappled
		ent.client->g.weaponstate = WEAPON_DROPPING;
		ent.client->ps.gunframe = 32;
	}

	const weapon_state prevstate = ent.client->g.weaponstate;
	Weapon_Generic (ent, 5, 9, 31, 36, { 10, 18, 27 }, { 6 }, Weapon_Grapple_Fire);

	// if we just switched back to grapple, immediately go to fire frame
	if (prevstate == WEAPON_ACTIVATING &&
		ent.client->g.weaponstate == WEAPON_READY &&
		ent.client->g.grapplestate > GRAPPLE_STATE_FLY)
	{
		if (!(ent.client->g.buttons & BUTTON_ATTACK))
			ent.client->ps.gunframe = 9;
		else
			ent.client->ps.gunframe = 5;
		ent.client->g.weaponstate = WEAPON_FIRING;
	}
}
#endif

#ifdef OFFHAND_HOOK
void GrappleCmd(entity &ent)
{
	string cmd = strlwr(gi.argv(1));
	
	if (cmd == "fire")
	{
		if (ent.g.health && ent.g.movetype != MOVETYPE_NOCLIP)
			CTFGrappleFire(ent, 10, true);
	}
	else if (cmd == "release")
		GrapplePlayerReset(ent);
};
#endif

#endif