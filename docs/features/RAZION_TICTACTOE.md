# Razion Tic-Tac-Toe

Razion Tic-Tac-Toe is a native RazionOS strategy game implemented with the
existing Yutani window server and Razion graphics stack. It does not require a
network connection, browser runtime, or third-party game framework.

## Playing

- Select a square with the pointer, or use the arrow keys.
- Press number keys 1 through 9 to play directly in a square.
- Click, press Enter, or press Space to place X.
- Select **New round** or press **R** to clear the board.
- Press **Esc** to close the game.

The player is X and always opens the round. The computer is O. Its move is
selected with solved opening rules, immediate win/block checks, and a minimax
search over the remaining board states, so it plays a legal best move rather
than choosing a random square. Wins, draws, and computer wins are counted for
the current application session.

## Performance

The first computer reply uses a constant-time opening rule. Later moves use
alpha-beta minimax with memoized compact ternary board states, allowing new
rounds to reuse earlier results. Immediate win and block checks avoid deeper
search when the correct move is obvious. The window redraws only when visible
state changes, avoiding unnecessary rendering for repeated pointer events.

## Integration

The application binary is `/bin/razion-tictactoe`. It is available from the
Games menu, Universal Search, and the Games category in Razion Store. The app
uses semantic RazionOS theme colors and supports both dark and light themes.
