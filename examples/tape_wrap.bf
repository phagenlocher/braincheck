A scan to the left that only terminates when the tape wraps around
The tape is four cells wide and the scan starts at cell zero with a nonzero value
Cells zero one and three are nonzero and cell two is zero
With the default wrapping pointer the scan runs off the low edge and continues at cell three
It then walks two cells to the left and stops at the zero in cell two and the program halts
With the no wrapping flag the pointer stays at cell zero and the scan never sees a zero
Run this example with memory size four to keep the tape short

+>+>>+<<<[<]_done_++++++++++.
