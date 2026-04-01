//========= Copyright Buster Bunny, All rights reserved. ============//
//
// Purpose:		Epic Ultra Convar
//
//=============================================================================//


#include <cbase.h>
#include <convar.h>

void EpicUltraConvar()
{
	Error("HAPPY APRIL'S FOOLS!!!!!!!!!!!");
}

ConCommand epic_ultra_convar("epic_ultra_convar", EpicUltraConvar, "ePIC uLTRA cONVAR", FCVAR_HIDDEN);
