# Shared installer

Sync-Toolkit's `installer` part vendors this folder to `lib/toolkit/installer`. The shared installer will find the game,
back up, copy, write a receipt and uninstall the files it owns. It moves here from iRacing Arcade's `install.ps1` in
Phase C1 of `dbce-project-mgmt/docs/architecture/2026-11/02-unity-family.md`. Until then, games keep their own installers.
