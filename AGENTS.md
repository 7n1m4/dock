# Engineering principles for this dock

## Fewer moving parts, more stable product

Crystal Dock's stability comes from what it does **not** run, not from carefully
nurturing complex code paths. Every optional mechanism that is not essential to
"show a panel, launch and switch applications" is a liability: it holds a
configuration surface, an interaction state machine, and a class of bugs that
only reproduce on someone else's machine.

So when a feature is proposed, the default question is not "how do we make this
robust?" but **"can the user's goal be met with less machinery?"**

Concretely, prefer:
- Deleting a feature over adding a toggle for it. A toggle is a second code path
  that still has to be maintained, tested, and explained.
- Static configuration over runtime discovery. Runtime discovery (scanning
  .desktop trees, querying a desktop shell, D-Bus lookups) is the most fragile
  machinery in a dock: it depends on a foreign process that may be absent,
  slow to answer, or answering differently by version.
- One code path over two. If a screen and a dialog both configure the same
  thing, keep the screen and delete the dialog.

## Popups are the top risk

Popups (context menus, cascading app browsers) combine a state machine with a
foreign process and a compositor race. They are where this dock's crashes and
hangs come from. Right-click menus were removed for this reason, and the
application-menu item was removed for the same reason.

Every remaining interactive surface must be checkable against one question:
**does this need a popup at all?** If the answer is "not really", it should not
be in the tree.

Still shipping, so the rule is only partly fulfilled — an open decision, not a
closed one:

| Popup | File | Verdict |
|---|---|---|
| Keyboard-layout picker | `keyboard_layout.cc:79` | Not redundant. The settings dialog only edits *which* layouts exist; this menu is the only way to *switch* between them (`onKeyboardLayoutSelected` drives IBus). Removing it needs a replacement switcher first. |
| Volume slider | `volume_control.cc:131-132` | Genuinely interactive, no replacement surface. Removing it means deleting the feature, not a straight cleanup. |
| Wi-Fi network picker | `wifi_manager.cc:178-180` | Same: it is the only way to pick a network. |

Deleting a popup is only right when a non-popup path already exists for the
same goal. Where it doesn't, the honest move is to build that path first.

## Deleting a feature: find its real consumers first

"Do not move parts" is not the same as "delete everything that mentions the
part." Before deleting, map every consumer and separate what the feature *only*
fed from what is load-bearing elsewhere.

The example to remember is the application menu. Deleting the obvious files
(`application_menu.{h,cc}`, its settings dialog, the feature key) was safe, but
`application_menu_config` — the `.desktop` scanner — had to stay: it is how the
dock resolves a running window to its `.desktop` entry, which is what gives
launchers and task-list items their identity. And the per-desktop-environment
`getApplicationMenuSystemCategories()` had to stay too, because the concrete
commands for Lock Screen and Log Out live inside those Session categories:
`getDefaultLaunchers()` returns `kLockScreenId`/`kLogOutId`, and the pinned
launcher for those ids is resolved through `findApplication()`. Removing the
categories would have silently killed two buttons the user actually uses.

So the rule: **deleting a consumer is safe; deleting a data source is not, until
you have followed every id and every config key it feeds.**

## Parallel index lists are a bug factory

When the same ordering is expressed more than once — a label list, a getter
list, a setter list, and a `switch` over the index — those copies must stay in
lockstep, and nothing enforces it. The Application Menu purge exposed exactly
this: `setFeatureVisible`'s `switch` was numbered 1..9 while the caller passed
a 0-based loop index, so the first checkbox did nothing and every other one
drove the feature one row below it. It went unnoticed because a second,
correctly-zero-based array wrote the same settings on the fallback path.

Prefer deriving the order once. If a numeric `switch` is unavoidable, assert the
alignment where the list is defined, and re-check every parallel array whenever
you add or remove a row.

## Keep the essential core lean and boring

The panel, the pinned-launcher list, the task (window) list, and the settings
screen are the product. Nothing else is sacred. Be willing to delete whole
subsystems — desktop-environment detection, extra components, alternative input
paths — provided the pinned launchers still launch, the task list still tracks
windows, and the settings window still configures what is left.
