# RazionOS boot animation timeline

The splash is event-driven. It never displays a percentage and does not advance
through boot stages without a matching startup event.

1. The framebuffer fades from matte black and draws the original Razion mark
   in seven monoline vector strokes.
2. The completed mark receives a restrained electric-blue under-stroke.
3. Startup scripts send real `@razion:<stage>` events through the existing PEX
   `splash` endpoint. The progress line eases only toward the corresponding
   milestone.
4. `!ready` is emitted immediately before the compositor starts. The splash
   contracts and fades for a bounded 360 ms, then releases the framebuffer.
5. The compositor applies its 420 ms desktop fade-in.
