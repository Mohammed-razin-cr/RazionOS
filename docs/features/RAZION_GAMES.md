# Razion Games

RazionOS includes three native games built directly on the Yutani window server
and Razion graphics stack. They require no browser runtime or network access and
use the system's semantic theme colors.

## Razion Snake

- Move with the arrow keys or WASD.
- Press Space or P to pause and resume; press R to restart.
- The round remains on a ready screen until the player starts it.
- Speed increases gradually as the score rises.
- The game automatically pauses when its window loses focus.
- Score and best score are retained for the application session.
- A departing tail cell is correctly treated as free when the snake is not
  growing, preventing a false self-collision.

Snake repaints only after a game tick or visible interaction. While ready,
paused, or finished, it uses a long event wait and does not continuously redraw.
Pending messages are drained without blocking. A monotonic clock schedules
movement independently of input activity, so pointer events cannot delay ticks.

## Razion Tiles

- Move with the arrow keys or WASD.
- Press U to undo the most recent successful move.
- Press R to start a new game.
- The interface reports the 2048 milestone and a board with no remaining moves.
- Score and best score are retained for the application session.
- New Game and Undo have pointer-accessible controls and visible interaction
  states; Undo is visually disabled when unavailable.

Only successful moves create an undo snapshot or add a new tile. Unrelated keys
and repeated pointer events do not trigger unnecessary repaints.

## Razion Tic-Tac-Toe

See [RAZION_TICTACTOE.md](RAZION_TICTACTOE.md) for controls and implementation
details. Its computer strategy memoizes board evaluations, and its window also
uses state-change-only rendering.

## Verification

The x86_64 build and ISO packaging passed. VirtualBox checks covered desktop
boot, Snake start/movement/pause/resume/game-over, Tiles movement and exact
one-step board restoration, and a Tic-Tac-Toe player move with a computer reply.
Pointer controls, long sessions, and best-score persistence across application
restarts have not been verified; best scores are intentionally session-only.
