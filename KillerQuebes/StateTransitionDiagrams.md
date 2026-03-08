# KillerQuebes — State Transition Diagrams

## Block State Diagram

Shows the lifecycle of each block as it takes hits from the marble.

```
                 first hit              second hit
    +---------+ ---------- +---------+ ---------- +---------+
    |  Alive  | ---------> |   Hit   | ---------> |  Dead   |
    +---------+            +---------+            +---------+
    Normal skin          "block_red.png"       Moved to (9999,
    No damage            First hit taken       9999, 9999)
    Active in            Active in             Excluded from
    collision loop       collision loop        collision loop
```

### Block State Descriptions

| State | Skin             | Position      | Collision |
|-------|------------------|---------------|-----------|
| Alive | Default          | Grid position | Active    |
| Hit   | block_red.png    | Grid position | Active    |
| Dead  | N/A (off-screen) | (9999, 9999, 9999) | Skipped |

---

## Game State Diagram

Shows how the game transitions between states during play.

```
                         Space pressed
               +-----------------------------+
               |                             v
          +---------+                   +----------+
          |  Ready  |                   |  Firing  |
          +---------+                   +----------+
               ^                          |      |
               |    marble Z < 0          |      |
               |    or Z > 155            |      |
               +------------------------ -+      |
               |                            collision
               |                           detected
               |                                 |
               |                                 v
               |                          +-----------+
               |                          |  Contact  |
               |                          +-----------+
               |                            |       |
               |   blocks remaining         |       |  all blocks
               | +--------------------------+       |  dead
               | |                                  |
               | v                                  v
               | +----------+              +---------+
               | |  Firing  |              |  Over   |
               | +----------+              +---------+
               |                           Marble turns
               |                           green with
               |                           "marble_green.png"
               |
               |         +-----------------------------+
               |         |      Any State               |
               |         |  Escape key --> Exit Game     |
               |         +-----------------------------+
               |
```

### Game State Descriptions

| State   | Entry Condition              | Behaviour                                      | Exit Condition                  |
|---------|------------------------------|-------------------------------------------------|---------------------------------|
| Ready   | Game start / marble reset    | Arrow aims with Z/X keys, Space to fire         | Space pressed -> Firing         |
| Firing  | Space pressed / Contact done | Marble moves, collisions checked each frame     | Collision -> Contact, OOB -> Ready|
| Contact | Block collision detected     | Count dead blocks (resolves same frame)         | All dead -> Over, else -> Firing |
| Over    | All blocks destroyed         | Marble is green, no gameplay, Escape only       | Escape -> Exit                  |

### Transition Summary

- **Ready -> Firing**: Player presses Space; velocity calculated from arrow angle
- **Firing -> Contact**: Marble collides with a block (Alive or Hit)
- **Firing -> Ready**: Marble goes behind blocks (Z > 155) or past start (Z < 0)
- **Contact -> Over**: All blocks in the grid are Dead; marble turns green
- **Contact -> Firing**: At least one block still exists
- **Any -> Exit**: Escape key pressed at any time
