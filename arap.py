from typing import List
import numpy as np
from copy import deepcopy
import matplotlib.pyplot as plt
from math import ceil


def sum_abs_diff(source, target, point, shift, N_half):
    sx, sy = point.init.astype(int)
    tx, ty = point.pos.astype(int)
    dx, dy = shift
    H, W = source.shape[:2]

    r_y_min = max(-N_half, -sy, -ty - dy)
    r_y_max = min(N_half, H - sy, H - ty - dy)
    r_x_min = max(-N_half, -sx, -tx - dx)
    r_x_max = min(N_half, W - sx, W - tx - dx)
    if r_y_min >= r_y_max or r_x_min >= r_x_max:
        return 1e9

    src_patch = source[sy + r_y_min : sy + r_y_max, sx + r_x_min : sx + r_x_max]
    tgt_patch = target[
        ty + dy + r_y_min : ty + dy + r_y_max, tx + dx + r_x_min : tx + dx + r_x_max
    ]
    return np.mean(np.abs(src_patch - tgt_patch))


class Point:
    def __init__(self, x, y):
        self.pos: np.ndarray = np.array([x, y], dtype=np.float64)
        self.init: np.ndarray = np.array([x, y], dtype=np.float64)
        self.instances: List[Point] = []

    def __repr__(self):
        return f"x: {self.pos[0]}, y: {self.pos[1]}"

    def rotate(self, R):
        self.pos = R @ self.pos

    def translate(self, t):
        self.pos = t + self.pos

    def add_instance(self, point):
        self.instances.append(point)

    def move_to_centroid(self):
        self.pos = centroid(self.instances, "pos").astype(int)

    def push(self, source, target, N, M):
        shift_vector = (0, 0)
        min_sum = sum_abs_diff(source, target, self, shift_vector, N // 2)
        for r in range(-M // 2, M // 2 + 1):
            for c in range(-M // 2, M // 2 + 1):
                test_shift = (c, r)
                test_sum = sum_abs_diff(source, target, self, test_shift, N // 2)
                if test_sum < min_sum:
                    shift_vector = test_shift
                    min_sum = test_sum
        self.pos += shift_vector


def perp(pos):
    return np.array([pos[1], -pos[0]])


def centroid(points: List[Point], attr: str = "pos") -> np.ndarray:
    res = np.zeros_like(points[0].pos).astype(np.float64)
    for point in points:
        res += eval(f"point.{attr}")
    return res / len(points)


def corners(top, left):
    return [top, left], [top, left + 1], [top + 1, left], [top + 1, left + 1]


class Square:
    def __init__(self, points):
        self.global_points: List[Point] = points
        self.local_points: List[Point] = deepcopy(points)

        for i in range(len(points)):
            self.global_points[i].add_instance(self.local_points[i])

    def __repr__(self):
        return f"""Square:\n\t{self.global_points[0]}\n\t{self.global_points[1]}\n\t{self.global_points[2]}\n\t{self.global_points[3]}\n"""

    def regularize(self):
        p_c = centroid(self.local_points, "init")
        q_c = centroid(self.global_points, "pos")

        a = 0.0
        b = 0.0
        for i in range(4):
            p_hat = self.local_points[i].init - p_c
            q_hat = self.global_points[i].pos - q_c
            a += q_hat @ p_hat
            b += q_hat @ perp(p_hat)

        mu = np.sqrt(a**2 + b**2)
        if mu < 1e-10:
            return

        R = np.array([[a, -b], [b, a]]) / mu

        for i in range(4):
            p_hat = self.local_points[i].init - p_c
            self.local_points[i].pos = R @ p_hat + q_c


class Lattice:
    def __init__(self, source: np.ndarray, target: np.ndarray, grid_size: int = 10):
        self.source = source
        self.target = target

        self.points: List[List[Point]] = []
        self.points_flat: List[Point] = []
        self.squares: List[Square] = []

        H, W, C = source.shape
        self.H, self.W, self.grid_size = H, W, grid_size
        self.rows = ceil(H / grid_size) + 1
        self.cols = ceil(W / grid_size) + 1

        i_range = (np.arange(self.rows) * grid_size).clip(0, H - 1)
        j_range = (np.arange(self.cols) * grid_size).clip(0, W - 1)
        I, J = np.meshgrid(i_range, j_range, indexing="ij")
        lattice_init = np.stack((I, J), axis=-1)

        for i in range(self.rows):
            row = []
            for j in range(self.cols):
                lattice_coord = lattice_init[i, j]
                lattice_point = Point(lattice_coord[1], lattice_coord[0])
                self.points_flat.append(lattice_point)
                row.append(lattice_point)
            self.points.append(row)

        for i in range(self.rows - 1):
            for j in range(self.cols - 1):
                points = []
                for r, c in corners(i, j):
                    points.append(self.points[r][c])
                lattice_sq = Square(points)
                self.squares.append(lattice_sq)

    def fit(self, N=10, M=20, iters=30):
        for _ in range(iters):
            for point in self.points_flat:
                point.push(self.source, self.target, N, M)
            for sq in self.squares:
                sq.regularize()
            for p in self.points_flat:
                p.move_to_centroid()

    def correspondence_map(self) -> np.ndarray:
        pos_grid = np.zeros((self.rows, self.cols, 2), dtype=np.float64)
        for i in range(self.rows):
            for j in range(self.cols):
                pos_grid[i, j] = self.points[i][j].pos

        ys, xs = np.meshgrid(np.arange(self.H), np.arange(self.W), indexing="ij")
        i_idx = ys // self.grid_size
        j_idx = xs // self.grid_size

        u = ((xs - j_idx * self.grid_size) / self.grid_size)[..., None]
        v = ((ys - i_idx * self.grid_size) / self.grid_size)[..., None]

        P00 = pos_grid[i_idx, j_idx]
        P01 = pos_grid[i_idx, j_idx + 1]
        P10 = pos_grid[i_idx + 1, j_idx]
        P11 = pos_grid[i_idx + 1, j_idx + 1]

        new_xy = (
            (1 - u) * (1 - v) * P00
            + u * (1 - v) * P01
            + (1 - u) * v * P10
            + u * v * P11
        )
        return np.stack([new_xy[..., 1], new_xy[..., 0]], axis=-1)


source = plt.imread("dummy_data/arap_test/small_walk_0000.png")[:, :, :3]
target = plt.imread("dummy_data/arap_test/small_walk_0002.png")[:, :, :3]

grid_size = 20
M = 40
iters = 20

test = Lattice(source, target, grid_size)
test.fit(grid_size, M, iters)
corr = test.correspondence_map()
map_y = np.rint(corr[..., 0]).astype(int)
map_x = np.rint(corr[..., 1]).astype(int)

np.savetxt("test_map_x.csv", map_x, fmt="%i", delimiter=",")
np.savetxt("test_map_y.csv", map_y, fmt="%i", delimiter=",")
