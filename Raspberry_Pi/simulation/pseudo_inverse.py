# https://cookierobotics.com/066/

import numpy as np

def normalize(B):
    scale = np.abs(B).max(axis=0)

    # Same scale on roll and pitch
    scale[1] = max(scale[1], scale[2])
    scale[2] = scale[1]

    return B / scale

A = np.array([[0, 1, 0, -0.23, 0, 0], [0, 0, 1, 0, 0.12, -0], [0, 0, 1, 0, -0.12, 0], [0, 1, 0, 0.117, 0, -0.12], [0, 1, 0, 0.117, -0, 0.12]])

B = np.linalg.pinv(A)

np.set_printoptions(precision=3)
np.set_printoptions(suppress=True)

print(f"A = {A}")
print(f"B = {B}")
print(f"normalized :\n{normalize(B)}")