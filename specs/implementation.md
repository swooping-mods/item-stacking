# Non-dust Capsule Stacking: behavior and implementation

Describes the exported implementation **0.1.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

Ctrl+K stacks nearby matching physical Sand or Stone capsules above the nearest capsule. Defaults are five capsules, 65 cm spacing and a three-meter search. The bounded scan inspects up to 2,048 records and retains at most 32 nearby candidates; carried/flying capsules and active carry sessions are rejected. Native carry/settle/release operations preserve existing capsule IDs and contents.

## State and lifecycle

Configuration controls count, spacing, radius and Sand/Stone admission. Native capsule poses use normal save behavior; the mod does not merge mass or invent inventory counts. Rejected placement attempts original-pose restoration and releases its temporary carry claim. The named action excludes Shift/Alt and consumes gated edges; host/world/focus/menu admission precedes mutation.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/item-stacking/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

Packaged defaults in [settings.ini](../source/mods/item-stacking/settings.ini); supported bounds and interpretation remain in the linked parser.

| Section | Setting | Default |
|---|---|---|
| `[stacking]` | `max_capsules` | `5` |
| `[stacking]` | `spacing_cm` | `65` |
| `[stacking]` | `radius_cm` | `300` |
| `[stacking]` | `sand` | `1` |
| `[stacking]` | `stone` | `1` |

## Implementation sources

- [mod.c](../source/mods/item-stacking/mod.c)
- [support.h](../source/mods/item-stacking/support.h)

## Verification boundary

Dust, equipment/tools, vehicles and structures are excluded. Physical stack stability, incomplete-cleanup recovery, save reload and co-op require native-game checks.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
