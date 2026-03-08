# KillerQuebes — State Transition Diagrams

## BLOCK STATE DIAGRAM
```
  [Alive] --first hit--> [Hit] --second hit--> [Dead]
```
  - **Alive**: normal skin
  - **Hit**: red skin ("block_red.png")
  - **Dead**: moved to (9999,9999,9999), excluded from collision

## GAME STATE DIAGRAM
```
  [Ready] --Space pressed--> [Firing]
  [Firing] --collision detected--> [Contact]
  [Firing] --marble Z < 0--> [Ready]
  [Firing] --marble Z > 155--> [Ready]
  [Contact] --all blocks dead--> [Over]
  [Contact] --blocks remaining--> [Firing]
  Any state --Escape--> Exit
```
  - marble turns green on entering Over state
