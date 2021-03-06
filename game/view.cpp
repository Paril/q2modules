#include "../lib/types.h"
#include "../lib/entity.h"
#include "../lib/gi.h"
#include "game.h"
#include "combat.h"
#include "view.h"
#include "itemlist.h"
#include "hud.h"
#include "m_player.h"

constexpr float FALL_TIME	= 0.3f;

static vector forward, right, up;
static float xyspeed, bobmove, bobfracsin;
static int32_t bobcycle;

/*
=============
P_WorldEffects
=============
*/
static inline void P_WorldEffects(entity &current_player)
{
	bool	breather;
	bool	envirosuit;
	int32_t	current_waterlevel, old_waterlevel;

	if (current_player.g.movetype == MOVETYPE_NOCLIP)
	{
		current_player.g.air_finished_framenum = level.framenum + 12 * BASE_FRAMERATE; // don't need air
		return;
	}

	current_waterlevel = current_player.g.waterlevel;
	old_waterlevel = current_player.client->g.old_waterlevel;
	current_player.client->g.old_waterlevel = current_waterlevel;

	breather = current_player.client->g.breather_framenum > level.framenum;
	envirosuit = current_player.client->g.enviro_framenum > level.framenum;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && current_waterlevel)
	{
#ifdef SINGLE_PLAYER
		PlayerNoise(current_player, current_player.s.origin, PNOISE_SELF);
#endif

		if (current_player.g.watertype & CONTENTS_LAVA)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/lava_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player.g.watertype & CONTENTS_SLIME)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player.g.watertype & CONTENTS_WATER)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		current_player.g.flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player.g.damage_debounce_framenum = level.framenum - 1 * BASE_FRAMERATE;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && !current_waterlevel)
	{
#ifdef SINGLE_PLAYER
		PlayerNoise(current_player, current_player.s.origin, PNOISE_SELF);
#endif
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
		current_player.g.flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != 3 && current_waterlevel == 3)
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_un.wav"), 1, ATTN_NORM, 0);

	//
	// check for head just coming out of water
	//
	if (old_waterlevel == 3 && current_waterlevel != 3)
	{
		if (current_player.g.air_finished_framenum < level.framenum)
#ifdef SINGLE_PLAYER
		{
#endif
			// gasp for air
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp1.wav"), 1, ATTN_NORM, 0);
#ifdef SINGLE_PLAYER
			PlayerNoise(current_player, current_player.s.origin, PNOISE_SELF);
		}
#endif
		else if (current_player.g.air_finished_framenum < level.framenum + 11 * BASE_FRAMERATE)
			// just break surface
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp2.wav"), 1, ATTN_NORM, 0);
	}

	//
	// check for drowning
	//
	if (current_waterlevel == 3)
	{
		// breather or envirosuit give air
		if (breather || envirosuit)
		{
			current_player.g.air_finished_framenum = level.framenum + 10 * BASE_FRAMERATE;

			if (((int32_t)(current_player.client->g.breather_framenum - level.framenum) % 25) == 0)
			{
				if (!current_player.client->g.breather_sound)
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_NORM, 0);

				current_player.client->g.breather_sound = !current_player.client->g.breather_sound;
#ifdef SINGLE_PLAYER
				PlayerNoise(current_player, current_player.s.origin, PNOISE_SELF);
#endif
			}
		}

		// if out of air, start drowning
		if (current_player.g.air_finished_framenum < level.framenum)
		{
			// drown!
			if (current_player.client->g.next_drown_framenum < level.framenum
				&& current_player.g.health > 0)
			{
				current_player.client->g.next_drown_framenum = level.framenum + 1 * BASE_FRAMERATE;

				// take more damage the longer underwater
				current_player.g.dmg += 2;
				if (current_player.g.dmg > 15)
					current_player.g.dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player.g.health <= current_player.g.dmg)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/drown1.wav"), 1, ATTN_NORM, 0);
				else if (Q_rand() & 1)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp2.wav"), 1, ATTN_NORM, 0);

				current_player.g.pain_debounce_framenum = level.framenum;

				T_Damage(current_player, world, world, vec3_origin, current_player.s.origin, vec3_origin, current_player.g.dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	}
	else
	{
		current_player.g.air_finished_framenum = level.framenum + 12 * BASE_FRAMERATE;
		current_player.g.dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if (current_waterlevel && (current_player.g.watertype & (CONTENTS_LAVA | CONTENTS_SLIME)))
	{
		if (current_player.g.watertype & CONTENTS_LAVA)
		{
			if (current_player.g.health > 0
				&& current_player.g.pain_debounce_framenum <= level.framenum
				&& current_player.client->g.invincible_framenum < level.framenum)
			{
				if (Q_rand() & 1)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);
				current_player.g.pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
			}

			if (envirosuit) // take 1/3 damage with envirosuit
				T_Damage(current_player, world, world, vec3_origin, current_player.s.origin, vec3_origin, 1 * current_waterlevel, 0, DAMAGE_NONE, MOD_LAVA);
			else
				T_Damage(current_player, world, world, vec3_origin, current_player.s.origin, vec3_origin, 3 * current_waterlevel, 0, DAMAGE_NONE, MOD_LAVA);
		}

			// no damage from slime with envirosuit
		if ((current_player.g.watertype & CONTENTS_SLIME) && !envirosuit)
			T_Damage(current_player, world, world, vec3_origin, current_player.s.origin, vec3_origin, 1 * current_waterlevel, 0, DAMAGE_NONE, MOD_SLIME);
	}
}

/*
===============
SV_CalcRoll
===============
*/
static inline float SV_CalcRoll(vector velocity)
{
	float side = velocity * right;
	float sign = side < 0.f ? -1.f : 1.f;
	side = fabs(side);
	const float &value = sv_rollangle.value;

	if (side < (float)sv_rollspeed)
		side = side * value / (float)sv_rollspeed;
	else
		side = value;

	return side * sign;
}

/*
=================
P_FallingDamage
=================
*/
static inline void P_FallingDamage(entity &ent)
{
	float	delta;
	int32_t	damage;
	vector	dir;

	if (ent.s.modelindex != MODEL_PLAYER)
		return;     // not in the player model

	if (ent.g.movetype == MOVETYPE_NOCLIP)
		return;

	if ((ent.client->g.oldvelocity[2] < 0.f) && (ent.g.velocity[2] > ent.client->g.oldvelocity[2]) && !ent.g.groundentity.has_value())
		delta = ent.client->g.oldvelocity[2];
	else if (!ent.g.groundentity.has_value())
		return;
	else
		delta = ent.g.velocity.z - ent.client->g.oldvelocity[2];

	delta = delta * delta * 0.0001f;

#ifdef HOOK_CODE
	// never take damage if just release grapple or on grapple
	if ((level.framenum - ent.client->g.grapplereleaseframenum) <= (2 * BASE_FRAMERATE) ||
		(ent.client->g.grapple.has_value() && 
		ent.client->g.grapplestate > GRAPPLE_STATE_FLY))
		return;
#endif

	// never take falling damage if completely underwater
	if (ent.g.waterlevel == 3)
		return;
	if (ent.g.waterlevel == 2)
		delta *= 0.25f;
	if (ent.g.waterlevel == 1)
		delta *= 0.5f;

	if (delta < 1)
		return;

	if (delta < 15)
	{
		ent.s.event = EV_FOOTSTEP;
		return;
	}

	ent.client->g.fall_value = min(40.f, delta * 0.5f);
	ent.client->g.fall_time = level.time + FALL_TIME;

	if (delta > 30)
	{
		if (ent.g.health > 0)
		{
			if (delta >= 55)
				ent.s.event = EV_FALLFAR;
			else
				ent.s.event = EV_FALL;
		}
		ent.g.pain_debounce_framenum = level.framenum;   // no normal pain sound
		damage = (int32_t)((delta - 30) / 2);
		if (damage < 1)
			damage = 1;
		dir = { 0, 0, 1 };

		if (!((dm_flags)dmflags & DF_NO_FALLING))
			T_Damage(ent, world, world, dir, ent.s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MOD_FALLING);
	}
	else
	{
		ent.s.event = EV_FALLSHORT;
		return;
	}
}

constexpr vector power_color = { 0.0, 1.0, 0.0 };
constexpr vector acolor = { 1.0, 1.0, 1.0 };
constexpr vector bcolor = { 1.0, 0.0, 0.0 };

/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
static void P_DamageFeedback(entity &player)
{
	float	side;
	float	realcount, dcount, kick;
	vector	v;
	int32_t	r, l;

	// flash the backgrounds behind the status numbers
	player.client->ps.stats[STAT_FLASHES] = 0;
	if (player.client->g.damage_blood)
		player.client->ps.stats[STAT_FLASHES] |= 1;
	if (player.client->g.damage_armor && !(player.g.flags & FL_GODMODE) && (player.client->g.invincible_framenum <= level.framenum))
		player.client->ps.stats[STAT_FLASHES] |= 2;

	// total points of damage shot at the player this frame
	dcount = (float)(player.client->g.damage_blood + player.client->g.damage_armor + player.client->g.damage_parmor);
	if (!dcount)
		return;     // didn't take any damage

	// start a pain animation if still in the player model
	if (player.client->g.anim_priority < ANIM_PAIN && player.s.modelindex == MODEL_PLAYER)
	{
		static int32_t i;

		player.client->g.anim_priority = ANIM_PAIN;
		if (player.client->ps.pmove.pm_flags & PMF_DUCKED)
		{
			player.s.frame = FRAME_crpain1 - 1;
			player.client->g.anim_end = FRAME_crpain4;
		}
		else
		{
			i = (i + 1) % 3;
			switch (i) {
			case 0:
				player.s.frame = FRAME_pain101 - 1;
				player.client->g.anim_end = FRAME_pain104;
				break;
			case 1:
				player.s.frame = FRAME_pain201 - 1;
				player.client->g.anim_end = FRAME_pain204;
				break;
			case 2:
				player.s.frame = FRAME_pain301 - 1;
				player.client->g.anim_end = FRAME_pain304;
				break;
			}
		}
	}

	realcount = dcount;
	if (dcount < 10)
		dcount = 10.f; // always make a visible effect

	// play an apropriate pain sound
	if ((level.framenum > player.g.pain_debounce_framenum) && !(player.g.flags & FL_GODMODE) && (player.client->g.invincible_framenum <= level.framenum))
	{
		r = 1 + (Q_rand() & 1);
		player.g.pain_debounce_framenum = (int)(level.framenum + 0.7f * BASE_FRAMERATE);
		if (player.g.health < 25)
			l = 25;
		else if (player.g.health < 50)
			l = 50;
		else if (player.g.health < 75)
			l = 75;
		else
			l = 100;
		gi.sound(player, CHAN_VOICE, gi.soundindex(va("*pain%i_%i.wav", l, r)), 1, ATTN_NORM, 0);
	}

	// the total alpha of the blend is always proportional to count
	if (player.client->g.damage_alpha < 0)
		player.client->g.damage_alpha = 0;
	player.client->g.damage_alpha += dcount * 0.01f;
	if (player.client->g.damage_alpha < 0.2f)
		player.client->g.damage_alpha = 0.2f;
	if (player.client->g.damage_alpha > 0.6f)
		player.client->g.damage_alpha = 0.6f;    // don't go too saturated

	// the color of the blend will vary based on how much was absorbed
	// by different armors
	v = vec3_origin;
	if (player.client->g.damage_parmor)
		v += ((float)player.client->g.damage_parmor / realcount * power_color);
	if (player.client->g.damage_armor)
		v += ((float)player.client->g.damage_armor / realcount *  acolor);
	if (player.client->g.damage_blood)
		v += ((float)player.client->g.damage_blood / realcount *  bcolor);
	player.client->g.damage_blend = v;


	//
	// calculate view angle kicks
	//
	kick = (float)fabs(player.client->g.damage_knockback);
	// kick of 0 means no view adjust at all
	if (kick && player.g.health > 0)
	{
		kick = kick * 100 / player.g.health;

		if (kick < dcount * 0.5f)
			kick = dcount * 0.5f;
		if (kick > 50)
			kick = 50.f;

		v = player.client->g.damage_from - player.s.origin;
		VectorNormalize(v);

		side = v * right;
		player.client->g.v_dmg_roll = kick * side * 0.3f;

		side = -(v * forward);
		player.client->g.v_dmg_pitch = kick * side * 0.3f;

		player.client->g.v_dmg_time = level.time + DAMAGE_TIME;
	}

	//
	// clear totals
	//
	player.client->g.damage_blood = 0;
	player.client->g.damage_armor = 0;
	player.client->g.damage_parmor = 0;
	player.client->g.damage_knockback = 0;
}

/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
static inline void SV_CalcViewOffset(entity &ent)
{
	float	bob;
	float	ratio;
	float	delta;
	vector	v;

//===================================

	// if dead, fix the angle and don't add any kick
	if (ent.g.deadflag)
	{
		ent.client->ps.kick_angles = vec3_origin;

		ent.client->ps.viewangles[ROLL] = 40.f;
		ent.client->ps.viewangles[PITCH] = -15.f;
		ent.client->ps.viewangles[YAW] = ent.client->g.killer_yaw;
	}
	else
	{
		// add angles based on weapon kick

		ent.client->ps.kick_angles = ent.client->g.kick_angles;

		// add angles based on damage kick

		ratio = (ent.client->g.v_dmg_time - level.time) / DAMAGE_TIME;
		if (ratio < 0)
		{
			ratio = 0;
			ent.client->g.v_dmg_pitch = 0;
			ent.client->g.v_dmg_roll = 0;
		}
		ent.client->ps.kick_angles[PITCH] += ratio * ent.client->g.v_dmg_pitch;
		ent.client->ps.kick_angles[ROLL] += ratio * ent.client->g.v_dmg_roll;

		// add pitch based on fall kick

		ratio = (ent.client->g.fall_time - level.time) / FALL_TIME;
		if (ratio < 0)
			ratio = 0;
		ent.client->ps.kick_angles[PITCH] += ratio * ent.client->g.fall_value;

		// add angles based on velocity

		delta = ent.g.velocity * forward;
		ent.client->ps.kick_angles[PITCH] += delta * (float)run_pitch;

		delta = ent.g.velocity * right;
		ent.client->ps.kick_angles[ROLL] += delta * (float)run_roll;

		// add angles based on bob

		delta = bobfracsin * (float)bob_pitch * xyspeed;
		if (ent.client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;     // crouching
		ent.client->ps.kick_angles[PITCH] += delta;
		delta = bobfracsin * (float)bob_roll * xyspeed;
		if (ent.client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;     // crouching
		if (bobcycle & 1)
			delta = -delta;
		ent.client->ps.kick_angles[ROLL] += delta;
	}

//===================================

	// base origin

	v = vec3_origin;

	// add view height

	v.z += ent.g.viewheight;

	// add fall height

	ratio = (ent.client->g.fall_time - level.time) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	v.z -= ratio * ent.client->g.fall_value * 0.4f;

	// add bob height

	bob = bobfracsin * xyspeed * (float)bob_up;
	if (bob > 6)
		bob = 6.f;

	v.z += bob;

	// add kick offset
	v += ent.client->g.kick_origin;

	// absolutely bound offsets
	// so the view can never be outside the player box
	if (v.x < -14)
		v.x = -14.f;
	else if (v.x > 14)
		v.x = 14.f;
	if (v.y < -14)
		v.y = -14.f;
	else if (v.y > 14)
		v.y = 14.f;
	if (v.z < -22)
		v.z = -22.f;
	else if (v.z > 30)
		v.z = 30.f;

	ent.client->ps.viewoffset = v;
}

/*
==============
SV_CalcGunOffset
==============
*/
static inline void SV_CalcGunOffset(entity &ent)
{
	// gun angles from bobbing
#ifdef GROUND_ZERO
	//ROGUE - heatbeam shouldn't bob so the beam looks right
	if (ent.client.pers.weapon && ent.client.pers.weapon->id != ITEM_PLASMA_BEAM)
	{
#endif
		ent.client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005f;
		ent.client->ps.gunangles[YAW] = xyspeed * bobfracsin * 0.01f;

		if (bobcycle & 1)
		{
			ent.client->ps.gunangles[ROLL] = -ent.client->ps.gunangles[ROLL];
			ent.client->ps.gunangles[YAW] = -ent.client->ps.gunangles[YAW];
		}

		ent.client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005f;

		// gun angles from delta movement
		for (int32_t i = 0; i < 3; i++)
		{
			float delta = ent.client->g.oldviewangles[i] - ent.client->ps.viewangles[i];
			if (delta > 180)
				delta -= 360.f;
			if (delta < -180)
				delta += 360.f;
			if (delta > 45)
				delta = 45.f;
			if (delta < -45)
				delta = -45.f;
			if (i == YAW)
				ent.client->ps.gunangles[ROLL] += 0.1f * delta;
			ent.client->ps.gunangles[i] += 0.2f * delta;
		}

#ifdef GROUND_ZERO
	}
	else
		ent.client.ps.gunangles = vec3_origin;
#endif

	// gun height
	ent.client->ps.gunoffset = vec3_origin;
}

/*
=============
SV_AddBlend
=============
*/
static inline void SV_AddBlend(vector rgb, float a, float *v_blend)
{
	float a2 = v_blend[3] + (1 - v_blend[3]) * a; // new total alpha
	float a3 = v_blend[3] / a2;   // fraction of color from old
	
	*((vector *)v_blend) = (*((vector *)v_blend) * a3) + (rgb * (1 - a3));

	v_blend[3] = a2;
}

constexpr vector lava_blend = { 1.0, 0.3f, 0.0 };
constexpr vector bonus_blend = { 0.85f, 0.7f, 0.3f };
constexpr vector slime_blend = { 0.0, 0.1f, 0.05f };
constexpr vector water_blend = { 0.5f, 0.3f, 0.2f };
constexpr vector quad_blend = { 0, 0, 1 };
constexpr vector invul_blend = { 1, 1, 0 };
constexpr vector enviro_blend = { 0, 1, 0 };
constexpr vector breather_blend = { 0.4f, 1, 0.4f };

#ifdef THE_RECKONING
constexpr vector quadfire_blend = '1 0.2 0.5';
#endif

#ifdef GROUND_ZERO
constexpr vector double_blend = '0.9 0.7 0';
constexpr vector nuke_blend = '1 1 1';
constexpr vector ir_blend = '1 0 0';
#endif

/*
=============
SV_CalcBlend
=============
*/
static inline void SV_CalcBlend(entity &ent)
{
	content_flags	contents;
	vector			vieworg;
	gtime			remaining;

	ent.client->ps.blend = { 0, 0, 0, 0 };

	// add for contents
	vieworg = ent.s.origin + ent.client->ps.viewoffset;
	contents = gi.pointcontents(vieworg);
	if (contents & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER))
		ent.client->ps.rdflags |= RDF_UNDERWATER;
	else
		ent.client->ps.rdflags &= ~RDF_UNDERWATER;

	if (contents & (CONTENTS_SOLID | CONTENTS_LAVA))
		SV_AddBlend(lava_blend, 0.6f, &ent.client->ps.blend[0]);
	else if (contents & CONTENTS_SLIME)
		SV_AddBlend(slime_blend, 0.6f, &ent.client->ps.blend[0]);
	else if (contents & CONTENTS_WATER)
		SV_AddBlend(water_blend, 0.4f, &ent.client->ps.blend[0]);

	// add for powerups
	if (ent.client->g.quad_framenum > level.framenum)
	{
		remaining = ent.client->g.quad_framenum - level.framenum;
		if (remaining == 30)    // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4))
			SV_AddBlend(quad_blend, 0.08f, &ent.client->ps.blend[0]);
	}
#ifdef THE_RECKONING
	// RAFAEL
	else if (ent.client.quadfire_framenum > level.framenum)
	{
		remaining = ent.client.quadfire_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/quadfire2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (quadfire_blend, 0.08f, &ent.client.ps.blend[0]);
	}
#endif
#ifdef GROUND_ZERO
	else if (ent.client.double_framenum > level.framenum)
	{
		remaining = ent.client.double_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (double_blend, 0.08f, &ent.client.ps.blend[0]);
	}
#endif
	else if (ent.client->g.invincible_framenum > level.framenum)
	{
		remaining = ent.client->g.invincible_framenum - level.framenum;
		if (remaining == 30)    // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4))
			SV_AddBlend(invul_blend, 0.08f, &ent.client->ps.blend[0]);
	}
	else if (ent.client->g.enviro_framenum > level.framenum)
	{
		remaining = ent.client->g.enviro_framenum - level.framenum;
		if (remaining == 30)    // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4))
			SV_AddBlend(enviro_blend, 0.08f, &ent.client->ps.blend[0]);
	}
	else if (ent.client->g.breather_framenum > level.framenum)
	{
		remaining = ent.client->g.breather_framenum - level.framenum;
		if (remaining == 30)    // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4))
			SV_AddBlend(breather_blend, 0.04f, &ent.client->ps.blend[0]);
	}

#ifdef GROUND_ZERO
	if(ent.client.nuke_framenum > level.framenum)
	{
		float brightness = (ent.client.nuke_framenum - level.framenum) / 20.0;
		SV_AddBlend (nuke_blend, brightness, &ent.client.ps.blend[0]);
	}

	if (ent.client.ir_framenum > level.framenum)
	{
		remaining = ent.client.ir_framenum - level.framenum;
		if(remaining > 30 || (remaining & 4))
		{
			ent.client.ps.rdflags |= RDF_IRGOGGLES;
			SV_AddBlend (ir_blend, 0.2, &ent.client.ps.blend[0]);
		}
		else
			ent.client.ps.rdflags &= ~RDF_IRGOGGLES;
	}
	else
		ent.client.ps.rdflags &= ~RDF_IRGOGGLES;
#endif

	// add for damage
	if (ent.client->g.damage_alpha > 0)
		SV_AddBlend(ent.client->g.damage_blend, ent.client->g.damage_alpha, &ent.client->ps.blend[0]);

	if (ent.client->g.bonus_alpha > 0)
		SV_AddBlend(bonus_blend, ent.client->g.bonus_alpha, &ent.client->ps.blend[0]);

	// drop the damage value
	ent.client->g.damage_alpha -= 0.06f;
	if (ent.client->g.damage_alpha < 0)
		ent.client->g.damage_alpha = 0;

	// drop the bonus value
	ent.client->g.bonus_alpha -= 0.1f;
	if (ent.client->g.bonus_alpha < 0)
		ent.client->g.bonus_alpha = 0;
}

/*
===============
G_SetClientEvent
===============
*/
static inline void G_SetClientEvent(entity &ent)
{
	if (ent.s.event)
		return;

	if (ent.g.groundentity.has_value() && xyspeed > 225)
		if ((int32_t)(ent.client->g.bobtime + bobmove) != bobcycle)
			ent.s.event = EV_FOOTSTEP;
}

/*
===============
G_SetClientEffects
===============
*/
static inline void G_SetClientEffects(entity &ent)
{
	gtime	remaining;

	ent.s.effects = EF_NONE;
	ent.s.renderfx = RF_IR_VISIBLE;

	if (ent.g.health <= 0 || level.intermission_framenum)
		return;

	if (ent.g.powerarmor_framenum > level.framenum)
	{
		gitem_id pa_type = PowerArmorType(ent);
		if (pa_type == ITEM_POWER_SCREEN)
			ent.s.effects |= EF_POWERSCREEN;
		else if (pa_type == ITEM_POWER_SHIELD)
		{
			ent.s.effects |= EF_COLOR_SHELL;
			ent.s.renderfx |= RF_SHELL_GREEN;
		}
	}

	if (ent.client->g.quad_framenum > level.framenum)
	{
		remaining = ent.client->g.quad_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4))
			ent.s.effects |= EF_QUAD;
	}

#ifdef THE_RECKONING
	// RAFAEL
	if (ent.client.quadfire_framenum > level.framenum)
	{
		remaining = ent.client.quadfire_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			ent.s.effects |= EF_QUAD;
	}
#endif

#ifdef GROUND_ZERO
	if (ent.client.double_framenum > level.framenum)
	{
		remaining = ent.client.double_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			ent.s.effects |= EF_DOUBLE;
	}

	if (ent.client.tracker_pain_framenum > level.framenum)
		ent.s.effects |= EF_TRACKERTRAIL;
#endif

	if (ent.client->g.invincible_framenum > level.framenum)
	{
		remaining = ent.client->g.invincible_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4))
			ent.s.effects |= EF_PENT;
	}

	// show cheaters!!!
	if (ent.g.flags & FL_GODMODE)
	{
		ent.s.effects |= EF_COLOR_SHELL;
		ent.s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	}

#ifdef CTF
	CTFEffects(ent);
#endif
}

/*
===============
G_SetClientSound
===============
*/
static inline void G_SetClientSound(entity &ent)
{
#ifdef SINGLE_PLAYER
	if (ent.client.pers.game_helpchanged != game.helpchanged)
	{
		ent.client.pers.game_helpchanged = game.helpchanged;
		ent.client.pers.helpchanged = 1;
	}

	// help beep (no more than three times)
	if (ent.client.pers.helpchanged && ent.client.pers.helpchanged <= 3 && !(level.framenum & 63))
	{
		ent.client.pers.helpchanged++;
		gi.sound(ent, CHAN_VOICE, gi.soundindex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
	}
#endif

	ent.s.sound = SOUND_NONE;

	if (ent.g.waterlevel && (ent.g.watertype & (CONTENTS_LAVA | CONTENTS_SLIME)))
		ent.s.sound = snd_fry;
	else if (ent.client->g.weapon_sound)
		ent.s.sound = ent.client->g.weapon_sound;
	else if (ent.client->g.pers.weapon)
	{
		gitem_id weap = ent.client->g.pers.weapon->id;

		if (weap == ITEM_RAILGUN)
			ent.s.sound = gi.soundindex("weapons/rg_hum.wav");
		else if (weap == ITEM_BFG)
			ent.s.sound = gi.soundindex("weapons/bfg_hum.wav");
	#ifdef THE_RECKONING
		else if (weap == ITEM_PHALANX)
			ent.s.sound = gi.soundindex("weapons/phaloop.wav");
	#endif
	}
}

/*
===============
G_SetClientFrame
===============
*/
static inline void G_SetClientFrame(entity &ent)
{
	if (ent.s.modelindex != MODEL_PLAYER)
		return;     // not in the player model

	const bool duck = ent.client->ps.pmove.pm_flags & PMF_DUCKED;
	const bool run = xyspeed;

	// check for stand/duck and stop/go transitions
	if (duck != ent.client->g.anim_duck && ent.client->g.anim_priority < ANIM_DEATH)
		goto newanim;
	if (run != ent.client->g.anim_run && ent.client->g.anim_priority == ANIM_BASIC)
		goto newanim;
	if (!ent.g.groundentity.has_value() && ent.client->g.anim_priority <= ANIM_WAVE)
		goto newanim;

	if (ent.client->g.anim_priority == ANIM_REVERSE)
	{
		if ((size_t)ent.s.frame > ent.client->g.anim_end)
		{
			ent.s.frame--;
			return;
		}
	}
	else if ((size_t)ent.s.frame < ent.client->g.anim_end)
	{
		// continue an animation
		ent.s.frame++;
		return;
	}

	if (ent.client->g.anim_priority == ANIM_DEATH)
		return;     // stay there
	if (ent.client->g.anim_priority == ANIM_JUMP)
	{
		if (!ent.g.groundentity.has_value())
			return;     // stay there
		ent.client->g.anim_priority = ANIM_WAVE;
		ent.s.frame = FRAME_jump3;
		ent.client->g.anim_end = FRAME_jump6;
		return;
	}

newanim:
	// return to either a running or standing frame
	ent.client->g.anim_priority = ANIM_BASIC;
	ent.client->g.anim_duck = duck;
	ent.client->g.anim_run = run;

	if (!ent.g.groundentity.has_value())
	{
#ifdef HOOK_CODE
		// if on grapple, don't go into jump frame, go into standing frame
		if (ent.client->g.grapple.has_value())
		{
			ent.s.frame = FRAME_stand01;
			ent.client->g.anim_end = FRAME_stand40;
		}
		else
		{
#endif
			ent.client->g.anim_priority = ANIM_JUMP;
			if (ent.s.frame != FRAME_jump2)
				ent.s.frame = FRAME_jump1;
			ent.client->g.anim_end = FRAME_jump2;
#ifdef HOOK_CODE
		}
#endif
	}
	else if (run)
	{
		// running
		if (duck)
		{
			ent.s.frame = FRAME_crwalk1;
			ent.client->g.anim_end = FRAME_crwalk6;
		}
		else
		{
			ent.s.frame = FRAME_run1;
			ent.client->g.anim_end = FRAME_run6;
		}
	}
	else
	{
		// standing
		if (duck)
		{
			ent.s.frame = FRAME_crstnd01;
			ent.client->g.anim_end = FRAME_crstnd19;
		}
		else
		{
			ent.s.frame = FRAME_stand01;
			ent.client->g.anim_end = FRAME_stand40;
		}
	}
}

/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame(entity &ent)
{
	float   bobtime;

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	ent.client->ps.pmove.set_origin(ent.s.origin);
	ent.client->ps.pmove.set_velocity(ent.g.velocity);

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermission_framenum)
	{
		// FIXME: add view drifting here?
		ent.client->ps.blend[3] = 0.f;
		ent.client->ps.fov = 90.f;
		G_SetStats(ent);
		return;
	}

	AngleVectors(ent.client->g.v_angle, &forward, &right, &up);

	// burn from lava, etc
	P_WorldEffects(ent);

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent.client->g.v_angle[PITCH] > 180)
		ent.s.angles[PITCH] = (-360 + ent.client->g.v_angle[PITCH]) / 3;
	else
		ent.s.angles[PITCH] = ent.client->g.v_angle[PITCH] / 3;
	ent.s.angles[YAW] = ent.client->g.v_angle[YAW];
	ent.s.angles[ROLL] = 0;
	ent.s.angles[ROLL] = SV_CalcRoll(ent.g.velocity) * 4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent.g.velocity.x * ent.g.velocity.x + ent.g.velocity.y * ent.g.velocity.y);

	if (xyspeed < 5)
	{
		bobmove = 0;
		ent.client->g.bobtime = 0;    // start at beginning of cycle again
	}
	else if (ent.g.groundentity.has_value())
	{
		// so bobbing only cycles when on ground
		if (xyspeed > 210)
			bobmove = 0.25f;
		else if (xyspeed > 100)
			bobmove = 0.125f;
		else
			bobmove = 0.0625f;
	}

	bobtime = (ent.client->g.bobtime += bobmove);

	if (ent.client->ps.pmove.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime * PI));

	// detect hitting the floor
	P_FallingDamage(ent);

	// apply all the damage taken this frame
	P_DamageFeedback(ent);

	// determine the view offsets
	SV_CalcViewOffset(ent);

	// determine the gun offsets
	SV_CalcGunOffset(ent);

	// determine the full screen color blend
	// must be after viewoffset, so eye contents can be
	// accurately determined
	// FIXME: with client prediction, the contents
	// should be determined by the client
	SV_CalcBlend(ent);

	// chase cam stuff
	if (ent.client->g.resp.spectator)
		G_SetSpectatorStats(ent);
	else
		G_SetStats(ent);

	G_CheckChaseStats(ent);

	G_SetClientEvent(ent);

	G_SetClientEffects(ent);

	G_SetClientSound(ent);

	G_SetClientFrame(ent);

	ent.client->g.oldvelocity = ent.g.velocity;
	ent.client->g.oldviewangles = ent.client->ps.viewangles;

	// clear weapon kicks
	ent.client->g.kick_origin = ent.client->g.kick_angles = vec3_origin;

	// if the scoreboard is up, update it
	if (ent.client->g.showscores && !(level.framenum & 31))
	{
#ifdef PMENU
		if (!ent.client.menu.open)
		{
#endif
			DeathmatchScoreboardMessage(ent, ent.g.enemy);
			gi.unicast(ent, false);
#ifdef PMENU
		}
#endif
	}
}
