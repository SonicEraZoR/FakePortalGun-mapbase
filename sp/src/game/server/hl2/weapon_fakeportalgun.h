//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_FAKEPORTALGUN_H
#define WEAPON_FAKEPORTALGUN_H

#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"
#include "Sprite.h"
#include "npcevent.h"
#include "beam_shared.h"

class CWeaponFakePortalGun;
class RocketTrail;
 
//###########################################################################
//	>> CFakePortalGunProjectile		(missile launcher class is below this one!)
//###########################################################################
class CFakePortalGunProjectile : public CBaseCombatCharacter
{
	DECLARE_CLASS( CFakePortalGunProjectile, CBaseCombatCharacter );

public:
	static const int EXPLOSION_RADIUS = 200;

	CFakePortalGunProjectile();
	~CFakePortalGunProjectile();

#ifdef HL1_DLL
	Class_T Classify( void ) { return CLASS_NONE; }
#else
	Class_T Classify( void ) { return CLASS_MISSILE; }
#endif
	
	void	Spawn( void );
	void	Precache( void );
	void	MissileTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	ShotDown( void );
	void	AccelerateThink( void );
	void	AugerThink( void );
	void	IgniteThink( void );
	void	SetGracePeriod( float flGracePeriod );

	int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void	Event_Killed( const CTakeDamageInfo &info );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;

	CHandle<CWeaponFakePortalGun>		m_hOwner;

	static CFakePortalGunProjectile *Create( const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner, bool bluePortal );

	void CreateDangerSounds( bool bState ){ m_bCreateDangerSounds = bState; }

	static void AddCustomDetonator( CBaseEntity *pEntity, float radius, float height = -1 );
	static void RemoveCustomDetonator( CBaseEntity *pEntity );

	bool	m_bBluePortal;

protected:
	virtual void DoExplosion();	
	virtual int AugerHealth() { return m_iMaxHealth - 20; }

	float					m_flAugerTime;		// Amount of time to auger before blowing up anyway
	float					m_flMarkDeadTime;
	float					m_flDamage;

	struct CustomDetonator_t
	{
		EHANDLE hEntity;
		float radiusSq;
		float halfHeight;
	};

	static CUtlVector<CustomDetonator_t> gm_CustomDetonators;

private:
	float					m_flGracePeriodEndsAt;
	bool					m_bCreateDangerSounds;

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// FakePortalGun
//-----------------------------------------------------------------------------
class CWeaponFakePortalGun : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponFakePortalGun, CBaseHLCombatWeapon );
public:

	CWeaponFakePortalGun();
	~CWeaponFakePortalGun();

	DECLARE_SERVERCLASS();

	void	Precache( void );

	void	PrimaryAttack( void );
	void	SecondaryAttack(void);
	virtual float GetFireRate( void ) { return 1; };
	void	ItemPostFrame( void );

	void	Activate( void );

	bool	Deploy( void );
	bool	Reload( void );

	int		GetMinBurst() { return 1; }
	int		GetMaxBurst() { return 1; }
	float	GetMinRestTime() { return 4.0; }
	float	GetMaxRestTime() { return 4.0; }

	bool	WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	int		WeaponRangeAttack1Condition( float flDot, float flDist );

	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	NotifyRocketDied( void );

	bool	HasAnyAmmo( void );

	void	Equip(CBaseCombatCharacter *pOwner);

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}
	
	CBaseEntity *GetMissile( void ) { return m_hMissile; }

	bool CanFirePortal1(void) { return m_bCanFirePortal1; }
	bool CanFirePortal2(void) { return m_bCanFirePortal2; }

	void SetCanFirePortal1(bool can_fire) { m_bCanFirePortal1 = can_fire; }
	void SetCanFirePortal2(bool can_fire) { m_bCanFirePortal2 = can_fire; }

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
	
protected:

	bool				m_bInitialStateUpdate;
	CHandle<CFakePortalGunProjectile>	m_hMissile;

private:
	bool					m_bCanFirePortal1 = true;	// Is able to use primary fire
	bool					m_bCanFirePortal2 = true;	// Is able to use secondary fire
};

#endif // WEAPON_FAKEPORTALGUN_H
