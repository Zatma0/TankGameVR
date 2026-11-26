# Sound credits

All recordings below are from [Freesound](https://freesound.org) under **CC0 1.0** (public domain, no attribution
required). They are listed anyway so anyone can find the originals. The game sounds in `Content/TankGame/Audio` were
cut, layered, pitched and levelled from them with ffmpeg (`Tools/Sound/build.py`).

| Recording | By | Used in |
|---|---|---|
| [bullet ricochet.wav](https://freesound.org/people/aust_paul/sounds/30932/) | aust_paul | Hull_Hit_Light |
| [Tank fire Mixed.wav](https://freesound.org/people/GaryQ/sounds/127845/) | GaryQ | Cannon_Fire, Cannon_Interior, Enemy_Cannon |
| [metal pipe foley.wav](https://freesound.org/people/Alternicity/sounds/133101/) | Alternicity | Casing_Drop |
| [three_diesel_locomotives_idling_loop1.wav](https://freesound.org/people/themanitou/sounds/146326/) | themanitou | Engine_Loop |
| [FireExtinguisher.wav](https://freesound.org/people/Planet-Leader/sounds/155516/) | Planet-Leader | Extinguisher_Loop, Smoke_Burst |
| [Howitzer Artillery Gun Shot Sound Effect 01](https://freesound.org/people/qubodup/sounds/161343/) | qubodup | ATGun_Fire, Cannon_Fire |
| [M203 Grenade Launcher 1](https://freesound.org/people/qubodup/sounds/162402/) | qubodup | Smoke_Burst, Smoke_Salvo |
| [engine_idle_loop.wav](https://freesound.org/people/ReadeOnly/sounds/186935/) | ReadeOnly | Enemy_Engine_Loop |
| [chiavistello.wav](https://freesound.org/people/gzooropa/sounds/191095/) | gzooropa | Breech_Close, Breech_Open, Cannon_Interior |
| [Explosion004.wav](https://freesound.org/people/klangfabrik/sounds/220062/) | klangfabrik | Enemy_Destroyed, Impact_AP, Impact_Dirt |
| [Bullet Hit Metal](https://freesound.org/people/coolguy244e/sounds/267893/) | coolguy244e | Enemy_Destroyed, Hull_Hit, Impact_AP |
| [Close switch/hatch on tape machine](https://freesound.org/people/hanneswannerberger/sounds/275630/) | hanneswannerberger | Dry_Fire, Switch_Click |
| [t26_moving_01.wav](https://freesound.org/people/greatmganga/sounds/329800/) | greatmganga | Tracks_Loop |
| [HeavyBulletPing.mp3](https://freesound.org/people/wilhellboy/sounds/351371/) | wilhellboy | Cannon_Interior, Dry_Fire, Hull_Hit, Hull_Hit_Light, Impact_AP |
| [Huge Explosion Part 3 - Long Crash](https://freesound.org/people/bevibeldesign/sounds/366086/) | bevibeldesign | Explosion_HE |
| [St Mullion Church Door Internal Shut_sel.wav](https://freesound.org/people/gropkcor/sounds/392240/) | gropkcor | Breech_Close, Hull_Hit, Shell_Seat |
| [Tank Turret Rotate](https://freesound.org/people/KieranKeegan/sounds/418881/) | KieranKeegan | Turret_Loop |
| [cannon.mp3](https://freesound.org/people/Kneeling/sounds/448002/) | Kneeling | ATGun_Fire, Autocannon_Shot, Cannon_Fire, Enemy_Cannon |
| [Bomb Explosion](https://freesound.org/people/LilMati/sounds/523365/) | LilMati | Explosion_HE |
| [Metal Scrape](https://freesound.org/people/magnuswaker/sounds/530075/) | magnuswaker | Shell_Ram |
| [bonfire flames sizzling](https://freesound.org/people/florianreichelt/sounds/563764/) | florianreichelt | Fire_Loop |
| [S38-30 Spaceship servo motors from 2010.wav](https://freesound.org/people/craigsmith/sounds/675726/) | craigsmith | Turret_Loop |
| [Big falling debris (crash)](https://freesound.org/people/xkeril/sounds/703247/) | xkeril | Enemy_Destroyed, Explosion_HE |
| [Fall debris (crash)](https://freesound.org/people/xkeril/sounds/703248/) | xkeril | Impact_Dirt |
| [Autocannon Three Shot Burst](https://freesound.org/people/qubodup/sounds/854186/) | qubodup | Autocannon_Shot |

Made from scratch (sine/noise synthesis in `build.py`, no recording): Game_Over, Gun_Ready, Lock_Beep, On_Target, Warning_Loop, Wave_Clear, Wave_Start.

Radio voice lines (`SW_Radio_*` except `Call_Loop` and `Pickup`): Windows' built-in offline speech synthesis
(Microsoft Zira) put through a handset radio chain in `Tools/Sound/radio.py`. Fine for the school project; replace
them with recorded lines (same file names) before any release. Radio ring and static: synthesized in `radio.py`.
