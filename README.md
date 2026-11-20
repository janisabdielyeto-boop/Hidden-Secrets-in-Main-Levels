# Hidden Secrets in Main Levels

A Geode mod for **Geometry Dash (2.2091)** that hides unique, playable secret routes and mechanics inside the main levels — complete with an in-game **Secrets List** inside the Pause menu!

## Currently Implemented Secrets (3 / 22)

| Level | Secret Name | How to Trigger | What Happens |
| :--- | :--- | :--- | :--- |
| **Level 1: Stereo Madness** | *Reverse Mode* | Fly high above the final ship section (98%–99%) right before touching the end wall (or hit the hidden Toggle Orb in *Secret Madness*). | Rewinds the end wall and lets you play the entire level backwards from **100.00% down to 0.00%** in free mode with reversed music and SFX. |
| **Level 10: xStep** | *Time Secret (Warp Time)* | Intentionally miss the yellow orb and crash on the spike between **8.50% and 8.70%**. | Shows a 3-second rule banner, then warps game speed (m_timeWarp) and music pitch on every action (**-30%** gravity portal, **+20%** pad, **+10%** orb, **-5%** jump). |
| **Level 22: Dash** | *Ultimate Crash* | Around **82%**, jump high into the hidden zone beneath the *ULTIMATE CRASH* sign before the final big Wave portal. | Locks the camera on the final Wave and loops it **25 times**, increasing speed and music pitch each loop until the game crashes on loop 25! |

## In-Game Secrets List (Pause Menu)
* Open the **Pause Menu** in any level and click the discreet **Secrets** button in the bottom-right frame.
* View all implemented secrets, track which ones are **FOUND** or **LOCKED**, and click any entry to open a scrollable guide explaining what it does and how to find it.

## Build & Install
Requires [Geode SDK](https://geode-sdk.org) for Geometry Dash 2.2091:
`ash
geode build
`
"@

 = @"
# Hidden Secrets in Main Levels

Discover hidden alternate routes, reality-warping mechanics, and secret endings hidden inside Geometry Dash's main levels!

### Features
- **3 Playable Secrets** across *Stereo Madness*, *xStep*, and *Dash* (works on both official main levels and custom copies!).
- **Built-in Secrets List** accessible from the Pause menu (Secrets button) with persistent progress tracking and detailed hints.
- **Custom Reversed Audio & Visual Effects** built directly with Cocos2d-x and FMOD.

*More main levels coming soon!*