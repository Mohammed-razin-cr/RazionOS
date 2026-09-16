# Razion Tic-Tac-Toe

Razion Tic-Tac-Toe is a native RazionOS strategy game implemented with the
existing Yutani window server and Razion graphics stack. It does not require a
network connection, browser runtime, or third-party game framework.

## Playing

- Select a square with the pointer, or use the arrow keys.
- Click, press Enter, or press Space to place X.
- Select **New round** or press **R** to clear the board.
- Press **Esc** to close the game.

The player is X and always opens the round. The computer is O. Its move is
selected with a minimax search over the remaining board states, so it plays a
legal best move rather than choosing a random square. Wins, draws, and computer
wins are counted for the current application session.

## Performance

Board evaluations are memoized by compact ternary board state, allowing later
computer moves and new rounds to reuse earlier results. Preferred move ordering
also finds strong candidates early. The window redraws only when visible state
changes, avoiding unnecessary rendering for repeated pointer events.

## Integration

The application binary is `/bin/razion-tictactoe`. It is available from the
Games menu, Universal Search, and the Games category in Razion Store. The app
uses semantic RazionOS theme colors and supports both dark and light themes.
