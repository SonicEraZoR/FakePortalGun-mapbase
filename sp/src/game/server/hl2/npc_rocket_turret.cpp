//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_senses.h"
#include "ai_memory.h"
#include "engine/IEngineSound.h"
#include "ammodef.h"
#include "Sprite.h"
#include "hl2/hl2_player.h"
#include "soundenvelope.h"
#include "explode.h"
#include "IEffects.h"
#include "animation.h"
#include "basehlcombatweapon_shared.h"
#include "iservervehicle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//Debug visualization
extern ConVar	g_debug_turret_ceiling;

#ifdef MAPBASE
#define ROCKET_TURRET_MODEL		GetTurretModel()
#else
#define	ROCKET_TURRET_MODEL		"models/props_bts/rocket_sentry.mdl"
#endif
#define ROCKET_TURRET_GLOW_SPRITE	"sprites/glow1.vmt"
/* // we now inherit these from the ai_basenpc baseclass
#define ROCKET_TURRET_BC_YAW		"aim_yaw"
#define ROCKET_TURRET_BC_PITCH		"aim_pitch"
*/
#define	ROCKET_TURRET_RANGE		1500
#define ROCKET_TURRET_SPREAD		VECTOR_CONE_2DEGREES
#define	ROCKET_TURRET_MAX_WAIT		5
#define	ROCKET_TURRET_PING_TIME	1.0f	//LPB!!

#define	ROCKET_TURRET_VOICE_PITCH_LOW	45
#define	ROCKET_TURRET_VOICE_PITCH_HIGH	100

//Aiming variables
#define	ROCKET_TURRET_MAX_NOHARM_PERIOD	0.0f
#define	ROCKET_TURRET_MAX_GRACE_PERIOD		3.0f

//Spawnflags
#define SF_ROCKET_TURRET_AUTOACTIVATE		0x00000020
#define SF_ROCKET_TURRET_STARTINACTIVE		0x00000040
#define SF_ROCKET_TURRET_NEVERRETIRE		0x00000080
#define SF_ROCKET_TURRET_OUT_OF_AMMO		0x00000100
#ifdef MAPBASE
#define SF_ROCKET_TURRET_NO_SPRITE			0x00000400
#endif

//Heights
#define	ROCKET_TURRET_RETRACT_HEIGHT	24
#define	ROCKET_TURRET_DEPLOY_HEIGHT	64

//Activities
int ACT_ROCKET_SENTRY_OPEN;
int ACT_ROCKET_SENTRY_CLOSE;
int ACT_ROCKET_SENTRY_OPEN_IDLE;
int ACT_ROCKET_SENTRY_CLOSED_IDLE;
int ACT_ROCKET_SENTRY_FIRE;
int ACT_ROCKET_SENTRY_DRYFIRE;

//Turret states
enum turretState_e
{
	TURRET_SEARCHING,
	TURRET_AUTO_SEARCHING,
	TURRET_ACTIVE,
	TURRET_DEPLOYING,
	TURRET_RETIRING,
	TURRET_DEAD,
};

//Eye states
enum eyeState_t
{
	TURRET_EYE_SEE_TARGET,			//Sees the target, bright and big
	TURRET_EYE_SEEKING_TARGET,		//Looking for a target, blinking (bright)
	TURRET_EYE_DORMANT,				//Not active
	TURRET_EYE_DEAD,				//Completely invisible
	TURRET_EYE_DISABLED,			//Turned off, must be reactivated before it'll deploy again (completely invisible)
};

//
// Rocket Turret
//

class CNPC_RocketTurret : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_RocketTurret, CAI_BaseNPC );
public:
	
	CNPC_RocketTurret( void );
	~CNPC_RocketTurret( void );

	void	Precache( void );
	void	Spawn( void );

	// Think functions
	void	Retire( void );
	void	Deploy( void );
	void	ActiveThink( void );
	void	SearchThink( void );
	void	AutoSearchThink( void );
	void	DeathThink( void );

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
#ifdef MAPBASE
	void	InputDepleteAmmo( inputdata_t &inputdata );
	void	InputRestoreAmmo( inputdata_t &inputdata );
	void	InputCreateSprite( inputdata_t &inputdata );
	void	InputDestroySprite( inputdata_t &inputdata );

	virtual void	StopLoopingSounds( void ) { StopSound(GetMoveSound()); }
#endif

	void	SetLastSightTime();
	
	float	MaxYawSpeed( void );

	int		OnTakeDamage( const CTakeDamageInfo &inputInfo );

	virtual bool CanBeAnEnemyOf( CBaseEntity *pEnemy );

	Class_T	Classify( void ) 
	{
		if( m_bEnabled ) 
			return CLASS_COMBINE;

		return CLASS_NONE;
	}
	
	bool	FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );

	Vector	EyeOffset( Activity nActivity ) 
	{
		Vector vecEyeOffset(0,0,-64);
		GetEyePosition( GetModelPtr(), vecEyeOffset );
		return vecEyeOffset;
	}

	Vector	EyePosition( void )
	{
		return GetAbsOrigin() + EyeOffset(GetActivity());
	}

	Vector	GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget ) 
	{
		return VECTOR_CONE_5DEGREES * ((CBaseHLCombatWeapon::GetDefaultProficiencyValues())[ WEAPON_PROFICIENCY_PERFECT ].spreadscale);
	}

protected:

#ifdef MAPBASE
	virtual const char *GetTurretModel() { return "models/props_bts/rocket_sentry.mdl"; }

	virtual const char *GetRetireSound()	{ return "NPC_CeilingTurret.Retire"; }
	virtual const char *GetDeploySound()	{ return "NPC_CeilingTurret.Deploy"; }
	virtual const char *GetMoveSound()		{ return "NPC_CeilingTurret.Move"; }
	virtual const char *GetActiveSound()	{ return "NPC_CeilingTurret.Active"; }
	virtual const char *GetAlertSound()		{ return "NPC_CeilingTurret.Alert"; }
	virtual const char *GetShootSound()		{ return "NPC_CeilingTurret.ShotSounds"; }
	virtual const char *GetPingSound()		{ return "NPC_CeilingTurret.Ping"; }
	virtual const char *GetDieSound()		{ return "NPC_CeilingTurret.Die"; }

	virtual float		GetFireRate(bool bFightingPlayer = false)		{ return bFightingPlayer ? 0.5f : 0.1f; }

	virtual void		SetIdleGoalAngles()		{ m_vecGoalAngles = GetAbsAngles(); }
#endif
	
#ifdef MAPBASE
	virtual bool	PreThink( turretState_e state );
	void	DryFire( void );
	const char *GetTracerType( void ) { return "AR2Tracer"; }
#else
	bool	PreThink( turretState_e state );
#endif
	void	Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy );
	void	SetEyeState( eyeState_t state );
	void	Ping( void );	
	void	Toggle( void );
	void	Enable( void );
	void	Disable( void );
	void	SpinUp( void );
	void	SpinDown( void );
#ifdef MAPBASE
	virtual
#endif
	void	SetHeight( float height );

#ifdef MAPBASE
	virtual
#endif
	bool	UpdateFacing( void );

	int		m_iAmmoType;
	int		m_iMinHealthDmg;

	bool	m_bAutoStart;
	bool	m_bActive;		//Denotes the turret is deployed and looking for targets
	bool	m_bBlinkState;
	bool	m_bEnabled;		//Denotes whether the turret is able to deploy or not
	
	float	m_flShotTime;
	float	m_flLastSight;
	float	m_flPingTime;

	QAngle	m_vecGoalAngles;

	CSprite	*m_pEyeGlow;

	COutputEvent m_OnDeploy;
	COutputEvent m_OnRetire;
#ifndef MAPBASE
	COutputEvent m_OnTipped;
#endif

	DECLARE_DATADESC();
};

//Datatable
BEGIN_DATADESC( CNPC_RocketTurret )

	DEFINE_FIELD( m_iAmmoType,		FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iMinHealthDmg, FIELD_INTEGER, "minhealthdmg" ),
	DEFINE_FIELD( m_bAutoStart,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bActive,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bBlinkState,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bEnabled,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flShotTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flLastSight,	FIELD_TIME ),
	DEFINE_FIELD( m_flPingTime,		FIELD_TIME ),
	DEFINE_FIELD( m_vecGoalAngles,	FIELD_VECTOR ),
	DEFINE_FIELD( m_pEyeGlow,		FIELD_CLASSPTR ),
#ifdef MAPBASE
	DEFINE_INPUT( m_flFieldOfView,	FIELD_FLOAT, "FieldOfView" ),
#endif

	DEFINE_THINKFUNC( Retire ),
	DEFINE_THINKFUNC( Deploy ),
	DEFINE_THINKFUNC( ActiveThink ),
	DEFINE_THINKFUNC( SearchThink ),
	DEFINE_THINKFUNC( AutoSearchThink ),
	DEFINE_THINKFUNC( DeathThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
#ifdef MAPBASE
	DEFINE_INPUTFUNC( FIELD_VOID, "DepleteAmmo", InputDepleteAmmo ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RestoreAmmo", InputRestoreAmmo ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CreateSprite", InputCreateSprite ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DestroySprite", InputDestroySprite ),
#endif

	DEFINE_OUTPUT( m_OnDeploy, "OnDeploy" ),
	DEFINE_OUTPUT( m_OnRetire, "OnRetire" ),
#ifndef MAPBASE
	DEFINE_OUTPUT( m_OnTipped, "OnTipped" ),
#endif

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_rocket_turret, CNPC_RocketTurret);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CNPC_RocketTurret::CNPC_RocketTurret( void )
{
	m_bActive			= false;
	m_pEyeGlow			= NULL;
	m_iAmmoType			= -1;
	m_iMinHealthDmg		= 0;
	m_bAutoStart		= false;
	m_flPingTime		= 0;
	m_flShotTime		= 0;
	m_flLastSight		= 0;
	m_bBlinkState		= false;
	m_bEnabled			= false;

	m_vecGoalAngles.Init();
}

CNPC_RocketTurret::~CNPC_RocketTurret( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Precache( void )
{
	PrecacheModel( DefaultOrCustomModel( ROCKET_TURRET_MODEL ) );	
	PrecacheModel( ROCKET_TURRET_GLOW_SPRITE );

	// Activities
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_OPEN );
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_CLOSE );
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_CLOSED_IDLE );
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_OPEN_IDLE );
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_FIRE );
	ADD_CUSTOM_ACTIVITY( CNPC_RocketTurret, ACT_ROCKET_SENTRY_DRYFIRE );

#ifdef MAPBASE
	PrecacheScriptSound( GetRetireSound() );
	PrecacheScriptSound( GetDeploySound() );
	PrecacheScriptSound( GetMoveSound() );
	PrecacheScriptSound( GetActiveSound() );
	PrecacheScriptSound( GetAlertSound() );
	PrecacheScriptSound( GetShootSound() );
	PrecacheScriptSound( GetPingSound() );
	PrecacheScriptSound( GetDieSound() );
#else
	PrecacheScriptSound( "NPC_CeilingTurret.Retire" );
	PrecacheScriptSound( "NPC_CeilingTurret.Deploy" );
	PrecacheScriptSound( "NPC_CeilingTurret.Move" );
	PrecacheScriptSound( "NPC_CeilingTurret.Active" );
	PrecacheScriptSound( "NPC_CeilingTurret.Alert" );
	PrecacheScriptSound( "NPC_CeilingTurret.ShotSounds" );
	PrecacheScriptSound( "NPC_CeilingTurret.Ping" );
	PrecacheScriptSound( "NPC_CeilingTurret.Die" );
#endif

	PrecacheScriptSound( "NPC_FloorTurret.DryFire" );
	
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the entity
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Spawn( void )
{ 
	Precache();

	SetModel( DefaultOrCustomModel( ROCKET_TURRET_MODEL ) );
	
	BaseClass::Spawn();

	m_HackedGunPos	= Vector( 0, 0, 12.75 );
	SetViewOffset( EyeOffset( ACT_IDLE ) );
#ifndef MAPBASE // We use this as a keyvalue now.
	m_flFieldOfView	= 0.0f;
#endif
	m_takedamage	= DAMAGE_YES;
#ifdef MAPBASE
	if (m_iHealth == 0)
		m_iHealth = 1000;
#else
	m_iHealth		= 1000;
#endif
	m_bloodColor	= BLOOD_COLOR_MECH;
	
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetHeight( ROCKET_TURRET_RETRACT_HEIGHT );

	AddFlag( FL_AIMTARGET );
	AddEFlags( EFL_NO_DISSOLVE );

	SetPoseParameter( m_poseAim_Yaw, 0 );
	SetPoseParameter( m_poseAim_Pitch, 0 );

	m_iAmmoType = GetAmmoDef()->Index( "AR2" );

	//Create our eye sprite
#ifdef MAPBASE
	if (!HasSpawnFlags(SF_ROCKET_TURRET_NO_SPRITE))
	{
#endif
	m_pEyeGlow = CSprite::SpriteCreate( ROCKET_TURRET_GLOW_SPRITE, GetLocalOrigin(), false );
	m_pEyeGlow->SetTransparency( kRenderTransAdd, 255, 0, 0, 128, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( this, 2 );
#ifdef MAPBASE
	}
#endif

	//Set our autostart state
	m_bAutoStart = !!( m_spawnflags & SF_ROCKET_TURRET_AUTOACTIVATE );
	m_bEnabled	 = ( ( m_spawnflags & SF_ROCKET_TURRET_STARTINACTIVE ) == false );

	//Do we start active?
	if ( m_bAutoStart && m_bEnabled )
	{
		SetThink( &CNPC_RocketTurret::AutoSearchThink );
		SetEyeState( TURRET_EYE_DORMANT );
	}
	else
	{
		SetEyeState( TURRET_EYE_DISABLED );
	}

	//Stagger our starting times
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.1f, 0.3f ) );

	// Don't allow us to skip animation setup because our attachments are critical to us!
	SetBoneCacheFlags( BCF_NO_ANIMATION_SKIP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_RocketTurret::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	if ( !m_takedamage )
		return 0;

	CTakeDamageInfo info = inputInfo;

	if ( m_bActive == false )
		info.ScaleDamage( 0.1f );

	// If attacker can't do at least the min required damage to us, don't take any damage from them
	if ( info.GetDamage() < m_iMinHealthDmg )
		return 0;

	m_iHealth -= info.GetDamage();

	if ( m_iHealth <= 0 )
	{
		m_iHealth = 0;
		m_takedamage = DAMAGE_NO;

		RemoveFlag( FL_NPC ); // why are they set in the first place???

		//FIXME: This needs to throw a ragdoll gib or something other than animating the retraction -- jdw

		ExplosionCreate( GetAbsOrigin(), GetLocalAngles(), this, 100, 100, false );
		SetThink( &CNPC_RocketTurret::DeathThink );

#ifdef MAPBASE
		StopSound( GetAlertSound() );
#else
		StopSound( "NPC_CeilingTurret.Alert" );
#endif

		m_OnDamaged.FireOutput( info.GetInflictor(), this );

		SetNextThink( gpGlobals->curtime + 0.1f );

		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Retract and stop attacking
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Retire( void )
{
	if ( PreThink( TURRET_RETIRING ) )
		return;

	//Level out the turret
	m_vecGoalAngles = GetAbsAngles();
	SetNextThink( gpGlobals->curtime );

	//Set ourselves to close
	if ( GetActivity() != ACT_ROCKET_SENTRY_CLOSE )
	{
		//Set our visible state to dormant
		SetEyeState( TURRET_EYE_DORMANT );

		SetActivity( (Activity) ACT_ROCKET_SENTRY_OPEN_IDLE );
		
		//If we're done moving to our desired facing, close up
		if ( UpdateFacing() == false )
		{
			SetActivity( (Activity) ACT_ROCKET_SENTRY_CLOSE );
#ifdef MAPBASE
			EmitSound( GetRetireSound() );
#else
			EmitSound( "NPC_CeilingTurret.Retire" );
#endif

			//Notify of the retraction
			m_OnRetire.FireOutput( NULL, this );
		}
	}
	else if ( IsActivityFinished() )
	{	
		SetHeight( ROCKET_TURRET_RETRACT_HEIGHT );

		m_bActive		= false;
		m_flLastSight	= 0;

		SetActivity( (Activity) ACT_ROCKET_SENTRY_CLOSED_IDLE );

		//Go back to auto searching
		if ( m_bAutoStart )
		{
			SetThink( &CNPC_RocketTurret::AutoSearchThink );
			SetNextThink( gpGlobals->curtime + 0.05f );
		}
		else
		{
			//Set our visible state to dormant
			SetEyeState( TURRET_EYE_DISABLED );
			SetThink( &CNPC_RocketTurret::SUB_DoNothing );
		}

#ifdef MAPBASE
		StopLoopingSounds();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Deploy and start attacking
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Deploy( void )
{
	if ( PreThink( TURRET_DEPLOYING ) )
		return;

	m_vecGoalAngles = GetAbsAngles();

	SetNextThink( gpGlobals->curtime );

	//Show we've seen a target
	SetEyeState( TURRET_EYE_SEE_TARGET );

	//Open if we're not already
	if ( GetActivity() != ACT_ROCKET_SENTRY_OPEN )
	{
		m_bActive = true;
		SetActivity( (Activity) ACT_ROCKET_SENTRY_OPEN );
#ifdef MAPBASE
		EmitSound( GetDeploySound() );
#else
		EmitSound( "NPC_CeilingTurret.Deploy" );
#endif

		//Notify we're deploying
#ifdef MAPBASE
		m_OnDeploy.FireOutput( GetEnemy(), this );
#else
		m_OnDeploy.FireOutput( NULL, this );
#endif
	}

	//If we're done, then start searching
	if ( IsActivityFinished() )
	{
		SetHeight( ROCKET_TURRET_DEPLOY_HEIGHT );

		SetActivity( (Activity) ACT_ROCKET_SENTRY_OPEN_IDLE );

		m_flShotTime  = gpGlobals->curtime + 1.0f;

		m_flPlaybackRate = 0;
		SetThink( &CNPC_RocketTurret::SearchThink );

#ifdef MAPBASE
		EmitSound( GetMoveSound() );
#else
		EmitSound( "NPC_CeilingTurret.Move" );
#endif
	}

	SetLastSightTime();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SetLastSightTime()
{
	if( HasSpawnFlags( SF_ROCKET_TURRET_NEVERRETIRE ) )
	{
		m_flLastSight = FLT_MAX;
	}
	else
	{
		m_flLastSight = gpGlobals->curtime + ROCKET_TURRET_MAX_WAIT;	
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the speed at which the turret can face a target
//-----------------------------------------------------------------------------
float CNPC_RocketTurret::MaxYawSpeed( void )
{
	//TODO: Scale by difficulty?
	return 360.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Causes the turret to face its desired angles
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::UpdateFacing( void )
{
	bool  bMoved = false;
	matrix3x4_t localToWorld;
	
	GetAttachment( LookupAttachment( "eyes" ), localToWorld );

	Vector vecGoalDir;
	AngleVectors( m_vecGoalAngles, &vecGoalDir );

	Vector vecGoalLocalDir;
	VectorIRotate( vecGoalDir, localToWorld, vecGoalLocalDir );

	if ( g_debug_turret_ceiling.GetBool() )
	{
		Vector	vecMuzzle, vecMuzzleDir;
		QAngle	vecMuzzleAng;

		GetAttachment( "eyes", vecMuzzle, vecMuzzleAng );
		AngleVectors( vecMuzzleAng, &vecMuzzleDir );

		NDebugOverlay::Cross3D( vecMuzzle, -Vector(2,2,2), Vector(2,2,2), 255, 255, 0, false, 0.05 );
		NDebugOverlay::Cross3D( vecMuzzle+(vecMuzzleDir*256), -Vector(2,2,2), Vector(2,2,2), 255, 255, 0, false, 0.05 );
		NDebugOverlay::Line( vecMuzzle, vecMuzzle+(vecMuzzleDir*256), 255, 255, 0, false, 0.05 );
		
		NDebugOverlay::Cross3D( vecMuzzle, -Vector(2,2,2), Vector(2,2,2), 255, 0, 0, false, 0.05 );
		NDebugOverlay::Cross3D( vecMuzzle+(vecGoalDir*256), -Vector(2,2,2), Vector(2,2,2), 255, 0, 0, false, 0.05 );
		NDebugOverlay::Line( vecMuzzle, vecMuzzle+(vecGoalDir*256), 255, 0, 0, false, 0.05 );
	}

	QAngle vecGoalLocalAngles;
	VectorAngles( vecGoalLocalDir, vecGoalLocalAngles );

	// Update pitch
	float flDiff = AngleNormalize( UTIL_ApproachAngle(  vecGoalLocalAngles.x, 0.0, 0.1f * MaxYawSpeed() ) );
	
	SetPoseParameter( m_poseAim_Pitch, GetPoseParameter( m_poseAim_Pitch ) + ( flDiff / 1.5f ) );

	if ( fabs( flDiff ) > 0.1f )
	{
		bMoved = true;
	}

	// Update yaw
	flDiff = AngleNormalize( UTIL_ApproachAngle(  vecGoalLocalAngles.y, 0.0, 0.1f * MaxYawSpeed() ) );

	SetPoseParameter( m_poseAim_Yaw, GetPoseParameter( m_poseAim_Yaw ) + ( flDiff / 1.5f ) );

	if ( fabs( flDiff ) > 0.1f )
	{
		bMoved = true;
	}

	InvalidateBoneCache();

	return bMoved;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	CBaseEntity	*pHitEntity = NULL;
	if ( BaseClass::FVisible( pEntity, traceMask, &pHitEntity ) )
		return true;

	// If we hit something that's okay to hit anyway, still fire
	if ( pHitEntity && pHitEntity->MyCombatCharacterPointer() )
	{
#ifdef MAPBASE
		if (IRelationType(pHitEntity) <= D_FR)
#else
		if (IRelationType(pHitEntity) == D_HT)
#endif
			return true;
	}

	if (ppBlocker)
	{
		*ppBlocker = pHitEntity;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the turret to fire on targets if they're visible
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::ActiveThink( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink( TURRET_ACTIVE ) )
		return;

	//Update our think time
	SetNextThink( gpGlobals->curtime + 0.1f );

	//If we've become inactive, go back to searching
	if ( ( m_bActive == false ) || ( GetEnemy() == NULL ) )
	{
		SetEnemy( NULL );
		SetLastSightTime();
		SetThink( &CNPC_RocketTurret::SearchThink );
		m_vecGoalAngles = GetAbsAngles();
		return;
	}
	
	//Get our shot positions
	Vector vecMid = EyePosition();
	Vector vecMidEnemy = GetEnemy()->GetAbsOrigin();

	//Store off our last seen location
	UpdateEnemyMemory( GetEnemy(), vecMidEnemy );

	//Look for our current enemy
	bool bEnemyVisible = FInViewCone( GetEnemy() ) && FVisible( GetEnemy() ) && GetEnemy()->IsAlive();

	//Calculate dir and dist to enemy
	Vector	vecDirToEnemy = vecMidEnemy - vecMid;	
	float	flDistToEnemy = VectorNormalize( vecDirToEnemy );

	//We want to look at the enemy's eyes so we don't jitter
	Vector	vecDirToEnemyEyes = GetEnemy()->WorldSpaceCenter() - vecMid;
	VectorNormalize( vecDirToEnemyEyes );

	QAngle vecAnglesToEnemy;
	VectorAngles( vecDirToEnemyEyes, vecAnglesToEnemy );

	//Draw debug info
	if ( g_debug_turret_ceiling.GetBool() )
	{
		NDebugOverlay::Cross3D( vecMid, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Cross3D( GetEnemy()->WorldSpaceCenter(), -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Line( vecMid, GetEnemy()->WorldSpaceCenter(), 0, 255, 0, false, 0.05 );

		NDebugOverlay::Cross3D( vecMid, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Cross3D( vecMidEnemy, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Line( vecMid, vecMidEnemy, 0, 255, 0, false, 0.05f );
	}

	//Current enemy is not visible
	if ( ( bEnemyVisible == false ) || ( flDistToEnemy > ROCKET_TURRET_RANGE ))
	{
		if ( m_flLastSight )
		{
			m_flLastSight = gpGlobals->curtime + 0.5f;
		}
		else if ( gpGlobals->curtime > m_flLastSight )
		{
			// Should we look for a new target?
			ClearEnemyMemory();
			SetEnemy( NULL );
			SetLastSightTime();
			SetThink( &CNPC_RocketTurret::SearchThink );
			m_vecGoalAngles = GetAbsAngles();
			
			SpinDown();

			return;
		}

		bEnemyVisible = false;
	}

	Vector vecMuzzle, vecMuzzleDir;
	QAngle vecMuzzleAng;
	
	GetAttachment( "eyes", vecMuzzle, vecMuzzleAng );
	AngleVectors( vecMuzzleAng, &vecMuzzleDir );
	
	if ( m_flShotTime < gpGlobals->curtime )
	{
		//Fire the gun
		if ( DotProduct( vecDirToEnemy, vecMuzzleDir ) >= 0.9848 ) // 10 degree slop
		{
#ifdef MAPBASE
			if ( m_spawnflags & SF_ROCKET_TURRET_OUT_OF_AMMO )
			{
				ResetActivity();
				SetActivity( (Activity) ACT_ROCKET_SENTRY_DRYFIRE );
				DryFire();
			}
			else
			{
				ResetActivity();
				SetActivity( (Activity) ACT_ROCKET_SENTRY_FIRE );
				Shoot( vecMuzzle, vecMuzzleDir );
			}
#else
			if ( m_spawnflags & SF_ROCKET_TURRET_OUT_OF_AMMO )
			{
				SetActivity( (Activity) ACT_ROCKET_SENTRY_DRYFIRE );
			}
			else
			{
				SetActivity( (Activity) ACT_ROCKET_SENTRY_FIRE );
			}
			
			//Fire the weapon
			Shoot( vecMuzzle, vecMuzzleDir );
#endif
		} 
	}
	else
	{
		SetActivity( (Activity) ACT_ROCKET_SENTRY_OPEN_IDLE );
	}

	//If we can see our enemy, face it
	if ( bEnemyVisible )
	{
		m_vecGoalAngles.y = vecAnglesToEnemy.y;
		m_vecGoalAngles.x = vecAnglesToEnemy.x;
	}

	//Turn to face
	UpdateFacing();
}

//-----------------------------------------------------------------------------
// Purpose: Target doesn't exist or has eluded us, so search for one
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SearchThink( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink( TURRET_SEARCHING ) )
		return;

	SetNextThink( gpGlobals->curtime + 0.05f );

	SetActivity( (Activity) ACT_ROCKET_SENTRY_OPEN_IDLE );

	//If our enemy has died, pick a new enemy
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->IsAlive() == false ) )
	{
		SetEnemy( NULL );
	}

	//Acquire the target
 	if ( GetEnemy() == NULL )
	{
		GetSenses()->Look( ROCKET_TURRET_RANGE );
		CBaseEntity *pEnemy = BestEnemy();
		if ( pEnemy )
		{
			SetEnemy( pEnemy );
		}
	}

	//If we've found a target, spin up the barrel and start to attack
	if ( GetEnemy() != NULL )
	{
#ifdef MAPBASE
		m_flShotTime = gpGlobals->curtime + GetFireRate( GetEnemy()->IsPlayer() );
#else
		//Give players a grace period
		if ( GetEnemy()->IsPlayer() )
		{
			m_flShotTime  = gpGlobals->curtime + 0.5f;
		}
		else
		{
			m_flShotTime  = gpGlobals->curtime + 0.1f;
		}
#endif

		m_flLastSight = 0;
		SetThink( &CNPC_RocketTurret::ActiveThink );
		SetEyeState( TURRET_EYE_SEE_TARGET );

		SpinUp();
#ifdef MAPBASE
		EmitSound( GetActiveSound() );
#else
		EmitSound( "NPC_CeilingTurret.Active" );
#endif
		return;
	}

	//Are we out of time and need to retract?
 	if ( gpGlobals->curtime > m_flLastSight )
	{
		//Before we retrace, make sure that we are spun down.
		m_flLastSight = 0;
		SetThink( &CNPC_RocketTurret::Retire );
		return;
	}
	
	//Display that we're scanning
	m_vecGoalAngles.x = 15.0f;
	m_vecGoalAngles.y = GetAbsAngles().y + ( sin( gpGlobals->curtime * 2.0f ) * 45.0f );

	//Turn and ping
	UpdateFacing();
	Ping();
}

//-----------------------------------------------------------------------------
// Purpose: Watch for a target to wander into our view
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::AutoSearchThink( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink( TURRET_AUTO_SEARCHING ) )
		return;

	//Spread out our thinking
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.2f, 0.4f ) );

	//If the enemy is dead, find a new one
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->IsAlive() == false ) )
	{
		SetEnemy( NULL );
	}

	//Acquire Target
	if ( GetEnemy() == NULL )
	{
		GetSenses()->Look( ROCKET_TURRET_RANGE );
		SetEnemy( BestEnemy() );
	}

	//Deploy if we've got an active target
	if ( GetEnemy() != NULL )
	{
		SetThink( &CNPC_RocketTurret::Deploy );
#ifdef MAPBASE
		EmitSound( GetAlertSound() );
#else
		EmitSound( "NPC_CeilingTurret.Alert" );
#endif
	}
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// I decided to move dry firing to its own function instead of keeping it in Shoot, similar to npc_turret_floor
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::DryFire( void )
{
	EmitSound( "NPC_FloorTurret.DryFire");
	EmitSound( GetActiveSound() );

  	if ( RandomFloat( 0, 1 ) > 0.7 )
	{
		m_flShotTime = gpGlobals->curtime + random->RandomFloat( 0.5, 1.5 );
	}
	else
	{
		m_flShotTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fire!
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy )
{
	FireBulletsInfo_t info;

	if ( GetEnemy() != NULL )
	{
		Vector vecDir = GetActualShootTrajectory( vecSrc );

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = VECTOR_CONE_PRECALCULATED;
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}
	else
	{
		// Just shoot where you're facing!
		
		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDirToEnemy;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = GetAttackSpread( NULL, NULL );
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}

	FireBullets( info );
#ifdef MAPBASE
	EmitSound( GetShootSound() );
#else
	EmitSound( "NPC_CeilingTurret.ShotSounds" );
#endif
	DoMuzzleFlash();
}
#else
//-----------------------------------------------------------------------------
// Purpose: Fire!
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy )
{
	if ( m_spawnflags & SF_ROCKET_TURRET_OUT_OF_AMMO )
	{
		EmitSound( "NPC_FloorTurret.DryFire");
		EmitSound( "NPC_CeilingTurret.Activate" );

  		if ( RandomFloat( 0, 1 ) > 0.7 )
		{
			m_flShotTime = gpGlobals->curtime + random->RandomFloat( 0.5, 1.5 );
		}
		else
		{
			m_flShotTime = gpGlobals->curtime;
		}
		return;
	}

	FireBulletsInfo_t info;

	if ( GetEnemy() != NULL )
	{
		Vector vecDir = GetActualShootTrajectory( vecSrc );

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = VECTOR_CONE_PRECALCULATED;
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}
	else
	{
		// Just shoot where you're facing!
		Vector vecMuzzle, vecMuzzleDir;
		QAngle vecMuzzleAng;
		
		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDirToEnemy;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = GetAttackSpread( NULL, NULL );
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}

	FireBullets( info );
	EmitSound( "NPC_CeilingTurret.ShotSounds" );
	DoMuzzleFlash();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Allows a generic think function before the others are called
// Input  : state - which state the turret is currently in
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::PreThink( turretState_e state )
{
	CheckPVSCondition();

	//Animate
	StudioFrameAdvance();

	//Do not interrupt current think function
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the state of the glowing eye attached to the turret
// Input  : state - state the eye should be in
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SetEyeState( eyeState_t state )
{
	//Must have a valid eye to affect
	if ( m_pEyeGlow == NULL )
		return;

	//Set the state
	switch( state )
	{
	default:
	case TURRET_EYE_SEE_TARGET: //Fade in and scale up
		m_pEyeGlow->SetColor( 255, 0, 0 );
		m_pEyeGlow->SetBrightness( 164, 0.1f );
		m_pEyeGlow->SetScale( 0.4f, 0.1f );
		break;

	case TURRET_EYE_SEEKING_TARGET: //Ping-pongs
		
		//Toggle our state
		m_bBlinkState = !m_bBlinkState;
		m_pEyeGlow->SetColor( 255, 128, 0 );

		if ( m_bBlinkState )
		{
			//Fade up and scale up
			m_pEyeGlow->SetScale( 0.25f, 0.1f );
			m_pEyeGlow->SetBrightness( 164, 0.1f );
		}
		else
		{
			//Fade down and scale down
			m_pEyeGlow->SetScale( 0.2f, 0.1f );
			m_pEyeGlow->SetBrightness( 64, 0.1f );
		}

		break;

	case TURRET_EYE_DORMANT: //Fade out and scale down
		m_pEyeGlow->SetColor( 0, 255, 0 );
		m_pEyeGlow->SetScale( 0.1f, 0.5f );
		m_pEyeGlow->SetBrightness( 64, 0.5f );
		break;

	case TURRET_EYE_DEAD: //Fade out slowly
		m_pEyeGlow->SetColor( 255, 0, 0 );
		m_pEyeGlow->SetScale( 0.1f, 3.0f );
		m_pEyeGlow->SetBrightness( 0, 3.0f );
		break;

	case TURRET_EYE_DISABLED:
		m_pEyeGlow->SetColor( 0, 255, 0 );
		m_pEyeGlow->SetScale( 0.1f, 1.0f );
		m_pEyeGlow->SetBrightness( 0, 1.0f );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a pinging noise so the player knows where we are
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Ping( void )
{
	//See if it's time to ping again
	if ( m_flPingTime > gpGlobals->curtime )
		return;

	//Ping!
#ifdef MAPBASE
	EmitSound( GetPingSound() );
#else
	EmitSound( "NPC_CeilingTurret.Ping" );
#endif

	SetEyeState( TURRET_EYE_SEEKING_TARGET );

	m_flPingTime = gpGlobals->curtime + ROCKET_TURRET_PING_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the turret's state
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Toggle( void )
{
	//Toggle the state
	if ( m_bEnabled )
	{
		Disable();
	}
	else 
	{
		Enable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enable the turret and deploy
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Enable( void )
{
	m_bEnabled = true;

	// if the turret is flagged as an autoactivate turret, re-enable its ability open self.
	if ( m_spawnflags & SF_ROCKET_TURRET_AUTOACTIVATE )
	{
		m_bAutoStart = true;
	}

	SetThink( &CNPC_RocketTurret::Deploy );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: Retire the turret until enabled again
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Disable( void )
{
	m_bEnabled = false;
	m_bAutoStart = false;

	SetEnemy( NULL );
	SetThink( &CNPC_RocketTurret::Retire );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the turret's state via input function
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Stops the turret from firing live rounds (still attempts to though)
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputDepleteAmmo( inputdata_t &inputdata )
{
	AddSpawnFlags( SF_ROCKET_TURRET_OUT_OF_AMMO );
}

//-----------------------------------------------------------------------------
// Purpose: Allows the turret to fire live rounds again
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputRestoreAmmo( inputdata_t &inputdata )
{
	RemoveSpawnFlags( SF_ROCKET_TURRET_OUT_OF_AMMO );
}

//-----------------------------------------------------------------------------
// Purpose: Creates the sprite if it has been destroyed
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputCreateSprite( inputdata_t &inputdata )
{
	if (m_pEyeGlow)
		return;

	m_pEyeGlow = CSprite::SpriteCreate( ROCKET_TURRET_GLOW_SPRITE, GetLocalOrigin(), false );
	m_pEyeGlow->SetTransparency( kRenderTransAdd, 255, 0, 0, 128, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( this, 2 );

	RemoveSpawnFlags(SF_ROCKET_TURRET_NO_SPRITE);
}

//-----------------------------------------------------------------------------
// Purpose: Destroys the sprite
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputDestroySprite( inputdata_t &inputdata )
{
	if (!m_pEyeGlow)
		return;

	UTIL_Remove(m_pEyeGlow);
	m_pEyeGlow = NULL;
	AddSpawnFlags(SF_ROCKET_TURRET_NO_SPRITE);
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SpinUp( void )
{
}

#define	ROCKET_TURRET_MIN_SPIN_DOWN	1.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SpinDown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::DeathThink( void )
{
	if ( PreThink( TURRET_DEAD ) )
		return;

	//Level out our angles
#ifdef MAPBASE
	SetIdleGoalAngles();
#else
	m_vecGoalAngles = GetAbsAngles();
#endif
	SetNextThink( gpGlobals->curtime );

	if ( m_lifeState != LIFE_DEAD )
	{
		m_lifeState = LIFE_DEAD;

#ifdef MAPBASE
		StopLoopingSounds();
#endif

#ifdef MAPBASE
		EmitSound( GetDieSound() );
#else
		EmitSound( "NPC_CeilingTurret.Die" );
#endif

		SetActivity( (Activity) ACT_ROCKET_SENTRY_CLOSE );
	}

	// lots of smoke
	Vector pos;
	CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1, 1, 1 ), &pos );
	
	CBroadcastRecipientFilter filter;
	
	te->Smoke( filter, 0.0, &pos, g_sModelIndexSmoke, 2.5, 10 );
	
	g_pEffects->Sparks( pos );

	if ( IsActivityFinished() && ( UpdateFacing() == false ) )
	{
		SetHeight( ROCKET_TURRET_RETRACT_HEIGHT );

		m_flPlaybackRate = 0;
		SetThink( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : height - 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SetHeight( float height )
{
	Vector forward, right, up;
	AngleVectors( GetLocalAngles(), &forward, &right, &up );

	Vector mins = ( forward * -16.0f ) + ( right * -16.0f );
	Vector maxs = ( forward *  16.0f ) + ( right *  16.0f ) + ( up * -height );

	if ( mins.x > maxs.x )
	{
		V_swap( mins.x, maxs.x );
	}

	if ( mins.y > maxs.y )
	{
		V_swap( mins.y, maxs.y );
	}

	if ( mins.z > maxs.z )
	{
		V_swap( mins.z, maxs.z );
	}

	SetCollisionBounds( mins, maxs );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::CanBeAnEnemyOf( CBaseEntity *pEnemy )
{
	// If we're out of ammo, make friendly companions ignore us
	if ( m_spawnflags & SF_ROCKET_TURRET_OUT_OF_AMMO )
	{
		if ( pEnemy->Classify() == CLASS_PLAYER_ALLY_VITAL )
			return false;
	} 

	return BaseClass::CanBeAnEnemyOf( pEnemy );
}
