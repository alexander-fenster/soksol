soksol
======

sokoban solver

A simple BFS-based Sokoban solver.
Author: Alexander Fenster, fenster@fenster.name, 2013
Reads the field from input.txt.

Note that maximum queue size can be as much as
    binomial(number of cells, number of boxes) * (number of cells - number of boxes).
This number can be huge for large fields.

Input file format:

The first two numbers define number of rows and columns in the field.
The field is defined in the following way:
- # for the wall
- . for the empty cell
- o for the destination cell
- x for the block
- X for the block in the destination cell
- s for the start position
- S for the start position in the destination cell

Everything after the field is ignored.
