import numpy as np
import matplotlib.pyplot as plt


def sum_abs_diff(source, target, pix, shift, N):
    y, x = pix
    y_shift, x_shift = shift

    src_pad = np.pad(source, ((N, N), (N, N), (0, 0)), mode="constant")
    tgt_pad = np.pad(target, ((N, N), (N, N), (0, 0)), mode="constant")

    src_patch = src_pad[
        y + y_shift - N : y + y_shift + N, x + x_shift - N : x + x_shift + N
    ]
    tgt_patch = tgt_pad[y - N : y + N, x - N : x + N]

    return np.sum(np.abs(src_patch - tgt_patch))


def push(source, target, lattice, N=8, M=8):
    R, C, _ = lattice.shape
    pushed_lattice = np.zeros_like(lattice)
    for i in range(R):
        for j in range(C):
            point = lattice[i, j, :]
            shift_vector = (0, 0)
            min_sum = 1e9
            for r in range(-M // 2, M // 2):
                for c in range(-M // 2, M // 2):
                    test_shift = (r, c)
                    test_sum = sum_abs_diff(source, target, point, test_shift, N // 2)
                    if test_sum < min_sum:
                        shift_vector = test_shift
                        min_sum = test_sum
            pushed_lattice[i, j, :] = point + shift_vector
    return pushed_lattice


def perp(pix):
    return np.array([pix[1], -pix[0]])


def square(top, left):
    return [top, top, top + 1, top + 1], [left, left + 1, left, left + 1]


def centroid(square):
    return square.mean(axis=0)


def regularize(lattice_rest, lattice_pushed):
    R, C, _ = lattice_rest.shape
    centroids = np.zeros_like(lattice_rest).astype(np.float16)
    contributing = np.zeros_like(lattice_rest).astype(np.float16)
    for r in range(R - 1):
        for c in range(C - 1):
            rows, cols = square(r, c)
            square_rest = lattice_rest[rows, cols]
            square_pushed = lattice_pushed[rows, cols]
            contributing[rows, cols] += 1

            p = centroid(square_rest)
            q = centroid(square_pushed)

            p_hat = square_rest - p
            q_hat = square_pushed - q

            a = 0
            b = 0
            for i in range(4):
                a += q_hat[i] @ p_hat[i].T
                b += q_hat[i] @ perp(p_hat[i]).T
            mu = np.sqrt(a**2 + b**2)

            R = 0
            for i in range(4):
                R += (
                    np.array([p_hat[i], perp(p_hat[i])])
                    @ np.array([q_hat[i], perp(q_hat[i])]).T
                )
            R /= mu

            t = p - R @ q

            square_reg = square_rest + t
            centroids[rows, cols] += square_reg

    return centroids / contributing


def arap(source, target, lattice_rest):
    while True:
        lattice_pushed = push(source, target, lattice_rest)
        lattice_reg = regularize(lattice_rest, lattice_pushed)
        diff = np.mean(np.linalg.norm(lattice_reg - lattice_pushed))
        if diff < 1e-4:
            break
        lattice_rest = lattice_reg.astype(np.int16)
        break
    return lattice_rest


source = plt.imread("source.png")[:, :, :3]
target = plt.imread("target.png")[:, :, :3]

grid_rows = 8
grid_cols = 8
grid_size = 10
i_range = np.arange(grid_rows) * grid_size + 10
j_range = np.arange(grid_cols) * grid_size + 10
I, J = np.meshgrid(i_range, j_range, indexing="ij")
lattice_rest = np.stack((I, J), axis=-1)

plt.imshow(source)
plt.scatter(J, I, c="red", s=10)
plt.show()

lattice_done = arap(source, target, lattice_rest)

plt.imshow(target)
plt.scatter(lattice_done[:, :, 1], lattice_done[:, :, 0], c="red", s=10)
plt.show()
