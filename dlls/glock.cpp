/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

#include "UserMessages.h"

extern bool CanAttack(float attack_time, float curtime, bool isPredicted);

LINK_ENTITY_TO_CLASS(weapon_glock, CGlock);
LINK_ENTITY_TO_CLASS(weapon_9mmhandgun, CGlock);
LINK_ENTITY_TO_CLASS_SPECIAL(weapon_silencer, CGlock, CGlock_SpawnSilenced);

void CGlock_SpawnSilenced(entvars_s* pev)
{
	pev->body = 1; 
}

void CGlock::Spawn()
{
	pev->classname = MAKE_STRING("weapon_9mmhandgun"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_GLOCK;
	
	if (pev->body != 0)
		SET_MODEL(ENT(pev), "models/w_silencer.mdl");
	else
		SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");

	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE;

	FallInit(); // get ready to fall down.
	
	if (pev->body != 0)
		SetTouch(&CGlock::DefaultTouch);
}

void CGlock::DefaultTouch(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer())
		return;

	auto pPlayer = reinterpret_cast<CBasePlayer*>(pOther);

	if (!pPlayer->HasWeaponBit(WEAPON_GLOCK_SILENCER) && pev->body != 0)
	{
		SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");
		pPlayer->SetWeaponBit(WEAPON_GLOCK_SILENCER);
		m_bSilencer = true;
	
		if (pPlayer->HasWeaponBit(WEAPON_GLOCK))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
			WRITE_BYTE(WEAPON_GLOCK);
			MESSAGE_END();
		}

		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);
	}

	CBasePlayerWeapon::DefaultTouch(pOther);
}

void CGlock::Precache()
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");
	
	PRECACHE_MODEL("models/w_silencer.mdl");

	m_iShell = PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND("weapons/pl_gun1.wav"); //silenced handgun
	PRECACHE_SOUND("weapons/pl_gun2.wav"); //silenced handgun
	PRECACHE_SOUND("weapons/pl_gun3.wav"); //handgun

	m_usFireGlock1 = PRECACHE_EVENT(1, "events/glock1.sc");
	m_usFireGlock2 = PRECACHE_EVENT(1, "events/glock2.sc");
}

bool CGlock::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP + 1;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;

	return true;
}

void CGlock::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

bool CGlock::Deploy()
{
	pev->body = m_bSilencer ? 1 : 0;
	return DefaultDeploy("models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded", pev->body);
}

void CGlock::AddSilencer()
{
	if (!m_pPlayer->HasWeaponBit(WEAPON_GLOCK_SILENCER))
		return;

	if (m_bSilencer)
	{
		SendWeaponAnim(GLOCK_HOLSTER);
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0f;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0f;
	}
	else
	{
		SendWeaponAnim(GLOCK_ADD_SILENCER);
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.8f;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 3.3f;
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.0f;

	m_iSilencerState = 1;
}

void CGlock::SecondaryAttack()
{
	GlockFire(0.1, 0.2, false);
}

void CGlock::PrimaryAttack()
{
	GlockFire(0.01, 0.3, true);
}

void CGlock::GlockFire(float flSpread, float flCycleTime, bool fUseAutoAim)
{
	if (m_iClip <= 0)
	{
		//if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.2);
		}

		return;
	}

	m_iClip--;


	int flags;

#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	// silenced
	if (pev->body == 1)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;


		m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	}

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming;

	if (fUseAutoAim)
	{
		vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);
	}
	else
	{
		vecAiming = gpGlobals->v_forward;
	}

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer(1, vecSrc, vecAiming, Vector(flSpread, flSpread, flSpread), 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed);

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), fUseAutoAim ? m_usFireGlock1 : m_usFireGlock2, 0.0, g_vecZero, g_vecZero, vecDir.x, vecDir.y, 0, 0, (m_iClip == 0) ? 1 : 0, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}


void CGlock::Reload()
{
	if (m_pPlayer->ammo_9mm <= 0)
		return;

	bool iResult;

	if (m_iClip == 0)
		iResult = DefaultReload(iMaxClip(), GLOCK_RELOAD, 1.5);
	else
		iResult = DefaultReload(iMaxClip(), GLOCK_RELOAD_NOT_EMPTY, 1.5);

	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
}


void CGlock::ItemPostFrame()
{
	// 17 + 1 reload
	if ((m_fInReload) && (m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase()))
	{
		int iMax = iMaxClip();

		if (m_iClip == 0)
			--iMax;

		// complete the reload.
		int j = V_min(iMax - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = false;
	}

	if (m_iSilencerState != 0 && m_iSilencerState != 5)
	{
		if (!m_bSilencer)
		{
			if (m_iSilencerState == 1)
			{
				m_iSilencerState = 2;
				SetBody(1);
				m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 2.1f;
			}
			else if (m_iSilencerState == 2)
			{
				m_iSilencerState = 0;
				m_bSilencer = true;
			}
		}
		else
		{
			if (m_iSilencerState != 0)
			{
				m_iSilencerState = 0;
				m_bSilencer = false;
				SendWeaponAnim(GLOCK_DRAW);
				SetBody(0);
				m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.3f;
			}
		}
	}

	if ((m_pPlayer->pev->button & IN_ATTACK) == 0)
	{
		m_flLastFireTime = 0.0f;
	}

	if ((m_pPlayer->pev->button & IN_ALT1) != 0 && CanAttack(m_flNextSecondaryAttack, gpGlobals->time, UseDecrement()))
	{
		m_pPlayer->TabulateAmmo();
		AddSilencer();
		m_pPlayer->pev->button &= ~IN_ALT1;
	}
	else
		CBasePlayerWeapon::ItemPostFrame();
}

void CGlock::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	m_iSilencerState = 0;

	SendWeaponAnim(GLOCK_HOLSTER);
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.98f;
}

void CGlock::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

//	Prevent client side jankiness
#ifdef CLIENT_DLL
	SetBody(m_bSilencer ? 1 : 0);
#endif
	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0.0, 1.0);

		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim(iAnim);
	}
}








class CGlockAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmclip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_GLOCKCLIP_GIVE, "9mm", _9MM_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_glockclip, CGlockAmmo);
LINK_ENTITY_TO_CLASS(ammo_9mmclip, CGlockAmmo);
