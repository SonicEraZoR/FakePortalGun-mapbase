#include "cbase.h"
#include "c_weapon__stubs.h"
#include "c_basehlcombatweapon.h"

class C_WeaponFakePortalGun : public C_BaseHLCombatWeapon
{
	DECLARE_CLASS( C_WeaponFakePortalGun, C_BaseHLCombatWeapon );
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLIENTCLASS();
	C_WeaponFakePortalGun() {};

	bool	ShouldDrawPickup(void);

private:
	C_WeaponFakePortalGun( const C_WeaponFakePortalGun & );
};

STUB_WEAPON_CLASS_IMPLEMENT(weapon_fakeportalgun, C_WeaponFakePortalGun);

IMPLEMENT_CLIENTCLASS_DT( C_WeaponFakePortalGun, DT_WeaponFakePortalGun, CWeaponFakePortalGun )
END_RECV_TABLE()

bool C_WeaponFakePortalGun::ShouldDrawPickup()
{
	return false;
}