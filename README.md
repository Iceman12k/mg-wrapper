# mg-wrapper
wrapper for Midnight Guns enabling steam integration

This code is a mess at current, as it was mostly assembled through trial and error + a poking at steam support to get them to fix bugs with the InventoryService + SteamGameServer (then eventually hacking around them).

Absolutely no guarantees are given with this code, yada yada, good luck.

current features are
Steam Avatar saving
SteamGameServer
SteamUserAuthentication (the new system)
SteamInventoryService (some of the features, not all yet)
Some SteamNetworking that I poked at but never did anything with

the other half of this code is located in [this mess of an FTEQW plugin](https://github.com/Iceman12k/fteqw-mguns/blob/master/plugins/steam/steam.c)
