# wonderful-chess
A UCI chess engine written in C. Challenge it here: https://lichess.org/@/wonderful-chess

## Build
Requires GCC or Clang (uses GCC builtins; MSVC is not supported).

```sh
# Linux
gcc -O3 -march=native -flto -fno-math-errno main.c magic.c -lm -o wonderful-chess

# Windows (MinGW) / macOS
gcc -O3 -march=native -flto -fno-math-errno main.c magic.c -o wonderful-chess
```

For a binary that runs on other machines, replace `-march=native` with `-march=x86-64-v2`.

## Usage

UCI engine: load it in any UCI GUI (Cute Chess, Arena).
Uses ~830 MB of RAM (transposition table).

Movegen sanity check:
```
position startpos
go perft 5
```
Expected total: 4865609
