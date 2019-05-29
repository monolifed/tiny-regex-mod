# tiny-regex-mod
Single file modification of [tiny-regex-c](https://github.com/kokke/tiny-regex-c) by Kokke  

Adds a few features and removes some minor issues:
- made it into a single file library
- modified to return a pointer to the start of the match (instead of an integer)
- added option to get a pointer to the end of the match
- removed static use of regex object
- added quantifier operator `{m,n}` (also `{m}`, `{m,}`)
- added lazy quantifiers `??`, `*?`, `+?` and `{m,n}?`
- merged quantifier (?,*,+,{}) matching into two function (one for greedy, one for lazy)
- added upper limits to quantifiers
- (hopefully) fixed class range matching
- (hopefully) fixed handling of escape characters
- (hopefully) fixed `.` matching (doesn't match `\r` or `\n`)
- probably butchered print functionality
