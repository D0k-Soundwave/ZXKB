"""Compare two level files block by block. Pass = every band within TOL of the old app.
TOL = 2 of 255: one key along a bar is ~17 (255 / 15 keys), so 2 can never move a bar's lit edge by a key."""
import sys
TOL = 2
a = [list(map(int, l.split())) for l in open(sys.argv[1])]
b = [list(map(int, l.split())) for l in open(sys.argv[2])]
n = min(len(a), len(b)); worst = 0; over = 0; total = 0
for i in range(n):
    for x, y in zip(a[i], b[i]):
        d = abs(x - y); worst = max(worst, d); total += 1; over += d > TOL
print(f"blocks: old {len(a)}, new {len(b)}; values compared {total}; worst difference {worst}; over {TOL}: {over}")
sys.exit(0 if len(a) == len(b) and over == 0 else 1)
