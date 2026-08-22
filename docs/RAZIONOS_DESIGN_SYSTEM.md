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

## Iconography

First-party applications use the project-owned `razion-*` icon family in
16 px, 24 px, and 48 px raster sizes, with editable SVG sources in
`assets/icons/`. Icons share a compact rounded-square silhouette, restrained
vertical highlight, subtle lower shadow, saturated semantic color, and a
high-contrast glyph. The family is intentionally Razion-specific and does not
reuse Apple, Microsoft, or other third-party application artwork.

Dock icons default to 40 px with 6 px spacing. Hover enlargement is limited
to 4 px and running applications use a small teal indicator rather than glow.

## Wallpaper

`base/usr/share/wallpapers/razion-premium.jpg` is the default original
1920×1080 project asset. The lossless source is retained at
`assets/razion-premium-source.png`; the installed JPEG uses 4:4:4 sampling for
the native decoder. Prompt:

> Create an original premium minimalist RazionOS wallpaper using deep
> black-navy, broad abstract flowing geometric forms, restrained teal with
> controlled blue and muted violet, a subtle geometric R made from layered
> planes and negative space slightly right of center, and a quiet left third
> for icons. No words, UI, people, third-party logos, watermark, neon gaming
> effects, stars, glassmorphism, clutter, or resemblance to another desktop.

The reference desktop image supplied for the final transformation was used
only as a quality and atmosphere reference; its branding and UI composition
were not copied. The Razion mark used by login and system information remains
deterministic geometric artwork created specifically for the project.
