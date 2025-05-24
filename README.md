Fake Portal Gun Mod (Mapbase edition)
=====

This is a mod for a "portal gun" that shoots explosive portals. Basically an RPG reskin, but shoots portals instead of rockets. It also has rocket jumping capabilities which can be tuned with some convars. This version is a port to Mapbase. Mapbase readme [here](https://github.com/SonicEraZoR/FakePortalGun-mapbase/blob/master/README/).

## Tuning rocket jumping

Rocket jumping can be tuned with some convars:

* sv_rocket_jump_damage_decrease (default is "0.75") - percent by which rocket jump damage is decreased for the player, can't be "1" though since in order to apply knock back at least some damage needs to be received.
* sv_rocket_jump_force_boost_x (default is "1") - percent by which rocket jump knock back is increased on x axis.
* sv_rocket_jump_force_boost_y (default is "1") - percent by which rocket jump knock back is increased on y axis.
* sv_rocket_jump_force_boost_z (default is "250") - percent by which rocket jump knock back is increased on z axis.

## Additional convars

This mod also implements these convars, which mimic Portal, should be self-explanatory

* sv_regeneration_wait_time (default 1)
* sv_regeneration_enable (default 0)
* sv_receive_fall_damage (default 1)

## Dependencies

### Windows
* [Visual Studio 2013 with Update 5](https://visualstudio.microsoft.com/vs/older-downloads/)

### macOS
* [Xcode 5.0.2](https://developer.apple.com/downloads/more)

### Linux
* GCC 4.8
* [Steam Client Runtime](http://media.steampowered.com/client/runtime/steam-runtime-sdk_latest.tar.xz)

## Building

Compiling process is the same as for Source SDK 2013. Instructions for building Source SDK 2013 can be found here: https://developer.valvesoftware.com/wiki/Source_SDK_2013

You will need Portal installed though

## Installing:

Same as Source SDK 2013 mod: https://developer.valvesoftware.com/wiki/Setup_mod_on_Steam
