from typing import List
import numpy as np
from copy import deepcopy
import matplotlib.pyplot as plt
from scipy.interpolate import griddata
from scipy.ndimage import map_coordinates


def sum_abs_diff(source, target, pix, shift, N_half):
    x, y = pix.astype(int)
    dx, dy = shift
    H, W = source.shape[:2]

    r_y_min = max(-N_half, -y, -y - dy)
    r_y_max = min(N_half, H - y, H - y - dy)

    r_x_min = max(-N_half, -x, -x - dx)
    r_x_max = min(N_half, W - x, W - x - dx)

    if r_y_min >= r_y_max or r_x_min >= r_x_max:
        return 1e9

    src_patch = source[y + r_y_min : y + r_y_max, x + r_x_min : x + r_x_max]
    tgt_patch = target[
        y + dy + r_y_min : y + dy + r_y_max, x + dx + r_x_min : x + dx + r_x_max
    ]

    return np.mean(np.abs(src_patch - tgt_patch))


class Point:
    def __init__(self, x, y):
        self.pos: np.ndarray = np.array([x, y])
        self.init: np.ndarray = np.array([x, y])
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
        if len(self.instances) == 0:
            return
        self.pos = centroid(self.instances)

    def push(self, source, target, N, M):
        shift_vector = (0, 0)
        min_sum = sum_abs_diff(source, target, self.pos, shift_vector, N // 2)
        for r in range(-M // 2, M // 2 + 1):
            for c in range(-M // 2, M // 2 + 1):
                test_shift = (c, r)
                test_sum = sum_abs_diff(source, target, self.pos, test_shift, N // 2)
                if test_sum < min_sum:
                    shift_vector = test_shift
                    min_sum = test_sum
        self.pos += shift_vector


def perp(pos):
    return np.array([pos[1], -pos[0]])


def centroid(points: List[Point]) -> np.ndarray:
    res = np.zeros_like(points[0].pos).astype(np.float64)
    for point in points:
        res += point.pos
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
        p_c = centroid(self.global_points)
        q_c = centroid(self.local_points)

        a = 0.0
        b = 0.0
        for i in range(4):
            p_hat = self.global_points[i].pos - p_c
            q_hat = self.local_points[i].pos - q_c
            a += p_hat @ q_hat
            b += p_hat @ perp(q_hat)

        mu = np.sqrt(a**2 + b**2)
        if mu < 1e-10:
            return

        R = np.array([[a, -b], [b, a]]) / mu

        for i in range(4):
            q_hat = self.local_points[i].pos - q_c
            self.local_points[i].pos = R @ q_hat + p_c


class Lattice:
    def __init__(self, source: np.ndarray, target: np.ndarray, grid_size: int = 10):
        self.points: List[List[Point]] = []
        self.points_flat: List[Point] = []
        self.squares: List[Square] = []

        H, W, C = source.shape

        assert H % grid_size == 0
        assert W % grid_size == 0

        grid_rows = H // grid_size - 1
        grid_cols = W // grid_size - 1

        i_range = np.arange(grid_rows) * grid_size + grid_size
        j_range = np.arange(grid_cols) * grid_size + grid_size
        I, J = np.meshgrid(i_range, j_range, indexing="ij")
        lattice_init = np.stack((I, J), axis=-1)

        for i in range(grid_rows):
            row = []
            for j in range(grid_cols):
                lattice_coord = lattice_init[i, j]
                lattice_point = Point(lattice_coord[1], lattice_coord[0])
                self.points_flat.append(lattice_point)
                row.append(lattice_point)
            self.points.append(row)

        for i in range(grid_rows - 1):
            for j in range(grid_cols - 1):
                points = []
                for r, c in corners(i, j):
                    points.append(self.points[r][c])
                lattice_sq = Square(points)
                self.squares.append(lattice_sq)


source = plt.imread("source.png")[:, :, :3]
target = plt.imread("target.png")[:, :, :3]
test = Lattice(source, target, 10)
for point in test.points_flat:
    point.push(source, target, 10, 20)

for square in test.squares:
    square.regularize()

for point in test.points_flat:
    point.move_to_centroid()

init_pos = np.array([p.init for p in test.points_flat], dtype=float)
final_pos = np.array([p.pos for p in test.points_flat], dtype=float)

H, W, C = source.shape
pts_target = final_pos
pts_source = init_pos

grid_y, grid_x = np.mgrid[0:H, 0:W]

map_x = griddata(pts_target, pts_source[:, 0], (grid_x, grid_y), method="linear")
map_y = griddata(pts_target, pts_source[:, 1], (grid_x, grid_y), method="linear")

nan_mask = np.isnan(map_x) | np.isnan(map_y)
map_x[nan_mask] = grid_x[nan_mask]
map_y[nan_mask] = grid_y[nan_mask]

warped = np.zeros_like(source)
for ch in range(C):
    warped[..., ch] = map_coordinates(
        source[..., ch], [map_y, map_x], order=1, mode="constant", cval=1.0
    )

fig, axes = plt.subplots(1, 3, figsize=(21, 7))

axes[0].imshow(source)
axes[0].scatter(init_pos[:, 0], init_pos[:, 1], color="red", s=14, zorder=3)
axes[0].set_title("Source")
axes[0].axis("off")

axes[1].imshow(target)
axes[1].set_title("Target")
axes[1].axis("off")

axes[2].imshow(np.clip(warped, 0, 1))
axes[2].scatter(final_pos[:, 0], final_pos[:, 1], color="blue", s=14, zorder=3)
for ip, fp in zip(init_pos, final_pos):
    if np.linalg.norm(fp - ip) > 0.5:
        axes[2].annotate(
            "",
            xy=(fp[0], fp[1]),
            xytext=(ip[0], ip[1]),
            arrowprops=dict(
                arrowstyle="->", color="green", lw=0.7, shrinkA=0, shrinkB=0
            ),
            zorder=2,
        )
axes[2].set_title("Warped source")
axes[2].axis("off")

plt.tight_layout()
plt.show()
