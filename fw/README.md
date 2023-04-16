# FW

Counts the frequency of words

```
Usage:
	fw [-n num] [ files ...]
Options:
	-n	Set the number of most frequent words to display. Defaults to 10.
	files	The files to read words from. Defaults to reading from stdin.
```

### BENCHMARK

```shell
$ wc -w Lorem-ipsum-dolor-sit-amet.txt
1000000 Lorem-ipsum-dolor-sit-amet.txt
$ time ./fw Lorem-ipsum-dolor-sit-amet.txt
The top 10 words (out of 340) are:
    15285 in
    15284 vestibulum
    14557 ut
    13100 ac
    13099 sed
    11646 et
    10918 quis
    10917 pellentesque
    10916 praesent
     9463 phasellus

________________________________________________________
Executed in  436.48 millis    fish           external
   usr time  435.92 millis  528.00 micros  435.39 millis
   sys time    0.17 millis  165.00 micros    0.00 millis
```
