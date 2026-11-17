# Blatant AVOID — implementation notes

This archive contains an improved development build of the Blatant AVOID source. It is not a claim that the feature is fully validated or perfect in live gameplay.

## Changes in this revision

- Player Threat is no longer triggered merely because another tee has hooked the local tee. The predictor compares the normal world, a second scenario in which other players release movement/jump but keep their hook, and a counterfactual world with other players removed. It activates only when a player-influenced scenario predicts an earlier freeze than the no-player baseline.
- During a detected player threat, candidate searches are run against both opponent continuations; each candidate is ranked by its worse predicted lifespan.
- Threat direction is estimated from the displacement difference between the player-influenced and no-player simulations. The prediction includes the existing DDNet world simulation, so incoming hooks and character collisions/pushes can affect the result.
- The pre-check only runs when an incoming hook, a nearby collision, or a hook flying toward the local tee makes player interaction plausible.
- Hook Pulse is gated by AVOID's own selected escape input. It does not cycle a hook that was already held manually. It resets when AVOID no longer owns an automated hook action. Candidate plans are also re-simulated with the release/re-engage cadence, so the planner does not rely solely on a continuously held hook.
- NSIF's stored plan is invalidated when manual direction/hook/jump/aim input changes, and when a player threat starts or ends.
- Added settings for player-threat sensitivity and whether AVOID predicts other players. Existing Hook Pulse interval and direction/hook/jump switches remain available.

## Status against the roadmap

1. **Stabilization:** manual-input changes now invalidate saved plans; player-threat entry/exit clears stale plans. Greedy/Candidates/NSIF still need controlled in-game regression tests.
2. **Physics prediction:** uses isolated DDNet prediction worlds with other characters, hooks and player collisions. A measured comparison against live gameplay has not been completed.
3. **Player Threat Detection:** detects player-caused freeze acceleration using current and neutral-opponent scenarios, including incoming hooks and nearby collision influence. This remains a prediction, not a guarantee for every opponent behavior.
4. **Player Escape System:** AVOID evaluates movement/hook/jump candidates and scores movement against the estimated external influence. The time-to-freeze is predicted in ticks; map-specific and latency-specific validation remains outstanding.
5. **Hook Pulse:** pulse release/re-engage is limited to an automated hook selected by AVOID; it stops when AVOID no longer owns that hook. Candidate plans are simulated with the pulse cadence in the current opponent snapshot, but pulse sequences are not yet re-simulated against every neutral-opponent scenario.
6. **Trajectory search:** candidate search now compares two opponent continuations and ranks each candidate by worst predicted survival; escape scoring uses the estimated external influence. This is only a small scenario set, not every possible opponent action. Broader scenario coverage and NSIF tuning remain incomplete.
7. **Settings and optimization:** added player prediction and threat-sensitivity settings, plus the existing pulse interval. Android performance profiling and map-by-map testing remain outstanding.

## Validation performed

The changed C++ translation units passed local `g++ -std=c++20 -fsyntax-only` checks:

- `src/game/client/avoid/blatant.cpp`
- `src/game/client/avoid/simulation.cpp`
- `src/game/client/components/controls.cpp`
- `src/game/client/components/menus_settings_blackout.cpp`

A full client build and Android APK build were not completed in this environment. CMake configuration is blocked by missing Rust/Cargo, Freetype, Ogg, Opus, Opusfile and SDL2 development dependencies. Therefore this archive still requires a real Android build and controlled in-game testing before it should be treated as stable.
