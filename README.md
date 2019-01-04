Easy to understand Garbage Collector implementation with mark-sweep algorithm.

This algorithm was invented by John McCarthy, the man who invented Lisp and beards.

It works like:

1.  Starting at the roots, traverse the entire object graph. Every time you reach an object, set 
    a “mark” bit on it to true.

2. Once that’s done, find all of the objects whose mark bits are not set and delete them.

According to Bob Nystrom's article: http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/