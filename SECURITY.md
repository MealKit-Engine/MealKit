# Security

MealKit games accept network packets from other players (lockstep sessions, LAN room discovery, relays) and read files a player can hand them (saves, replays, bug report bundles). Problems there, such as a crash or memory corruption from a crafted packet or file, are security issues.

## Reporting

Report privately through GitHub: **Security** tab of the repository → **Report a vulnerability**. Please don't open a public issue for it.
Include the engine version (`VERSION`, or `info.txt` in a bug report zip), what you sent or opened, and what happened. A packet capture, file or input recording that reproduces it helps most.

You'll get an answer within a week. Fixes go into the latest version only while MealKit is in beta (v0.x).
