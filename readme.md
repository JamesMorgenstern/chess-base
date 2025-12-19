First, I added a FEN string to board function.

Then, I added Knight, King, and Pawn movement as well as swapping between white and black every move.
I also started the generateAllMoves function that will be used for negamax later.

I most recently added AI using negamax with alpha beta pruning. It still only can use Knight, King, and Pawn movement but it still evaluates all the pieces properly.
One big problem I had when implementing was that the AI thought it was playing as white when it should've been playing as black.
Also, I had a problem where the stateString was being updated so after a few turns the memory would have problems and the program would crash. This took me way too long to figure out how to fix.
The negamax plays with a depth of 3 and I feel like the AI plays wonderfully; better than I could play.
