# RazionOS Design System

## Direction

RazionOS uses clean geometry, strong information hierarchy, restrained depth,
and high-contrast typography. The first theme is **Razion Dark**. It avoids
pure black as the primary surface, expensive blur as a requirement, excessive
gradients, and decorative effects without functional value.

The accent is a restrained teal rather than the upstream bright blue. Dark
charcoal layers distinguish the desktop background, panel, menus, title bars,
dialogs, and terminal without relying on translucency.

## Semantic color tokens

The canonical compile-time tokens are in
`base/usr/include/razion/theme.h`. Consumers use semantic aliases instead of
copying RGB values.

| Token | Razion Dark purpose |
| --- | --- |
| `background` | deepest application/terminal layer |
| `surface` | windows, dialogs, and menus |
| `surface-secondary` | controls and menu bars |
| `surface-hover` | hover and elevated interaction state |
| `border` | separators and inactive focus boundary |
| `text-primary` | primary readable content |
| `text-secondary` | captions, metadata, inactive state |
| `accent` | brand and active-state emphasis |
| `accent-hover` | interactive link/hover emphasis |
| `success` | successful state |
| `warning` | caution state |
| `error` | destructive/error state |
| `selection` | selected rows, tabs, and active regions |
| `focus` | keyboard/input focus ring |

Razion Light values are defined beside the dark values to establish a stable
API, but runtime theme switching is intentionally deferred.

## Typography

No external font was added. RazionOS uses the fonts already supplied by the
ToaruOS font service:

- Display: `sans-serif.bold`, 20 px and above;
- Title: `sans-serif.bold`, 12–16 px;
- Heading: `sans-serif.bold`, 14 px;
- Body: `sans-serif`, 13–14 px;
- Caption: `sans-serif`, 11–12 px with secondary text color;
- Monospace: `monospace`, terminal cell metrics controlled by the terminal.

This avoids new font licenses and preserves the shared-memory font-rendering
path. The current toolkit has no centralized line-height or density engine, so
spacing remains component-level.

## Component rules

- Panel: 32 px tall, opaque-enough dark surface for legibility, 4 px internal
  insets, 6 px interaction rounding, and teal active cues.
- Menus: 24 px rows, dark layered surface, 16 px icons, teal selection, and
  keyboard navigation inherited from the existing menu library.
- Decorations: 33 px title bar, dark active/inactive surfaces, readable title
  contrast, teal focus edge, and red close-button hover.
- Dialogs: dark surface, primary title, secondary body, teal links.
- Terminal: dark navy-charcoal default cell background, readable light
  foreground, Razion prompt colors, and existing terminal behavior retained.
- Login: dark inputs, semantic border/focus/error colors, original wallpaper
  treatment, and a minimal Razion mark.

## Wallpaper

`base/usr/share/wallpapers/razion-dark.jpg` is an original 1920×1080 project
asset generated for this stage. Prompt:

> Create an original premium minimalist desktop wallpaper for an open-source
> developer-focused operating system called RazionOS. Landscape 16:9, high
> resolution. Deep layered charcoal and blue-black background (not pure
> black), clean geometric architecture made of a few broad angled planes and
> a subtle abstract radial R-shaped negative-space motif centered slightly
> right. Restrained teal and cyan accent glow, very subtle depth, crisp clean
> geometry, calm professional mood, lots of uncluttered space for desktop
> icons and windows, no text, no logos, no UI mockup, no stars, no neon gaming
> aesthetic, no excessive gradients, no glassmorphism, no resemblance to
> Windows/macOS/GNOME/KDE wallpapers.

The repository stores a JPEG for compatibility with existing wallpaper
loaders. The Razion mark used by login and system information is deterministic
geometric artwork created specifically for the project.
