# Razion Companion

Razion Companion is an optional, native desktop companion for RazionOS. It is
a small personality layer over the desktop, not an online service or an AI
feature. It works completely offline and is disabled by default.

Open **Settings → Personalization → Desktop Companion**, search for **Desktop
Companion**, or choose it from **Applications → Accessories**. Enabling the
feature starts the transparent desktop surface. Disabling it closes that
surface and leaves no companion process resident.

## Architecture

The implementation is split into narrow user-space components:

- `libtoaru_razion_companion` owns the species catalogue, needs, progression,
  actions, notification policy, validation, and persistence format.
- `razion-companion` without arguments is the decorated native control center.
- `razion-companion --overlay` is the transparent, shaped desktop surface.
- a small PEX control endpoint applies reload, show, hide, and action commands
  to the running surface. This is also the boundary reserved for controlled
  future automation.
- the existing RazionOS toast service displays rate-limited notices when it is
  available. The companion's speech bubble remains the visual fallback.

The feature is entirely in user space. It does not change the kernel, window
manager protocol, VirtualBox integration, file manager, terminal, or AI
engine.

## Pet system

The catalogue contains 22 selectable original companions: Cat, Dog, Lion,
Tiger, Wolf, Fox, Rabbit, Panda, Bear, Penguin, Eagle, Owl, Parrot, Snake,
Turtle, Frog, Dragon, Dinosaur, Unicorn, Horse, Monkey, and Red Panda.

Each catalogue entry declares a stable ID, display name, short voice,
temperament, colors, silhouette family, energy bias, and social bias. New
species can be appended to the table without changing the needs or persistence
engine. Artwork is generated from compact RazionOS geometric silhouettes at
runtime; the repository does not copy or bundle reference-application art.

The control center supports name, species, non-destructive color layer,
accessory, personality, size, desktop position mode, animation speed,
always-on-top, click-through, sound preference, visibility, and notification
frequency. The first artwork color is always the species' original palette.

## State and interactions

Local state tracks happiness, hunger, energy, health, level, experience, last
update, last interaction, and last notification. Feed, Pet, Play, Sleep, and
Talk adjust these values through the library API and trigger a short visual
reaction. A small offline reflex activity exercises the same Play path and
awards progression locally. The first interaction on a new day grants a small
daily bonus. Additional colors unlock with early levels, accessories through
level five, and animation styles through level three. The 22 core species stay
selectable so customization never turns into a punitive grind; future bonus
species can use the same local level gate.

Needs advance from elapsed wall time rather than a busy polling loop. Catch-up
is capped at seven days so returning after a long absence is never punitive.
Health only decreases when hunger or energy is very low and recovers slowly
when the companion is cared for. Progression unlock metadata is represented by
level and experience; future colors, accessories, and animations can use these
values without an account or format change.

## Persistence

Configuration and state are stored per user in:

```text
~/.razion/companion.conf
```

The parser starts from safe defaults, ignores unknown keys for forward
compatibility, bounds every numeric field, and sanitizes the user-visible
name. The file contains no credentials, telemetry, conversation content, or
network identifiers. State is saved after interactions, customization, drag
position changes, and at a bounded periodic checkpoint.

## Desktop behavior and performance

The companion surface supports free movement, a bottom-corner anchor, and a
stay-near-position mode. Transparent pixels are excluded from hit testing.
Click-through makes the entire surface non-interactive; Settings remains the
recovery path. Right-clicking an interactive companion opens its control
center, and dragging is permitted outside corner mode.

The overlay waits on compositor and control-endpoint file descriptors. It does
not continuously poll. Visible animation uses a deliberately low frequency of
roughly 1–3 frames per second, depending on the selected speed. The overlay
samples the kernel's existing idle counters every 15 seconds and cuts its
animation rate to one third while CPU availability is low. Hidden mode stops
animation and wakes at most once per minute for state bookkeeping.
Rendering is procedural and uses a single 230×230 double buffer, so there are
no decoded animation sheets or asset caches. The disabled startup probe reads
one small local file and exits.

Notifications are off, rare, balanced, or frequent, with minimum intervals of
six hours, three hours, or one hour. Only a meaningful need can trigger a
notice. Short, original procedural tones use RazionOS's existing `beep`
utility and vary by species and action. If audio or the toast endpoint is
absent, interaction remains fully usable through visual reactions; no mixer,
codec, or bundled sound-file dependency is introduced.

## Future Razion AI integration

AI integration is not implemented. A future privileged broker may translate
approved requests such as “feed my pet” or “what does Nova need?” into the
same bounded action/query API. It must authenticate the caller, validate all
arguments, preserve local-only operation, and never give a model direct write
access to `companion.conf` or the desktop surface.

## Verification

The `test-razion-companion` regression covers catalogue uniqueness, defaults,
elapsed-time need changes, interactions, notification rate limiting, and
save/load round trips. Release validation should additionally cover graphical
selection and naming, each action, color/accessory layers, hide/show,
click-through recovery, reboot persistence, missing audio/toast fallback,
idle CPU use, and BIOS/UEFI boot in the documented VirtualBox configuration.
