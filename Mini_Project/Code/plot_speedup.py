import os
import matplotlib.pyplot as plt
from pathlib import Path

T100 = {1:15.225, 2:15.6005, 4:8.55156, 8:5.51891, 16:8.32291}
T500 = {1:461.751, 2:354.978, 4:195.055, 8:100.018, 16:60.7459}

def speedup(T):
    t1 = T[1]
    xs = sorted(T.keys())
    ys = [t1 / T[p] for p in xs]
    return xs, ys

x1, s100 = speedup(T100)
x2, s500 = speedup(T500)
ideal = x2[:]

plt.figure()
plt.plot(x1, s100, marker='o', label='N=100')
plt.plot(x2, s500, marker='o', label='N=500')
plt.plot(x2, ideal, linestyle='--', label='Ideal')

plt.xlabel('MPI tasks')
plt.ylabel('Speedup (T1 / Tp)')
plt.xticks(x2)
plt.legend()
plt.tight_layout()

out = Path(__file__).resolve().parent / "speedup.png"   # same folder as plot_speedup.py which is Mini_Project
plt.savefig(out, dpi=200)
print("Wrote:", out)

plt.show()  # show the plot on screen as well

