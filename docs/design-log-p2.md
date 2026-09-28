# Design Log — Project 2

**Author:** Diego Mejia

## Growth factor and amortized cost

`Conversation` starts with no buffer. The first `append` allocates 4 slots. After that, whenever the array is full (`size_ == capacity_`), `append` allocates a buffer twice as large, moves the existing messages into it, and deletes the old one. The capacities are therefore 4, 8, 16, 32, and so on.

**Claim:** n appends take O(n) total work, so one append is O(1) amortized.

**Proof.** Every append writes one message into a free slot, which is n writes in total. The expensive part is reallocation. A reallocation happens only when the array is full, i.e. at sizes 4, 8, 16, …, m, where m is the largest power-of-two multiple of 4 below n. Reallocating at size s moves s messages, so the total number of moves is

4 + 8 + 16 + … + m = 2m − 4 < 2n.

Adding the n writes, the total is under 3n, which is O(n). Dividing by n appends gives O(1) per append.

This works because capacity is *multiplied*. With "+1" growth, every append would reallocate, costing 1 + 2 + … + n = O(n²). I chose 2 because it gives the simplest proof and never leaves more than half the buffer unused.

## Rule of Five evidence

- **Destructor:** `delete[] data_`. It is safe on an empty or moved-from object, because deleting `nullptr` does nothing.
- **Copy constructor:** allocates a new buffer and copies every message into it, so the copy and the original never share memory. It allocates even when the source is empty, so `begin()` always differs. If copying a string throws, it deletes the new buffer before rethrowing, so nothing leaks.
- **Copy assignment:** copy-and-swap. It builds a full copy first, then swaps with it, and the temporary frees the old buffer. This handles self-assignment, and `*this` is unchanged if the copy fails.
- **Move constructor:** takes the source's pointer, size and capacity, then sets the source to `nullptr`, 0 and 0. No messages are copied.
- **Move assignment:** deletes its own buffer, takes the source's, and zeroes the source. It skips all of this for self-move.

The tests check each of these directly:
- `CopyIsDeep` asserts that the `begin()` pointers differ and that changing the original doesn't change the copy.
- `MoveStealsPointer` asserts that the new object's `begin()` equals the old pointer and that the source is `nullptr` and empty.

The suite runs under AddressSanitizer, so a double free or a leak fails the run. The final code has zero sanitizer errors.

## Sentinel scanner: bounded pending_ proof

Let S be the sentinel length (20 for `<|end_conversation|>`). On each `feed`, the scanner searches `text = pending_ + chunk`:

- If the sentinel is found at position p, it returns `text[0, p)` as safe text, clears `pending_`, and reports `sentinel_found = true`.
- Otherwise it keeps the last `k = min(|text|, S − 1)` characters in `pending_` and returns the rest as safe.

**Bound.** `pending_` is only ever assigned an empty string or a string of length k ≤ S − 1, and it starts empty. By induction on the number of calls, |pending_| ≤ S − 1 after every call. For this sentinel that is at most 19 characters, however long the stream is.

**Correctness.** Suppose the sentinel starts at some position in the stream but is not finished in the current chunk. Then at most S − 1 of its characters have arrived so far, and they are the last characters of `text`. Those are exactly the characters kept in `pending_`, so no part of the sentinel is ever emitted as safe text. On a later call they are searched together with the next chunk, so the full sentinel is found wherever the chunk boundaries fall. If the stream ends without a sentinel, `flush()` returns what is left.

**Cost.** Each call searches only S − 1 + |chunk| characters, instead of the O(N²) of re-searching the whole reply. `ScannerPendingStaysBounded` feeds 4 MB of near-misses one byte at a time and asserts `pending_size() ≤ 19` after every byte.

## What I would change differently

The scanner always holds back the last 19 characters, even when they cannot start the sentinel, so the end of every reply appears late. I would hold back only the longest ending of the text that matches the beginning of the sentinel. That is usually zero characters, so text would print immediately, with the same bound.
