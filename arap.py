def sum_abs_diff(image, pix, shift, N):
    res = 0
    y, x = pix
    y_shift, x_shift = shift
    H, W = image.shape
    for r in range(max(0, y - N), min(H - 1, y + N)):
        for c in range(max(0, x - N), min(W - 1, x + N)):
            res += abs(image[y + y_shift + r][x + x_shift + c] - image[y + r][x + c])
    return res


def push(lattice):
    N = 16
    M = 48
    for point in lattice:
        shift_vector = (0, 0)
        min_sum = 1e9
        x, y = point
        for r in range(-M // 2, M // 2):
            for c in range(-M // 2, M // 2):
                test_shift = (r, c)
                test_sum = sum_abs_diff(image, point, test_shift, N // 2)
                if test_sum < min_sum:
                    shift_vector = test_shift
                    min_sum = test_sum
        point = point + shift_vector


def perp(pix):
    return [pix[1], -pix[0]]


def regularize(lattice_rest, lattice_pushed):
    centroids = np.array(num_points, 3)
    contributing = np.array(num_points)
    for i in range(len(lattice)):
        square_rest = lattice_rest[i]
        square_pushed = lattice_pushed[i]
        a, b, c, d = square_rest
        contributing[a, b, c, d] += 1

        p = centroid(square_rest)
        q = centroid(square_pushed)

        p_hat = square_rest - p
        q_hat = square_pushed - q

        a = 0
        b = 0
        for i in range(4):
            a += q_hat[i] @ p_hat[i].T
            b += q_hat[i] @ perp(p_hat[i]).T
        mu = sqrt(a**2 + b**2)

        R = 0
        for i in range(4):
            R += [p_hat[i], perp(p_hat[i])] @ [q_hat[i], perp(q_hat[i])].T
        R /= mu

        t = p - R @ q

        square_reg = square_rest + t
        centroids[a, b, c, d] += square_reg
    return centroids / contributing


def arap(image):
    while True:
        lattice_rest = image.grid
        lattice_pushed = push(lattice_rest)
        lattice_reg = regularize(lattice_rest, lattice_pushed)
        diff = np.mean(np.linalg.norm(lattice_reg - lattice_pushed))
        if diff < 1e-4:
            break
        lattice_rest = lattice_reg
