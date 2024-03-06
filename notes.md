# Introduction
CISC bad, RISC good
Specialized instructions were added to make assembly programming easier, but simple instructions are easier to optimize
Dynamic scoping examples: C preprocessor macros and virtual method resolution
* Call By Name:
    Not favored partially because of often unexpected behavior when used with expressions
    Are C preprocessor macros call by name? 
    * Similar. Call By Name requires local variable renaming in the called procedure -- macro expansion performs only textual substitution, and no renaming.
* Aliasing potential kills optimizations -- eliminating aliasing could be good? Rust does this with ownership in some instances since ownership of a passed parameter cannot happen twice?

# Chapter 2
