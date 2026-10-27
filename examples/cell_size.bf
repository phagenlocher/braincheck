A loop that only ends when the cell is too narrow to hold 256
The first loop adds sixteen to the second cell sixteen times which makes 256
With eight bit cells that value wraps to zero so the following loop is skipped and the program halts
With sixteen or thirty two bit cells the second cell holds 256 and the following loop adds 512 per round
Adding 512 never changes the remainder modulo 512 and 256 is not zero modulo 512
So the second cell never becomes zero and the loop never ends

++++++++++++++++[>++++++++++++++++<-]>[<++++++++++++++++[>++++++++++++++++++++++++++++++++<-]>]_halt_++++++++++.
