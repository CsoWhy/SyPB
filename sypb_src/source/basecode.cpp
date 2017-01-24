//
// Copyright (c) 2003-2009, by Yet Another POD-Bot Development Team.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// $Id:$
//

#include <core.h>

// console variables
ConVar sypb_debug ("sypb_debug", "0");
ConVar sypb_debuggoal("sypb_debuggoal", "-1");
ConVar sypb_gamemod("sypb_gamemod", "0");

ConVar sypb_followuser ("sypb_followuser", "3");
ConVar sypb_knifemode ("sypb_knifemode", "0");
ConVar sypb_walkallow ("sypb_walkallow", "1");
ConVar sypb_stopbots ("sypb_stopbots", "0");
ConVar sypb_spraypaints ("sypb_spraypaints", "1");
ConVar sypb_restrictweapons ("sypb_restrictweapons", "ump45;p90;elite;tmp;mac10;m3;xm1014");

int Bot::GetMessageQueue (void)
{
   // this function get the current message from the bots message queue

   int message = m_messageQueue[m_actMessageIndex++];
   m_actMessageIndex &= 0x1f; // wraparound

   return message;
}

void Bot::PushMessageQueue (int message)
{
   // this function put a message into the bot message queue

   if (message == CMENU_SAY)
   {
      // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
      int entityIndex = GetIndex ();

      for (int i = 0; i < engine->GetMaxClients (); i++)
      {
         Bot *otherBot = g_botManager->GetBot (i);

         if (otherBot != null && otherBot->pev != pev)
         {
            if (IsAlive (GetEntity ()) == IsAlive (otherBot->GetEntity ()))
            {
               otherBot->m_sayTextBuffer.entityIndex = entityIndex;
               strcpy (otherBot->m_sayTextBuffer.sayText, m_tempStrings);
            }
            otherBot->m_sayTextBuffer.timeNextChat = engine->GetTime () + otherBot->m_sayTextBuffer.chatDelay;
         }
      }
   }
   m_messageQueue[m_pushMessageIndex++] = message;
   m_pushMessageIndex &= 0x1f; // wraparound
}

float Bot::InFieldOfView (Vector destination)
{
   float entityAngle = AngleMod (destination.ToYaw ()); // find yaw angle from source to destination...
   float viewAngle = AngleMod (pev->v_angle.y); // get bot's current view angle...

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead, 45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle
   float absoluteAngle = fabsf (viewAngle - entityAngle);

   if (absoluteAngle > 180.0f)
      absoluteAngle = 360.0f - absoluteAngle;

   return absoluteAngle;
}

bool Bot::IsInViewCone (Vector origin)
{
   // this function returns true if the spatial vector location origin is located inside
   // the field of view cone of the bot entity, false otherwise. It is assumed that entities
   // have a human-like field of view, that is, about 90 degrees.

   return ::IsInViewCone (origin, GetEntity ());
}

// SyPB Pro P.41 - Look up enemy improve
bool Bot::CheckVisibility(entvars_t *targetEntity, Vector *origin, uint8_t *bodyPart)
{
	TraceResult tr;
	*bodyPart = 0;

	Vector botHead = EyePosition();
	// SyPB Pro P.48 - Look up entity improve
	if (pev->flags & FL_DUCKING)
		botHead = botHead + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);

	TraceLine(botHead, GetEntityOrigin(ENT(targetEntity)), true, true, GetEntity(), &tr);
	if (tr.pHit == ENT(targetEntity) || tr.flFraction >= 1.0f)
	{
		*bodyPart |= VISIBILITY_BODY;
		*origin = tr.vecEndPos;
	}

	if (!IsValidPlayer(ENT(targetEntity)))
		return (*bodyPart != 0);

	// SyPB Pro P.42 - Get Head Origin 
	Vector headOrigin = GetPlayerHeadOrigin(ENT(targetEntity));
	TraceLine(botHead, headOrigin, true, true, GetEntity(), &tr);
	if (tr.pHit == ENT(targetEntity) || tr.flFraction >= 1.0f)
	{
		*bodyPart |= VISIBILITY_HEAD;
		*origin = headOrigin;
	}

	return (*bodyPart != 0);
}

// SyPB Pro P.41 - Look up enemy improve
bool Bot::IsEnemyViewable(edict_t *entity, bool setEnemy, bool allCheck, bool checkOnly)
{
	if (FNullEnt(entity) || !IsAlive (entity))
		return false;

	if (IsNotAttackLab(entity))
		return false;

	if (!allCheck)
	{
		if (!IsInViewCone(GetEntityOrigin(entity)))
		{
			if (m_backCheckEnemyTime == 0.0f)
			{
				// SyPB Pro P.42 - Look up enemy improve / Zombie Ai improve
				if (IsZombieEntity(GetEntity()))
				{
					if (!FNullEnt(m_enemy))
						return false;

					// SyPB Pro P.47 - Zombie Ai improve
					if (!FNullEnt(m_moveTargetEntity))
					{
						if (m_damageTime < engine->GetTime() && m_damageTime != 0.0f)
							m_backCheckEnemyTime = engine->GetTime() + engine->RandomFloat(1.0f, 1.5f);
					}
				}
				else if (GetGameMod() == MODE_DM)
				{
					if (!FNullEnt(m_enemy))
						return false;

					if (m_skill < 90)
					{
						if (m_skill >= 70)
							m_backCheckEnemyTime = engine->GetTime() + engine->RandomFloat(0.5f, 0.7f);
						else
							m_backCheckEnemyTime = engine->GetTime() + engine->RandomFloat(0.7f, 1.3f);
					}
				}
				else if (GetGameMod() == MODE_ZP)
				{
					if (!FNullEnt (m_enemy))
						m_backCheckEnemyTime = engine->GetTime() + engine->RandomFloat(0.5f, 1.5f);
				}
				else
				{
					float addTime = engine->RandomFloat(0.2f, 0.5f);
					if (m_skill == 100)
						addTime = 0.0f;
					else if (!FNullEnt(m_enemy))
						addTime += 0.2f;

					m_backCheckEnemyTime = addTime;
				}

				// SyPB Pro P.47 - Small Fixed 
				if (m_backCheckEnemyTime != 0.0f && m_backCheckEnemyTime >= engine->GetTime())
					return false;
			}
		}
	}

	Vector entityOrigin;
	uint8_t visibility;
	bool seeEntity = CheckVisibility(VARS(entity), &entityOrigin, &visibility);

	if (checkOnly)
		return seeEntity;

	if (setEnemy)
	{
		m_enemyOrigin = entityOrigin;
		m_visibility = visibility;
	}

	if (seeEntity)
	{
		m_backCheckEnemyTime = 0.0f;
		m_seeEnemyTime = engine->GetTime();

		SetLastEnemy(entity);
		return true;
	}

	return false;
}

bool Bot::ItemIsVisible (Vector destination, char *itemName)//, bool bomb)
{
   TraceResult tr;

   // trace a line from bot's eyes to destination..
   TraceLine (EyePosition (), destination, true, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction < 1.0f)
   {
      // check for standard items
      if (tr.flFraction > 0.97f && strcmp (STRING (tr.pHit->v.classname), itemName) == 0)
         return true;

      if (tr.flFraction > 0.95f && strncmp (STRING (tr.pHit->v.classname), "csdmw_", 6) == 0)
         return true;

      return false;
   }

   return true;
}

bool Bot::EntityIsVisible (Vector dest, bool fromBody)
{
   TraceResult tr;

   // trace a line from bot's eyes to destination...
   TraceLine (fromBody ? pev->origin - Vector (0.0f, 0.0f, 1.0f) : EyePosition (), dest, true, true, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0f;
}

// SyPB Pro P.30 - new Fun Mod Ai
void Bot::ZombieModeAi(void)
{
	edict_t *entity = null;
	if (IsZombieEntity(GetEntity()))
	{
		if (FNullEnt(m_enemy) && FNullEnt(m_moveTargetEntity))
		{
			edict_t *targetEnt = null;
			float targetDistance = 9999.9f;

			// SyPB Pro P.48 - Zombie Mode Base improve
			for (int i = 0; i < engine->GetMaxClients(); i++)
			{
				if (!(g_clients[i].flags & CFLAG_USED) || !(g_clients[i].flags & CFLAG_ALIVE))
					continue;

				if (GetGameMod() == MODE_ZH)
					entity = g_clients[i].ent;
				else
				{
					Bot *bot = g_botManager->GetBot(i);
					if (bot == null || bot == this || !IsAlive (bot->GetEntity ()))
						continue;

					if (bot->m_enemy == null && bot->m_moveTargetEntity == null)
						continue;

					entity = (bot->m_enemy == null) ? bot->m_moveTargetEntity : bot->m_enemy;
					if (GetTeam(GetEntity()) == GetTeam(bot->GetEntity()))
					{
						if (entity == targetEnt || GetTeam(GetEntity()) == GetTeam(entity))
							continue;
					}
					else
					{
						if (bot->GetEntity() == targetEnt || GetTeam(GetEntity()) != GetTeam(entity))
							continue;

						entity = bot->GetEntity();
					}
				}

				if (GetTeam(GetEntity()) == GetTeam(entity))
					continue;

				float distance = GetEntityDistance(entity);
				if (distance < targetDistance)
				{
					targetDistance = distance;
					targetEnt = entity;
				}
			}

			if (!FNullEnt(targetEnt))
				SetMoveTarget(targetEnt);

		}

		return;
	}

	while (!FNullEnt(entity = FIND_ENTITY_BY_CLASSNAME(entity, "human_escape")))
	{
		if (entity->v.effects & EF_NODRAW)
			continue;

		int nearestIndex = g_waypoint->FindNearest(GetEntityOrigin(entity));
		if ((nearestIndex >= 0) && (nearestIndex < g_numWaypoints))
		{
			if (nearestIndex != -1 && m_currentWaypointIndex != nearestIndex)
				GetCurrentTask()->data = nearestIndex;

			break;
		}
	}
}

// SyPB Pro P.38 - Zombie Mode Camp Action
void Bot::ZmCampPointAction(int mode)
{
	// SyPB Pro P.47 - Zombie Mode Human Camp Action
	if (g_waypoint->m_zmHmPoints.IsEmpty())
		return;

	float campAction = 0.0f;
	int campPointWaypointIndex = -1;

	if (g_waypoint->IsZBCampPoint(m_currentWaypointIndex))
	{
		if (mode == 1)
		{
			campAction = 1.0f;
			campPointWaypointIndex = m_currentWaypointIndex;
		}
		else
		{
			campAction = 1.6f;
			campPointWaypointIndex = m_currentWaypointIndex;
		}
	}
	else if (mode == 0 && GetCurrentTask()->data != -1 && g_waypoint->IsZBCampPoint(GetCurrentTask()->data))
	{
		if (&m_navNode[0] != null)
		{
			PathNode *navid = &m_navNode[0];
			navid = navid->next;
			int movePoint = 0;
			while (navid != null && movePoint <= 2)
			{
				movePoint++;
				if (GetCurrentTask()->data == navid->index)
				{
					if ((g_waypoint->GetPath(navid->index)->origin - pev->origin).GetLength2D() <= 250.0f &&
						g_waypoint->GetPath(navid->index)->origin.z + 40.0f <= pev->origin.z &&
						g_waypoint->GetPath(navid->index)->origin.z - 25.0f >= pev->origin.z &&
						IsWaypointUsed(navid->index))
					{
						campAction = (movePoint * 1.8f);
						campPointWaypointIndex = GetCurrentTask()->data;
						break;
					}
				}
				navid = navid->next;
			}
		}
	}
	// -----

	if (campAction == 0.0f || campPointWaypointIndex == -1)
		m_checkCampPointTime = 0.0f;
	else if (m_checkCampPointTime == 0.0f && campAction != 1.0f)
		m_checkCampPointTime = engine->GetTime() + campAction;
	else if ((m_checkCampPointTime  < engine->GetTime()) || campAction == 1.0f)
	{
		m_zhCampPointIndex = campPointWaypointIndex;

		m_campButtons = 0;
		SelectBestWeapon();
		MakeVectors(pev->v_angle);

		m_timeCamping = engine->GetTime() + 120.0f;
		PushTask(TASK_CAMP, TASKPRI_CAMP, -1, m_timeCamping, true);

		m_camp.x = g_waypoint->GetPath(campPointWaypointIndex)->campStartX;
		m_camp.y = g_waypoint->GetPath(campPointWaypointIndex)->campStartY;
		m_camp.z = 0;

		m_aimFlags |= AIM_CAMP;
		m_campDirection = 0;

		m_moveToGoal = false;
		m_checkTerrain = false;

		m_moveSpeed = 0.0f;
		m_strafeSpeed = 0.0f;

		m_checkCampPointTime = 0.0f;
	}
}

// SyPB Pro P.47 - Small Change for Avoid Entity
void Bot::AvoidEntity(void)
{
	if (FNullEnt(m_avoidEntity) || 
		(m_avoidEntity->v.flags & FL_ONGROUND) || (m_avoidEntity->v.effects & EF_NODRAW))
	{
		m_avoidEntity = null;
		m_needAvoidEntity = 0;
	}

	edict_t *entity = null;
	int i, allEntity = 0;
	int avoidEntityId[checkEntityNum];
	for (i = 0; i < checkEntityNum; i++)
		avoidEntityId[i] = -1;

	while (!FNullEnt(entity = FIND_ENTITY_BY_CLASSNAME(entity, "grenade")))
	{
		if (strcmp(STRING(entity->v.model) + 9, "smokegrenade.mdl") == 0)
			continue;

		avoidEntityId[allEntity] = ENTINDEX(entity);
		allEntity++;

		if (allEntity >= checkEntityNum)
			break;
	}

	for (i = 0; i < entityNum; i++)
	{
		if (allEntity >= checkEntityNum)
			break;

		if (g_entityId[i] == -1 || g_entityAction[i] != 2)
			continue;

		if (GetTeam(GetEntity()) == g_entityTeam[i])
			continue;

		entity = INDEXENT(g_entityId[i]);
		if (FNullEnt(entity) || entity->v.effects & EF_NODRAW)
			continue;

		avoidEntityId[allEntity] = g_entityId[i];
		allEntity++;
	}

	for (i = 0; i < allEntity; i++)
	{
		if (avoidEntityId[i] == -1)
			continue;

		entity = INDEXENT(avoidEntityId[i]);

		if (strcmp(STRING(entity->v.classname), "grenade") == 0)
		{
			if (IsZombieEntity(GetEntity()))
				continue;

			if (strcmp(STRING(entity->v.model) + 9, "flashbang.mdl") == 0 &&
				GetGameMod() != MODE_BASE && GetGameMod() != MODE_DM)
				continue;

			if (strcmp(STRING(entity->v.model) + 9, "hegrenade.mdl") == 0 && 
					(GetTeam(entity->v.owner) == GetTeam(GetEntity()) && entity->v.owner != GetEntity()))
				continue;
		}

		if (InFieldOfView(GetEntityOrigin(entity) - EyePosition()) > pev->fov * 0.5f &&
			!EntityIsVisible(GetEntityOrigin(entity)))
			continue;

		if (strcmp(STRING(entity->v.model) + 9, "flashbang.mdl") == 0)
		{
			Vector position = (GetEntityOrigin(entity) - EyePosition()).ToAngles();
			if (m_skill >= 70)
			{
				pev->v_angle.y = AngleNormalize(position.y + 180.0f);
				m_canChooseAimDirection = false;
				return;
			}
		}

		if ((entity->v.flags & FL_ONGROUND) == 0)
		{
			float distance = (GetEntityOrigin(entity) - pev->origin).GetLength();
			float distanceMoved = ((GetEntityOrigin(entity) + entity->v.velocity * m_frameInterval) - pev->origin).GetLength();

			if (distanceMoved < distance && distance < 500)
			{
				if (m_currentWaypointIndex != -1)
				{
					if ((GetEntityOrigin(entity) - g_waypoint->GetPath(m_currentWaypointIndex)->origin).GetLength() > distance ||
						((GetEntityOrigin(entity) + entity->v.velocity * m_frameInterval) - g_waypoint->GetPath(m_currentWaypointIndex)->origin).GetLength() > distanceMoved)
						continue;
				}

				MakeVectors(pev->v_angle);

				Vector dirToPoint = (pev->origin - GetEntityOrigin(entity)).Normalize2D();
				Vector rightSide = g_pGlobals->v_right.Normalize2D();

				if ((dirToPoint | rightSide) > 0)
					m_needAvoidEntity = -1;
				else
					m_needAvoidEntity = 1;

				m_avoidEntity = entity;

				return;
			}
		}
	}
}

bool Bot::IsBehindSmokeClouds (edict_t *ent)
{
   edict_t *pentGrenade = null;
   Vector betweenUs = (GetEntityOrigin (ent) - pev->origin).Normalize ();

   while (!FNullEnt (pentGrenade = FIND_ENTITY_BY_CLASSNAME (pentGrenade, "grenade")))
   {
      // if grenade is invisible don't care for it
      if ((pentGrenade->v.effects & EF_NODRAW) || !(pentGrenade->v.flags & (FL_ONGROUND | FL_PARTIALGROUND)) || strcmp (STRING (pentGrenade->v.model) + 9, "smokegrenade.mdl"))
         continue;

      // check if visible to the bot
      if (InFieldOfView (GetEntityOrigin (ent) - EyePosition ()) > pev->fov / 3 && !EntityIsVisible(GetEntityOrigin(ent)))
         continue;

      Vector betweenNade = (GetEntityOrigin (pentGrenade) - pev->origin).Normalize ();
      Vector betweenResult = ((Vector (betweenNade.y, betweenNade.x, 0.0f) * 150.0f + GetEntityOrigin (pentGrenade)) - pev->origin).Normalize ();

      if ((betweenNade | betweenUs) > (betweenNade | betweenResult))
         return true;
   }
   return false;
}

int Bot::GetBestWeaponCarried (void)
{
   // this function returns the best weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   // take the shield in account
   if (HasShield ())
      weapons |= (1 << WEAPON_SHIELDGUN);

   for (int i = 0; i < Const_NumWeapons; i++)
   {
      if (weapons & (1 << weaponTab[*ptr].id))
         weaponIndex = i;

      ptr++;
   }
   return weaponIndex;
}

int Bot::GetBestSecondaryWeaponCarried (void)
{
   // this function returns the best secondary weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   // take the shield in account
   if (HasShield ())
      weapons |= (1 << WEAPON_SHIELDGUN);

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < Const_NumWeapons; i++)
   {
      int id = weaponTab[*ptr].id;

      if ((weapons & (1 << weaponTab[*ptr].id)) && (id == WEAPON_USP || id == WEAPON_GLOCK18 || id == WEAPON_DEAGLE || id == WEAPON_P228 || id == WEAPON_ELITE || id == WEAPON_FN57))
      {
         weaponIndex = i;
         break;
      }
      ptr++;
   }
   return weaponIndex;
}

bool Bot::RateGroundWeapon (edict_t *ent)
{
   // this function compares weapons on the ground to the one the bot is using

   int hasWeapon = 0;
   int groundIndex = 0;
   int *ptr = g_weaponPrefs[m_personality];

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < Const_NumWeapons; i++)
   {
      if (strcmp (weaponTab[*ptr].modelName, STRING (ent->v.model) + 9) == 0)
      {
         groundIndex = i;
         break;
      }
      ptr++;
   }

   if (IsRestricted (weaponTab[groundIndex].id) && HasPrimaryWeapon ())
      return false;

   if (groundIndex < 7)
      hasWeapon = GetBestSecondaryWeaponCarried ();
   else
      hasWeapon = GetBestWeaponCarried ();

   return groundIndex > hasWeapon;
}

// SyPB Pro P.40 - Breakable 
edict_t *Bot::FindBreakable(void)
{
	if (m_waypointOrigin == nullvec)
		return null;

	// SyPB Pro P.40 - Breakable improve
	bool checkBreakable = false;
	float distance = (pev->origin - m_waypointOrigin).GetLength();
	
	// SyPB Pro P.45 - Breakable improve
	if (distance > 250)
		distance = 250.0f;

	edict_t *ent = null;
	while (!FNullEnt(ent = FIND_ENTITY_BY_CLASSNAME(ent, "func_breakable")))
	{
		if ((pev->origin - GetEntityOrigin(ent)).GetLength() > distance)
			continue;

		checkBreakable = true;
		break;
	}

	if (!checkBreakable)
		return null;

	TraceResult tr;
	TraceLine(pev->origin, m_waypointOrigin, false, false, GetEntity(), &tr);

	if (tr.flFraction != 1.0f && !FNullEnt(tr.pHit) && IsShootableBreakable(tr.pHit))
	{
		m_breakable = tr.vecEndPos;
		m_breakableEntity = tr.pHit;
		m_destOrigin = m_breakable;
		

		if (pev->origin.z > m_breakable.z)
			m_campButtons = IN_DUCK;
		else
			m_campButtons = pev->button & IN_DUCK;

		return tr.pHit;
	}

	return null;
}

// SyPB Pro P.42 - Find Item 
void Bot::FindItem(void)
{
	if ((GetCurrentTask()->taskID == TASK_ESCAPEFROMBOMB || GetCurrentTask()->taskID == TASK_PLANTBOMB))
		return;

	if (IsOnLadder())
	{
		m_pickupItem = null;
		m_pickupType = PICKTYPE_NONE;
		return;
	}

	edict_t *ent = null;

	if (!FNullEnt(m_pickupItem))
	{
		// SyPB Pro P.48 - Base improve
		while (!FNullEnt(ent = FIND_ENTITY_IN_SPHERE(ent, pev->origin, 400.0f)))
		{
			if (ent != m_pickupItem || (ent->v.effects & EF_NODRAW) || IsValidPlayer(ent->v.owner))
				continue; // someone owns this weapon or it hasn't re spawned yet

			if (ItemIsVisible(GetEntityOrigin(ent), const_cast <char *> (STRING(ent->v.classname))))
				return;

			break;
		}
	}

	edict_t *pickupItem = null;
	m_pickupItem = null;
	m_pickupType = PICKTYPE_NONE;
	ent = null;
	pickupItem = null;

	PickupType pickupType = PICKTYPE_NONE;

	float minDistance = 401.0f;

	while (!FNullEnt(ent = FIND_ENTITY_IN_SPHERE(ent, pev->origin, 400.0f)))
	{
		pickupType = PICKTYPE_NONE;
		if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore)
			continue;

		if (strncmp("hostage_entity", STRING(ent->v.classname), 14) == 0)
			pickupType = PICKTYPE_HOSTAGE;
		else if (strncmp("weaponbox", STRING(ent->v.classname), 9) == 0 && strcmp(STRING(ent->v.model) + 9, "backpack.mdl") == 0)
			pickupType = PICKTYPE_DROPPEDC4;
		else if (strncmp("weaponbox", STRING(ent->v.classname), 9) == 0 && strcmp(STRING(ent->v.model) + 9, "backpack.mdl") == 0 && !m_isUsingGrenade)
			pickupType = PICKTYPE_DROPPEDC4;
		else if ((strncmp("weaponbox", STRING(ent->v.classname), 9) == 0 || strncmp("armoury_entity", STRING(ent->v.classname), 14) == 0 || strncmp("csdm", STRING(ent->v.classname), 4) == 0) && !m_isUsingGrenade)
			pickupType = PICKTYPE_WEAPON;
		else if (strncmp("weapon_shield", STRING(ent->v.classname), 13) == 0 && !m_isUsingGrenade)
			pickupType = PICKTYPE_SHIELDGUN;
		else if (strncmp("item_thighpack", STRING(ent->v.classname), 14) == 0 && GetTeam(GetEntity()) == TEAM_COUNTER && !m_hasDefuser)
			pickupType = PICKTYPE_DEFUSEKIT;
		else if (strncmp("grenade", STRING(ent->v.classname), 7) == 0 && strcmp(STRING(ent->v.model) + 9, "c4.mdl") == 0)
			pickupType = PICKTYPE_PLANTEDC4;
		else
		{
			for (int i = 0; (i < entityNum && pickupType == PICKTYPE_NONE); i++)
			{
				if (g_entityId[i] == -1 || g_entityAction[i] != 3)
					continue;

				if (GetTeam(GetEntity()) != g_entityTeam[i] && g_entityTeam[i] != 2)
					continue;

				if (ent != INDEXENT(g_entityId[i]))
					continue;

				pickupType = PICKTYPE_GETENTITY;
			}
		}

		if (IsZombieEntity(GetEntity()) && pickupType != PICKTYPE_GETENTITY)
			continue;

		// SyPB Pro P.42 - AMXX API
		if (m_blockWeaponPickAPI &&
			(pickupType == PICKTYPE_WEAPON || pickupType == PICKTYPE_SHIELDGUN || pickupType == PICKTYPE_SHIELDGUN))
			continue;

		if (pickupType == PICKTYPE_NONE)
			continue;

		Vector entityOrigin = GetEntityOrigin(ent);
		float distance = (pev->origin - entityOrigin).GetLength();
		if (distance > minDistance)
			continue;
		
		if (!ItemIsVisible(entityOrigin, const_cast <char *> (STRING(ent->v.classname))))
		{
			if (strncmp("grenade", STRING(ent->v.classname), 7) != 0 || strcmp(STRING(ent->v.model) + 9, "c4.mdl") != 0)
				continue;

			if (distance > 80.0f)
				continue;
		}

		bool allowPickup = true;
		if (pickupType == PICKTYPE_GETENTITY)
			allowPickup = true;
		else if (pickupType == PICKTYPE_WEAPON)
		{
			int weaponCarried = GetBestWeaponCarried();
			int secondaryWeaponCarried = GetBestSecondaryWeaponCarried();

			// SyPB Pro P.45 - AMXX API improve
			int weaponAmmoMax, secondaryWeaponAmmoMax;
			if (m_weaponClipAPI > 0)
				secondaryWeaponAmmoMax = weaponAmmoMax = m_weaponClipAPI;
			else
			{
				secondaryWeaponAmmoMax = g_weaponDefs[g_weaponSelect[secondaryWeaponCarried].id].ammo1Max;
				weaponAmmoMax = g_weaponDefs[g_weaponSelect[weaponCarried].id].ammo1Max;
			}

			if (secondaryWeaponCarried < 7 && 
				(m_ammo[g_weaponSelect[secondaryWeaponCarried].id] > 0.3 * secondaryWeaponAmmoMax) &&
				strcmp(STRING(ent->v.model) + 9, "w_357ammobox.mdl") == 0)
				allowPickup = false;
			else if (!m_isVIP && weaponCarried >= 7 && 
				(m_ammo[g_weaponSelect[weaponCarried].id] > 0.3 * weaponAmmoMax) &&
				strncmp(STRING(ent->v.model) + 9, "w_", 2) == 0)
			{
				if (strcmp(STRING(ent->v.model) + 9, "w_9mmarclip.mdl") == 0 && !(weaponCarried == WEAPON_FAMAS || weaponCarried == WEAPON_AK47 || weaponCarried == WEAPON_M4A1 || weaponCarried == WEAPON_GALIL || weaponCarried == WEAPON_AUG || weaponCarried == WEAPON_SG552))
					allowPickup = false;
				else if (strcmp(STRING(ent->v.model) + 9, "w_shotbox.mdl") == 0 && !(weaponCarried == WEAPON_M3 || weaponCarried == WEAPON_XM1014))
					allowPickup = false;
				else if (strcmp(STRING(ent->v.model) + 9, "w_9mmclip.mdl") == 0 && !(weaponCarried == WEAPON_MP5 || weaponCarried == WEAPON_TMP || weaponCarried == WEAPON_P90 || weaponCarried == WEAPON_MAC10 || weaponCarried == WEAPON_UMP45))
					allowPickup = false;
				else if (strcmp(STRING(ent->v.model) + 9, "w_crossbow_clip.mdl") == 0 && !(weaponCarried == WEAPON_AWP || weaponCarried == WEAPON_G3SG1 || weaponCarried == WEAPON_SCOUT || weaponCarried == WEAPON_SG550))
					allowPickup = false;
				else if (strcmp(STRING(ent->v.model) + 9, "w_chainammo.mdl") == 0 && weaponCarried == WEAPON_M249)
					allowPickup = false;
			}
			else if (m_isVIP || !RateGroundWeapon(ent))
				allowPickup = false;
			else if (strcmp(STRING(ent->v.model) + 9, "medkit.mdl") == 0 && (pev->health >= 100))
				allowPickup = false;
			else if ((strcmp(STRING(ent->v.model) + 9, "kevlar.mdl") == 0 || strcmp(STRING(ent->v.model) + 9, "battery.mdl") == 0) && pev->armorvalue >= 100) // armor vest
				allowPickup = false;
			else if (strcmp(STRING(ent->v.model) + 9, "flashbang.mdl") == 0 && (pev->weapons & (1 << WEAPON_FBGRENADE))) // concussion grenade
				allowPickup = false;
			else if (strcmp(STRING(ent->v.model) + 9, "hegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_HEGRENADE))) // explosive grenade
				allowPickup = false;
			else if (strcmp(STRING(ent->v.model) + 9, "smokegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_SMGRENADE))) // smoke grenade
				allowPickup = false;
		}
		else if (pickupType == PICKTYPE_SHIELDGUN)
		{
			if ((pev->weapons & (1 << WEAPON_ELITE)) || HasShield() || m_isVIP || (HasPrimaryWeapon() && !RateGroundWeapon(ent)))
				allowPickup = false;
		}
		else if (GetTeam(GetEntity()) != TEAM_TERRORIST && GetTeam(GetEntity()) != TEAM_COUNTER)
			allowPickup = false;
		else if (GetTeam(GetEntity()) == TEAM_TERRORIST)
		{
			if (pickupType == PICKTYPE_DROPPEDC4)
			{
				allowPickup = true;
				m_destOrigin = entityOrigin;
			}
			else if (pickupType == PICKTYPE_HOSTAGE)
			{
				m_itemIgnore = ent;
				allowPickup = false;

				if (m_skill > 80 && engine->RandomInt(0, 100) < 50 && GetCurrentTask()->taskID != TASK_MOVETOPOSITION && GetCurrentTask()->taskID != TASK_CAMP)
				{
					int index = FindDefendWaypoint(entityOrigin);

					PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + engine->RandomFloat(60.0f, 120.0f), true); // push camp task on to stack
					PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine->GetTime() + engine->RandomFloat(3.0f, 6.0f), true); // push move command

					if (g_waypoint->GetPath(index)->vis.crouch <= g_waypoint->GetPath(index)->vis.stand)
						m_campButtons |= IN_DUCK;
					else
						m_campButtons &= ~IN_DUCK;

					return;
				}
			}
			else if (pickupType == PICKTYPE_PLANTEDC4)
			{
				allowPickup = false;

				if (!m_defendedBomb)
				{
					m_defendedBomb = true;

					int index = FindDefendWaypoint(entityOrigin);
					float timeMidBlowup = g_timeBombPlanted + ((engine->GetC4TimerTime() / 2) + engine->GetC4TimerTime() / 4) - g_waypoint->GetTravelTime(pev->maxspeed, pev->origin, g_waypoint->GetPath(index)->origin);

					if (timeMidBlowup > engine->GetTime())
					{
						RemoveCertainTask(TASK_MOVETOPOSITION);

						PushTask(TASK_CAMP, TASKPRI_CAMP, -1, timeMidBlowup, true);
						PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeMidBlowup, true);

						if (g_waypoint->GetPath(index)->vis.crouch <= g_waypoint->GetPath(index)->vis.stand)
							m_campButtons |= IN_DUCK;
						else
							m_campButtons &= ~IN_DUCK;
					}
					else
						RadioMessage(Radio_ShesGonnaBlow);
				}
			}
		}
		else if (GetTeam(GetEntity()) == TEAM_COUNTER)
		{
			if (pickupType == PICKTYPE_HOSTAGE)
			{
				if (FNullEnt(ent) || (ent->v.health <= 0))
					allowPickup = false; // never pickup dead hostage
				else
				{
					Bot *bot = null;
					for (int i = 0; (i < engine->GetMaxClients() && allowPickup); i++)
					{
						bot = g_botManager->GetBot(i);
						if (bot == null || !IsAlive(bot->GetEntity()))
							continue;

						for (int j = 0; (j < Const_MaxHostages && allowPickup); j++)
						{
							if (bot->m_hostages[j] == ent)
								allowPickup = false;
						}
					}
				}
			}
			else if (pickupType == PICKTYPE_DROPPEDC4)
			{
				m_itemIgnore = ent;
				allowPickup = false;

				if (m_skill > 80 && engine->RandomInt(0, 100) < 90)
				{
					int index = FindDefendWaypoint(entityOrigin);

					PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + engine->RandomFloat(60.0f, 120.0f), true); // push camp task on to stack
					PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine->GetTime() + engine->RandomFloat(10.0f, 30.0f), true); // push move command

					if (g_waypoint->GetPath(index)->vis.crouch <= g_waypoint->GetPath(index)->vis.stand)
						m_campButtons |= IN_DUCK;
					else
						m_campButtons &= ~IN_DUCK;

					return;
				}
			}
			else if (pickupType == PICKTYPE_PLANTEDC4)
			{
				if (m_states & (STATE_SEEINGENEMY) || OutOfBombTimer())
				{
					allowPickup = false;
					return;
				}

				allowPickup = !IsBombDefusing(g_waypoint->GetBombPosition());
				if (!m_defendedBomb && !allowPickup)
				{
					m_defendedBomb = true;

					int index = FindDefendWaypoint(entityOrigin);
					float timeBlowup = g_timeBombPlanted + engine->GetC4TimerTime() - g_waypoint->GetTravelTime(pev->maxspeed, pev->origin, g_waypoint->GetPath(index)->origin);

					RemoveCertainTask(TASK_MOVETOPOSITION); // remove any move tasks

					PushTask(TASK_CAMP, TASKPRI_CAMP, -1, timeBlowup, true); // push camp task on to stack
					PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeBlowup, true); // push move command

					m_campButtons &= ~IN_DUCK;
				}
			}
		}

		if (allowPickup)
		{
			minDistance = distance;
			pickupItem = ent;
			m_pickupType = pickupType;
		}
	}

	if (!FNullEnt(pickupItem))
	{
		Bot *bot = null;
		for (int i = 0; i < engine->GetMaxClients(); i++)
		{
			bot = g_botManager->GetBot(i);
			if (bot != null && bot != this && IsAlive(bot->GetEntity()) && bot->m_pickupItem == pickupItem)
			{
				m_pickupItem = null;
				m_pickupType = PICKTYPE_NONE;

				return;
			}
		}

		Vector pickupOrigin = GetEntityOrigin(pickupItem);
		if (pickupOrigin.z > EyePosition().z + 12.0f || IsDeadlyDrop(pickupOrigin))
		{
			m_pickupItem = null;
			m_pickupType = PICKTYPE_NONE;

			return;
		}
		m_pickupItem = pickupItem;
	}
}

void Bot::GetCampDirection (Vector *dest)
{
   // this function check if view on last enemy position is blocked - replace with better vector then
   // mostly used for getting a good camping direction vector if not camping on a camp waypoint

   TraceResult tr;
   Vector src = EyePosition ();

   TraceLine (src, *dest, true, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
   {
      float length = (tr.vecEndPos - src).GetLengthSquared ();

      if (length > 10000.0f)
         return;

      float minDistance = FLT_MAX;
      float maxDistance = FLT_MAX;

      int enemyIndex = -1, tempIndex = -1;

      // find nearest waypoint to bot and position
      for (int i = 0; i < g_numWaypoints; i++)
      {
         float distance = (g_waypoint->GetPath (i)->origin - pev->origin).GetLengthSquared ();

         if (distance < minDistance)
         {
            minDistance = distance;
            tempIndex = i;
         }
         distance = (g_waypoint->GetPath (i)->origin - *dest).GetLengthSquared ();

         if (distance < maxDistance)
         {
            maxDistance = distance;
            enemyIndex = i;
         }
      }

      if (tempIndex == -1 || enemyIndex == -1)
         return;

      minDistance = FLT_MAX;

      int lookAtWaypoint = -1;
      Path *path = g_waypoint->GetPath (tempIndex);

      for (int i = 0; i < Const_MaxPathIndex; i++)
      {
         if (path->index[i] == -1)
            continue;

         float distance = g_waypoint->GetPathDistanceFloat (path->index[i], enemyIndex);

         if (distance < minDistance)
         {
            minDistance = distance;
            lookAtWaypoint = path->index[i];
         }
      }
      if (lookAtWaypoint != -1 && lookAtWaypoint < g_numWaypoints)
         *dest = g_waypoint->GetPath (lookAtWaypoint)->origin;
   }
}

void Bot::SwitchChatterIcon (bool show)
{
   // this function depending on show boolen, shows/remove chatter, icon, on the head of bot.

   if (g_gameVersion == CSVER_VERYOLD)
      return;

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      edict_t *ent = INDEXENT (i);

      if (!IsValidPlayer (ent) || IsValidBot (ent) || GetTeam (ent) != GetTeam (GetEntity ()))
         continue;

      MESSAGE_BEGIN (MSG_ONE, g_netMsg->GetId (NETMSG_BOTVOICE), null, ent); // begin message
         WRITE_BYTE (show); // switch on/off
         WRITE_BYTE (GetIndex ());
      MESSAGE_END ();

   }
}

void Bot::RadioMessage (int message)
{
   // this function inserts the radio message into the message queue
   
   // SyPB Pro P.15
   if (GetGameMod () == MODE_DM)
   	   return;

   if (GetNearbyFriendsNearPosition (pev->origin, 9999) == 0)
      return;

   m_radioSelect = message;
   PushMessageQueue (CMENU_RADIO);
}

void Bot::CheckMessageQueue (void)
{
   // this function checks and executes pending messages

   // no new message?
   if (m_actMessageIndex == m_pushMessageIndex)
      return;

   // get message from stack
   int currentQueueMessage = GetMessageQueue ();

   // nothing to do?
   if (currentQueueMessage == CMENU_IDLE)
      return;

   int team = GetTeam (GetEntity ());

   switch (currentQueueMessage)
   {
   case CMENU_BUY: // general buy message

      // buy weapon
      if (m_nextBuyTime > engine->GetTime ())
      {
         // keep sending message
         PushMessageQueue (CMENU_BUY);
         return;
      }

      if (!m_inBuyZone)
      {
         m_buyPending = true;
         m_buyingFinished = true;

         break;
      }

      m_buyPending = false;
      m_nextBuyTime = engine->GetTime () + engine->RandomFloat (0.3f, 0.8f);

      // if fun-mode no need to buy
      if (sypb_knifemode.GetBool ())
      {
         m_buyState = 6;
         SelectWeaponByName ("weapon_knife");
      }

      // prevent vip from buying
      if (g_mapType & MAP_AS)
      {
         if (*(INFOKEY_VALUE (GET_INFOKEYBUFFER (GetEntity ()), "model")) == 'v')
         {
            m_isVIP = true;
            m_buyState = 6;
            m_pathType = 1;
         }
      }

      // prevent terrorists from buying on es maps
      if ((g_mapType & MAP_ES) && GetTeam (GetEntity ()) == TEAM_TERRORIST)
         m_buyState = 6;

      // prevent teams from buying on fun maps
	  if (g_mapType & (MAP_KA | MAP_FY | MAP_AWP)) // SyPB Pro P.32 - AWP MAP TYPE
      {
         m_buyState = 6;

         if (g_mapType & MAP_KA)
            sypb_knifemode.SetInt (1);
      }

      if (m_buyState > 5)
      {
         m_buyingFinished = true;
         return;
      }

      PushMessageQueue (CMENU_IDLE);
      PerformWeaponPurchase ();

      break;

   case CMENU_RADIO: // general radio message issued
     // if last bot radio command (global) happened just a second ago, delay response
	   // SyPB Pro P.42 - Fixed 
	   if ((team == 0 || team == 1) && (g_lastRadioTime[team] + 1.0f < engine->GetTime()))
	   //if (g_lastRadioTime[team] + 1.0f < engine->GetTime())
	   {
		   // if same message like previous just do a yes/no
		   if (m_radioSelect != Radio_Affirmative && m_radioSelect != Radio_Negative)
		   {
			   if (m_radioSelect == g_lastRadio[team] && g_lastRadioTime[team] + 1.5 > engine->GetTime())
				   m_radioSelect = -1;
			   else
			   {
				   if (m_radioSelect != Radio_ReportingIn)
					   g_lastRadio[team] = m_radioSelect;
				   else
					   g_lastRadio[team] = -1;

				   for (int i = 0; i < engine->GetMaxClients(); i++)
				   {
					   if (g_botManager->GetBot(i))
					   {
						   if (pev != g_botManager->GetBot(i)->pev && GetTeam(g_botManager->GetBot(i)->GetEntity()) == team)
						   {
							   g_botManager->GetBot(i)->m_radioOrder = m_radioSelect;
							   g_botManager->GetBot(i)->m_radioEntity = GetEntity();
						   }
					   }
				   }
			   }
		   }
		   
		   if (m_radioSelect != Radio_ReportingIn)
		   {
			   if (m_radioSelect < Radio_GoGoGo)
				   FakeClientCommand(GetEntity(), "radio1");
			   else if (m_radioSelect < Radio_Affirmative)
			   {
				   m_radioSelect -= Radio_GoGoGo - 1;
				   FakeClientCommand(GetEntity(), "radio2");
			   }
			   else
			   {
				   m_radioSelect -= Radio_Affirmative - 1;
				   FakeClientCommand(GetEntity(), "radio3");
			   }

			   // select correct menu item for this radio message
			   FakeClientCommand(GetEntity(), "menuselect %d", m_radioSelect);
		   }

		   g_lastRadioTime[team] = engine->GetTime(); // store last radio usage
	   }
	   else
		   PushMessageQueue(CMENU_RADIO);
	   break;


   // team independent saytext
   case CMENU_SAY:
      SayText (m_tempStrings);
      break;

   // team dependent saytext
   case CMENU_TEAMSAY:
      TeamSayText (m_tempStrings);
      break;

   case CMENU_IDLE:
   default:
      return;
   }
}

bool Bot::IsRestricted (int weaponIndex)
{
   // this function checks for weapon restrictions.

   if (IsNullString (sypb_restrictweapons.GetString ()))
      //return false; // no banned weapons
	  return IsRestrictedAMX (weaponIndex); // SyPB Pro P.24 - Buy Ai

   Array <String> bannedWeapons = String (sypb_restrictweapons.GetString ()).Split (";");

   ITERATE_ARRAY (bannedWeapons, i)
   {
      const char *banned = STRING (GetWeaponReturn (true, null, weaponIndex));

      // check is this weapon is banned
      if (strncmp (bannedWeapons[i], banned, bannedWeapons[i].GetLength ()) == 0)
         return true;
   }

   if (m_buyingFinished)
      return false;

   return IsRestrictedAMX (weaponIndex);
}

bool Bot::IsRestrictedAMX (int weaponIndex)
{
   // this function checks restriction set by AMX Mod, this function code is courtesy of KWo.

   const char *restrictedWeapons = CVAR_GET_STRING ("amx_restrweapons");
   const char *restrictedEquipment = CVAR_GET_STRING ("amx_restrequipammo");

   // check for weapon restrictions
   if ((1 << weaponIndex) & (WeaponBits_Primary | WeaponBits_Secondary | WEAPON_SHIELDGUN))
   {
      if (IsNullString (restrictedWeapons))
         return false;

      int indices[] = {4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (strlen (restrictedWeapons)))
         return false;

      return restrictedWeapons[index] != '0';
   }
   else // check for equipment restrictions
   {
      if (IsNullString (restrictedEquipment))
         return false;

      int indices[] = {-1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 0, 1, 5};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (strlen (restrictedEquipment)))
         return false;

      return restrictedEquipment[index] != '0';
   }
}

bool Bot::IsMorePowerfulWeaponCanBeBought (void)
{
   // this function determines currently owned primary weapon, and checks if bot has
   // enough money to buy more powerful weapon.

   // if bot is not rich enough or non-standard weapon mode enabled return false
   if (g_weaponSelect[25].teamStandard != 1 || m_moneyAmount < 4000 || IsNullString (sypb_restrictweapons.GetString ()))
      return false;

   // also check if bot has really bad weapon, maybe it's time to change it
   if (UsesBadPrimary ())
      return true;

   Array <String> bannedWeapons = String (sypb_restrictweapons.GetString ()).Split (";");

   // check if its banned
   ITERATE_ARRAY (bannedWeapons, i)
   {
      if (m_currentWeapon == GetWeaponReturn (false, bannedWeapons[i]))
         return true;
   }

   if (m_currentWeapon == WEAPON_SCOUT && m_moneyAmount > 5000)
      return true;
   else if (m_currentWeapon == WEAPON_MP5 && m_moneyAmount > 6000)
      return true;
   else if (m_currentWeapon == WEAPON_M3 || m_currentWeapon == WEAPON_XM1014 && m_moneyAmount > 4000)
      return true;

   return false;
}

// SyPB Pro P.24 - Buy Ai
void Bot::PerformWeaponPurchase (void) 
{
	m_nextBuyTime = engine->GetTime ();
	WeaponSelect *selectedWeapon = null;

	int team = GetTeam (GetEntity ());
	int *ptr = g_weaponPrefs[m_personality] + Const_NumWeapons;

	switch (m_buyState)
	{
	case 0:
		if ((!HasShield () && !HasPrimaryWeapon ()) && (g_botManager->EconomicsValid (team) || IsMorePowerfulWeaponCanBeBought ()))
		{
			int gunMoney = 0, playerMoney = m_moneyAmount;
			int likeGunId[2] = {0, 0};
			int loadTime = 0;

			do
			{
				ptr--;
				gunMoney = 0;

				InternalAssert (*ptr > -1);
				InternalAssert (*ptr < Const_NumWeapons);

				selectedWeapon = &g_weaponSelect[*ptr];
				loadTime++;

				if (selectedWeapon->buyGroup == 1)
					continue;

				if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != team)
					continue;

				if (g_gameVersion == CSVER_VERYOLD && selectedWeapon->buySelect == -1)
					continue;

				if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != team)
					continue;

				if (IsRestricted (selectedWeapon->id))
					continue;

				gunMoney = selectedWeapon->price;

				if (playerMoney <= gunMoney)
					continue;

				int gunMode = BuyWeaponMode (selectedWeapon->id);

				if (playerMoney < gunMoney + (gunMode * 100))
					continue;

				if (likeGunId[0] == 0)
					likeGunId[0] = selectedWeapon->id;
				else 
				{
					if (gunMode <= BuyWeaponMode (likeGunId[0]))
					{
						if ((BuyWeaponMode (likeGunId[1]) > BuyWeaponMode (likeGunId[0])) ||
							(BuyWeaponMode (likeGunId[1]) == BuyWeaponMode (likeGunId[0]) && (engine->RandomInt (1, 2) == 2)))
							likeGunId[1] = likeGunId[0];

						likeGunId[0] = selectedWeapon->id;
					}
					else
					{
						if (likeGunId[1] != 0)
						{
							if (gunMode <= BuyWeaponMode (likeGunId[1]))
								likeGunId[1] = selectedWeapon->id;
						}
						else
							likeGunId[1] = selectedWeapon->id;
					}
				}
			} while (loadTime < Const_NumWeapons);

			if (likeGunId[0] != 0)
			{
				WeaponSelect *buyWeapon = &g_weaponSelect[0];
				int weaponId = likeGunId[0];
				if (likeGunId[1] != 0)
					weaponId = likeGunId[(engine->RandomInt (1, 7) > 3) ? 0 : 1];

				for (int i = 0; i < Const_NumWeapons; i++)
				{
					if (buyWeapon[i].id == weaponId)
					{
						FakeClientCommand (GetEntity (), "buy;menuselect %d", buyWeapon[i].buyGroup);

						if (g_gameVersion == CSVER_VERYOLD)
							FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].buySelect);
						else
						{
							if (GetTeam (GetEntity ()) == TEAM_TERRORIST)
								FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].newBuySelectT);
							else
								FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].newBuySelectCT);
						}
					}
				}
			}
		}
		else if (HasPrimaryWeapon () && !HasShield ())
			m_reloadState = RSTATE_PRIMARY;

		break;

	case 1:
		if (pev->armorvalue < engine->RandomInt (50, 80) && (g_botManager->EconomicsValid (team) && HasPrimaryWeapon ()))
		{
			if (m_moneyAmount > 1500 && !IsRestricted (WEAPON_KEVHELM))
				FakeClientCommand (GetEntity (), "buyequip;menuselect 2");
			else
				FakeClientCommand (GetEntity (), "buyequip;menuselect 1");
		}
		break;

	case 2:
		if ((pev->weapons & ((1 << WEAPON_USP) | (1 << WEAPON_GLOCK18))) && ((HasPrimaryWeapon () && m_moneyAmount > engine->RandomInt (7500, 9000)) || !HasPrimaryWeapon ()))
		{
			int likeGunId = 0;
			int loadTime = 0;
			do
			{
				ptr--;

				InternalAssert (*ptr > -1);
				InternalAssert (*ptr < Const_NumWeapons);

				selectedWeapon = &g_weaponSelect[*ptr];
				loadTime++;

				if (selectedWeapon->buyGroup != 1)
					continue;

				if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != team)
					continue;

				if (g_gameVersion == CSVER_VERYOLD && selectedWeapon->buySelect == -1)
					continue;

				if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != team)
					continue;

				if (IsRestricted (selectedWeapon->id))
					continue;

				if (m_moneyAmount <= (selectedWeapon->price + 120))
					continue;

				int gunMode = BuyWeaponMode (selectedWeapon->id);

				if (likeGunId == 0)
				{
					if ((pev->weapons & ((1 << WEAPON_USP) | (1 << WEAPON_GLOCK18))))
					{
						if (gunMode < 2)
							likeGunId = selectedWeapon->id;
					}
					else 
						likeGunId = selectedWeapon->id;
				}
				else 
				{
					if (gunMode < BuyWeaponMode (likeGunId))
						likeGunId = selectedWeapon->id;
				}
			} while (loadTime < Const_NumWeapons);

			if (likeGunId != 0)
			{
				WeaponSelect *buyWeapon = &g_weaponSelect[0];
				int weaponId = likeGunId;

				for (int i = 0; i < Const_NumWeapons; i++)
				{
					if (buyWeapon[i].id == weaponId)
					{
						FakeClientCommand (GetEntity (), "buy;menuselect %d", buyWeapon[i].buyGroup);

						if (g_gameVersion == CSVER_VERYOLD)
							FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].buySelect);
						else
						{
							if (GetTeam (GetEntity ()) == TEAM_TERRORIST)
								FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].newBuySelectT);
							else
								FakeClientCommand (GetEntity (), "menuselect %d", buyWeapon[i].newBuySelectCT);
						}
					}
				}
			}
		}
		break;

	case 3:
		if (engine->RandomInt (1, 100) < g_grenadeBuyPrecent[0] && m_moneyAmount >= g_grenadeBuyMoney[0] && !IsRestricted (WEAPON_HEGRENADE))
		{
			FakeClientCommand (GetEntity (), "buyequip");
			FakeClientCommand (GetEntity (), "menuselect 4");
		}

		if (engine->RandomInt (1, 100) < g_grenadeBuyPrecent[1] && m_moneyAmount >= g_grenadeBuyMoney[1] && g_botManager->EconomicsValid (team) && !IsRestricted (WEAPON_FBGRENADE))
		{
			FakeClientCommand (GetEntity (), "buyequip");
			FakeClientCommand (GetEntity (), "menuselect 3");
		}

		if (engine->RandomInt (1, 100) < g_grenadeBuyPrecent[2] && m_moneyAmount >= g_grenadeBuyMoney[2] && g_botManager->EconomicsValid (team) && !IsRestricted (WEAPON_SMGRENADE))
		{
			FakeClientCommand (GetEntity (), "buyequip");
			FakeClientCommand (GetEntity (), "menuselect 5");
		}
		break;

	case 4:
		if ((g_mapType & MAP_DE) && GetTeam (GetEntity ()) == TEAM_COUNTER && engine->RandomInt (1, 100) < 80 && m_moneyAmount > 200 && !IsRestricted (WEAPON_DEFUSER))
		{
			if (g_gameVersion == CSVER_VERYOLD)
				FakeClientCommand (GetEntity (), "buyequip;menuselect 6");
			else
				FakeClientCommand (GetEntity (), "defuser"); // use alias in SteamCS
		}

		break;

	case 5:
		for (int i = 0; i <= 5; i++)
			FakeClientCommand (GetEntity (), "buyammo%d", engine->RandomInt (1, 2)); // simulate human

		if (HasPrimaryWeapon ())
			FakeClientCommand (GetEntity (), "buy;menuselect 7");

		FakeClientCommand (GetEntity (), "buy;menuselect 6");

		if (m_reloadState != RSTATE_PRIMARY)
			m_reloadState = RSTATE_SECONDARY;

		break;
	}

	m_buyState++;
	PushMessageQueue (CMENU_BUY);
}

int Bot::BuyWeaponMode (int weaponId)
{
	int gunMode = 10;
	switch (weaponId)
	{
	case WEAPON_SHIELDGUN:
		gunMode = 8;
		break;

	case WEAPON_TMP:
	case WEAPON_UMP45:
	case WEAPON_P90:
	case WEAPON_MAC10:
	case WEAPON_SCOUT:
	case WEAPON_M3:
	case WEAPON_M249:
	case WEAPON_FN57:
	case WEAPON_P228:
		gunMode = 5;
		break;

	case WEAPON_XM1014:
	case WEAPON_G3SG1:
	case WEAPON_SG550:
	case WEAPON_GALIL:	
	case WEAPON_ELITE:
	case WEAPON_SG552:
	case WEAPON_AUG:
		gunMode = 4;
		break;

	case WEAPON_MP5:
	case WEAPON_FAMAS:
	case WEAPON_USP:
	case WEAPON_GLOCK18:
		gunMode = 3;
		break;

	case WEAPON_AWP:
	case WEAPON_DEAGLE:
		gunMode = 2;
		break;

	case WEAPON_AK47:
	case WEAPON_M4A1:
		gunMode = 1;
		break;
	}

	return gunMode;
}


Task *ClampDesire (Task *first, float min, float max)
{
   // this function iven some values min and max, clamp the inputs to be inside the [min, max] range.

   if (first->desire < min)
      first->desire = min;
   else if (first->desire > max)
      first->desire = max;

   return first;
}

Task *MaxDesire (Task *first, Task *second)
{
   // this function returns the behavior having the higher activation level.

   if (first->desire > second->desire)
      return first;

   return second;
}

Task *SubsumeDesire (Task *first, Task *second)
{
   // this function returns the first behavior if its activation level is anything higher than zero.

   if (first->desire > 0)
      return first;

   return second;
}

Task *ThresholdDesire (Task *first, float threshold, float desire)
{
   // this function returns the input behavior if it's activation level exceeds the threshold, or some default
   // behavior otherwise.

   if (first->desire < threshold)
      first->desire = desire;

   return first;
}

float HysteresisDesire (float cur, float min, float max, float old)
{
   // this function clamp the inputs to be the last known value outside the [min, max] range.

   if (cur <= min || cur >= max)
      old = cur;

   return old;
}

void Bot::SetConditions (void)
{
   // this function carried out each frame. does all of the sensing, calculates emotions and finally sets the desired
   // action after applying all of the Filters

   int team = GetTeam (GetEntity ());
   m_aimFlags = 0;

   // slowly increase/decrease dynamic emotions back to their base level
   if (m_nextEmotionUpdate < engine->GetTime ())
   {
	   if (m_skill >= 80)
      {
         m_agressionLevel *= 2;
         m_fearLevel *= 0.5f;
      }

      if (m_agressionLevel > m_baseAgressionLevel)
         m_agressionLevel -= 0.05f;
      else
         m_agressionLevel += 0.05f;

      if (m_fearLevel > m_baseFearLevel)
         m_fearLevel -= 0.05f;
      else
         m_fearLevel += 0.05f;

      if (m_agressionLevel < 0.0f)
         m_agressionLevel = 0.0f;

      if (m_fearLevel < 0.0f)
         m_fearLevel = 0.0f;

      m_nextEmotionUpdate = engine->GetTime () + 0.5f;
   }

   // does bot see an enemy?
   if (LookupEnemy())
   {
	   m_states |= STATE_SEEINGENEMY;
	   SetMoveTarget(null);
   }
   else
   {
	   m_states &= ~STATE_SEEINGENEMY;
	   SetEnemy(null);
   }

   // SyPB Pro P.42 - Small improve
   if (m_lastVictim != null && (!IsAlive(m_lastVictim) || !IsValidPlayer(m_lastVictim)))
	   m_lastVictim = null;

   // did bot just kill an enemy?
   if (!FNullEnt (m_lastVictim))
   {
      if (GetTeam (m_lastVictim) != team)
      {
         // add some aggression because we just killed somebody
         m_agressionLevel += 0.1f;

         if (m_agressionLevel > 1.0f)
            m_agressionLevel = 1.0f;

         if (engine->RandomInt (1, 100) > 50)
            ChatMessage (CHAT_KILL);

         if (engine->RandomInt (1, 100) < 10)
            RadioMessage (Radio_EnemyDown);

         // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
         if (GetTeam (GetEntity ()) == TEAM_COUNTER && m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0 && g_bombPlanted)
         {
            SelectWeaponByName ("weapon_knife");

            // order team to regroup
            RadioMessage (Radio_RegroupTeam);
         }
      }
	  else
         ChatMessage (CHAT_TEAMKILL, true);

      m_lastVictim = null;
   }

   // check if our current enemy is still valid
   if (!FNullEnt(m_lastEnemy))
   {
	   //if (!IsAlive (m_lastEnemy) && m_shootAtDeadTime < engine->GetTime ())
		// SyPB Pro P.26 - DM Mod Protect Time
	   if (IsNotAttackLab(m_lastEnemy) || (!IsAlive(m_lastEnemy) && m_shootAtDeadTime < engine->GetTime()))
		   SetLastEnemy(null);
   }
   else
	   SetLastEnemy(null);

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (m_soundUpdateTime <= engine->GetTime () && m_blindTime < engine->GetTime ())
   {
      ReactOnSound ();
      m_soundUpdateTime = engine->GetTime () + 0.3f; // SyPB Pro P.26 - no timer sound var
   }
   else if (m_heardSoundTime < engine->GetTime ())
      m_states &= ~STATE_HEARENEMY;
   
   // SyPB Pro P.40 - Game mode setting
   if (FNullEnt (m_enemy) && !FNullEnt (m_lastEnemy) && m_lastEnemyOrigin != nullvec && 
	   (GetGameMod () == MODE_BASE || GetGameMod () == MODE_DM) && 
	   (pev->origin - m_lastEnemyOrigin).GetLength() < 1600.0f)
   {
      TraceResult tr;
      TraceLine (EyePosition (), m_lastEnemyOrigin, true, GetEntity (), &tr);

      if ((tr.flFraction >= 0.2f || tr.pHit != g_worldEdict))
      {
         m_aimFlags |= AIM_PREDICTENEMY;

         if (EntityIsVisible (m_lastEnemyOrigin))
            m_aimFlags |= AIM_LASTENEMY;
      }
   }

   CheckGrenadeThrow();

   // check if there are items needing to be used/collected
   if (m_itemCheckTime < engine->GetTime () || !FNullEnt (m_pickupItem))
   {
      m_itemCheckTime = engine->GetTime () + 2.5f;
      FindItem ();
   }

   float tempFear = m_fearLevel;
   float tempAgression = m_agressionLevel;

   // decrease fear if teammates near
   int friendlyNum = 0;

   if (m_lastEnemyOrigin != nullvec)
      friendlyNum = GetNearbyFriendsNearPosition (pev->origin, 300) - GetNearbyEnemiesNearPosition (m_lastEnemyOrigin, 500);

   if (friendlyNum > 0)
      tempFear = tempFear * 0.5f;

   // increase/decrease fear/aggression if bot uses a sniping weapon to be more careful
   if (UsesSniper ())
   {
      tempFear = tempFear * 1.5f;
      tempAgression = tempAgression * 0.5f;
   }

   // initialize & calculate the desire for all actions based on distances, emotions and other stuff
   GetCurrentTask ();

   // bot found some item to use?
   if (m_pickupType != PICKTYPE_NONE && !FNullEnt (m_pickupItem) && !(m_states & STATE_SEEINGENEMY))
   {
      m_states |= STATE_PICKUPITEM;

      if (m_pickupType == PICKTYPE_BUTTON)
         g_taskFilters[TASK_PICKUPITEM].desire = 50.0f; // always pickup button
      else
      {
         float distance = (500.0f - (GetEntityOrigin (m_pickupItem) - pev->origin).GetLength ()) * 0.2f;

         if (distance > 60.0f)
            distance = 60.0f;

         g_taskFilters[TASK_PICKUPITEM].desire = distance;
      }
   }
   else
   {
      m_states &= ~STATE_PICKUPITEM;
      g_taskFilters[TASK_PICKUPITEM].desire = 0.0f;
   }

   float desireLevel = 0.0f;

   // calculate desire to attack
   if ((m_states & STATE_SEEINGENEMY) && ReactOnEnemy ())
      g_taskFilters[TASK_FIGHTENEMY].desire = TASKPRI_FIGHTENEMY;
   else
      g_taskFilters[TASK_FIGHTENEMY].desire = 0;

   // calculate desires to seek cover or hunt
   if (IsValidPlayer (m_lastEnemy) && m_lastEnemyOrigin != nullvec && !((g_mapType & MAP_DE) && g_bombPlanted) && !(pev->weapons & (1 << WEAPON_C4)) && (m_loosedBombWptIndex == -1 && GetTeam (GetEntity ()) == TEAM_TERRORIST))
   {
      float distance = (m_lastEnemyOrigin - pev->origin).GetLength ();
      
      // SyPB Pro P.16
      float timeSeen = m_seeEnemyTime - engine->GetTime ();
      float timeHeard = m_heardSoundTime - engine->GetTime ();
      float ratio = 0.0f;
      float retreatLevel = (100.0f - pev->health) * tempFear;
      
      if (timeSeen > timeHeard)
      {
         timeSeen += 10.0f;
         ratio = timeSeen * 0.1f;
      }
      else
      {
         timeHeard += 10.0f;
         ratio = timeHeard * 0.1f;
      }
      
      if (IsZombieEntity (GetEntity ()))
      	  ratio = 0;
      else 
      {
      	  if (!FNullEnt (m_enemy) && IsZombieEntity (m_enemy))
      	  	  ratio *= 10;
      	  else if (!FNullEnt (m_lastEnemy) && IsZombieEntity (m_lastEnemy))
      	  	  ratio *= 6;
      	  else if (g_bombPlanted || m_isStuck)
      	  	  ratio /= 3;
      	  else if (m_isVIP || m_isReloading)
      	  	  ratio *= 2;
      }
      
	  // SyPB Pro P.38 - Small Change
	  if (distance > 500.0f || (!IsZombieEntity(GetEntity()) &&
		  ((!FNullEnt(m_enemy) && IsZombieEntity(m_enemy)) || (!FNullEnt(m_lastEnemy) && IsZombieEntity(m_lastEnemy)))))
		  g_taskFilters[TASK_SEEKCOVER].desire = retreatLevel * ratio;
      
      // if half of the round is over, allow hunting
      // FIXME: it probably should be also team/map dependant
      if (FNullEnt (m_enemy) && (g_timeRoundMid < engine->GetTime ()) && !m_isUsingGrenade && 
		  m_personality != PERSONALITY_CAREFUL && m_currentWaypointIndex != g_waypoint->FindNearest(m_lastEnemyOrigin))
      {
         desireLevel = 4096.0f - ((1.0f - tempAgression) * distance);
         desireLevel = (100 * desireLevel) / 4096.0f;
         desireLevel -= retreatLevel;

         if (desireLevel > 89)
            desireLevel = 89;

		 // SyPB Pro P.10
		 if (GetGameMod () == MODE_DM)
			 desireLevel *= 2;
		 else if (GetGameMod () == MODE_ZP) // SyPB Pro P.26 - Zombie mode Ai
			 desireLevel = 0;
		 else if (GetGameMod () == MODE_ZH)
			 desireLevel = 0;
		 else 
		 {
			 if (g_mapType & MAP_DE)
			 {
				 if ((g_bombPlanted && GetTeam (GetEntity ()) == TEAM_COUNTER) || (!g_bombPlanted && GetTeam (GetEntity ()) == TEAM_TERRORIST))
					 desireLevel *= 1.5;
				 if ((g_bombPlanted && GetTeam (GetEntity ()) == TEAM_TERRORIST) || (!g_bombPlanted && GetTeam (GetEntity ()) == TEAM_COUNTER))
					 desireLevel *= 0.5;
			 }
		 }

         g_taskFilters[TASK_HUNTENEMY].desire = desireLevel;
      }
      else
         g_taskFilters[TASK_HUNTENEMY].desire = 0;
   }
   else
   {
      g_taskFilters[TASK_SEEKCOVER].desire = 0;
      g_taskFilters[TASK_HUNTENEMY].desire = 0;
   }

   // blinded behaviour
   if (m_blindTime > engine->GetTime ())
      g_taskFilters[TASK_BLINDED].desire = TASKPRI_BLINDED;
   else
      g_taskFilters[TASK_BLINDED].desire = 0.0f;

   // now we've initialized all the desires go through the hard work
   // of filtering all actions against each other to pick the most
   // rewarding one to the bot.

   // FIXME: instead of going through all of the actions it might be
   // better to use some kind of decision tree to sort out impossible
   // actions.

   // most of the values were found out by trial-and-error and a helper
   // utility i wrote so there could still be some weird behaviors, it's
   // hard to check them all out.

   m_oldCombatDesire = HysteresisDesire (g_taskFilters[TASK_FIGHTENEMY].desire, 40.0, 90.0, m_oldCombatDesire);
   g_taskFilters[TASK_FIGHTENEMY].desire = m_oldCombatDesire;

   Task *taskOffensive = &g_taskFilters[TASK_FIGHTENEMY];
   Task *taskPickup = &g_taskFilters[TASK_PICKUPITEM];

   // calc survive (cover/hide)
   Task *taskSurvive = ThresholdDesire (&g_taskFilters[TASK_SEEKCOVER], 40.0, 0.0f);
   taskSurvive = SubsumeDesire (&g_taskFilters[TASK_HIDE], taskSurvive);

   Task *def = ThresholdDesire (&g_taskFilters[TASK_HUNTENEMY], 60.0, 0.0f); // don't allow hunting if desire's 60<
   taskOffensive = SubsumeDesire (taskOffensive, taskPickup); // if offensive task, don't allow picking up stuff

   Task *taskSub = MaxDesire (taskOffensive, def); // default normal & careful tasks against offensive actions
   Task *final = SubsumeDesire (&g_taskFilters[TASK_BLINDED], MaxDesire (taskSurvive, taskSub)); // reason about fleeing instead

   if (m_tasks != null)
      final = MaxDesire (final, m_tasks);

   PushTask (final);
}

void Bot::ResetTasks (void)
{
   // this function resets bot tasks stack, by removing all entries from the stack.

   if (m_tasks == null)
      return; // reliability check

   Task *next = m_tasks->next;
   Task *prev = m_tasks;

   while (m_tasks != null)
   {
      prev = m_tasks->prev;

      delete m_tasks;
      m_tasks = prev;
   }
   m_tasks = next;

   while (m_tasks != null)
   {
      next = m_tasks->next;

      delete m_tasks;
      m_tasks = next;
   }
   m_tasks = null;
}

void Bot::CheckTasksPriorities (void)
{
   // this function checks the tasks priorities.

   if (m_tasks == null)
   {
      GetCurrentTask ();
      return;
   }

   Task *oldTask = m_tasks;
   Task *prev = null;
   Task *next = null;

   while (m_tasks->prev != null)
   {
      prev = m_tasks->prev;
      m_tasks = prev;
   }

   Task *firstTask = m_tasks;
   Task *maxDesiredTask = m_tasks;

   float maxDesire = m_tasks->desire;

   // search for the most desired task
   while (m_tasks->next != null)
   {
      next = m_tasks->next;
      m_tasks = next;

      if (m_tasks->desire >= maxDesire)
      {
         maxDesiredTask = m_tasks;
         maxDesire = m_tasks->desire;
      }
   }
   m_tasks = maxDesiredTask; // now we found the most desired pushed task...

   // something was changed with priorities, check if some task doesn't need to be deleted...
   if (oldTask != maxDesiredTask)
   {
      m_tasks = firstTask;

      while (m_tasks != null)
      {
         next = m_tasks->next;

         // some task has to be deleted if cannot be continued...
         if (m_tasks != maxDesiredTask && !m_tasks->canContinue)
         {
            if (m_tasks->prev != null)
               m_tasks->prev->next = m_tasks->next;

            if (m_tasks->next != null)
               m_tasks->next->prev = m_tasks->prev;

            delete m_tasks;
         }
         m_tasks = next;
      }
   }
   m_tasks = maxDesiredTask;

   if (sypb_debuggoal.GetInt () != -1)
      m_chosenGoalIndex = sypb_debuggoal.GetInt ();
   else
      m_chosenGoalIndex = m_tasks->data;
}

void Bot::PushTask (BotTask taskID, float desire, int data, float time, bool canContinue)
{
   // build task
   Task task = {null, null, taskID, desire, data, time, canContinue};
   PushTask (&task); // use standard function to start task
}

void Bot::PushTask (Task *task)
{
   // this function adds task pointer on the bot task stack.

   bool newTaskDifferent = false;
   bool foundTaskExisting = false;
   bool checkPriorities = false;

   Task *oldTask = GetCurrentTask (); // remember our current task

   // at the beginning need to clean up all null tasks...
   if (m_tasks == null)
   {
      m_lastCollTime = engine->GetTime () + 0.5f;

      Task *newTask = new Task;

      newTask->taskID = TASK_NORMAL;
      newTask->desire = TASKPRI_NORMAL;
      newTask->canContinue = true;
      newTask->time = 0.0f;
      newTask->data = -1;
      newTask->next = newTask->prev = null;

      m_tasks = newTask;
      DeleteSearchNodes ();

      if (task == null)
         return;
      else if (task->taskID == TASK_NORMAL)
      {
         m_tasks->desire = TASKPRI_NORMAL;
         m_tasks->data = task->data;
         m_tasks->time = task->time;
         
         return;
      }
   }
   else if (task == null)
      return;

   // it shouldn't happen this condition now as false...
   if (m_tasks != null)
   {
      if (m_tasks->taskID == task->taskID)
      {
         if (m_tasks->data != task->data)
         {
            m_lastCollTime = engine->GetTime () + 0.5f;

            DeleteSearchNodes ();
            m_tasks->data = task->data;
         }

         if (m_tasks->desire != task->desire)
         {
            m_tasks->desire = task->desire;
            checkPriorities = true;
         }
         else if (m_tasks->data == task->data)
            return;
      }
      else
      {
         // find the first task on the stack and don't allow push the new one like the same already existing one
         while (m_tasks->prev != null)
         {
            m_tasks = m_tasks->prev;

            if (m_tasks->taskID == task->taskID)
            {
               foundTaskExisting = true;

               if (m_tasks->desire != task->desire)
                  checkPriorities = true;

               m_tasks->desire = task->desire;
               m_tasks->data = task->data;
               m_tasks->time = task->time;
               m_tasks->canContinue = task->canContinue;
               m_tasks = oldTask;

               break; // now we may need to check the current max desire or next tasks...
            }
         }

         // now go back to the previous stack position and try to find the same task as one of "the next" ones (already pushed before and not finished yet)
         if (!foundTaskExisting && !checkPriorities)
         {
            m_tasks = oldTask;

            while (m_tasks->next != null)
            {
               m_tasks = m_tasks->next;

               if (m_tasks->taskID == task->taskID)
               {
                  foundTaskExisting = true;

                  if (m_tasks->desire != task->desire)
                     checkPriorities = true;

                  m_tasks->desire = task->desire;
                  m_tasks->data = task->data;
                  m_tasks->time = task->time;
                  m_tasks->canContinue = task->canContinue;
                  m_tasks = oldTask;

                  break; // now we may need to check the current max desire...
               }
            }
         }

         if (!foundTaskExisting)
            newTaskDifferent = true; // we have some new task pushed on the stack...
      }
   }
   m_tasks = oldTask;

   if (newTaskDifferent)
   {
      Task *newTask = new Task;

      newTask->taskID = task->taskID;
      newTask->desire = task->desire;
      newTask->canContinue = task->canContinue;
      newTask->time = task->time;
      newTask->data = task->data;
      newTask->next = null;

      while (m_tasks->next != null)
         m_tasks = m_tasks->next;

      newTask->prev = m_tasks;
      m_tasks->next = newTask;

      checkPriorities = true;
   }
   m_tasks = oldTask;

   // needs check the priorities and setup the task with the max desire...
   if (!checkPriorities)
      return;
 
   CheckTasksPriorities ();

   // the max desired task has been changed...
   if (m_tasks != oldTask)
   {
      DeleteSearchNodes ();
      m_lastCollTime = engine->GetTime () + 0.5f;

      // leader bot?
      if (newTaskDifferent && m_isLeader && m_tasks->taskID == TASK_SEEKCOVER)
         CommandTeam (); // reorganize team if fleeing

	  if (newTaskDifferent && m_tasks->taskID == TASK_CAMP)
		  SelectBestWeapon();
   }
}

Task *Bot::GetCurrentTask (void)
{
   if (m_tasks != null)
      return m_tasks;

   Task *newTask = new Task;

   newTask->taskID = TASK_NORMAL;
   newTask->desire = TASKPRI_NORMAL;
   newTask->data = -1;
   newTask->time = 0.0f;
   newTask->canContinue = true;
   newTask->next = newTask->prev = null;

   m_tasks = newTask;
   m_lastCollTime = engine->GetTime () + 0.5f;

   DeleteSearchNodes ();
   return m_tasks;
}

void Bot::RemoveCertainTask (BotTask taskID)
{
   // this function removes one task from the bot task stack.

   if (m_tasks == null || (m_tasks != null && m_tasks->taskID == TASK_NORMAL))
      return; // since normal task can be only once on the stack, don't remove it...

   bool checkPriorities = false;

   Task *task = m_tasks;
   Task *oldTask = m_tasks;
   Task *oldPrevTask = task->prev;
   Task *oldNextTask = task->next;

   while (task->prev != null)
      task = task->prev;

   while (task != null)
   {
      Task *next = task->next;
      Task *prev = task->prev;

      if (task->taskID == taskID)
      {
         if (prev != null)
            prev->next = next;

         if (next != null)
            next->prev = prev;

         if (task == oldTask)
            oldTask = null;
         else if (task == oldPrevTask)
            oldPrevTask = null;
         else if (task == oldNextTask)
            oldNextTask = null;

         delete task;

         checkPriorities = true;
         break;
      }
      task = next;
   }

   if (oldTask != null)
      m_tasks = oldTask;
   else if (oldPrevTask != null)
      m_tasks = oldPrevTask;
   else if (oldNextTask != null)
      m_tasks = oldNextTask;
   else
      GetCurrentTask ();

   if (checkPriorities)
      CheckTasksPriorities ();
}

void Bot::TaskComplete (void)
{
   // this function called whenever a task is completed.

   if (m_tasks == null)
   {
      DeleteSearchNodes (); // delete all path finding nodes
      return;
   }

   if (m_tasks->taskID == TASK_NORMAL)
   {
      DeleteSearchNodes (); // delete all path finding nodes
      
      m_tasks->data = -1;
      m_chosenGoalIndex = -1;

      return;
   }

   Task *next = m_tasks->next;
   Task *prev = m_tasks->prev;

   if (next != null)
      next->prev = prev;
   
   if (prev != null)
      prev->next = next;

   delete m_tasks;
   m_tasks = null;

   if (prev != null && next != null)
   {
      if (prev->desire >= next->desire)
         m_tasks = prev;
      else
         m_tasks = next;
   }
   else if (prev != null)
      m_tasks = prev;
   else if (next != null)
      m_tasks = next;

   if (m_tasks == null)
      GetCurrentTask ();

   CheckTasksPriorities ();
   DeleteSearchNodes ();
}

// SyPB Pro P.42 - Grenade small improve
void Bot::CheckGrenadeThrow(void)
{
	if (sypb_knifemode.GetBool() || m_grenadeCheckTime >= engine->GetTime() ||
		m_isUsingGrenade || GetCurrentTask()->taskID == TASK_PLANTBOMB ||
		GetCurrentTask()->taskID == TASK_DEFUSEBOMB || m_isReloading)
	{
		m_states &= ~(STATE_THROWEXPLODE | STATE_THROWFLASH | STATE_THROWSMOKE);
		return;
	}

	m_grenadeCheckTime = engine->GetTime() + 1.0f;

	int grenadeToThrow = CheckGrenades();
	if (grenadeToThrow == -1)
	{
		m_states &= ~(STATE_THROWEXPLODE | STATE_THROWFLASH | STATE_THROWSMOKE);
		m_grenadeCheckTime = engine->GetTime() + 12.0f;
		return;
	}

	edict_t *targetEntity = null;
	if (GetGameMod() == MODE_BASE)
		targetEntity = m_lastEnemy;
	else if (GetGameMod() == MODE_ZP)
	{
		if (IsZombieEntity(GetEntity()) && !FNullEnt(m_moveTargetEntity))
			targetEntity = m_moveTargetEntity;
		else if (!IsZombieEntity(GetEntity()) && !FNullEnt(m_enemy))
			targetEntity = m_enemy;
	}

	if (FNullEnt(targetEntity))
	{
		m_states &= ~(STATE_THROWEXPLODE | STATE_THROWFLASH | STATE_THROWSMOKE);
		m_grenadeCheckTime = engine->GetTime() + 5.0f;
		return;
	}

	if ((grenadeToThrow == WEAPON_HEGRENADE || grenadeToThrow == WEAPON_SMGRENADE) && engine->RandomInt(0, 100) < 45 && !(m_states & (STATE_SEEINGENEMY | STATE_THROWEXPLODE | STATE_THROWFLASH | STATE_THROWSMOKE)))
	{
		float distance = (GetEntityOrigin(targetEntity) - pev->origin).GetLength();

		// is enemy to high to throw
		if ((GetEntityOrigin(targetEntity).z > (pev->origin.z + 650.0f)) || !(targetEntity->v.flags & (FL_ONGROUND | FL_DUCKING)))
			distance = FLT_MAX; // just some crazy value

		// enemy is within a good throwing distance ?
		if (distance > (grenadeToThrow == WEAPON_SMGRENADE ? 400 : 600) && distance < 1000.0f)
		{
			// care about different types of grenades
			if (grenadeToThrow == WEAPON_HEGRENADE)
			{
				bool allowThrowing = true;

				// check for teammates
				if (GetNearbyFriendsNearPosition(GetEntityOrigin(targetEntity), 256) > 0)
					allowThrowing = false;

				if (allowThrowing && m_seeEnemyTime + 2.0 < engine->GetTime ())
				{
					Vector enemyPredict = ((targetEntity->v.velocity * 0.5).SkipZ() + GetEntityOrigin(targetEntity));
					int searchTab[4], count = 4;

					float searchRadius = targetEntity->v.velocity.GetLength2D();

					// check the search radius
					if (searchRadius < 128.0f)
						searchRadius = 128.0f;

					// search waypoints
					g_waypoint->FindInRadius(enemyPredict, searchRadius, searchTab, &count);

					while (count > 0)
					{
						allowThrowing = true;

						// check the throwing
						m_throw = g_waypoint->GetPath(searchTab[count--])->origin;
						Vector src = CheckThrow(EyePosition(), m_throw);

						if (src.GetLengthSquared() < 100.0f)
							src = CheckToss(EyePosition(), m_throw);

						if (src == nullvec)
							allowThrowing = false;
						else
							break;
					}
				}

				// start explosive grenade throwing?
				if (allowThrowing)
					m_states |= STATE_THROWEXPLODE;
				else
					m_states &= ~STATE_THROWEXPLODE;
			}
			else if (GetGameMod () == MODE_BASE && grenadeToThrow == WEAPON_SMGRENADE)
			{
				// start smoke grenade throwing?
				if ((m_states & STATE_SEEINGENEMY) && GetShootingConeDeviation(m_enemy, &pev->origin) >= 0.90)
					m_states &= ~STATE_THROWSMOKE;
				else
					m_states |= STATE_THROWSMOKE;
			}
		}
	}
	else if (GetGameMod () == MODE_BASE && grenadeToThrow == WEAPON_FBGRENADE && 
		(GetEntityOrigin(targetEntity) - pev->origin).GetLength() < 800 && !(m_aimFlags & AIM_ENEMY) && engine->RandomInt(0, 100) < 60)
	{
		bool allowThrowing = true;
		Array <int> inRadius;

		g_waypoint->FindInRadius(inRadius, 256, GetEntityOrigin(targetEntity) + (targetEntity->v.velocity * 0.5).SkipZ());

		ITERATE_ARRAY(inRadius, i)
		{
			if (GetNearbyFriendsNearPosition(g_waypoint->GetPath(i)->origin, 256) != 0)
				continue;

			m_throw = g_waypoint->GetPath(i)->origin;
			Vector src = CheckThrow(EyePosition(), m_throw);

			if (src.GetLengthSquared() < 100)
				src = CheckToss(EyePosition(), m_throw);

			if (src == nullvec)
				continue;

			allowThrowing = true;
			break;
		}

		// start concussion grenade throwing?
		if (allowThrowing)
			m_states |= STATE_THROWFLASH;
		else
			m_states &= ~STATE_THROWFLASH;
	}

	const float randTime = engine->GetTime() + engine->RandomFloat(2.0f, 3.5f);

	if (m_states & STATE_THROWEXPLODE)
		PushTask(TASK_THROWHEGRENADE, TASKPRI_THROWGRENADE, -1, randTime, false);
	else if (m_states & STATE_THROWFLASH)
		PushTask(TASK_THROWFBGRENADE, TASKPRI_THROWGRENADE, -1, randTime, false);
	else if (m_states & STATE_THROWSMOKE)
		PushTask(TASK_THROWSMGRENADE, TASKPRI_THROWGRENADE, -1, randTime, false);
}

bool Bot::IsOnAttackDistance(edict_t *targetEntity, float distance)
{
	Vector origin = GetEntityOrigin(GetEntity());
	Vector targetOrigin = GetEntityOrigin(targetEntity);

	for (int i = 0; i < 3; i++)
	{
		if (i == 1)
		{
			targetOrigin = GetTopOrigin(targetEntity);
			targetOrigin.z -= 1;
		}
		else if (i == 2)
		{
			targetOrigin = GetBottomOrigin(targetEntity);
			targetOrigin.z += 1;
		}

		if ((origin - targetOrigin).GetLength() < distance)
			return true;
	}

	return false;
}

bool Bot::EnemyIsThreat (void)
{   	   
   // SyPB Pro P.16
   if (FNullEnt (m_enemy))
   	   return false;

   if (GetCurrentTask ()->taskID == TASK_SEEKCOVER)
   	   return false;

   // SyPB Pro P.48 - Zombie Mode Human Camp improve
   if (GetCurrentTask()->taskID == TASK_CAMP && m_zhCampPointIndex == -1)
	   return false;

   // SyPB Pro P.43 - Enemy Ai small improve 
   if (IsOnAttackDistance(m_enemy, 256) ||
	   (m_currentWaypointIndex != WEAPON_KNIFE && IsInViewCone(GetEntityOrigin(m_enemy))))
	   return true;

   return false;
}

// SyPB Pro P.43 - Enemy Ai improve (for more mode)
bool Bot::ReactOnEnemy(void)
{
	if (!EnemyIsThreat())
		return false;

	if (m_enemyReachableTimer < engine->GetTime())
	{
		if (IsZombieEntity(GetEntity()))
			m_isEnemyReachable = true;
		else
		{
			// SyPB Pro P.48 - Base Enemy Ai improve
			int i = GetEntityWaypoint(GetEntity());
			int enemyIndex = GetEntityWaypoint(m_enemy);
			float pathDist = g_waypoint->GetPathDistanceFloat(i, enemyIndex);
			float enemyDistance = (pev->origin - GetEntityOrigin(m_enemy)).GetLength();

			m_isEnemyReachable = false;
			if (enemyDistance <= 150.0f)
				m_isEnemyReachable = true;
			else if (m_zhCampPointIndex != -1)
			{
				if (enemyIndex == m_zhCampPointIndex)
					m_isEnemyReachable = true;
				else if (enemyDistance <= 240.0f)
				{
					for (int j = 0; j < Const_MaxPathIndex; j++)
					{
						if (g_waypoint->GetPath(enemyIndex)->index[j] == i &&
							// SyPB Pro P.47 - Zombie Mode Human Camp improve
							!(g_waypoint->GetPath(enemyIndex)->connectionFlags[j] & PATHFLAG_JUMP))
						{
							m_isEnemyReachable = true;
							break;
						}
					}
				}
			}
			else if (IsZombieEntity(m_enemy))
			{
				m_isEnemyReachable = false;
				if (m_navNode == null)
					m_isEnemyReachable = true;
				else
				{
					if (pathDist <= 700.0f)
					{
						// SyPB Pro P.48 - Zombie Mode Human Action improve
						if (g_waypoint->GetPathDistanceFloat(m_navNode->next->index, enemyIndex) < pathDist)
							m_isEnemyReachable = true;
					}
				}
			}
			else
			{
				// SyPB Pro P.43 - Knife Ai improve
				if (m_currentWeapon == WEAPON_KNIFE)
				{
					m_isEnemyReachable = false;

					if (i == enemyIndex || enemyDistance <= 150.0f)
						m_isEnemyReachable = true;
					// SyPB Pro P.44 - Knife Ai improve
					else if (m_navNode != null && m_navNode->index == enemyIndex)
						m_isEnemyReachable = true;
						
					if (!m_isEnemyReachable)
					{
						SetMoveTarget(m_enemy);

						// SyPB Pro P.45 - Knife Ai improve
						// Bot will try to change best weapon / not use knife only....
						SelectBestWeapon();
					}
				}
				else
				{
					float lineDist = (GetEntityOrigin(m_enemy) - pev->origin).GetLength();
					if (pathDist - lineDist > 112.0f)
						m_isEnemyReachable = false;
					else
						m_isEnemyReachable = true;
				}
			}
		}

		m_enemyReachableTimer = engine->GetTime() + engine->RandomFloat (0.3f, 0.5f);
	}
	// SyPB Pro P.48 - Base improve
	else if ((pev->origin - GetEntityOrigin(m_enemy)).GetLength() <= 150.0f)
		m_isEnemyReachable = true;

	if (m_isEnemyReachable)
	{
		m_navTimeset = engine->GetTime(); // override existing movement by attack movement
		return true;
	}

	return false;
}

bool Bot::LastEnemyShootable (void)
{
   // don't allow shooting through walls when pausing or camping
   if (!(m_aimFlags & AIM_LASTENEMY) || FNullEnt (m_lastEnemy) || GetCurrentTask ()->taskID == TASK_PAUSE || GetCurrentTask ()->taskID == TASK_CAMP)
      return false;

   return GetShootingConeDeviation (GetEntity (), &m_lastEnemyOrigin) >= 0.90;
}

void Bot::CheckRadioCommands (void)
{
   // this function handling radio and reactings to it

   float distance = (GetEntityOrigin (m_radioEntity) - pev->origin).GetLength ();
   
   // SyPB Pro P.15
   if (GetGameMod () == MODE_DM || GetGameMod () == MODE_ZP || 
	   (GetGameMod () == MODE_ZH && IsZombieEntity (GetEntity ())))
   {
   	   m_radioOrder = 0;
   	   return;
   }
   
   // don't allow bot listen you if bot is busy
   if (GetCurrentTask ()->taskID == TASK_DEFUSEBOMB || GetCurrentTask ()->taskID == TASK_PLANTBOMB || HasHostage () && m_radioOrder != Radio_ReportTeam)
   {
      m_radioOrder = 0;
      return;
   }

   switch (m_radioOrder)
   {
   case Radio_CoverMe:
   case Radio_FollowMe:
      // check if line of sight to object is not blocked (i.e. visible)
      if (EntityIsVisible (GetEntityOrigin (m_radioEntity)))
      {
         //if (FNullEnt (m_targetEntity) && FNullEnt (m_enemy) && engine->RandomInt (0, 100) < (m_personality == PERSONALITY_CAREFUL ? 80 : 50))
		  if ((FNullEnt (m_targetEntity) && FNullEnt (m_enemy) && engine->RandomInt (0, 100) < (m_personality == PERSONALITY_CAREFUL ? 80 : 50)) || 
			  (GetGameMod () == MODE_ZP) && engine->RandomInt (70, 90) != 80)  // SyPB Pro P.10
         {
            int numFollowers = 0;

            // Check if no more followers are allowed
            for (int i = 0; i < engine->GetMaxClients (); i++)
            {
               if (g_botManager->GetBot (i))
               {
                  if (IsAlive (g_botManager->GetBot (i)->GetEntity ()))
                  {
                     if (g_botManager->GetBot (i)->m_targetEntity == m_radioEntity)
                        numFollowers++;
                  }
               }
            }

            int allowedFollowers = sypb_followuser.GetInt ();

            if (m_radioEntity->v.weapons & (1 << WEAPON_C4))
               allowedFollowers = 1;

            if (numFollowers < allowedFollowers)
            {
               RadioMessage (Radio_Affirmative);
               m_targetEntity = m_radioEntity;

               // don't pause/camp/follow anymore
               BotTask taskID = GetCurrentTask ()->taskID;

               if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
                  m_tasks->time = engine->GetTime ();

               PushTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, -1, 0.0, true);
            }
            else if (numFollowers > allowedFollowers)
            {
               for (int i = 0; (i < engine->GetMaxClients () && numFollowers > allowedFollowers) ; i++)
               {
                  if (g_botManager->GetBot (i))
                  {
                     if (IsAlive (g_botManager->GetBot (i)->GetEntity ()))
                     {
                        if (g_botManager->GetBot (i)->m_targetEntity == m_radioEntity)
                        {
                           g_botManager->GetBot (i)->m_targetEntity = null;
                           numFollowers--;
                        }
                     }
                  }
               }
            }
            else
               RadioMessage (Radio_Negative);
         }
         else
            RadioMessage (Radio_Negative);
      }
      break;

   case Radio_HoldPosition:
      if (!FNullEnt (m_targetEntity))
      {
         if (m_targetEntity == m_radioEntity)
         {
            m_targetEntity = null;
            RadioMessage (Radio_Affirmative);

            m_campButtons = 0;

            PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, engine->GetTime () + engine->RandomFloat (30.0f, 60.0f), false);
         }
      }
      break;

   case Radio_TakingFire:
      if (FNullEnt (m_targetEntity))
      {
         if (FNullEnt (m_enemy))
         {
            // Decrease Fear Levels to lower probability of Bot seeking Cover again
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f)
               m_fearLevel = 0.0f;

			if (m_radioOrder == Radio_NeedBackup)
               RadioMessage (Radio_Affirmative);

            // don't pause/camp anymore
            BotTask taskID = GetCurrentTask ()->taskID;

            if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
               m_tasks->time = engine->GetTime ();

            m_position = GetEntityOrigin (m_radioEntity);
            DeleteSearchNodes ();

            PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0, true);
         }
         else if (engine->RandomInt (0, 100) < 80 && m_radioOrder == Radio_TakingFire)
            RadioMessage (Radio_Negative);
      }
      break;

   case Radio_YouTakePoint:
      if (m_isLeader && EntityIsVisible(GetEntityOrigin(m_radioEntity)))
         RadioMessage (Radio_Affirmative);
      break;

   case Radio_NeedBackup:
      if (distance < 2048.0f || !m_moveToC4 || 
		  (FNullEnt(m_enemy) && EntityIsVisible(GetEntityOrigin(m_radioEntity))))
      {
         m_fearLevel -= 0.1f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;

		 if (m_radioOrder == Radio_NeedBackup)
            RadioMessage (Radio_Affirmative);

         // don't pause/camp anymore
         BotTask taskID = GetCurrentTask ()->taskID;

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
            m_tasks->time = engine->GetTime ();

         m_moveToC4 = true;
         m_position = GetEntityOrigin (m_radioEntity);

         DeleteSearchNodes ();

         // start move to position task
         PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0, true);
      }
      else if (engine->RandomInt (0, 100) < 80 && m_radioOrder == Radio_NeedBackup)
         RadioMessage (Radio_Negative);
      break;

   case Radio_GoGoGo:
      if (m_radioEntity == m_targetEntity)
      {
         RadioMessage (Radio_Affirmative);

         m_targetEntity = null;
         m_fearLevel -= 0.3f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;
      }
      else if (distance < 2048.0f || (FNullEnt (m_enemy) && EntityIsVisible (GetEntityOrigin (m_radioEntity))))
      {
         BotTask taskID = GetCurrentTask ()->taskID;

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
         {
            m_fearLevel -= 0.3f;

            if (m_fearLevel < 0.0f)
               m_fearLevel = 0.0f;

            RadioMessage (Radio_Affirmative);
            // don't pause/camp anymore
            m_tasks->time = engine->GetTime ();

            m_targetEntity = null;
            MakeVectors (m_radioEntity->v.v_angle);

            m_position = GetEntityOrigin (m_radioEntity) + g_pGlobals->v_forward * engine->RandomFloat (1024.0f, 2048.0f);

            DeleteSearchNodes ();
            PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);
         }
      }
      else if (!FNullEnt (m_doubleJumpEntity))
      {
         RadioMessage (Radio_Affirmative);
         ResetDoubleJumpState ();
      }
      else
         RadioMessage (Radio_Negative);
      break;

   case Radio_ShesGonnaBlow:
       if (FNullEnt (m_enemy) && distance < 2048 && g_bombPlanted == true && GetTeam (GetEntity ()) == TEAM_TERRORIST)
      {
         RadioMessage (Radio_Affirmative);

         if (GetCurrentTask ()->taskID == TASK_CAMP)
            RemoveCertainTask (TASK_CAMP);

         m_targetEntity = null;
         PushTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0, true);
      }
      else
        RadioMessage (Radio_Negative);

      break;

   case Radio_RegroupTeam:
      // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
      if ((GetTeam (GetEntity ()) == TEAM_COUNTER) && m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0 && g_bombPlanted && GetCurrentTask ()->taskID != TASK_DEFUSEBOMB)
      {
         SelectWeaponByName ("weapon_knife");

         DeleteSearchNodes ();
         MoveToVector (g_waypoint->GetBombPosition ());

         RadioMessage (Radio_Affirmative);
      }
      break;

   case Radio_StormTheFront:
      if (distance < 1024.0f || (FNullEnt (m_enemy) && EntityIsVisible (GetEntityOrigin (m_radioEntity))))
      {
         RadioMessage (Radio_Affirmative);

         // don't pause/camp anymore
         BotTask taskID = GetCurrentTask ()->taskID;

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
            m_tasks->time = engine->GetTime ();

         m_targetEntity = null;

         MakeVectors (m_radioEntity->v.v_angle);
         m_position = GetEntityOrigin (m_radioEntity) + g_pGlobals->v_forward * engine->RandomFloat (1024.0f, 2048.0f);

         DeleteSearchNodes ();
         PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);

         m_fearLevel -= 0.3f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;

         m_agressionLevel += 0.3f;

         if (m_agressionLevel > 1.0f)
            m_agressionLevel = 1.0f;
      }
      break;

   case Radio_Fallback:
      if (distance < 1024.0f || (FNullEnt (m_enemy) && EntityIsVisible (GetEntityOrigin (m_radioEntity))))
      {
         m_fearLevel += 0.5f;

         if (m_fearLevel > 1.0f)
            m_fearLevel = 1.0f;

         m_agressionLevel -= 0.5f;

         if (m_agressionLevel < 0.0f)
            m_agressionLevel = 0.0f;

         if (GetCurrentTask ()->taskID == TASK_CAMP)
            m_tasks->time += engine->RandomFloat (10.0f, 15.0f);
         else
         {
            // don't pause/camp anymore
            BotTask taskID = GetCurrentTask ()->taskID;

            if (taskID == TASK_PAUSE)
               m_tasks->time = engine->GetTime ();

            m_targetEntity = null;
            m_seeEnemyTime = engine->GetTime ();

            // if bot has no enemy
            if (m_lastEnemyOrigin == nullvec)
            {
               int team = GetTeam (GetEntity ());
               float nearestDistance = FLT_MAX;

               // take nearest enemy to ordering player
               for (int i = 0; i < engine->GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CFLAG_USED) || !(g_clients[i].flags & CFLAG_ALIVE) || g_clients[i].team == team)
                     continue;

                  edict_t *enemy = g_clients[i].ent;
                  float nowDistance = (GetEntityOrigin (m_radioEntity) - GetEntityOrigin (enemy)).GetLengthSquared ();

                  if (nowDistance < nearestDistance)
                  {
                     nearestDistance = nowDistance;

					 SetLastEnemy(enemy);
                  }
               }
            }
            DeleteSearchNodes ();
         }
      }
      break;

   // SyPB Pro P.20 - Radio Debug
   case Radio_ReportTeam:
	   if (IsZombieEntity (GetEntity ()))
		   RadioMessage ((!FNullEnt (m_enemy) || !FNullEnt (m_moveTargetEntity)) ? Radio_EnemySpotted : Radio_SectorClear);
	   else
	   {
		   if (g_bombPlanted)
		   {
			   if (GetTeam (GetEntity ()) != TEAM_COUNTER)
			   {
				   if (!FNullEnt (m_enemy) || !FNullEnt (m_lastEnemy))
					   RadioMessage (Radio_EnemySpotted);
				   else
					   RadioMessage (GetCurrentTask ()->taskID == TASK_CAMP ? Radio_InPosition : Radio_SectorClear);
			   }
			   else
				   RadioMessage (Radio_StormTheFront);
		   }
		   else
			   RadioMessage ((!FNullEnt (m_enemy) || !FNullEnt (m_lastEnemy)) ? Radio_EnemySpotted : Radio_SectorClear);
	   }

	   break;

   case Radio_SectorClear:
      // is bomb planted and it's a ct
      if (g_bombPlanted)
      {
         int bombPoint = -1;

         // check if it's a ct command
         if (GetTeam (m_radioEntity) == TEAM_COUNTER && GetTeam (GetEntity ()) == TEAM_COUNTER && IsValidBot (m_radioEntity))
         {
            if (g_timeNextBombUpdate < engine->GetTime ())
            {
               float minDistance = FLT_MAX;

               // find nearest bomb waypoint to player
               ITERATE_ARRAY (g_waypoint->m_goalPoints, i)
               {
                  distance = (g_waypoint->GetPath (g_waypoint->m_goalPoints[i])->origin - GetEntityOrigin (m_radioEntity)).GetLengthSquared ();

                  if (distance < minDistance)
                  {
                     minDistance = distance;
                     bombPoint = g_waypoint->m_goalPoints[i];
                  }
               }

               // mark this waypoint as restricted point
               if (bombPoint != -1 && !g_waypoint->IsGoalVisited (bombPoint))
                  g_waypoint->SetGoalVisited (bombPoint);

               g_timeNextBombUpdate = engine->GetTime () + 0.5f;
            }
            // Does this Bot want to defuse?
            if (GetCurrentTask ()->taskID == TASK_NORMAL)
            {
               // Is he approaching this goal?
               if (m_tasks->data == bombPoint)
               {
                  m_tasks->data = -1;
                  RadioMessage (Radio_Affirmative);
               }
            }
         }
      }
      break;

   case Radio_GetInPosition:
      if (distance < 1024.0f || (FNullEnt (m_enemy) && EntityIsVisible (GetEntityOrigin (m_radioEntity))))
      {
         RadioMessage (Radio_Affirmative);

         if (GetCurrentTask ()->taskID == TASK_CAMP)
            m_tasks->time = engine->GetTime () + engine->RandomFloat (30.0f, 60.0f);
         else
         {
            // don't pause anymore
            BotTask taskID = GetCurrentTask ()->taskID;

            if (taskID == TASK_PAUSE)
               m_tasks->time = engine->GetTime ();

            m_targetEntity = null;
            m_seeEnemyTime = engine->GetTime ();

            // If Bot has no enemy
            if (m_lastEnemyOrigin == nullvec)
            {
               int team = GetTeam (GetEntity ());
               float nearestDistance = FLT_MAX;

               // Take nearest enemy to ordering Player
               for (int i = 0; i < engine->GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CFLAG_USED) || !(g_clients[i].flags & CFLAG_ALIVE) || g_clients[i].team == team)
                     continue;

                  edict_t *enemy = g_clients[i].ent;
                  float nowDistance = (GetEntityOrigin (m_radioEntity) - GetEntityOrigin (enemy)).GetLengthSquared ();

                  if (nowDistance < nearestDistance)
                  {
                     nearestDistance = nowDistance;
					 SetLastEnemy(enemy);
                  }
               }
            }
            DeleteSearchNodes ();

            int index = FindDefendWaypoint (GetEntityOrigin (m_radioEntity));

            // push camp task on to stack
            PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime () + engine->RandomFloat (30.0f, 60.0f), true);
            // push move command
            PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine->GetTime () + engine->RandomFloat (30.0f, 60.0f), true);

            if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
               m_campButtons |= IN_DUCK;
            else
               m_campButtons &= ~IN_DUCK;
         }
      }
      break;
   }
   m_radioOrder = 0; // radio command has been handled, reset
}

void Bot::SelectLeaderEachTeam (int team)
{
   Bot *botLeader = null;

   if (g_mapType & MAP_AS)
   {
      if (m_isVIP && !g_leaderChoosen[TEAM_COUNTER])
      {
         // vip bot is the leader
         m_isLeader = true;

         if (engine->RandomInt (1, 100) < 50)
         {
            RadioMessage (Radio_FollowMe);
            m_campButtons = 0;
         }
         g_leaderChoosen[TEAM_COUNTER] = true;
      }
      else if ((team == TEAM_TERRORIST) && !g_leaderChoosen[TEAM_TERRORIST])
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 45)
               botLeader->RadioMessage (Radio_FollowMe);
         }
         g_leaderChoosen[TEAM_TERRORIST] = true;
      }
   }
   else if (g_mapType & MAP_DE)
   {
      if (team == TEAM_TERRORIST && !g_leaderChoosen[TEAM_TERRORIST])
      {
         if (pev->weapons & (1 << WEAPON_C4))
         {
            // bot carrying the bomb is the leader
            m_isLeader = true;

            // terrorist carrying a bomb needs to have some company
            if (engine->RandomInt (1, 100) < 80)
               m_campButtons = 0;

            g_leaderChoosen[TEAM_TERRORIST] = true;
         }
      }
      else if (!g_leaderChoosen[TEAM_COUNTER])
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }
         g_leaderChoosen[TEAM_COUNTER] = true;
      }
   }
   else if (g_mapType & (MAP_ES | MAP_KA | MAP_FY))
   {
      if (team == TEAM_TERRORIST)
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }
      }
      else
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }
      }
   }
   else
   {
      if (team == TEAM_TERRORIST)
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }
      }
      else
      {
         botLeader = g_botManager->GetHighestFragsBot (team);

         if (botLeader != null)
         {
            botLeader->m_isLeader = true;

            if (engine->RandomInt (1, 100) < 40)
               botLeader->RadioMessage (Radio_FollowMe);
         }
      }
   }
}

float Bot::GetWalkSpeed(void)
{
	// SyPB Pro P.48 - Base improve
	if (GetGameMod () == MODE_ZH || GetGameMod () == MODE_ZP || 
		pev->maxspeed <= 180.f || m_currentTravelFlags & PATHFLAG_JUMP || 
		pev->button & IN_JUMP || pev->oldbuttons & IN_JUMP ||
		pev->flags & FL_DUCKING || pev->button & IN_DUCK || pev->oldbuttons & IN_DUCK || IsInWater())
		return pev->maxspeed;

	return static_cast <float> ((static_cast <int> (pev->maxspeed) * 0.5f) + (static_cast <int> (pev->maxspeed) / 50)) - 18;
}

// SyPB Pro P.40 - Is Anti Block
bool Bot::IsAntiBlock(edict_t *entity)
{
	if (entity->v.solid == SOLID_NOT)
		return true;

	return false;
}

bool Bot::IsNotAttackLab(edict_t *entity)
{
	if (FNullEnt(entity))
		return true;

	// SyPB Pro P.48 - Base improve
	if (entity->v.takedamage == DAMAGE_NO)
		return true;

	// SyPB Pro P.29 - New Invisible get
	if (entity->v.rendermode == kRenderTransAlpha)
	{
		float renderamt = entity->v.renderamt;

		if (renderamt <= 30)
			return true;

		if (renderamt > 160)
			return false;

		float enemy_distance = (GetEntityOrigin(entity) - pev->origin).GetLength();
		return (renderamt <= (enemy_distance/5));
	}

	return false;
}


void Bot::ChooseAimDirection (void)
{
   if (!m_canChooseAimDirection)
      return;

   TraceResult tr;
   memset (&tr, 0, sizeof (TraceResult));

   unsigned int flags = m_aimFlags;

   // SyPB Pro P.38 - Small change
   if (m_currentWaypointIndex < 0 || m_currentWaypointIndex >= g_numWaypoints)
	   GetValidWaypoint();

   // check if last enemy vector valid
   if (m_lastEnemyOrigin != nullvec)
   {
	   // SyPB Pro P.45 - Small improve
	   if (FNullEnt(m_enemy) && (pev->origin - m_lastEnemyOrigin).GetLength() >= 1600.0f &&
		   m_seeEnemyTime + 7.0f < engine->GetTime())
	   {
		   TraceLine(EyePosition(), m_lastEnemyOrigin, false, true, GetEntity(), &tr);
		   if (!UsesSniper() || (tr.flFraction <= 0.2f && tr.pHit == g_hostEntity))
		   {
			   if ((m_aimFlags & (AIM_LASTENEMY | AIM_PREDICTENEMY)) && m_wantsToFire)
				   m_wantsToFire = false;

			   m_lastEnemyOrigin = nullvec;
			   m_aimFlags &= ~(AIM_LASTENEMY | AIM_PREDICTENEMY);

			   flags &= ~(AIM_LASTENEMY | AIM_PREDICTENEMY);
		   }
	   }
   }
   else
   {
      m_aimFlags &= ~(AIM_LASTENEMY | AIM_PREDICTENEMY);
      flags &= ~(AIM_LASTENEMY | AIM_PREDICTENEMY);
   }

   // don't allow bot to look at danger positions under certain circumstances
   if (!(flags & (AIM_GRENADE | AIM_ENEMY | AIM_ENTITY)))
   {
      if (IsOnLadder () || IsInWater () || (m_waypointFlags & WAYPOINT_LADDER) || (m_currentTravelFlags & PATHFLAG_JUMP))
         flags &= ~(AIM_LASTENEMY | AIM_PREDICTENEMY);
   }

   if (flags & AIM_OVERRIDE)
      m_lookAt = m_camp;
   else if (flags & AIM_GRENADE)
      m_lookAt = m_throw + Vector (0.0f, 0.0f, 1.0f * m_grenade.z);
   else if (flags & AIM_ENEMY)
      FocusEnemy ();
   else if (flags & AIM_ENTITY)
      m_lookAt = m_entity;
   else if (flags & AIM_LASTENEMY)
   {
      m_lookAt = m_lastEnemyOrigin;

	  // SyPB Pro P.48 - Shootable Thru Obstacle improve
	  if (m_seeEnemyTime + 3.0f - m_actualReactionTime + m_baseAgressionLevel <= engine->GetTime())
	  {
		  m_aimFlags &= ~AIM_LASTENEMY;

		  if ((pev->origin - m_lastEnemyOrigin).GetLength() >= 1600.0f)
			  m_lastEnemyOrigin = nullvec;
	  }
   }
   else if (flags & AIM_PREDICTENEMY)
   {
	   TraceLine(EyePosition(), m_lastEnemyOrigin, false, true, GetEntity(), &tr);
	   if (((pev->origin - m_lastEnemyOrigin).GetLength() < 1600.0f || UsesSniper()) && (tr.flFraction >= 0.2f || tr.pHit != g_worldEdict))
	   {
		   bool recalcPath = true;

		   if (!FNullEnt(m_lastEnemy) && m_trackingEdict == m_lastEnemy && m_timeNextTracking < engine->GetTime())
			   recalcPath = false;

		   if (recalcPath)
		   {
			   m_lookAt = g_waypoint->GetPath(GetAimingWaypoint(m_lastEnemyOrigin))->origin;
			   m_camp = m_lookAt;

			   m_timeNextTracking = engine->GetTime() + 0.5f;
			   m_trackingEdict = m_lastEnemy;

			   // feel free to fire if shoot able
			   if (LastEnemyShootable())
				   m_wantsToFire = true;
		   }
		   else
			   m_lookAt = m_camp;
	   }
	   else // forget an enemy far away
	   {
		   m_aimFlags &= ~AIM_PREDICTENEMY;

		   if ((pev->origin - m_lastEnemyOrigin).GetLength() >= 1600.0f)
			   m_lastEnemyOrigin = nullvec;
	   }
   }
   else if (flags & AIM_CAMP)
      m_lookAt = m_camp;
   // SyPB Pro P.38 - Look At improve
   else if (flags & AIM_NAVPOINT)
   {
	   // SyPB Pro P.45 - LookAt improve
	   m_lookAt = m_destOrigin;
	   if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LADDER || IsOnLadder())
		   m_lookAt = m_destOrigin;
	   else if (!FNullEnt(m_breakableEntity))
		   m_lookAt = m_breakable;
	   else if (!(m_currentTravelFlags & PATHFLAG_JUMP))
	   {
		   if (!FNullEnt(m_moveTargetEntity) && m_currentWaypointIndex == m_prevGoalIndex)
			   m_lookAt = GetEntityOrigin(m_moveTargetEntity);
	   }

	   if (m_lookAt == nullvec)
	   {
		   // SyPB Pro P.40 - Loot At improve
		   if (m_currentWaypointIndex != -1)
			   m_lookAt = g_waypoint->GetPath(m_currentWaypointIndex)->origin;
		   else if (&m_navNode[0] != null)
			   m_lookAt = g_waypoint->GetPath(m_navNode->index)->origin;
	   }
   }

   if (m_lookAt == nullvec)
	   m_lookAt = m_destOrigin;
}

void Bot::Think(void)
{
   pev->button = 0;
   pev->flags |= FL_FAKECLIENT; // restore fake client bit, if it were removed by some evil action =)

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;
   m_moveAngles = nullvec;

   m_canChooseAimDirection = true;
   m_notKilled = IsAlive (GetEntity ());

   m_frameInterval = engine->GetTime () - m_lastThinkTime;
   m_lastThinkTime = engine->GetTime ();

   // SyPB Pro P.38 - Damage Victim Action
   if (m_damageTime < engine->GetTime() && m_damageTime != 0.0f)
	   m_damageTime = 0.0f;

   // is bot movement enabled
   bool botMovement = false;

   if (m_notStarted) // if the bot hasn't selected stuff to start the game yet, go do that...
      StartGame (); // select team & class
   else if (!m_notKilled)
   {
	   if (m_voteMap != 0) // host wants the bots to vote for a map?
	   {
		   FakeClientCommand(GetEntity(), "votemap %d", m_voteMap);
		   m_voteMap = 0;
	   }

      extern ConVar sypb_chat;

      if (sypb_chat.GetBool () && !RepliesToPlayer () && m_lastChatTime + 10.0f < engine->GetTime () && 
		  g_lastChatTime + 5.0f < engine->GetTime ()) // bot chatting turned on?
      {
         // say a text every now and then
         //if (engine->RandomInt (1, 1500) < 2)
		  // SyPB Pro P.43 - Small Fixed
		  if (engine->RandomInt (1, 1500) < 2 && !g_chatFactory[CHAT_DEAD].IsEmpty ())
         {
            m_lastChatTime = engine->GetTime ();
            g_lastChatTime = engine->GetTime ();

            char *pickedPhrase = g_chatFactory[CHAT_DEAD].GetRandomElement ();
            bool sayBufferExists = false;

            // search for last messages, sayed
            ITERATE_ARRAY (m_sayTextBuffer.lastUsedSentences, i)
            {
               if (strncmp (m_sayTextBuffer.lastUsedSentences[i], pickedPhrase, m_sayTextBuffer.lastUsedSentences[i].GetLength ()) == 0)
                  sayBufferExists = true;
            }

            if (!sayBufferExists)
            {
               PrepareChatMessage (pickedPhrase);
               PushMessageQueue (CMENU_SAY);

               // add to ignore list
               m_sayTextBuffer.lastUsedSentences.Push (pickedPhrase);
            }

            // clear the used line buffer every now and then
            if (m_sayTextBuffer.lastUsedSentences.GetElementNumber () > engine->RandomInt (4, 6))
               m_sayTextBuffer.lastUsedSentences.RemoveAll ();
         }
      }
   }
   else if (m_buyingFinished &&
	   !(pev->maxspeed < 10 && GetCurrentTask()->taskID != TASK_PLANTBOMB && 
	   GetCurrentTask()->taskID != TASK_DEFUSEBOMB))
      botMovement = true;

   // SyPB Pro P.42 - Fixed
   int team = GetTeam(GetEntity());

   // remove voice icon
   if ((team != 0 && team != 1) || g_lastRadioTime[team] + engine->RandomFloat (0.8f, 2.1f) < engine->GetTime ())
      SwitchChatterIcon (false); // hide icon

   static float secondThinkTimer = 0.0f;

   // check is it time to execute think (called once per second (not frame))
   if (secondThinkTimer < engine->GetTime ())
   {
      SecondThink ();

      // update timer to one second
      secondThinkTimer = engine->GetTime () + 1.05f;
   }
   CheckMessageQueue (); // check for pending messages

   // SyPB Pro P.30 - Start Think 
   if (!sypb_stopbots.GetBool() && botMovement && m_notKilled)
   {
	   // SyPB Pro P.37 - Game Mode Ai
	   if (GetGameMod() == MODE_BASE || GetGameMod() == MODE_DM || GetGameMod() == MODE_NOTEAM || 
		   GetGameMod() == MODE_ZP || GetGameMod() == MODE_ZH)
		   BotAI();
	   else
		   FunBotAI();
	   
	   MoveAction();
	   
	   DebugModeMsg();
   }
}

void Bot::SecondThink (void)
{
   // this function is called from main think function every second (second not frame).

   if (g_bombPlanted && GetTeam (GetEntity ()) == TEAM_COUNTER && 
	   (pev->origin - g_waypoint->GetBombPosition ()).GetLength () < 700 && 
	   !IsBombDefusing (g_waypoint->GetBombPosition ()))
      ResetTasks ();
}

void Bot::MoveAction(void)
{
	if (m_moveAIAPI) // SyPB Pro P.30 - AMXX API
	{
		m_moveSpeed = 0.0f;
		m_strafeSpeed = 0.0f;
	}

	if (!(pev->button & (IN_FORWARD | IN_BACK)))
	{
		if (m_moveSpeed > 0)
			pev->button |= IN_FORWARD;
		else if (m_moveSpeed < 0)
			pev->button |= IN_BACK;
	}

	if (!(pev->button & (IN_MOVELEFT | IN_MOVERIGHT)))
	{
		if (m_strafeSpeed > 0)
			pev->button |= IN_MOVERIGHT;
		else if (m_strafeSpeed < 0)
			pev->button |= IN_MOVELEFT;
	}
}

void Bot::RunTask (void)
{
   // this is core function that handle task execution

   int team = GetTeam (GetEntity ());
   int destIndex, i;

   Vector src, destination;
   TraceResult tr;

   bool exceptionCaught = false;
   float fullDefuseTime, timeToBlowUp, defuseRemainingTime;

   switch (GetCurrentTask ()->taskID)
   {
   // normal task
   case TASK_NORMAL:
      m_aimFlags |= AIM_NAVPOINT;

	  if ((g_mapType & MAP_CS) && team == TEAM_COUNTER)
	  {
		  // SyPB Pro P.38 - Base Mode Improve
		  int hostageWptIndex = FindHostage();
		  if (hostageWptIndex != -1 && m_currentWaypointIndex != hostageWptIndex)
			  GetCurrentTask()->data = hostageWptIndex;
	  }

      if ((g_mapType & MAP_DE) && team == TEAM_TERRORIST)
      {
         if (!g_bombPlanted)
         {
            m_loosedBombWptIndex = FindLoosedBomb ();

            if (m_loosedBombWptIndex != -1 && m_currentWaypointIndex != m_loosedBombWptIndex && engine->RandomInt (0, 100) < (GetNearbyFriendsNearPosition (g_waypoint->GetPath (m_loosedBombWptIndex)->origin, 650) >= 1 ? 40 : 90))
               GetCurrentTask ()->data = m_loosedBombWptIndex;
         }
         else if (!m_defendedBomb)
         {
            int plantedBombWptIndex = g_waypoint->FindNearest (g_waypoint->GetBombPosition ());

            if (plantedBombWptIndex != -1 && m_currentWaypointIndex != plantedBombWptIndex)
               GetCurrentTask ()->data = plantedBombWptIndex;
         }
      }

      // user forced a waypoint as a goal?
      if (sypb_debuggoal.GetInt () != -1 && sypb_debuggoal.GetInt() >= 0 && sypb_debuggoal.GetInt() < g_numWaypoints)
      {
         // check if we reached it
         if (((g_waypoint->GetPath (m_currentWaypointIndex)->origin - pev->origin).SkipZ ()).GetLengthSquared () < 16 && GetCurrentTask ()->data == sypb_debuggoal.GetInt ())
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_checkTerrain = false;
            m_moveToGoal = false;

            return; // we can safely return here
         }

         if (GetCurrentTask ()->data != sypb_debuggoal.GetInt ())
         {
            DeleteSearchNodes ();
            m_tasks->data = sypb_debuggoal.GetInt ();
         }
      }

	  // SyPB Pro P.42 - AMXX API
	  if (m_waypointGoalAPI != -1 && m_waypointGoalAPI >= 0 && m_waypointGoalAPI < g_numWaypoints)
	  {
		  if (GetCurrentTask()->data != m_waypointGoalAPI)
		  {
			  DeleteSearchNodes();
			  m_tasks->data = m_waypointGoalAPI;
		  }
	  }
      
	  // SyPB Pro P.9
	  // New knife attack skill
	  if (m_currentWeapon == WEAPON_KNIFE && !FNullEnt(m_enemy) && IsAlive(m_enemy) && !HasShield())
	  {
		  // SyPB Pro P.32 - Base Knife Attack
		  if (m_knifeAttackTime < engine->GetTime())
		  {
			  KnifeAttack();
			  m_knifeAttackTime = engine->GetTime() + engine->RandomFloat(2.6f, 3.8f);
		  }
	  }

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (m_reloadState == RSTATE_NONE && GetAmmo () != 0)
         m_reloadState = RSTATE_PRIMARY;

      // if bomb planted and it's a CT calculate new path to bomb point if he's not already heading for
      if (g_bombPlanted && team == TEAM_COUNTER && GetCurrentTask ()->data != -1 && !(g_waypoint->GetPath (m_tasks->data)->flags & WAYPOINT_GOAL) && GetCurrentTask ()->taskID != TASK_ESCAPEFROMBOMB && (g_waypoint->GetPath (m_tasks->data)->origin - g_waypoint->GetBombPosition ()).GetLength () > 128.0f)
      {
         DeleteSearchNodes ();
         m_tasks->data = -1;
      }

      if (!g_bombPlanted && m_currentWaypointIndex != -1 && (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_GOAL) && engine->RandomInt (0, 100) < 80 && GetNearbyEnemiesNearPosition (pev->origin, 650) == 0)
         RadioMessage (Radio_SectorClear);

      // reached the destination (goal) waypoint?
      if (DoWaypointNav ())
      {
         TaskComplete ();
         m_prevGoalIndex = -1;

         // spray logo sometimes if allowed to do so
         if (m_timeLogoSpray < engine->GetTime () && sypb_spraypaints.GetBool () && engine->RandomInt (1, 100) < 80 && m_moveSpeed > GetWalkSpeed ())
            PushTask (TASK_SPRAYLOGO, TASKPRI_SPRAYLOGO, -1, engine->GetTime () + 1.0f, false);

         // reached waypoint is a camp waypoint
         if ((g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CAMP) && GetGameMod () == MODE_BASE )
         {
            // check if bot has got a primary weapon and hasn't camped before
            if (HasPrimaryWeapon () && m_timeCamping + 10.0f < engine->GetTime () && !HasHostage ())
            {
               bool campingAllowed = true;

			   // Check if it's not allowed for this team to camp here
			   if (team == TEAM_TERRORIST)
			   {
				   if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_COUNTER)
					   campingAllowed = false;
			   }
			   else
			   {
				   if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_TERRORIST)
					   campingAllowed = false;
			   }

			   // don't allow vip on as_ maps to camp + don't allow terrorist carrying c4 to camp
			   if (((g_mapType & MAP_AS) && *(INFOKEY_VALUE(GET_INFOKEYBUFFER(GetEntity()), "model")) == 'v') || ((g_mapType & MAP_DE) && GetTeam(GetEntity()) == TEAM_TERRORIST && !g_bombPlanted && (pev->weapons & (1 << WEAPON_C4))))
				   campingAllowed = false;

               // check if another bot is already camping here
               if (IsWaypointUsed (m_currentWaypointIndex))
                  campingAllowed = false;

               if (campingAllowed)
               {
                  // crouched camping here?
                  if (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CROUCH)
                     m_campButtons = IN_DUCK;
                  else
                     m_campButtons = 0;

                  SelectBestWeapon ();

                  if (!(m_states & (STATE_SEEINGENEMY | STATE_HEARENEMY)) && !m_reloadState)
                     m_reloadState = RSTATE_PRIMARY;

                  MakeVectors (pev->v_angle);

                  m_timeCamping = engine->GetTime () + engine->RandomFloat (g_skillTab[m_skill / 20].campStartDelay, g_skillTab[m_skill / 20].campEndDelay);
                  PushTask (TASK_CAMP, TASKPRI_CAMP, -1, m_timeCamping, true);

                  src.x = g_waypoint->GetPath (m_currentWaypointIndex)->campStartX;
                  src.y = g_waypoint->GetPath (m_currentWaypointIndex)->campStartY;
                  src.z = 0;

                  m_camp = src;
                  m_aimFlags |= AIM_CAMP;
                  m_campDirection = 0;

                  // tell the world we're camping
                  if (engine->RandomInt (0, 100) < 95)
                     RadioMessage (Radio_InPosition);

                  m_moveToGoal = false;
                  m_checkTerrain = false;

                  m_moveSpeed = 0.0f;
                  m_strafeSpeed = 0.0f;
               }
            }
         }
		 // SyPB Pro P.30 - Zombie Mode Human Camp
		 else if (GetGameMod() == MODE_ZP && !IsZombieEntity(GetEntity()))
			 ZmCampPointAction(1);
         else if (GetGameMod () == MODE_BASE)
         {
            // some goal waypoints are map dependant so check it out...
            if (g_mapType & MAP_CS)
			{
				// SyPB Pro P.28 - CS Ai
				if (team == TEAM_TERRORIST)
				{
					if (m_skill >= 80 || engine->RandomInt(0, 100) < m_skill)
					{
						int index = FindDefendWaypoint(g_waypoint->GetPath(m_currentWaypointIndex)->origin);

						PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + engine->RandomFloat(60.0, 120.0f), true); // push camp task on to stack
						PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine->GetTime() + engine->RandomFloat(10.0, 30.0f), true); // push move command

						if (g_waypoint->GetPath(index)->vis.crouch <= g_waypoint->GetPath(index)->vis.stand)
							m_campButtons |= IN_DUCK;
						else
							m_campButtons &= ~IN_DUCK;
					}
				}
				else if (team == TEAM_COUNTER)
				{
					if (HasHostage())
					{
						// and reached a Rescue Point?
						if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_RESCUE)
						{
							for (i = 0; i < Const_MaxHostages; i++)
								//m_hostages[i] = null; // clear array of hostage pointers
							{
								// SyPB Pro P.38 - Base improve 
								if (FNullEnt(m_hostages[i]))
									continue;

								if (g_waypoint->GetPath(g_waypoint->FindNearest(GetEntityOrigin(m_hostages[i])))->flags & WAYPOINT_RESCUE)
									m_hostages[i] = null;
							}
						}
					} 			
				}
            }
			
            if ((g_mapType & MAP_DE) && ((g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_GOAL) || m_inBombZone) && FNullEnt (m_enemy))
            {
               // is it a terrorist carrying the bomb?
               if (pev->weapons & (1 << WEAPON_C4))
               {
                  if ((m_states & STATE_SEEINGENEMY) && GetNearbyFriendsNearPosition (pev->origin, 768) == 0)
                  {
                     // request an help also
                     RadioMessage (Radio_NeedBackup);

                     PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime () + engine->RandomFloat (4.0f, 8.0f), true);
                  }
                  else
                     PushTask (TASK_PLANTBOMB, TASKPRI_PLANTBOMB, -1, 0.0, false);
               }
               else if (team == TEAM_COUNTER && m_timeCamping + 10.0f < engine->GetTime ())
               {
                  if (!g_bombPlanted && GetNearbyFriendsNearPosition (pev->origin, 250) < 4 && engine->RandomInt (0, 100) < 60)
                  {
                     m_timeCamping = engine->GetTime () + engine->RandomFloat (g_skillTab[m_skill / 20].campStartDelay, g_skillTab[m_skill / 20].campEndDelay);

                     int index = FindDefendWaypoint (g_waypoint->GetPath (m_currentWaypointIndex)->origin);

                     PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime () + engine->RandomFloat (35.0f, 60.0f), true); // push camp task on to stack
                     PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine->GetTime () + engine->RandomFloat (10.0f, 15.0f), true); // push move command

                     if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
                        m_campButtons |= IN_DUCK;
                     else
                        m_campButtons &= ~IN_DUCK;
                  }
               }
            }
         }
      }
      else if (!GoalIsValid ()) // no more nodes to follow - search new ones (or we have a momb)
      {
         m_moveSpeed = pev->maxspeed;

         DeleteSearchNodes ();

         // did we already decide about a goal before?
         if (GetCurrentTask ()->data != -1 && !(pev->weapons & (1 << WEAPON_C4)))
            destIndex = m_tasks->data;
         else
            destIndex = FindGoal ();

         m_prevGoalIndex = destIndex;

         // remember index
         m_tasks->data = destIndex;

         // do pathfinding if it's not the current waypoint
         if (destIndex != m_currentWaypointIndex)
         {
            if ((g_bombPlanted && team == TEAM_COUNTER) || sypb_debuggoal.GetInt () != -1)
               FindShortestPath (m_currentWaypointIndex, destIndex);
            else
               FindPath (m_currentWaypointIndex, destIndex, m_pathType);
         }
      }
      else
      {
		  if (!(pev->flags & FL_DUCKING) && m_minSpeed != pev->maxspeed)
			  m_moveSpeed = m_minSpeed;

		  // SyPB Pro P.30 - Zombie Mode Human Camp
		  if (GetGameMod() == MODE_ZP && !IsZombieEntity(GetEntity()) && 
			  // SyPB Pro P.48 - Zombie Mode Human Camp Fixed
			 !g_waypoint->m_zmHmPoints.IsEmpty())
		  {
			  ZmCampPointAction();

			  // SyPB Pro P.47 - Zombie Mode Human Camp improve
			  if (GetCurrentTask()->data != -1 && !(g_waypoint->IsZBCampPoint(GetCurrentTask()->data)))
			  {
				  m_prevGoalIndex = -1;
				  GetCurrentTask()->data = -1;
			  }
		  }
      }

      if (engine->IsFootstepsOn () && m_skill > 80 && !(m_aimFlags & AIM_ENEMY) && 
		  (m_heardSoundTime + 13.0f >= engine->GetTime () || (m_states & (STATE_HEARENEMY))) && 
		  GetNearbyEnemiesNearPosition (pev->origin, 1024) >= 1 && !(m_currentTravelFlags & PATHFLAG_JUMP) && 
		  !(pev->button & IN_DUCK) && !(pev->flags & FL_DUCKING) && !sypb_knifemode.GetBool () && 
		  !g_bombPlanted && !IsZombieEntity (GetEntity ()))
         m_moveSpeed = GetWalkSpeed ();

      // bot hasn't seen anything in a long time and is asking his teammates to report in
      if (m_seeEnemyTime != 0.0f && m_seeEnemyTime + engine->RandomFloat (30.0f, 80.0f) < engine->GetTime () && engine->RandomInt (0, 100) < 70 && g_timeRoundStart + 20.0f < engine->GetTime () && m_askCheckTime + engine->RandomFloat (20.0, 30.0f) < engine->GetTime ())
      {
         m_askCheckTime = engine->GetTime ();
         RadioMessage (Radio_ReportTeam);
      }

      break;

   // bot sprays messy logos all over the place...
   case TASK_SPRAYLOGO:
      m_aimFlags |= AIM_ENTITY;

      // bot didn't spray this round?
      if (m_timeLogoSpray <= engine->GetTime () && m_tasks->time > engine->GetTime ())
      {
         MakeVectors (pev->v_angle);
         Vector sprayOrigin = EyePosition () + (g_pGlobals->v_forward * 128);

         TraceLine (EyePosition (), sprayOrigin, true, GetEntity (), &tr);

         // no wall in front?
         if (tr.flFraction >= 1.0f)
            sprayOrigin.z -= 128.0f;

         m_entity = sprayOrigin;

         if (m_tasks->time - 0.5 < engine->GetTime ())
         {
            // emit spraycan sound
            EMIT_SOUND_DYN2 (GetEntity (), CHAN_VOICE, "player/sprayer.wav", 1.0, ATTN_NORM, 0, 100);
            TraceLine (EyePosition (), EyePosition () + g_pGlobals->v_forward * 128.0f, true, GetEntity (), &tr);

            // paint the actual logo decal
            DecalTrace (pev, &tr, m_logotypeIndex);
            m_timeLogoSpray = engine->GetTime () + engine->RandomFloat (30.0f, 45.0f);
         }
      }
      else
         TaskComplete ();

      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = engine->GetTime ();
      m_moveSpeed = 0;
      m_strafeSpeed = 0.0f;

      break;

   // hunt down enemy
   case TASK_HUNTENEMY:
      m_aimFlags |= AIM_NAVPOINT;
      m_checkTerrain = true;

      // if we've got new enemy...
      if (!FNullEnt (m_enemy) || FNullEnt (m_lastEnemy))
      {
         // forget about it...
         TaskComplete ();
         m_prevGoalIndex = -1;

		 SetLastEnemy(null);
      }
      else if (GetTeam (m_lastEnemy) == team)
      {
         // don't hunt down our teammate...
         RemoveCertainTask (TASK_HUNTENEMY);
         m_prevGoalIndex = -1;
      }
      else if (DoWaypointNav ()) // reached last enemy pos?
      {
         // forget about it...
         TaskComplete ();
         m_prevGoalIndex = -1;

		 SetLastEnemy(null);
      }
      else if (!GoalIsValid ()) // do we need to calculate a new path?
      {
         DeleteSearchNodes ();

         // is there a remembered index?
		 if (GetCurrentTask()->data != -1 && GetCurrentTask()->data < g_numWaypoints)
			 destIndex = m_tasks->data;
		 else // no. we need to find a new one
			//destIndex = g_waypoint->FindNearest (m_lastEnemyOrigin);
			 destIndex = GetEntityWaypoint(m_lastEnemy);

         // remember index
         m_prevGoalIndex = destIndex;
         m_tasks->data = destIndex;

         if (destIndex != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, destIndex, m_pathType);
      }

      // bots skill higher than 60?
      if (m_skill > 60 && engine->IsFootstepsOn ())
      {
         // then make him move slow if near enemy
         if (!(m_currentTravelFlags & PATHFLAG_JUMP))
         {
            if (m_currentWaypointIndex != -1)
            {
               if (g_waypoint->GetPath (m_currentWaypointIndex)->radius < 32 && !IsOnLadder () && !IsInWater () && m_seeEnemyTime + 4.0f > engine->GetTime () && m_skill < 80)
                  pev->button |= IN_DUCK;
            }

            if ((m_lastEnemyOrigin - pev->origin).GetLength () < 512.0f && !(pev->flags & FL_DUCKING))
               m_moveSpeed = GetWalkSpeed ();
         }
      }
      break;

   // bot seeks cover from enemy
   case TASK_SEEKCOVER:
      m_aimFlags |= AIM_NAVPOINT;

      if (FNullEnt (m_lastEnemy) || !IsAlive (m_lastEnemy))
      {
         TaskComplete ();
         m_prevGoalIndex = -1;
      }
      else if (DoWaypointNav ()) // reached final cover waypoint?
      {
         // yep. activate hide behaviour
         TaskComplete ();

         m_prevGoalIndex = -1;
         m_pathType = 1;

         // start hide task
         PushTask (TASK_HIDE, TASKPRI_HIDE, -1, engine->GetTime () + engine->RandomFloat (5.0f, 15.0f), false);
         destination = m_lastEnemyOrigin;

         // get a valid look direction
         GetCampDirection (&destination);

         m_aimFlags |= AIM_CAMP;
         m_camp = destination;
         m_campDirection = 0;

         // chosen waypoint is a camp waypoint?
         if (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CAMP)
         {
            // use the existing camp wpt prefs
            if (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CROUCH)
               m_campButtons = IN_DUCK;
            else
               m_campButtons = 0;
         }
         else
         {
            // choose a crouch or stand pos
            if (g_waypoint->GetPath (m_currentWaypointIndex)->vis.crouch <= g_waypoint->GetPath (m_currentWaypointIndex)->vis.stand)
               m_campButtons = IN_DUCK;
            else
               m_campButtons = 0;

            // enter look direction from previously calculated positions
            g_waypoint->GetPath (m_currentWaypointIndex)->campStartX = destination.x;
            g_waypoint->GetPath (m_currentWaypointIndex)->campStartY = destination.y;

            g_waypoint->GetPath (m_currentWaypointIndex)->campStartX = destination.x;
            g_waypoint->GetPath (m_currentWaypointIndex)->campEndY = destination.y;
         }

         if ((m_reloadState == RSTATE_NONE) && (GetAmmoInClip () < 8) && (GetAmmo () != 0))
            m_reloadState = RSTATE_PRIMARY;

         m_moveSpeed = 0;
         m_strafeSpeed = 0;

         m_moveToGoal = false;
         m_checkTerrain = true;
      }
      else if (!GoalIsValid ()) // we didn't choose a cover waypoint yet or lost it due to an attack?
      {
         DeleteSearchNodes ();

		 // SyPB Pro P.38 - Zombie Mode Camp improve
		 if (GetGameMod() == MODE_ZP && !IsZombieEntity(GetEntity()) && !g_waypoint->m_zmHmPoints.IsEmpty())
			 destIndex = FindGoal();
		 else if (GetCurrentTask()->data != -1)
			 destIndex = m_tasks->data;
		 else
			 destIndex = FindCoverWaypoint(1024.0f);

		 if (destIndex < 0 || destIndex >= g_numWaypoints)
			 destIndex = g_waypoint->FindFarest(pev->origin, 500.0f);

         m_campDirection = 0;
         m_prevGoalIndex = destIndex;
         m_tasks->data = destIndex;

         if (destIndex != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, destIndex, 2);
      }

      break;

   // plain attacking
   case TASK_FIGHTENEMY:
      m_moveToGoal = true;
      m_checkTerrain = true;

      if (!FNullEnt (m_enemy))
         CombatFight ();
      else
      {
         TaskComplete ();
         //FindWaypoint ();

		 // SyPB Pro P.43 - Waypoint improve
		 m_prevGoalIndex = -1;
		 m_tasks->data = -1;

		 SetEntityWaypoint(GetEntity());
		 GetValidWaypoint();
		 // --------------

         m_destOrigin = m_lastEnemyOrigin;
      }

      m_navTimeset = engine->GetTime ();
      break;

   // Bot is pausing
   case TASK_PAUSE:
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = engine->GetTime ();
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_aimFlags |= AIM_NAVPOINT;

      // is bot blinded and above average skill?
      if (m_viewDistance < 500.0f && m_skill > 60)
      {
         // go mad!
         m_moveSpeed = -fabsf ((m_viewDistance - 500.0f) / 2.0f);

         if (m_moveSpeed < -pev->maxspeed)
            m_moveSpeed = -pev->maxspeed;

         MakeVectors (pev->v_angle);
         m_camp = EyePosition () + (g_pGlobals->v_forward * 500);

         m_aimFlags |= AIM_OVERRIDE;
         m_wantsToFire = true;
      }
      else
         pev->button |= m_campButtons;

      // stop camping if time over or gets hurt by something else than bullets
      if (m_tasks->time < engine->GetTime () || m_lastDamageType > 0)
         TaskComplete ();
      break;

   // blinded (flashbanged) behaviour
   case TASK_BLINDED:
      m_moveToGoal = false;
      m_checkTerrain = false;
      m_navTimeset = engine->GetTime ();

      // if bot remembers last enemy position
      if (m_skill > 70 && m_lastEnemyOrigin != nullvec && IsValidPlayer (m_lastEnemy) && !UsesSniper ())
      {
         m_lookAt = m_lastEnemyOrigin; // face last enemy
         m_wantsToFire = true; // and shoot it
      }

	  // SyPB Pro P.48 - Blind Action improve
	  if (m_blindCampPoint == -1)
	  {
		  m_moveSpeed = m_blindMoveSpeed;
		  m_strafeSpeed = m_blindSidemoveSpeed;
		  pev->button |= m_blindButton;
	  }
	  else
	  {
		  m_moveToGoal = true;

		  DeleteSearchNodes();

		  PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + engine->RandomFloat(15.0f, 35.0f), true);
		  PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, m_blindCampPoint, engine->GetTime() + engine->RandomFloat(10.0f, 20.0f), true);

		  if (g_waypoint->GetPath(m_blindCampPoint)->vis.crouch <= g_waypoint->GetPath(m_blindCampPoint)->vis.stand)
			  m_campButtons |= IN_DUCK;
		  else
			  m_campButtons &= ~IN_DUCK;

		  m_blindCampPoint = -1;
	  }

      if (m_blindTime < engine->GetTime ())
         TaskComplete ();

      break;

   // camping behaviour
   case TASK_CAMP:
	   // SyPB Pro P.42 - Base Change 
	   if (IsZombieEntity(GetEntity()) || (GetGameMod() == MODE_DM && pev->health > pev->max_health / 2))
	   {
		   TaskComplete();
		   break;
	   }
   	   
      m_aimFlags |= AIM_CAMP;
      m_checkTerrain = false;
      m_moveToGoal = false;

      // if bomb is planted and no one is defusing it kill the task
      if (g_bombPlanted && m_defendedBomb && !IsBombDefusing (g_waypoint->GetBombPosition ()) && !OutOfBombTimer () && team == TEAM_COUNTER)
      {
         m_defendedBomb = false;
         TaskComplete ();
      }

	  // SyPB Pro P.47 - Zombie Mode Camp Fixed 
	  if (m_zhCampPointIndex != -1)
	  {
		  if ((g_waypoint->GetPath(m_zhCampPointIndex)->origin - pev->origin).GetLength2D() > 250.0f || 
			  (g_waypoint->GetPath(m_zhCampPointIndex)->origin.z - 18.0f > pev->origin.z))
		  {
			  m_zhCampPointIndex = -1;
			  TaskComplete();
			  GetValidWaypoint();
			  break;
		  }
	  } 

      // half the reaction time if camping because you're more aware of enemies if camping
      m_idealReactionTime = (engine->RandomFloat (g_skillTab[m_skill / 20].minSurpriseTime, g_skillTab[m_skill / 20].maxSurpriseTime)) / 2;
      m_navTimeset = engine->GetTime ();

      m_moveSpeed = 0;
      m_strafeSpeed = 0.0f;

      GetValidWaypoint ();

      if (m_nextCampDirTime < engine->GetTime ())
      {
         m_nextCampDirTime = engine->GetTime () + engine->RandomFloat (2.0f, 5.0f);

         if (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CAMP)
         {
            destination.z = 0;

            // switch from 1 direction to the other
            if (m_campDirection < 1)
            {
               destination.x = g_waypoint->GetPath (m_currentWaypointIndex)->campStartX;
               destination.y = g_waypoint->GetPath (m_currentWaypointIndex)->campStartY;
               m_campDirection ^= 1;
            }
            else
            {
               destination.x = g_waypoint->GetPath (m_currentWaypointIndex)->campEndX;
               destination.y = g_waypoint->GetPath (m_currentWaypointIndex)->campEndY;
               m_campDirection ^= 1;
            }

            // find a visible waypoint to this direction...
            // i know this is ugly hack, but i just don't want to break compatiability :)
            int numFoundPoints = 0;
            int foundPoints[3];
            int distanceTab[3];

            Vector dotA = (destination - pev->origin).Normalize2D ();

            for (i = 0; i < g_numWaypoints; i++)
            {
               // skip invisible waypoints or current waypoint
               if (i == m_currentWaypointIndex || g_waypoint->IsVisible(m_currentWaypointIndex, i))
                  continue;

               Vector dotB = (g_waypoint->GetPath (i)->origin - pev->origin).Normalize2D ();

               if ((dotA | dotB) > 0.9)
               {
                  int distance = static_cast <int> ((pev->origin - g_waypoint->GetPath (i)->origin).GetLength ());

                  if (numFoundPoints >= 3)
                  {
                     for (int j = 0; j < 3; j++)
                     {
                        if (distance > distanceTab[j])
                        {
                           distanceTab[j] = distance;
                           foundPoints[j] = i;

                           break;
                        }
                     }
                  }
                  else
                  {
                     foundPoints[numFoundPoints] = i;
                     distanceTab[numFoundPoints] = distance;

                     numFoundPoints++;
                  }
               }
            }

            if (--numFoundPoints >= 0)
               m_camp = g_waypoint->GetPath (foundPoints[engine->RandomInt (0, numFoundPoints)])->origin;
            else
               m_camp = g_waypoint->GetPath (GetCampAimingWaypoint())->origin;
         }
         else
            m_camp = g_waypoint->GetPath (GetCampAimingWaypoint())->origin;
      }
      // press remembered crouch button
      pev->button |= m_campButtons;

      // stop camping if time over or gets hurt by something else than bullets
      if (m_tasks->time < engine->GetTime () || m_lastDamageType > 0)
         TaskComplete ();

      break;

   // hiding behaviour
   case TASK_HIDE:
	   // SyPB Pro P.37 - small change
	   if (IsZombieEntity(GetEntity()) || (GetGameMod () == MODE_DM && pev->health > pev->max_health/2))
	   {
		   TaskComplete();
		   break;
	   }

      m_aimFlags |= AIM_CAMP;
      m_checkTerrain = false;
      m_moveToGoal = false;

      // half the reaction time if camping
      m_idealReactionTime = (engine->RandomFloat (g_skillTab[m_skill / 20].minSurpriseTime, g_skillTab[m_skill / 20].maxSurpriseTime)) / 2;

      m_navTimeset = engine->GetTime ();
      m_moveSpeed = 0;
      m_strafeSpeed = 0.0f;

      GetValidWaypoint ();

      if (HasShield () && !m_isReloading)
      {
         if (!IsShieldDrawn ())
            pev->button |= IN_ATTACK2; // draw the shield!
         else
            pev->button |= IN_DUCK; // duck under if the shield is already drawn
      }

      // if we see an enemy and aren't at a good camping point leave the spot
      if ((m_states & STATE_SEEINGENEMY) || m_inBombZone)
      {
         if (!(g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CAMP))
         {
            TaskComplete ();

            m_campButtons = 0;
            m_prevGoalIndex = -1;

            if (!FNullEnt (m_enemy))
               CombatFight ();

            break;
         }
      }
      else if (m_lastEnemyOrigin == nullvec) // If we don't have an enemy we're also free to leave
      {
         TaskComplete ();

         m_campButtons = 0;
         m_prevGoalIndex = -1;

         if (GetCurrentTask ()->taskID == TASK_HIDE)
            TaskComplete ();

         break;
      }

      pev->button |= m_campButtons;
      m_navTimeset = engine->GetTime ();

	  // SyPB Pro P.37 - Base Mode improve
	  if (m_lastDamageType > 0 || m_tasks->time < engine->GetTime())
	  {
		  if (m_isReloading && (!FNullEnt(m_enemy) || !FNullEnt(m_lastEnemy)) && m_skill > 70)
			  m_tasks->time += 2.0f;
		  else if (GetGameMod () == MODE_DM && pev->health <= 20.0f && m_skill > 70)
			  m_tasks->time += 5.0f;
		  else
			  TaskComplete();
	  }

      break;

   // moves to a position specified in position has a higher priority than task_normal
   case TASK_MOVETOPOSITION:
      m_aimFlags |= AIM_NAVPOINT;

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (m_reloadState == RSTATE_NONE && GetAmmo () != 0)
         m_reloadState = RSTATE_PRIMARY;

      if (DoWaypointNav ()) // reached destination?
      {
         TaskComplete (); // we're done

         m_prevGoalIndex = -1;
         m_position = nullvec;
      }
      else if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         if (GetCurrentTask ()->data != -1 && GetCurrentTask ()->data < g_numWaypoints)
            destIndex = m_tasks->data;
         else
            destIndex = g_waypoint->FindNearest (m_position);

         if (destIndex >= 0 && destIndex < g_numWaypoints)
         {
            m_prevGoalIndex = destIndex;
            m_tasks->data = destIndex;

            FindPath (m_currentWaypointIndex, destIndex, m_pathType);
         }
         else
            TaskComplete ();
      }
      break;

   // planting the bomb right now
   case TASK_PLANTBOMB:
      m_aimFlags |= AIM_CAMP;

      if (pev->weapons & (1 << WEAPON_C4)) // we're still got the C4?
      {
         SelectWeaponByName ("weapon_c4");
		 RadioMessage (Radio_CoverMe);  // SyPB Pro P.10

         if (IsAlive (m_enemy) || !m_inBombZone)
            TaskComplete ();
         else
         {
            m_moveToGoal = false;
            m_checkTerrain = false;
            m_navTimeset = engine->GetTime ();

            if (g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CROUCH)
               pev->button |= (IN_ATTACK | IN_DUCK);
            else
               pev->button |= IN_ATTACK;

            m_moveSpeed = 0;
            m_strafeSpeed = 0;
         }
      }
      else // done with planting
      {
         TaskComplete ();

         // tell teammates to move over here...
         if (GetNearbyFriendsNearPosition (pev->origin, 1200) != 0)
            RadioMessage (Radio_NeedBackup);

         DeleteSearchNodes ();

         int index = FindDefendWaypoint (pev->origin);
         float halfTimer = engine->GetTime () + ((engine->GetC4TimerTime () / 2) + (engine->GetC4TimerTime () / 4));

         // push camp task on to stack
         PushTask (TASK_CAMP, TASKPRI_CAMP, -1, halfTimer, true);
         // push move command
         PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, halfTimer, true);

         if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
            m_campButtons |= IN_DUCK;
         else
            m_campButtons &= ~IN_DUCK;
      }
      break;

   // bomb defusing behaviour
   case TASK_DEFUSEBOMB:
   	   timeToBlowUp = GetBombTimeleft ();
   	   fullDefuseTime = m_hasDefuser ? 7.0f : 12.0f;
   	   defuseRemainingTime = fullDefuseTime;
   	   
   	   if (m_hasProgressBar && IsOnFloor ())
   	   	   defuseRemainingTime = fullDefuseTime - engine->GetTime();
   	   
   	   if (g_waypoint->GetBombPosition () == nullvec)
   	   {
   	   	   exceptionCaught = true;
   	   	   g_bombPlanted = false;
   	   	   
   	   	   RadioMessage (Radio_SectorClear);
   	   }
   	   else if (!FNullEnt (m_enemy) && !IsZombieEntity (m_enemy))
   	   {
   	   	   int friends = GetNearbyFriendsNearPosition (pev->origin, 768);
   	   	   if (friends < 2 && defuseRemainingTime < timeToBlowUp)
   	   	   {
   	   	   	   exceptionCaught = true;
   	   	   	   
   	   	   	   if ((defuseRemainingTime+2.0f) > timeToBlowUp)
   	   	   	   	   exceptionCaught = false;
   	   	   	   
   	   	   	   if (GetNearbyFriendsNearPosition (pev->origin, 9999) > friends)
   	   	   	   	   RadioMessage (Radio_NeedBackup);
   	   	   }
   	   }
   	   else if (defuseRemainingTime > timeToBlowUp)
   	   	   exceptionCaught = true;
   	   
   	   if (exceptionCaught)
   	   {
   	   	   m_checkTerrain = true;
   	   	   m_moveToGoal = true;
   	   	   
   	   	   m_destOrigin = nullvec;
   	   	   m_entity = nullvec;
   	   	   
   	   	   if (GetCurrentTask ()->taskID == TASK_DEFUSEBOMB)
   	   	   {
   	   	   	   m_aimFlags &= ~AIM_ENTITY;
   	   	   	   TaskComplete ();
   	   	   }
   	   	   break;
   	   }
   	   
   	   m_aimFlags |= AIM_ENTITY;
   	   
   	   m_destOrigin = g_waypoint->GetBombPosition ();
   	   m_entity = g_waypoint->GetBombPosition ();

	   // SyPB Pro P.15
	   if (m_entity != nullvec)
	   {
		   // SyPB Pro P.29 - Small change 
		   if (GetNearbyEnemiesNearPosition(pev->origin, 9999) == 0)
			   SelectWeaponByName("weapon_knife");

		   if (((m_entity - pev->origin).GetLength2D()) < 60.0f)
		   {
			   m_moveToGoal = false;
			   m_checkTerrain = false;
			   m_strafeSpeed = 0.0f;
			   m_moveSpeed = 0.0f;

			   if (m_isReloading)
			   {
				   int friendsN = GetNearbyFriendsNearPosition(pev->origin, 768);
				   if (friendsN > 2 && GetNearbyEnemiesNearPosition(pev->origin, 768) < friendsN)
				   {
					   SelectWeaponByName("weapon_knife");
					   m_isReloading = false;
					   m_reloadState = RSTATE_NONE;
				   }
			   }

			   //pev->button |= IN_USE;
			   MDLL_Use(m_pickupItem, GetEntity());
			   RadioMessage(Radio_CoverMe);
		   }
	   }

	   break;

   // follow user behaviour
   case TASK_FOLLOWUSER:
      if (FNullEnt (m_targetEntity) || !IsAlive (m_targetEntity))
      {
         m_targetEntity = null;
         TaskComplete ();

         break;
      }

      if (m_targetEntity->v.button & IN_ATTACK)
      {
         MakeVectors (m_targetEntity->v.v_angle);
         TraceLine (GetEntityOrigin (m_targetEntity) + m_targetEntity->v.view_ofs, g_pGlobals->v_forward * 500, true, true, GetEntity (), &tr);

		 m_followWaitTime = 0.0f;  // SyPB Pro P.25 - follow Ai

         if (!FNullEnt (tr.pHit) && IsValidPlayer (tr.pHit) && GetTeam (tr.pHit) != team)
         {
            m_targetEntity = null;
			SetLastEnemy(tr.pHit);

            TaskComplete ();
            break;
         }
      }

	  // SyPB Pro P.45 - Move Speed improve
	  if (m_targetEntity->v.maxspeed != 0.0f)
	  {
		  if (m_targetEntity->v.maxspeed < pev->maxspeed)
			  m_moveSpeed = GetWalkSpeed();
		  else
			  m_moveSpeed = pev->maxspeed;
	  }

      if (m_reloadState == RSTATE_NONE && GetAmmo () != 0)
         m_reloadState = RSTATE_PRIMARY;

	  // SyPB Pro P.25 - follow Ai
	  if (GetGameMod () == MODE_ZP || GetGameMod () == MODE_ZH || 
		  ((GetEntityOrigin (m_targetEntity) - pev->origin).GetLength () > 80))
		  m_followWaitTime = 0.0f;
	  else
	  {
		  if (m_followWaitTime == 0.0f)
			  m_followWaitTime = engine->GetTime ();
		  else 
		  {
			  if (m_followWaitTime + 5.0f < engine->GetTime ())
			  {
				  m_targetEntity = null;

				  RadioMessage (Radio_YouTakePoint);
				  TaskComplete ();

				  break;
			  }
		  }
	  }

      m_aimFlags |= AIM_NAVPOINT;

      if (m_targetEntity->v.maxspeed < m_moveSpeed)
         m_moveSpeed = GetWalkSpeed ();

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (DoWaypointNav ()) // reached destination?
         GetCurrentTask ()->data = -1;

      if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

		 destIndex = GetEntityWaypoint(m_targetEntity);

         Array <int> points;
         g_waypoint->FindInRadius (points, 200, GetEntityOrigin (m_targetEntity));

         while (!points.IsEmpty ())
         {
            int newIndex = points.Pop ();

            // if waypoint not yet used, assign it as dest
            if (!IsWaypointUsed (newIndex) && (newIndex != m_currentWaypointIndex))
               destIndex = newIndex;
         }

         if (destIndex >= 0 && destIndex < g_numWaypoints && destIndex != m_currentWaypointIndex)
         {
            m_prevGoalIndex = destIndex;
            m_tasks->data = destIndex;

            // always take the shortest path
            FindShortestPath (m_currentWaypointIndex, destIndex);
         }
         else
         {
            m_targetEntity = null;
            TaskComplete ();
         }
      }
      break;

	  // SyPB Pro P.24 - Move Target
   case TASK_MOVETOTARGET:
	   m_moveTargetOrigin = GetEntityOrigin(m_moveTargetEntity);

	   // SyPB Pro P.42 - Move Target Fixed 
	   if (FNullEnt(m_moveTargetEntity) || m_moveTargetOrigin == nullvec)
	   {
		   SetMoveTarget(null);
		   break;
	   }

	   m_aimFlags |= AIM_NAVPOINT;

	   SetLastEnemy(null);
	   m_destOrigin = m_moveTargetOrigin;
	   m_moveSpeed = pev->maxspeed;

	   //SyPB Pro P.40 - Move Target improve
	   if (DoWaypointNav())
		   DeleteSearchNodes();

	   destIndex = GetEntityWaypoint(m_moveTargetEntity);
	   
	   if (destIndex >= 0 && destIndex < g_numWaypoints)
	   {
		   // SyPB Pro P.42 - Move Target Fixed 
		   bool needMoveToTarget = false;
		   if (!GoalIsValid())
			   needMoveToTarget = true;
		   else if (GetCurrentTask()->data != destIndex)
		   {
			   needMoveToTarget = true;
			   if (&m_navNode[0] != null)
			   {
				   PathNode *node = m_navNode;

				   while (node->next != null)
					   node = node->next;

				   if (node->index == destIndex)
					   needMoveToTarget = false;
			   }
		   }

		   if (needMoveToTarget)
		   {
			   DeleteSearchNodes();

			   m_prevGoalIndex = destIndex;
			   m_tasks->data = destIndex;

			   //FindShortestPath(m_currentWaypointIndex, destIndex);

			   // SyPB Pro P.42 - Move Target improve
			   int moveMode = 0;
			   if (pev->health < pev->max_health / 3)
				   moveMode = 8;
			   else
			   {
				   for (i = 0; i < engine->GetMaxClients(); i++)
				   {
					   Bot *otherBot = g_botManager->GetBot(i);

					   if (otherBot != null && otherBot->pev != pev &&
						   team == GetTeam(otherBot->GetEntity()) && destIndex == otherBot->m_prevGoalIndex)
						   moveMode++;
				   }
			   }

			   if (moveMode >= engine->RandomInt (3, 8))
				   FindPath(m_currentWaypointIndex, destIndex, m_pathType);
			   else
				   FindShortestPath(m_currentWaypointIndex, destIndex);
		   }
	   }

	   break;
	   
   // HE grenade throw behaviour
   case TASK_THROWHEGRENADE:
      m_aimFlags |= AIM_GRENADE;
      destination = m_throw;

      if (!(m_states & STATE_SEEINGENEMY))
      {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         m_moveToGoal = false;
      }
      else if (!FNullEnt (m_enemy))
         destination = GetEntityOrigin (m_enemy) + (m_enemy->v.velocity.SkipZ () * 0.5f);

      m_isUsingGrenade = true;
      m_checkTerrain = false;

      if ((pev->origin - destination).GetLengthSquared () < 400.0f * 400.0f)
      {
         // heck, I don't wanna blow up myself
         m_grenadeCheckTime = engine->GetTime () + Const_GrenadeTimer;

         SelectBestWeapon ();
         TaskComplete ();

         break;
      }

      m_grenade = CheckThrow (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () < 100)
         m_grenade = CheckToss (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () <= 100)
      {
         m_grenadeCheckTime = engine->GetTime () + Const_GrenadeTimer;
         m_grenade = m_lookAt;

         SelectBestWeapon ();
         TaskComplete ();
      }
      else
      {
         edict_t *ent = null;

         while (!FNullEnt (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
         {
            if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0)
            {
               // set the correct velocity for the grenade
               if (m_grenade.GetLengthSquared () > 100)
                  ent->v.velocity = m_grenade;

               m_grenadeCheckTime = engine->GetTime () + Const_GrenadeTimer;

               SelectBestWeapon ();
               TaskComplete ();

               break;
            }
         }

         if (FNullEnt (ent))
         {
            if (m_currentWeapon != WEAPON_HEGRENADE)
            {
               if (pev->weapons & (1 << WEAPON_HEGRENADE))
                  SelectWeaponByName ("weapon_hegrenade");
            }
            else if (!(pev->oldbuttons & IN_ATTACK))
               pev->button |= IN_ATTACK;
			else // SyPB Pro P.27 - Debug Grenade
			{
				SelectBestWeapon();
				TaskComplete();
			}
         }
      }
      pev->button |= m_campButtons;
      break;

   // flashbang throw behavior (basically the same code like for HE's)
   case TASK_THROWFBGRENADE:
      m_aimFlags |= AIM_GRENADE;
      destination = m_throw;

      if (!(m_states & STATE_SEEINGENEMY))
      {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         m_moveToGoal = false;
      }
      else if (!FNullEnt (m_enemy))
         destination = GetEntityOrigin (m_enemy) + (m_enemy->v.velocity.SkipZ () * 0.5);

      m_isUsingGrenade = true;
      m_checkTerrain = false;

      m_grenade = CheckThrow (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () < 100)
         m_grenade = CheckToss (pev->origin, destination);

      if (m_grenade.GetLengthSquared () <= 100)
      {
         m_grenadeCheckTime = engine->GetTime () + Const_GrenadeTimer;
         m_grenade = m_lookAt;

         SelectBestWeapon ();
         TaskComplete ();
      }
      else
      {
         edict_t *ent = null;
         while (!FNullEnt (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
         {
            if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0)
            {
               // set the correct velocity for the grenade
               if (m_grenade.GetLengthSquared () > 100)
                  ent->v.velocity = m_grenade;

               m_grenadeCheckTime = engine->GetTime () + Const_GrenadeTimer;

               SelectBestWeapon ();
               TaskComplete ();
               break;
            }
         }

         if (FNullEnt (ent))
         {
            if (m_currentWeapon != WEAPON_FBGRENADE)
            {
               if (pev->weapons & (1 << WEAPON_FBGRENADE))
                  SelectWeaponByName ("weapon_flashbang");
            }
            else if (!(pev->oldbuttons & IN_ATTACK))
               pev->button |= IN_ATTACK;
			else // SyPB Pro P.27 - Debug Grenade
			{
				SelectBestWeapon();
				TaskComplete();
			}
         }
      }
      pev->button |= m_campButtons;
      break;

   // smoke grenade throw behavior
   // a bit different to the others because it mostly tries to throw the sg on the ground
   case TASK_THROWSMGRENADE:
      m_aimFlags |= AIM_GRENADE;

      if (!(m_states & STATE_SEEINGENEMY))
      {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         m_moveToGoal = false;
      }

      m_checkTerrain = false;
      m_isUsingGrenade = true;

      src = m_lastEnemyOrigin - pev->velocity;

      // predict where the enemy is in 0.5 secs
      if (!FNullEnt (m_enemy))
         src = src + m_enemy->v.velocity * 0.5f;

      m_grenade = (src - EyePosition ()).Normalize ();

      if (m_tasks->time < engine->GetTime () + 0.5)
      {
         m_aimFlags &= ~AIM_GRENADE;
         m_states &= ~STATE_THROWSMOKE;

         TaskComplete ();
         break;
      }

      if (m_currentWeapon != WEAPON_SMGRENADE)
      {
         if (pev->weapons & (1 << WEAPON_SMGRENADE))
         {
            SelectWeaponByName ("weapon_smokegrenade");
            m_tasks->time = engine->GetTime () + Const_GrenadeTimer;
         }
         else
            m_tasks->time = engine->GetTime () + 0.1f;
      }
      else if (!(pev->oldbuttons & IN_ATTACK))
         pev->button |= IN_ATTACK;
	  else // SyPB Pro P.27 - Debug Grenade
	  {
		  SelectBestWeapon();
		  TaskComplete();
	  }
      break;

   // bot helps human player (or other bot) to get somewhere
   case TASK_DOUBLEJUMP:
      if (FNullEnt (m_doubleJumpEntity) || !IsAlive (m_doubleJumpEntity) || (m_aimFlags & AIM_ENEMY) || (m_travelStartIndex != -1 && m_tasks->time + (g_waypoint->GetTravelTime (pev->maxspeed, g_waypoint->GetPath (m_travelStartIndex)->origin, m_doubleJumpOrigin) + 11.0f) < engine->GetTime ()))
      {
         ResetDoubleJumpState ();
         break;
      }
      m_aimFlags |= AIM_NAVPOINT;

      if (m_jumpReady)
      {
         m_moveToGoal = false;
         m_checkTerrain = true;

         m_navTimeset = engine->GetTime ();
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         if (m_duckForJump < engine->GetTime ())
            pev->button |= IN_DUCK;

         MakeVectors (nullvec);

         TraceLine (EyePosition (), EyePosition () + (g_pGlobals->v_up + 999.0f), false, true, GetEntity (), &tr);

         if (tr.flFraction < 1.0f && tr.pHit == m_doubleJumpEntity)
         {
            if (m_doubleJumpEntity->v.button & IN_JUMP)
            {
               m_duckForJump = engine->GetTime () + engine->RandomFloat (3.0f, 5.0f);
               m_tasks->time = engine->GetTime ();
            }
         }
         break;
      }

      if (m_currentWaypointIndex == m_prevGoalIndex)
      {
         m_waypointOrigin = m_doubleJumpOrigin;
         m_destOrigin = m_doubleJumpOrigin;
      }

      if (DoWaypointNav ()) // reached destination?
         GetCurrentTask ()->data = -1;

      if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         destIndex = g_waypoint->FindNearest (m_doubleJumpOrigin);

         if (destIndex >= 0 && destIndex < g_numWaypoints)
         {
            m_prevGoalIndex = destIndex;
            m_tasks->data = destIndex;
            m_travelStartIndex = m_currentWaypointIndex;

            // Always take the shortest path
            FindShortestPath (m_currentWaypointIndex, destIndex);

            if (m_currentWaypointIndex == destIndex)
               m_jumpReady = true;
         }
         else
            ResetDoubleJumpState ();
      }
      break;

   // escape from bomb behaviour
   case TASK_ESCAPEFROMBOMB:
	   // SyPB Pro P.42 - Miss C4 Action improve
	   m_aimFlags |= AIM_NAVPOINT;

	   if (!g_bombPlanted || DoWaypointNav())
	   {
		   TaskComplete();

		   if (GetNearbyEnemiesNearPosition(pev->origin, 2048))
		   {
			   m_campButtons = IN_DUCK;

			   if (IsShieldDrawn())
				   pev->button |= IN_ATTACK2;
		   }

		   PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + 10.0f, true);
	   }
	   else if (!GoalIsValid())
	   {
		   if (m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition(pev->origin, 9999) == 0)
			   SelectWeaponByName("weapon_knife");

		   destIndex = -1;
		   DeleteSearchNodes();
		   float safeRadius = engine->RandomFloat(1024.0f, 2048.0f), minPathDistance = 4096.0f;
		   for (i = 0; i < g_numWaypoints; i++)
		   {
			   if ((g_waypoint->GetPath(i)->origin - g_waypoint->GetBombPosition()).GetLength() < safeRadius)
				   continue;

			   float pathDistance = g_waypoint->GetPathDistanceFloat(m_currentWaypointIndex, i);

			   if (minPathDistance > pathDistance)
			   {
				   minPathDistance = pathDistance;
				   destIndex = i;
			   }
		   }

		   if (destIndex < 0)
			   destIndex = g_waypoint->FindFarest(pev->origin, safeRadius);

		   m_prevGoalIndex = destIndex;
		   m_tasks->data = destIndex;
		   FindShortestPath(m_currentWaypointIndex, destIndex);
	   }

	   break;

   // shooting breakables in the way action
   case TASK_DESTROYBREAKABLE:
      m_aimFlags |= AIM_OVERRIDE;

	  // SyPB Pro P.45 - Breakable improve
	  if (FNullEnt(m_breakableEntity) || !FNullEnt(m_enemy) || (pev->origin - m_breakable).GetLength() > 250.0f || 
		  (!IsShootableBreakable(m_breakableEntity) && FNullEnt(m_breakableEntity = FindBreakable())))
	  {
		  TaskComplete();

		  m_breakable = nullvec;
		  m_breakableEntity = null;

		  break;
	  }

      pev->button |= m_campButtons;

      m_checkTerrain = false;
      m_moveToGoal = false;
      m_navTimeset = engine->GetTime ();

      src = m_breakable;
      m_camp = src;

	  // SyPB Pro P.38 - Attack Breakable 
	  if (m_currentWeapon == WEAPON_KNIFE)
	  {
		  KnifeAttack();
		  m_checkTerrain = true;
		  
		  if (!IsZombieEntity(GetEntity()))
			  SelectBestWeapon();
	  }
	  m_wantsToFire = true;

	  break;

   // picking up items and stuff behaviour
   case TASK_PICKUPITEM:
      if (FNullEnt (m_pickupItem))
      {
         m_pickupItem = null;
         TaskComplete ();

         break;
      }

      destination = GetEntityOrigin (m_pickupItem);
      m_destOrigin = destination;
      m_entity = destination;

	  // SyPB Pro P.32 - Base Change
	  if (m_moveSpeed <= 0)
		  m_moveSpeed = pev->maxspeed;

      // find the distance to the item
      float itemDistance = (destination - pev->origin).GetLength ();

      switch (m_pickupType)
      {
      // SyPB Pro P.9
      case PICKTYPE_GETENTITY:
      	  m_aimFlags |= AIM_NAVPOINT;
      
          if (FNullEnt (m_pickupItem) || (GetTeam (m_pickupItem) != -1 && GetTeam (GetEntity ()) != GetTeam (m_pickupItem)))
          {
          	  m_pickupItem = null;
          	  m_pickupType = PICKTYPE_NONE;
          }
          break;
      	  
      case PICKTYPE_WEAPON:
         m_aimFlags |= AIM_NAVPOINT;

		 // SyPB Pro P.42 - AMXX API
		 if (m_blockWeaponPickAPI)
		 {
			 m_pickupItem = null;
			 TaskComplete();
			 break;
		 }

         // near to weapon?
         if (itemDistance < 60)
         {
            for (i = 0; i < 7; i++)
            {
               if (strcmp (g_weaponSelect[i].modelName, STRING (m_pickupItem->v.model) + 9) == 0)
                  break;
            }

            if (i < 7)
            {
               // secondary weapon. i.e., pistol
               int weaponID = 0;

               for (i = 0; i < 7; i++)
               {
                  if (pev->weapons & (1 << g_weaponSelect[i].id))
                     weaponID = i;
               }

               if (weaponID > 0)
               {
                  SelectWeaponbyNumber (weaponID);
                  FakeClientCommand (GetEntity (), "drop");

                  if (HasShield ()) // If we have the shield...
                     FakeClientCommand (GetEntity (), "drop"); // discard both shield and pistol
               }
               EquipInBuyzone (0);
            }
            else
            {
               // primary weapon
               int weaponID = GetHighestWeapon ();

               if ((weaponID > 6) || HasShield ())
               {
                  SelectWeaponbyNumber (weaponID);
                  FakeClientCommand (GetEntity (), "drop");
               }
               EquipInBuyzone (0);
            }
            CheckSilencer (); // check the silencer

			// SyPB Pro P.42 - Waypoint improve
			if (m_currentWaypointIndex != -1)
			{
				if (itemDistance > g_waypoint->GetPath(m_currentWaypointIndex)->radius)
				{
					SetEntityWaypoint(GetEntity());
					m_currentWaypointIndex = -1;
					GetValidWaypoint();
				}
			}
         }
         break;

      case PICKTYPE_SHIELDGUN:
         m_aimFlags |= AIM_NAVPOINT;

		 // SyPB Pro P.42 - AMXX API
		 if (HasShield () || m_blockWeaponPickAPI)
         //if (HasShield ())
         {
            m_pickupItem = null;
            break;
         }
         else if (itemDistance < 60) // near to shield?
         {
            // get current best weapon to check if it's a primary in need to be dropped
            int weaponID = GetHighestWeapon ();

			if (weaponID > 6)
			{
				SelectWeaponbyNumber(weaponID);
				FakeClientCommand(GetEntity(), "drop");

				// SyPB Pro P.42 - Waypoint improve
				if (m_currentWaypointIndex != -1)
				{
					if (itemDistance > g_waypoint->GetPath(m_currentWaypointIndex)->radius)
					{
						SetEntityWaypoint(GetEntity());
						m_currentWaypointIndex = -1;
						GetValidWaypoint();
					}
				}
			}
         }
         break;

      case PICKTYPE_PLANTEDC4:
         m_aimFlags |= AIM_ENTITY;

         if (team == TEAM_COUNTER && itemDistance < 80)
         {
            // notify team of defusing
            if (GetNearbyFriendsNearPosition (pev->origin, 600) < 1 && GetNearbyFriendsNearPosition (pev->origin, 9999) >= 1)
               RadioMessage (Radio_NeedBackup);

            m_moveToGoal = false;
            m_checkTerrain = false;

            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            PushTask (TASK_DEFUSEBOMB, TASKPRI_DEFUSEBOMB, -1, 0.0, false);
         }
         break;

      case PICKTYPE_HOSTAGE:
         m_aimFlags |= AIM_ENTITY;
         src = EyePosition ();

		 // SyPB Pro P.42 - Small fixed 
         //if (!IsAlive (m_pickupItem))
		 if (!IsAlive(m_pickupItem) || GetTeam (GetEntity ()) != TEAM_COUNTER)
         {
            // don't pickup dead hostages
            m_pickupItem = null;
            TaskComplete ();

            break;
         }

         if (itemDistance < 60)
         {
            float angleToEntity = InFieldOfView (destination - src);

            if (angleToEntity <= 10) // bot faces hostage?
            {
               // use game dll function to make sure the hostage is correctly 'used'
               MDLL_Use (m_pickupItem, GetEntity ());

               for (i = 0; i < Const_MaxHostages; i++)
               {
                  if (FNullEnt (m_hostages[i])) // store pointer to hostage so other bots don't steal from this one or bot tries to reuse it
                  {
                     m_hostages[i] = m_pickupItem;
                     m_pickupItem = null;

                     break;
                  }
               }
            }
			// SyPB Pro P.42 - Hostages Ai improve
			m_itemCheckTime = engine->GetTime() + 0.1f;
            m_lastCollTime = engine->GetTime () + 0.1f; // also don't consider being stuck
         }
         break;

      case PICKTYPE_DEFUSEKIT:
         m_aimFlags |= AIM_NAVPOINT;

		 // SyPB Pro P.42 - Small fixed 
		 if (m_hasDefuser || GetTeam(GetEntity()) != TEAM_COUNTER)
         // if (m_hasDefuser)
         {
            m_pickupItem = null;
            m_pickupType = PICKTYPE_NONE;
         }
         break;

      case PICKTYPE_BUTTON:
         m_aimFlags |= AIM_ENTITY;

         if (FNullEnt (m_pickupItem) || m_buttonPushTime < engine->GetTime ()) // it's safer...
         {
            TaskComplete ();
            m_pickupType = PICKTYPE_NONE;

            break;
         }

         // find angles from bot origin to entity...
         src = EyePosition ();
         float angleToEntity = InFieldOfView (destination - src);

         if (itemDistance < 90) // near to the button?
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;
            m_moveToGoal = false;
            m_checkTerrain = false;

            if (angleToEntity <= 10) // facing it directly?
            {
               MDLL_Use (m_pickupItem, GetEntity ());

               m_pickupItem = null;
               m_pickupType = PICKTYPE_NONE;
               m_buttonPushTime = engine->GetTime () + 3.0f;

               TaskComplete ();
            }
         }
         break;
      }
      break;
   }
}

// SyPB Pro P.30 - debug
void Bot::DebugModeMsg(void)
{
	// SyPB Pro P.48 - Debug Msg
	int debugMode = sypb_debug.GetInt();
	if (FNullEnt(g_hostEntity) || debugMode <= 0 || debugMode == 2)
		return;

	static float timeDebugUpdate = 0.0f;

	int specIndex = g_hostEntity->v.iuser2;

	if (specIndex == ENTINDEX(GetEntity()))
	{
		static int index, goal, taskID;

		if (m_tasks != null)
		{
			if (taskID != m_tasks->taskID || index != m_currentWaypointIndex || goal != m_tasks->data || timeDebugUpdate < engine->GetTime())
			{
				taskID = m_tasks->taskID;
				index = m_currentWaypointIndex;
				goal = m_tasks->data;

				char taskName[80];

				switch (taskID)
				{
				case TASK_NORMAL:
					sprintf(taskName, "Normal");
					break;

				case TASK_PAUSE:
					sprintf(taskName, "Pause");
					break;

				case TASK_MOVETOPOSITION:
					sprintf(taskName, "MoveToPosition");
					break;

				case TASK_FOLLOWUSER:
					sprintf(taskName, "FollowUser");
					break;

				case TASK_MOVETOTARGET:
					sprintf(taskName, "MoveToTarget");
					break;

				case TASK_PICKUPITEM:
					sprintf(taskName, "PickupItem");
					break;

				case TASK_CAMP:
					sprintf(taskName, "Camp");
					break;

				case TASK_PLANTBOMB:
					sprintf(taskName, "PlantBomb");
					break;

				case TASK_DEFUSEBOMB:
					sprintf(taskName, "DefuseBomb");
					break;

				case TASK_FIGHTENEMY:
					sprintf(taskName, "AttackEnemy");
					break;

				case TASK_HUNTENEMY:
					sprintf(taskName, "HuntEnemy");
					break;

				case TASK_SEEKCOVER:
					sprintf(taskName, "SeekCover");
					break;

				case TASK_THROWHEGRENADE:
					sprintf(taskName, "ThrowExpGrenade");
					break;

				case TASK_THROWFBGRENADE:
					sprintf(taskName, "ThrowFlashGrenade");
					break;

				case TASK_THROWSMGRENADE:
					sprintf(taskName, "ThrowSmokeGrenade");
					break;

				case TASK_DOUBLEJUMP:
					sprintf(taskName, "PerformDoubleJump");
					break;

				case TASK_ESCAPEFROMBOMB:
					sprintf(taskName, "EscapeFromBomb");
					break;

				case TASK_DESTROYBREAKABLE:
					sprintf(taskName, "ShootBreakable");
					break;

				case TASK_HIDE:
					sprintf(taskName, "Hide");
					break;

				case TASK_BLINDED:
					sprintf(taskName, "Blinded");
					break;

				case TASK_SPRAYLOGO:
					sprintf(taskName, "SprayLogo");
					break;
				}

				char weaponName[80], aimFlags[32], botType[32];
				char enemyName[80] , pickName[80];

				// SyPB Pro P.42 - small improve
				if (!FNullEnt(m_enemy))
					sprintf(enemyName, "[E]: %s", GetEntityName (m_enemy));
				else if (!FNullEnt(m_moveTargetEntity))
					sprintf(enemyName, "[MT]: %s", GetEntityName(m_moveTargetEntity));
				else if (!FNullEnt(m_lastEnemy))
					sprintf(enemyName, "[LE]: %s", GetEntityName(m_lastEnemy));
				else
					sprintf(enemyName, ": %s", GetEntityName(null));

				sprintf(pickName, "%s", GetEntityName(m_pickupItem));

				WeaponSelect *selectTab = &g_weaponSelect[0];
				char weaponCount = 0;

				while (m_currentWeapon != selectTab->id && weaponCount < Const_NumWeapons)
				{
					selectTab++;
					weaponCount++;
				}

				// set the aim flags
				sprintf(aimFlags, "%s%s%s%s%s%s%s%s",
					m_aimFlags & AIM_NAVPOINT ? " NavPoint" : "",
					m_aimFlags & AIM_CAMP ? " CampPoint" : "",
					m_aimFlags & AIM_PREDICTENEMY ? " PredictEnemy" : "",
					m_aimFlags & AIM_LASTENEMY ? " LastEnemy" : "",
					m_aimFlags & AIM_ENTITY ? " Entity" : "",
					m_aimFlags & AIM_ENEMY ? " Enemy" : "",
					m_aimFlags & AIM_GRENADE ? " Grenade" : "",
					m_aimFlags & AIM_OVERRIDE ? " Override" : "");

				// set the bot type
				sprintf(botType, "%s%s%s", m_personality == PERSONALITY_RUSHER ? " Rusher" : "",
					m_personality == PERSONALITY_CAREFUL ? " Careful" : "",
					m_personality == PERSONALITY_NORMAL ? " Normal" : "");

				if (weaponCount >= Const_NumWeapons)
				{
					// prevent printing unknown message from known weapons
					switch (m_currentWeapon)
					{
					case WEAPON_HEGRENADE:
						sprintf(weaponName, "weapon_hegrenade");
						break;

					case WEAPON_FBGRENADE:
						sprintf(weaponName, "weapon_flashbang");
						break;

					case WEAPON_SMGRENADE:
						sprintf(weaponName, "weapon_smokegrenade");
						break;

					case WEAPON_C4:
						sprintf(weaponName, "weapon_c4");
						break;

					default:
						sprintf(weaponName, "Unknown! (%d)", m_currentWeapon);
					}
				}
				else
					sprintf(weaponName, selectTab->weaponName);

				char gamemodName[80];
				switch (GetGameMod())
				{
				case MODE_BASE:
					sprintf(gamemodName, "Normal");
					break;

				case MODE_DM:
					sprintf(gamemodName, "DeathMatch");
					break;

				case MODE_ZP:
					sprintf(gamemodName, "Zombie");
					break;

				case MODE_NOTEAM:
					sprintf(gamemodName, "No Team");
					break;

				case MODE_ZH:
					sprintf(gamemodName, "Zombie Hell");
					break;

				default:
					sprintf(gamemodName, "UNKNOWN MODE");
				}

				PathNode *navid = &m_navNode[0];
				int navIndex[2] = { 0, 0 };

				while (navid != null)
				{
					if (navIndex[0] == 0)
						navIndex[0] = navid->index;
					else
					{
						navIndex[1] = navid->index;
						break;
					}

					navid = navid->next;
				}

				int client = ENTINDEX(GetEntity ()) - 1;

				char outputBuffer[512];
				sprintf(outputBuffer, "\n\n\n\n\n\n\n Game Mode: %s"
					"\n [%s] \n Task: %s  AimFlags: %s \n"
					"Weapon: %s  Clip: %d   Ammo: %d \n"
					"Type: %s  Money: %d  Bot Ai: %s \n"
					"Enemy%s  Pickup: %s  \n\n"

					"CWI: %d  GI: %d  TD: %d \n"
					"Nav: %d  Next Nav: %d \n"
					"GEWI: %d GEWI2: %d \n"
					"Move Speed: %2.f  Strafe Speed: %2.f \n "
					"Check Terran: %d  Stuck: %d \n", 
					gamemodName,
					GetEntityName (GetEntity ()), taskName, aimFlags,
					&weaponName[7], GetAmmoInClip(), GetAmmo(),
					botType, m_moneyAmount, IsZombieEntity(GetEntity()) ? "Zombie" : "Normal",
					enemyName, pickName,
					m_currentWaypointIndex, m_prevGoalIndex, m_tasks->data,
					navIndex[0], navIndex[1],
					g_clients[client].wpIndex, g_clients[client].wpIndex2, 
					m_moveSpeed, m_strafeSpeed,
					m_checkTerrain, m_isStuck);

				MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, null, g_hostEntity);
				WRITE_BYTE(TE_TEXTMESSAGE);
				WRITE_BYTE(1);
				WRITE_SHORT(FixedSigned16(-1, 1 << 13));
				WRITE_SHORT(FixedSigned16(0, 1 << 13));
				WRITE_BYTE(0);
				WRITE_BYTE(GetTeam(GetEntity()) == TEAM_COUNTER ? 0 : 255);
				WRITE_BYTE(100);
				WRITE_BYTE(GetTeam(GetEntity()) != TEAM_COUNTER ? 0 : 255);
				WRITE_BYTE(0);
				WRITE_BYTE(255);
				WRITE_BYTE(255);
				WRITE_BYTE(255);
				WRITE_BYTE(0);
				WRITE_SHORT(FixedUnsigned16(0, 1 << 8));
				WRITE_SHORT(FixedUnsigned16(0, 1 << 8));
				WRITE_SHORT(FixedUnsigned16(1.0, 1 << 8));
				WRITE_STRING(const_cast <const char *> (&outputBuffer[0]));
				MESSAGE_END();

				timeDebugUpdate = engine->GetTime() + 2.0f;
			}

			if (m_moveTargetOrigin != nullvec)
				engine->DrawLine(g_hostEntity, EyePosition(), m_moveTargetOrigin, Color(0, 255, 0, 200), 10, 0, 5, 1, LINE_SIMPLE);

			if (m_enemyOrigin != nullvec && !FNullEnt(m_enemy))
				engine->DrawLine(g_hostEntity, EyePosition(), m_enemyOrigin, Color(255, 0, 0, 200), 10, 0, 5, 1, LINE_SIMPLE);

			// now draw line from source to destination
			PathNode *node = &m_navNode[0];

			// SyPB Pro P.40 - Debug MSG
			Vector src = nullvec;
			while (node != null)
			{
				Path *path = g_waypoint->GetPath(node->index);
				src = path->origin;
				node = node->next;

				if (node != null)
				{
					bool jumpPoint = false;
					for (int j = 0; j < Const_MaxPathIndex; j++)
					{
						if (path->index[j] == node->index &&
							path->connectionFlags[j] & PATHFLAG_JUMP)
						{
							jumpPoint = true;
							break;
						}
					}

					if (jumpPoint)
						engine->DrawLine(g_hostEntity, src, g_waypoint->GetPath(node->index)->origin,
							Color(255, 0, 0, 100), 15, 0, 8, 1, LINE_SIMPLE);
					else
						engine->DrawLine(g_hostEntity, src, g_waypoint->GetPath(node->index)->origin,
							Color(255, 100, 55, 20), 15, 0, 8, 1, LINE_SIMPLE);
				}
			}

			if (m_prevWptIndex[0] != -1)
			{
				src = g_waypoint->GetPath(m_prevWptIndex[0])->origin;
				engine->DrawLine(g_hostEntity, src, src + Vector(0.0f, 0.0f, 40.0f),
					Color(255, 0, 0, 100), 15, 0, 8, 1, LINE_SIMPLE);
			}

			if (m_currentWaypointIndex != -1)
			{
				src = g_waypoint->GetPath(m_currentWaypointIndex)->origin;
				engine->DrawLine(g_hostEntity, src, src+ Vector (0.0f, 0.0f, 40.0f),
					Color(0, 255, 0, 100), 15, 0, 8, 1, LINE_SIMPLE);
			}

			if (m_lookAt != nullvec)
			{
				src = m_lookAt;
				engine->DrawLine(g_hostEntity, src - Vector(0.0f, 0.0f, 20.0f), src + Vector(0.0f, 0.0f, 20.0f),
					Color(0, 0, 255, 100), 15, 0, 8, 1, LINE_SIMPLE);
			}

			if (m_prevGoalIndex != -1)
			{
				src = g_waypoint->GetPath(m_prevGoalIndex)->origin;
				engine->DrawLine(g_hostEntity, src, src + Vector(0.0f, 0.0f, 40.0f),
					Color(0, 255, 255, 100), 15, 0, 8, 1, LINE_SIMPLE);
			}
		}
	}
}

void Bot::FunBotAI(void)
{
	// Other Mode Bot Ai Here !!!!!
}

void Bot::BotAI (void)
{
   // this function gets called each frame and is the core of all bot ai. from here all other subroutines are called

   float movedDistance = 2.0f; // length of different vector (distance bot moved)
   TraceResult tr;

   int team = GetTeam (GetEntity ());

   // SyPB Pro P.43 - Base Mode Small improve
   if (m_checkKnifeSwitch && m_buyingFinished && m_spawnTime + engine->RandomFloat(4.0f, 6.5f) < engine->GetTime())
   {
	   m_checkKnifeSwitch = false;

	   if (GetGameMod() == MODE_BASE)
	   {
		   if (sypb_spraypaints.GetBool() && engine->RandomInt(1, 100) < 2)
			   PushTask(TASK_SPRAYLOGO, TASKPRI_SPRAYLOGO, -1, engine->GetTime() + 1.0f, false);

		   if (m_skill > 75 && engine->RandomInt(0, 100) < (m_personality == PERSONALITY_RUSHER ? 99 : 80) &&
			   !m_isReloading && (g_mapType & (MAP_CS | MAP_DE | MAP_ES | MAP_AS)))
			   SelectWeaponByName("weapon_knife");
	   }
   }


   // SyPB Pro P.30 - Zombie Ai
   if (IsZombieEntity(GetEntity()) && m_currentWeapon != WEAPON_KNIFE)
	   SelectWeaponByName("weapon_knife");

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + engine->RandomFloat (2.0f, 3.5f) < engine->GetTime ())
   {
      if (HasShield () && IsShieldDrawn ())
        pev->button |= IN_ATTACK2;
      else
      {
         switch (m_currentWeapon)
         {
         case WEAPON_M4A1:
         case WEAPON_USP:
            CheckSilencer ();
            break;

         case WEAPON_FAMAS:
         case WEAPON_GLOCK18:
            if (engine->RandomInt (0, 100) < 50)
               pev->button |= IN_ATTACK2;
            break;
         }
      }

      // select a leader bot for this team
	  if (GetGameMod() == MODE_BASE)
		  SelectLeaderEachTeam (team);

      m_checkWeaponSwitch = false;
   }

   // warning: the following timers aren't frame independent so it varies on slower/faster computers

   // increase reaction time
   m_actualReactionTime += 0.2f;

   if (m_actualReactionTime > m_idealReactionTime)
      m_actualReactionTime = m_idealReactionTime;

   // bot could be blinded by flashbang or smoke, recover from it
   m_viewDistance += 3.0f;

   if (m_viewDistance > m_maxViewDistance)
      m_viewDistance = m_maxViewDistance;

   if (m_blindTime > engine->GetTime ())
      m_maxViewDistance = 4096.0f;

   m_moveSpeed = pev->maxspeed;

   if (m_prevTime <= engine->GetTime ())
   {
      // see how far bot has moved since the previous position...
      movedDistance = (m_prevOrigin - pev->origin).GetLength ();

      // save current position as previous
      m_prevOrigin = pev->origin;
      m_prevTime = engine->GetTime () + 0.2f;
   }

   // SyPB Pro P.30 - Block Radio
   // if there's some radio message to respond, check it
   if (m_radioOrder != 0 && GetGameMod () == MODE_BASE)
      CheckRadioCommands ();

   // do all sensing, calculate/filter all actions here
   SetConditions ();

   // SyPB Pro P.30 - Zombie Mode Ai
   if (GetGameMod() == MODE_ZP || GetGameMod() == MODE_ZH)
	   ZombieModeAi();

   Vector src, dest;

   m_checkTerrain = true;
   m_moveToGoal = true;
   m_wantsToFire = false;

   //AvoidGrenades (); // avoid flyings grenades
   AvoidEntity();
   m_isUsingGrenade = false;
   
   RunTask (); // execute current task
   ChooseAimDirection (); // choose aim direction
   
   FacePosition (); // and turn to chosen aim direction

   // the bots wants to fire at something?
   if (m_wantsToFire && !m_isUsingGrenade && m_shootTime <= engine->GetTime ())
      FireWeapon (); // if bot didn't fire a bullet try again next frame

   // check for reloading
   if (m_reloadCheckTime <= engine->GetTime ())
      CheckReload ();

   // set the reaction time (surprise momentum) different each frame according to skill
   m_idealReactionTime = engine->RandomFloat (g_skillTab[m_skill / 20].minSurpriseTime, g_skillTab[m_skill / 20].maxSurpriseTime);

   // SyPB Pro P.34 - Base Ai
   Vector directionOld = m_destOrigin - (pev->origin + pev->velocity * m_frameInterval);
   Vector directionNormal = directionOld.Normalize ();
   Vector direction = directionNormal;
   directionNormal.z = 0.0f;

   m_moveAngles = directionOld.ToAngles ();

   m_moveAngles.ClampAngles ();
   m_moveAngles.x *= -1.0f; // invert for engine

   // SyPB Pro P.48 - Base improve 
   if (!IsOnLadder() && GetCurrentTask()->taskID != TASK_CAMP && FNullEnt(m_moveTargetEntity) &&
	   (((m_aimFlags & AIM_ENEMY) || (m_states & (STATE_SEEINGENEMY)) || !FNullEnt(m_enemy)) ||
	   ((GetCurrentTask()->taskID == TASK_SEEKCOVER) && (m_isReloading || m_isVIP))) &&
		   ((GetGameMod() == MODE_BASE && m_skill >= 75) || (GetGameMod() == MODE_DM && m_skill >= 60) ||
	   (IsZombieEntity(GetEntity())) || UsesSniper ()))
   {
	   m_moveToGoal = false; // don't move to goal
	   m_navTimeset = engine->GetTime();

	   if (!FNullEnt(m_enemy))
		   CombatFight();
   }

   // SyPB Pro P.42 - Miss C4 Action improve
   if ((g_mapType & MAP_DE) && g_bombPlanted && m_notKilled && OutOfBombTimer ())
   {
	   if (GetCurrentTask()->taskID != TASK_ESCAPEFROMBOMB && GetCurrentTask()->taskID != TASK_CAMP)
	   {
		   if ((g_waypoint->GetPath(m_currentWaypointIndex)->origin - g_waypoint->GetBombPosition()).GetLength() < 1024.0f)
		   {
			   TaskComplete();
			   PushTask(TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0, true);
		   }
	   }
   }

   // allowed to move to a destination position?
   if (m_moveToGoal && !m_moveAIAPI) // SyPB Pro P.30 - AMXX API
   {
      GetValidWaypoint ();

      // Press duck button if we need to
      if ((g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CROUCH) && !(g_waypoint->GetPath (m_currentWaypointIndex)->flags & WAYPOINT_CAMP))
         pev->button |= IN_DUCK;

      m_timeWaypointMove = engine->GetTime ();

      if (IsInWater ()) // special movement for swimming here
      {
         // check if we need to go forward or back press the correct buttons
         if (InFieldOfView (m_destOrigin - EyePosition ()) > 90.0f)
            pev->button |= IN_BACK;
		 else
            pev->button |= IN_FORWARD;

         if (m_moveAngles.x > 60.0f)
            pev->button |= IN_DUCK;
         else if (m_moveAngles.x < -60.0f)
            pev->button |= IN_JUMP;
      }
   }

   // SyPB Pro P.40 - Fall Ai
   bool fixFall = false;
   if (!m_checkFall)
   {
	   if (!IsOnFloor() && !IsOnLadder() && !IsInWater())
	   {
		   if (m_checkFallPoint[0] != nullvec && m_checkFallPoint[1] != nullvec)
			   m_checkFall = true;
	   }
	   else if (IsOnFloor())
	   {
		   if (!FNullEnt(m_enemy))
		   {
			   m_checkFallPoint[0] = pev->origin;
			   m_checkFallPoint[1] = GetEntityOrigin(m_enemy);
		   }
		   else
		   {
			   if (m_prevWptIndex[0] != -1)
				   m_checkFallPoint[0] = g_waypoint->GetPath(m_prevWptIndex[0])->origin;
			   else
				   m_checkFallPoint[0] = pev->origin;

			   if (m_currentWaypointIndex != -1)
				   m_checkFallPoint[1] = g_waypoint->GetPath(m_currentWaypointIndex)->origin;
			   else if (&m_navNode[0] != null)
				   m_checkFallPoint[1] = g_waypoint->GetPath(m_navNode->index)->origin;
		   }
	   }
   }
   else
   {
	   if (IsOnLadder() || IsInWater())
	   {
		   m_checkFallPoint[0] = nullvec;
		   m_checkFallPoint[1] = nullvec;
		   m_checkFall = false;
	   }
	   else if (IsOnFloor())
	   {
		   float baseDistance = (m_checkFallPoint[0] - m_checkFallPoint[1]).GetLength();
		   float nowDistance = (pev->origin - m_checkFallPoint[1]).GetLength();

		   if (nowDistance > baseDistance &&
			   (nowDistance > baseDistance * 1.2 || nowDistance > baseDistance + 200.0f) &&
			   baseDistance >= 80.0f && nowDistance >= 100.0f)
			   fixFall = true;
		   else if (pev->origin.z + 128.0f < m_checkFallPoint[1].z && pev->origin.z + 128.0f < m_checkFallPoint[0].z)
			   fixFall = true;

		   if (m_currentWaypointIndex != -1 && !fixFall)
		   {
			   float distance2D = (pev->origin - g_waypoint->GetPath(m_currentWaypointIndex)->origin).GetLength();
			   if (distance2D <= 32.0f && pev->origin.z + 16.0f < g_waypoint->GetPath(m_currentWaypointIndex)->origin.z)
				   fixFall = true;
		   }

		   m_checkFallPoint[0] = nullvec;
		   m_checkFallPoint[1] = nullvec;
		   m_checkFall = false;
	   }
   }

   if (fixFall)
   {
	   // SyPB Pro P.42 - Fall Ai improve
	   SetEntityWaypoint(GetEntity(), -2);
	   m_currentWaypointIndex = -1;
	   GetValidWaypoint();

	   // SyPB Pro P.39 - Fall Ai improve
	   if (!FNullEnt(m_enemy) || !FNullEnt(m_moveTargetEntity))
		   m_enemyUpdateTime = engine->GetTime();
   }

   if (m_moveAIAPI) // SyPB Pro P.30 - AMXX API
	   m_checkTerrain = false;

   // SyPB Pro P.27 - new check terrain
   if (m_checkTerrain)
   {
	   m_isStuck = false;
	   CheckCloseAvoidance(directionNormal);

	   // SyPB Pro P.42 - Bot Stuck improve
	   if ((m_moveSpeed <= -10 || m_moveSpeed >= 10 || m_strafeSpeed >= 10 || m_strafeSpeed <= -10) &&
		   m_lastCollTime < engine->GetTime())
	   {
		   // SyPB Pro P.38 - Get Stuck improve
		   if (m_damageTime >= engine->GetTime() && IsZombieEntity(GetEntity()))
		   {
			   m_lastCollTime = m_damageTime + 0.01f;
			   m_firstCollideTime = 0.0f;
			   m_isStuck = false;
		   }
		   else
		   {
			   if (movedDistance < 2.0f && m_prevSpeed >= 20.0f)
			   {
				   m_prevTime = engine->GetTime();
				   m_isStuck = true;

				   if (m_firstCollideTime == 0.0f)
					   m_firstCollideTime = engine->GetTime() + 0.2f;
			   }
			   else
			   {
				   // test if there's something ahead blocking the way
				   if (!IsOnLadder() && CantMoveForward(directionNormal, &tr))
				   {
					   if (m_firstCollideTime == 0.0f)
						   m_firstCollideTime = engine->GetTime() + 0.2f;

					   else if (m_firstCollideTime <= engine->GetTime())
						   m_isStuck = true;
				   }
				   else
					   m_firstCollideTime = 0.0f;
			   }
		   }
	   }

	   if (!m_isStuck) // not stuck?
	   {
		   if (m_probeTime + 0.5f < engine->GetTime())
			   ResetCollideState(); // reset collision memory if not being stuck for 0.5 secs
		   else
		   {
			   // remember to keep pressing duck if it was necessary ago
			   if (m_collideMoves[m_collStateIndex] == COSTATE_DUCK && IsOnFloor() || IsInWater())
				   pev->button |= IN_DUCK;
		   }
	   }
	   // SyPB Pro P.47 - Base improve
	   else
	   {
		   // not yet decided what to do?
		   if (m_collisionState == COSTATE_UNDECIDED)
		   {
			   int bits = 0;

			   if (IsOnLadder())
				   bits |= COPROBE_STRAFE;
			   else if (IsInWater())
				   bits |= (COPROBE_JUMP | COPROBE_STRAFE);
			   else
				   bits |= (COPROBE_STRAFE | (engine->RandomInt(0, 10) > 7 ? COPROBE_JUMP : 0));

			   // collision check allowed if not flying through the air
			   if (IsOnFloor() || IsOnLadder() || IsInWater())
			   {
				   int state[8];
				   int i = 0;

				   // first 4 entries hold the possible collision states
				   state[i++] = COSTATE_STRAFELEFT;
				   state[i++] = COSTATE_STRAFERIGHT;
				   state[i++] = COSTATE_JUMP;
				   state[i++] = COSTATE_DUCK;

				   if (bits & COPROBE_STRAFE)
				   {
					   state[i] = 0;
					   state[i + 1] = 0;

					   // to start strafing, we have to first figure out if the target is on the left side or right side
					   MakeVectors(m_moveAngles);

					   Vector dirToPoint = (pev->origin - m_destOrigin).Normalize2D();
					   Vector rightSide = g_pGlobals->v_right.Normalize2D();

					   bool dirRight = false;
					   bool dirLeft = false;
					   bool blockedLeft = false;
					   bool blockedRight = false;

					   if ((dirToPoint | rightSide) > 0.0f)
						   dirRight = true;
					   else
						   dirLeft = true;

					   const Vector &testDir = m_moveSpeed > 0.0f ? g_pGlobals->v_forward : -g_pGlobals->v_forward;

					   // now check which side is blocked
					   src = pev->origin + g_pGlobals->v_right * 32.0f;
					   dest = src + testDir * 32.0f;

					   TraceHull(src, dest, true, head_hull, GetEntity(), &tr);

					   if (tr.flFraction != 1.0f)
						   blockedRight = true;

					   src = pev->origin - g_pGlobals->v_right * 32.0f;
					   dest = src + testDir * 32.0f;

					   TraceHull(src, dest, true, head_hull, GetEntity(), &tr);

					   if (tr.flFraction != 1.0f)
						   blockedLeft = true;

					   if (dirLeft)
						   state[i] += 5;
					   else
						   state[i] -= 5;

					   if (blockedLeft)
						   state[i] -= 5;

					   i++;

					   if (dirRight)
						   state[i] += 5;
					   else
						   state[i] -= 5;

					   if (blockedRight)
						   state[i] -= 5;
				   }

				   // now weight all possible states
				   if (bits & COPROBE_JUMP)
				   {
					   state[i] = 0;

					   if (CanJumpUp(directionNormal))
						   state[i] += 10;

					   if (m_destOrigin.z >= pev->origin.z + 18.0f)
						   state[i] += 5;

					   if (EntityIsVisible(m_destOrigin))
					   {
						   MakeVectors(m_moveAngles);

						   src = EyePosition();
						   src = src + g_pGlobals->v_right * 15.0f;

						   TraceLine(src, m_destOrigin, true, true, GetEntity(), &tr);

						   if (tr.flFraction >= 1.0f)
						   {
							   src = EyePosition();
							   src = src - g_pGlobals->v_right * 15.0f;

							   TraceLine(src, m_destOrigin, true, true, GetEntity(), &tr);

							   if (tr.flFraction >= 1.0f)
								   state[i] += 5;
						   }
					   }
					   if (pev->flags & FL_DUCKING)
						   src = pev->origin;
					   else
						   src = pev->origin + Vector(0.0f, 0.0f, -17.0f);

					   dest = src + directionNormal * 30.0f;
					   TraceLine(src, dest, true, true, GetEntity(), &tr);

					   if (tr.flFraction != 1.0f)
						   state[i] += 10;
				   }
				   else
					   state[i] = 0;
				   i++;
				   state[i] = 0;
				   i++;

				   // weighted all possible moves, now sort them to start with most probable
				   bool isSorting = false;

				   do
				   {
					   isSorting = false;
					   for (i = 0; i < 3; i++)
					   {
						   if (state[i + 3] < state[i + 3 + 1])
						   {
							   int temp = state[i];

							   state[i] = state[i + 1];
							   state[i + 1] = temp;

							   temp = state[i + 3];

							   state[i + 3] = state[i + 4];
							   state[i + 4] = temp;

							   isSorting = true;
						   }
					   }
				   } while (isSorting);

				   for (i = 0; i < 3; i++)
					   m_collideMoves[i] = state[i];

				   m_collideTime = engine->GetTime();
				   m_probeTime = engine->GetTime() + 0.5f;
				   m_collisionProbeBits = bits;
				   m_collisionState = COSTATE_PROBING;
				   m_collStateIndex = 0;
			   }
		   }

		   if (m_collisionState == COSTATE_PROBING)
		   {
			   if (m_probeTime < engine->GetTime())
			   {
				   m_collStateIndex++;
				   m_probeTime = engine->GetTime() + 0.5f;

				   if (m_collStateIndex > 3)
				   {
					   m_navTimeset = engine->GetTime() - 5.0f;
					   ResetCollideState();
				   }
			   }

			   if (m_collStateIndex < 3)
			   {
				   switch (m_collideMoves[m_collStateIndex])
				   {
				   case COSTATE_JUMP:
					   if (IsOnFloor() || IsInWater())
					   {
						   if (IsInWater() || !IsZombieEntity(GetEntity()) || m_damageTime < engine->GetTime() ||
							   m_currentTravelFlags & PATHFLAG_JUMP || KnifeAttack())
							   pev->button |= IN_JUMP;
					   }
					   break;

				   case COSTATE_DUCK:
					   if (IsOnFloor() || IsInWater())
						   pev->button |= IN_DUCK;
					   break;

				   case COSTATE_STRAFELEFT:
					   pev->button |= IN_MOVELEFT;
					   SetStrafeSpeed(directionNormal, -pev->maxspeed);
					   break;

				   case COSTATE_STRAFERIGHT:
					   pev->button |= IN_MOVERIGHT;
					   SetStrafeSpeed(directionNormal, pev->maxspeed);
					   break;
				   }
			   }
		   }
	   }
	   
   }
   else
	   m_isStuck = false;

   // must avoid a grenade?
   if (m_needAvoidEntity != 0)
   {
      // Don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * m_needAvoidEntity;
   }

   // time to reach waypoint
   if (m_navTimeset + GetEstimatedReachTime () < engine->GetTime () && m_moveToGoal)
   {
	   // SyPB Pro P.40 - Base Change for Waypoint OS
	   if (FNullEnt (m_enemy) || m_currentWaypointIndex == -1 ||
		   (pev->origin - g_waypoint->GetPath (m_currentWaypointIndex)->origin).GetLength () > (g_waypoint->GetPath (m_currentWaypointIndex)->radius) * 3)
		   GetValidWaypoint ();

	   if (FNullEnt(m_enemy))
	   {
		   // clear these pointers, bot mingh be stuck getting to them
		   if (!FNullEnt(m_pickupItem) && m_pickupType != PICKTYPE_PLANTEDC4)
			   m_itemIgnore = m_pickupItem;

		   m_pickupItem = null;
		   m_breakableEntity = null;
		   m_breakable = nullvec;
		   m_itemCheckTime = engine->GetTime() + 5.0f;
		   m_pickupType = PICKTYPE_NONE;
	   }
   }

   // SyPB Pro P.21 - Ladder Strengthen
   bool OnLadderNoDuck = false;

   // SyPB Pro P.45 - Ladder Strengthen
   if (IsOnLadder() || (m_currentWaypointIndex != -1 && g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LADDER))
   {
	   if (!(g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_CROUCH))
		   OnLadderNoDuck = true;
   }

   if (OnLadderNoDuck)
   {
   	   m_campButtons &= ~IN_DUCK;
   	   pev->button &= ~IN_DUCK;
   }
   else if (m_duckTime > engine->GetTime ())
      pev->button |= IN_DUCK;

   // SyPB Pro P.39 - Small change for Jump
   if (pev->button & IN_JUMP)
   {
	   if (m_currentTravelFlags & PATHFLAG_JUMP)
	   {
		   // SyPB Pro P.40 - Jump improve
		   Vector point1Origin, point2Origin;
		   if (m_prevWptIndex[0] != -1)
			   point1Origin = g_waypoint->GetPath(m_prevWptIndex[0])->origin;
		   else if (IsOnFloor())
			   point1Origin = pev->origin;

		   if (m_currentWaypointIndex != -1)
			   point2Origin = g_waypoint->GetPath(m_currentWaypointIndex)->origin;

		   if (point1Origin != nullvec && point2Origin != nullvec)
		   {
			   if ((point1Origin - point2Origin).GetLength () >= 100.0f)
				   m_jumpTime = engine->GetTime() + engine->RandomFloat(0.8f, 1.2f);
			   else if (point1Origin.z > point2Origin.z)
				   m_jumpTime = engine->GetTime();
			   else
				   m_jumpTime = engine->GetTime() + engine->RandomFloat(0.5f, 0.8f);
		   }
	   }
	   else
		   m_jumpTime = engine->GetTime() + engine->RandomFloat(0.3f, 0.5f);
   }

   if (m_jumpTime >= engine->GetTime() && !IsOnFloor() && !IsInWater() && !IsOnLadder())
	   pev->button |= IN_DUCK; 

   // save the previous speed (for checking if stuck)
   m_prevSpeed = fabsf (m_moveSpeed);
   m_lastDamageType = -1; // reset damage
}

bool Bot::HasHostage (void)
{
	// SyPB Pro P.42 - Game Mode support improve 
	if (GetGameMod() != MODE_BASE)
		return false;

   for (int i = 0; i < Const_MaxHostages; i++)
   {
      if (!FNullEnt (m_hostages[i]))
      {
         // don't care about dead hostages
         if (m_hostages[i]->v.health <= 0 || (pev->origin - GetEntityOrigin (m_hostages[i])).GetLength () > 600)
         {
            m_hostages[i] = null;
            continue;
         }
         return true;
      }
   }
   return false;
}

void Bot::ResetCollideState (void)
{
   m_collideTime = 0.0f;
   m_probeTime = 0.0f;

   m_collisionProbeBits = 0;
   m_collisionState = COSTATE_UNDECIDED;
   m_collStateIndex = 0;

   for (int i = 0; i < 4; i++)
      m_collideMoves[i] = 0;
}

int Bot::GetAmmo (void)
{
   if (g_weaponDefs[m_currentWeapon].ammo1 == -1)
      return 0;

   return m_ammo[g_weaponDefs[m_currentWeapon].ammo1];
}

// SyPB Pro P.28 - New Damage Msg
void Bot::TakeDamage(edict_t *inflictor, int /*damage*/, int /*armor*/, int bits)
{
	if (FNullEnt(inflictor) || inflictor == GetEntity())
		return;

	if (m_blindTime > engine->GetTime()) // SyPB Pro P.34 - Flash Fixed
		return;

	// SyPB Pro P.28 - Msg Debug
	if (!IsValidPlayer(inflictor))
		return;

	if (m_blockCheckEnemyTime > engine->GetTime())  // SyPB Pro P.30 - AMXX API
		return;

	// SyPB Pro P.23 - Bot Ai
	m_damageTime = engine->GetTime() + 0.3f;

	if (GetTeam(inflictor) == GetTeam(GetEntity()))
		return;

	m_lastDamageType = bits;
	RemoveCertainTask(TASK_HIDE);

	// SyPB Pro P.43 - Zombie Ai improve
	if (IsZombieEntity(GetEntity()))
	{
		if (FNullEnt(m_enemy) && FNullEnt(m_moveTargetEntity))
		{
			if (IsEnemyViewable(inflictor, false, true))
				SetMoveTarget(inflictor);
			else
				goto lastly;
		}

		return;
	}
	if (FNullEnt(m_enemy))
	{
	lastly:

		SetLastEnemy(inflictor);

		m_seeEnemyTime = engine->GetTime();
	}
}

void Bot::TakeBlinded (Vector fade, int alpha)
{
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind froumd the grenade.

   if (fade.x != 255 || fade.y != 255 || fade.z != 255 || alpha <= 170) // SyPB Pro P.37 - small change for flash
      return;

   SetEnemy(null);

   m_maxViewDistance = engine->RandomFloat (10.0f, 20.0f);
   m_blindTime = engine->GetTime () + static_cast <float> (alpha - 200) / 16.0f;

   if (m_skill < 80)
   {
      m_blindMoveSpeed = 0.0f;
      m_blindSidemoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;
	  m_blindCampPoint = -1;
	  return;
   }

   // SyPB Pro P.48 - Blind Action improve
   m_blindCampPoint = FindDefendWaypoint(GetEntityOrigin(GetEntity()));
   if ((g_waypoint->GetPath(m_blindCampPoint)->origin - GetEntityOrigin(GetEntity())).GetLength() >= 512.0f)
	   m_blindCampPoint = -1;

   if (m_blindCampPoint != -1)
	   return;

   m_blindMoveSpeed = -pev->maxspeed;
   m_blindSidemoveSpeed = 0.0f;

   if (engine->RandomInt(0, 100) > 50)
	   m_blindSidemoveSpeed = pev->maxspeed;
   else
	   m_blindSidemoveSpeed = -pev->maxspeed;

   if (pev->health < 85.0f)
	   m_blindMoveSpeed = -GetWalkSpeed();
   else if (m_personality == PERSONALITY_CAREFUL)
   {
	   m_blindMoveSpeed = 0.0f;
	   m_blindButton = IN_DUCK;
   }
   else
	   m_blindMoveSpeed = pev->maxspeed;
}

void Bot::ChatMessage (int type, bool isTeamSay)
{
   extern ConVar sypb_chat;

   if (g_chatFactory[type].IsEmpty () || !sypb_chat.GetBool ())
      return;

   const char *pickedPhrase = g_chatFactory[type].GetRandomElement ();

   if (IsNullString (pickedPhrase))
      return;

   PrepareChatMessage (const_cast <char *> (pickedPhrase));
   PushMessageQueue (isTeamSay ? CMENU_TEAMSAY : CMENU_SAY);
}

void Bot::DiscardWeaponForUser (edict_t *user, bool discardC4)
{
   // this function, asks bot to discard his current primary weapon (or c4) to the user that requsted it with /drop*
   // command, very useful, when i'm don't have money to buy anything... )

   if (IsAlive (user) && m_moneyAmount >= 2000 && HasPrimaryWeapon () && (GetEntityOrigin (user) - pev->origin).GetLength () <= 240)
   {
      m_aimFlags |= AIM_ENTITY;
      m_lookAt = GetEntityOrigin (user);

      if (discardC4)
      {
         SelectWeaponByName ("weapon_c4");
         FakeClientCommand (GetEntity (), "drop");

         SayText (FormatBuffer ("Here! %s, and now go and setup it!", GetEntityName (user)));
      }
      else
      {
         SelectBestWeapon ();
         FakeClientCommand (GetEntity (), "drop");

         SayText (FormatBuffer ("Here the weapon! %s, feel free to use it ;)", GetEntityName(user)));
      }

      m_pickupItem = null;
      m_pickupType = PICKTYPE_NONE;
      m_itemCheckTime = engine->GetTime () + 5.0f;

      if (m_inBuyZone)
      {
         m_buyingFinished = false;
         m_buyState = 0;

         PushMessageQueue (CMENU_BUY);
         m_nextBuyTime = engine->GetTime ();
      }
   }
   else
      SayText (FormatBuffer ("Sorry %s, i don't want discard my %s to you!", GetEntityName(user), discardC4 ? "bomb" : "weapon"));
}

void Bot::ResetDoubleJumpState (void)
{
   TaskComplete ();
   DeleteSearchNodes ();

   m_doubleJumpEntity = null;
   m_duckForJump = 0.0f;
   m_doubleJumpOrigin = nullvec;
   m_travelStartIndex = -1;
   m_jumpReady = false;
}

Vector Bot::CheckToss (const Vector &start, Vector end)
{
   // this function returns the velocity at which an object should looped from start to land near end.
   // returns null vector if toss is not feasible.

   TraceResult tr;
   float gravity = engine->GetGravity () * 0.55f;

   end = end - pev->velocity;
   end.z -= 15.0f;

   if (fabsf (end.z - start.z) > 500.0f)
      return nullvec;

   Vector midPoint = start + (end - start) * 0.5f;
   TraceHull (midPoint, midPoint + Vector (0.0f, 0.0f, 500.0f), true, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0f)
   {
      midPoint = tr.vecEndPos;
      midPoint.z = tr.pHit->v.absmin.z - 1.0f;
   }

   if ((midPoint.z < start.z) || (midPoint.z < end.z))
      return nullvec;

   float timeOne = sqrtf ((midPoint.z - start.z) / (0.5f * gravity));
   float timeTwo = sqrtf ((midPoint.z - end.z) / (0.5f * gravity));

   if (timeOne < 0.1)
      return nullvec;

   Vector nadeVelocity = (end - start) / (timeOne + timeTwo);
   nadeVelocity.z = gravity * timeOne;

   Vector apex = start + nadeVelocity * timeOne;
   apex.z = midPoint.z;

   TraceHull (start, apex, false, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0f || tr.fAllSolid)
      return nullvec;

   TraceHull (end, apex, true, head_hull, ENT (pev), &tr);

   if (tr.flFraction != 1.0f)
   {
      float dot = -(tr.vecPlaneNormal | (apex - end).Normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) // 60 degrees
         return nullvec;
   }
   return nadeVelocity * 0.777f;
}

Vector Bot::CheckThrow (const Vector &start, Vector end)
{
   // this function returns the velocity vector at which an object should be thrown from start to hit end.
   // returns null vector if throw is not feasible.

   Vector nadeVelocity = (end - start);
   TraceResult tr;

   float gravity = engine->GetGravity () * 0.55f;
   float time = nadeVelocity.GetLength () / 195.0f;

   if (time < 0.01f)
      return nullvec;
   else if (time > 2.0f)
      time = 1.2f;

   nadeVelocity = nadeVelocity * (1.0f / time);
   nadeVelocity.z += gravity * time * 0.5f;

   Vector apex = start + (end - start) * 0.5f;
   apex.z += 0.5f * gravity * (time * 0.5f) * (time * 0.5f);

   TraceHull (start, apex, false, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0f)
      return nullvec;

   TraceHull (end, apex, true, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0f || tr.fAllSolid)
   {
      float dot = -(tr.vecPlaneNormal | (apex - end).Normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f)
         return nullvec;
   }
   return nadeVelocity * 0.7793f;
}

Vector Bot::CheckBombAudible (void)
{
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!g_bombPlanted || (GetCurrentTask ()->taskID == TASK_ESCAPEFROMBOMB))
      return nullvec; // reliability check

   Vector bombOrigin = g_waypoint->GetBombPosition ();

   if (m_skill > 90)
      return bombOrigin;
   
   float timeElapsed = ((engine->GetTime () - g_timeBombPlanted) / engine->GetC4TimerTime ()) * 100;
   float desiredRadius = 768.0f;

   // start the manual calculations
   if (timeElapsed > 85.0f)
      desiredRadius = 4096.0f;
   else if (timeElapsed > 68.0f)
      desiredRadius = 2048.0f;
   else if (timeElapsed > 52.0f)
      desiredRadius = 1280.0f;
   else if (timeElapsed > 28.0f)
      desiredRadius = 1024.0f;

   // we hear bomb if length greater than radius
   if (desiredRadius < (pev->origin - bombOrigin).GetLength2D ())
      return bombOrigin;

   return nullvec;
}

void Bot::MoveToVector (Vector to)
{
   if (to == nullvec)
      return;

   FindPath (m_currentWaypointIndex, g_waypoint->FindNearest (to), 2);
}

void Bot::RunPlayerMovement(void)
{
	// the purpose of this function is to compute, according to the specified computation
	// method, the msec value which will be passed as an argument of pfnRunPlayerMove. This
	// function is called every frame for every bot, since the RunPlayerMove is the function
	// that tells the engine to put the bot character model in movement. This msec value
	// tells the engine how long should the movement of the model extend inside the current
	// frame. It is very important for it to be exact, else one can experience bizarre
	// problems, such as bots getting stuck into each others. That's because the model's
	// bounding boxes, which are the boxes the engine uses to compute and detect all the
	// collisions of the model, only exist, and are only valid, while in the duration of the
	// movement. That's why if you get a pfnRunPlayerMove for one bot that lasts a little too
	// short in comparison with the frame's duration, the remaining time until the frame
	// elapses, that bot will behave like a ghost : no movement, but bullets and players can
	// pass through it. Then, when the next frame will begin, the stucking problem will arise !

	// SyPB Pro P.41 - Run Player Move
	m_msecVal = static_cast <uint8_t> ((engine->GetTime() - m_msecInterval) * 1000.0f);
	m_msecInterval = engine->GetTime();

	// SyPB Pro P.48 - Run Player Move
	/*
	// SyPB Pro P.45 - Run Player Move
	if (m_msecVal < 1)
		m_msecVal = 1;

	if (m_msecVal > 100)
		m_msecVal = 100; */

	(*g_engfuncs.pfnRunPlayerMove) (GetEntity(), 
		m_moveAnglesForRunMove, m_moveSpeedForRunMove, m_strafeSpeedForRunMove, 0.0f, 
		static_cast <unsigned short> (pev->button), 
		0, 
		// static_cast <uint8> (pev->impulse),
		static_cast <uint8_t> (m_msecVal));
}


void Bot::CheckBurstMode (float distance)
{
   // this function checks burst mode, and switch it depending distance to to enemy.

   if (HasShield ())
      return; // no checking when shiled is active

   // if current weapon is glock, disable burstmode on long distances, enable it else
   if (m_currentWeapon == WEAPON_GLOCK18 && distance < 300.0f && m_weaponBurstMode == BURST_DISABLED)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_GLOCK18 && distance >= 30.0f && m_weaponBurstMode == BURST_ENABLED)
      pev->button |= IN_ATTACK2;

   // if current weapon is famas, disable burstmode on short distances, enable it else
   if (m_currentWeapon == WEAPON_FAMAS && distance > 400.0f && m_weaponBurstMode == BURST_DISABLED)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_FAMAS && distance <= 400.0f && m_weaponBurstMode == BURST_ENABLED)
      pev->button |= IN_ATTACK2;
}

void Bot::CheckSilencer (void)
{
   if ((m_currentWeapon == WEAPON_USP && m_skill < 90) || m_currentWeapon == WEAPON_M4A1 && !HasShield ())
   {
      int random = (m_personality == PERSONALITY_RUSHER ? 35 : 65);

      // aggressive bots don't like the silencer
      if (engine->RandomInt (1, 100) <= (m_currentWeapon == WEAPON_USP ? random / 3 : random))
      {
         if (pev->weaponanim > 6) // is the silencer not attached...
            pev->button |= IN_ATTACK2; // attach the silencer
      }
      else
      {
         if (pev->weaponanim <= 6) // is the silencer attached...
            pev->button |= IN_ATTACK2; // detach the silencer
      }
   }
}

float Bot::GetBombTimeleft (void)
{
   if (!g_bombPlanted)
      return 0.0f;

   float timeLeft = ((g_timeBombPlanted + engine->GetC4TimerTime ()) - engine->GetTime ());

   if (timeLeft < 0.0f)
      return 0.0f;

   return timeLeft;
}

float Bot::GetEstimatedReachTime (void)
{
	// SyPB Pro P.48 - Zombie Ai improve
	if (m_damageTime < engine->GetTime() && m_damageTime != 0.0f && !m_isStuck && 
		IsZombieEntity (GetEntity ()))
		return engine->GetTime();

	float estimatedTime = 5.0f; // time to reach next waypoint

	// calculate 'real' time that we need to get from one waypoint to another
	if (m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints && m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
	{
		float distance = (g_waypoint->GetPath(m_prevWptIndex[0])->origin - g_waypoint->GetPath(m_currentWaypointIndex)->origin).GetLength();

		// caclulate estimated time
		if (pev->maxspeed <= 0.0f)
			estimatedTime = 5.0f * distance / 240.0f;
		else
			estimatedTime = 5.0f * distance / pev->maxspeed;

		// check for special waypoints, that can slowdown our movement
		if ((g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_CROUCH) || (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LADDER) || (pev->button & IN_DUCK))
			estimatedTime *= 3.0f;

		// check for too low values
		if (estimatedTime < 3.0f)
			estimatedTime = 3.0f;

		// check for too high values
		if (estimatedTime > 8.0f)
			estimatedTime = 8.0f;
	}
	return estimatedTime;
}

bool Bot::OutOfBombTimer (void)
{
   if (m_currentWaypointIndex < 0 || m_currentWaypointIndex >= g_numWaypoints || ((g_mapType & MAP_DE) && (m_hasProgressBar || GetCurrentTask ()->taskID == TASK_ESCAPEFROMBOMB)))
      return false; // if CT bot already start defusing, or already escaping, return false

   // calculate left time
   float timeLeft = GetBombTimeleft ();

   // if time left greater than 13, no need to do other checks
   if (timeLeft > 13)
      return false;

   Vector bombOrigin = g_waypoint->GetBombPosition ();

   // for terrorist, if timer is lower than eleven seconds, return true
   if (static_cast <int> (timeLeft) < 12 && GetTeam (GetEntity ()) == TEAM_TERRORIST && (bombOrigin - pev->origin).GetLength () < 1000)
      return true;

   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      Bot *bot = null; // temporaly pointer to bot

      // search players with defuse kit
      if ((bot = g_botManager->GetBot (i)) != null && GetTeam (bot->GetEntity ()) == TEAM_COUNTER && bot->m_hasDefuser && (bombOrigin - bot->pev->origin).GetLength () < 500)
      {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   float reachTime = g_waypoint->GetTravelTime (pev->maxspeed, g_waypoint->GetPath (m_currentWaypointIndex)->origin, bombOrigin);

   // for counter-terrorist check alos is we have time to reach position plus average defuse time
   if ((timeLeft < reachTime + 10.0f && !m_hasDefuser && !hasTeammatesWithDefuserKit) || (timeLeft < reachTime + 5.0f && m_hasDefuser))
      return true;

   if (m_hasProgressBar && IsOnFloor () && (m_hasDefuser ? 6.0f : 11.0f > GetBombTimeleft ()))
      return true;

   return false; // return false otherwise
}

// SyPB Pro P.48 - React Sound improve
void Bot::ReactOnSound (void)
{
	if (GetGameMod() != MODE_BASE)
		return;

	// SyPB Pro P.30 - AMXX API
	if (m_blockCheckEnemyTime > engine->GetTime())
		return;

	if (!FNullEnt(m_enemy))
		return;

	int ownIndex = GetIndex();
	if (g_clients[ownIndex].timeSoundLasting <= engine->GetTime())
		return;

	edict_t *player = null;

	float hearEnemyDistance = 0.0f;
	int hearEnemyIndex = -1;

	// loop through all enemy clients to check for hearable stuff
	for (int i = 0; i < engine->GetMaxClients(); i++)
	{
		if (!(g_clients[i].flags & CFLAG_USED) || !(g_clients[i].flags & CFLAG_ALIVE) || 
			g_clients[i].ent == GetEntity() || GetTeam (GetEntity ()) == GetTeam (g_clients[i].ent))
			continue;

		float distance = (g_clients[i].soundPosition - pev->origin).GetLength();
		float hearingDistance = g_clients[i].hearingDistance;

		if (distance > hearingDistance || hearingDistance >= 2048.0f)
			continue;

		hearEnemyIndex = i;
		hearEnemyDistance = distance;
	}

	if (hearEnemyIndex >= 0)
		player = g_clients[hearEnemyIndex].ent;

	// did the bot hear someone ?
	if (!FNullEnt(player))
	{
		// change to best weapon if heard something
		if (!(m_states & STATE_SEEINGENEMY) && IsOnFloor() && m_currentWeapon != WEAPON_C4 && m_currentWeapon != WEAPON_HEGRENADE && m_currentWeapon != WEAPON_SMGRENADE && m_currentWeapon != WEAPON_FBGRENADE && !sypb_knifemode.GetBool())
			SelectBestWeapon();

		m_heardSoundTime = engine->GetTime() + 5.0f;
		m_states |= STATE_HEARENEMY;

		m_aimFlags |= AIM_LASTENEMY;

		// didn't bot already have an enemy ? take this one...
		if (m_lastEnemyOrigin == nullvec || m_lastEnemy == null)
			SetLastEnemy(player);
		else // bot had an enemy, check if it's the heard one
		{
			if (player == m_lastEnemy)
			{
				// bot sees enemy ? then bail out !
				if (m_states & STATE_SEEINGENEMY)
					return;

				m_lastEnemyOrigin = GetEntityOrigin(player);
			}
			else
			{
				// if bot had an enemy but the heard one is nearer, take it instead
				float distance = (m_lastEnemyOrigin - pev->origin).GetLength();
				if (distance <= (GetEntityOrigin(player) - pev->origin).GetLength() || m_seeEnemyTime + 2.0f >= engine->GetTime())
					return;

				SetLastEnemy(player);
				return;
			}
		}

		// check if heard enemy can be seen
		// SyPB Pro P.30 - small change
		if (m_lastEnemy == player && m_seeEnemyTime + 4.0f > engine->GetTime() && m_skill >= 60 &&
			(engine->RandomInt(1, 100) < g_skillTab[m_skill / 20].heardShootThruProb) &&
			IsShootableThruObstacle(player))
		{
			SetEnemy(player);
			SetLastEnemy(player);

			m_states |= STATE_SEEINGENEMY;
			m_seeEnemyTime = engine->GetTime();
		}
	}
}

// SyPB Pro P.47 - Breakable improve
bool Bot::IsShootableBreakable(edict_t *ent)
{
	if (FNullEnt(ent))
		return false;

	if (FClassnameIs(ent, "func_breakable") ||
		(FClassnameIs(ent, "func_pushable") && (ent->v.spawnflags & SF_PUSH_BREAKABLE)))
	{
		if (ent->v.takedamage != DAMAGE_NO && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY))
			return (ent->v.movetype == MOVETYPE_PUSH || ent->v.movetype == MOVETYPE_PUSHSTEP);
	}

	return false;
}

void Bot::EquipInBuyzone (int iBuyCount)
{
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   static float lastEquipTime = 0.0f;

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (lastEquipTime + 15.0f < engine->GetTime () && m_inBuyZone && g_timeRoundStart + engine->RandomFloat (10.0f, 20.0f) + engine->GetBuyTime () < engine->GetTime () && !g_bombPlanted && m_moneyAmount > 1500)
   {
      m_buyingFinished = false;
      m_buyState = iBuyCount;

      // push buy message
      PushMessageQueue (CMENU_BUY);

      m_nextBuyTime = engine->GetTime ();
      lastEquipTime = engine->GetTime ();
   }
}

bool Bot::IsBombDefusing (Vector bombOrigin)
{
   // this function finds if somebody currently defusing the bomb.

   bool defusingInProgress = false;

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      Bot *bot = g_botManager->GetBot (i);

      if (bot == null || bot == this)
         continue; // skip invalid bots

      if (GetTeam (GetEntity ()) != GetTeam (bot->GetEntity ()) || bot->GetCurrentTask ()->taskID == TASK_ESCAPEFROMBOMB)
         continue; // skip other mess

      if ((bot->pev->origin - bombOrigin).GetLength () < 80 && (bot->GetCurrentTask ()->taskID == TASK_DEFUSEBOMB || bot->m_hasProgressBar))
      {
         defusingInProgress = true;
         break;
      }

      // take in account peoples too
      if (defusingInProgress || !(g_clients[i].flags & CFLAG_USED) || !(g_clients[i].flags & CFLAG_ALIVE) || g_clients[i].team != GetTeam (GetEntity ()) || IsValidBot (g_clients[i].ent))
         continue;

      if ((GetEntityOrigin (g_clients[i].ent) - bombOrigin).GetLength () < 80)
      {
         defusingInProgress = true;
         break;
      }
   }
   return defusingInProgress;
}
