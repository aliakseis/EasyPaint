import cv2
import numpy as np

# Thinning types (match C++ enum)
THINNING_ZHANGSUEN = 0
THINNING_GUOHALL = 1

# Try to import fast ximgproc thinning if available
try:
    from cv2.ximgproc import thinning as x_thinning
    HAVE_XTHIN = True
except Exception:
    HAVE_XTHIN = False

# -------------------------
# Exact Python thinning (fallback)
# -------------------------
def _thinning_iteration(img: np.ndarray, iter_idx: int, thinning_type: int) -> None:
    rows, cols = img.shape
    marker = np.zeros_like(img, dtype=np.uint8)

    if thinning_type == THINNING_ZHANGSUEN:
        for i in range(1, rows - 1):
            for j in range(1, cols - 1):
                p2 = img[i - 1, j]
                p3 = img[i - 1, j + 1]
                p4 = img[i, j + 1]
                p5 = img[i + 1, j + 1]
                p6 = img[i + 1, j]
                p7 = img[i + 1, j - 1]
                p8 = img[i, j - 1]
                p9 = img[i - 1, j - 1]

                A = (int(p2 == 0 and p3 == 1) + int(p3 == 0 and p4 == 1) +
                     int(p4 == 0 and p5 == 1) + int(p5 == 0 and p6 == 1) +
                     int(p6 == 0 and p7 == 1) + int(p7 == 0 and p8 == 1) +
                     int(p8 == 0 and p9 == 1) + int(p9 == 0 and p2 == 1))
                B = int(p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9)
                if iter_idx == 0:
                    m1 = p2 * p4 * p6
                    m2 = p4 * p6 * p8
                else:
                    m1 = p2 * p4 * p8
                    m2 = p2 * p6 * p8

                if (A == 1) and (2 <= B <= 6) and (m1 == 0) and (m2 == 0):
                    marker[i, j] = 1

    if thinning_type == THINNING_GUOHALL:
        for i in range(1, rows - 1):
            for j in range(1, cols - 1):
                p2 = img[i - 1, j]
                p3 = img[i - 1, j + 1]
                p4 = img[i, j + 1]
                p5 = img[i + 1, j + 1]
                p6 = img[i + 1, j]
                p7 = img[i + 1, j - 1]
                p8 = img[i, j - 1]
                p9 = img[i - 1, j - 1]

                C = (int(p2 == 0) & (p3 | p4)) + (int(p4 == 0) & (p5 | p6)) + \
                    (int(p6 == 0) & (p7 | p8)) + (int(p8 == 0) & (p9 | p2))
                N1 = (p9 | p2) + (p3 | p4) + (p5 | p6) + (p7 | p8)
                N2 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9)
                N = N1 if N1 < N2 else N2
                if iter_idx == 0:
                    m = ((p6 | p7 | int(p9 == 0)) & p8)
                else:
                    m = ((p2 | p3 | int(p5 == 0)) & p4)

                if (C == 1) and ((N >= 2) and ((int((N <= 3)) & int(m == 0)) != 0)):
                    marker[i, j] = 1

    np.bitwise_and(img, np.bitwise_not(marker), out=img)


def _thinning_python(src: np.ndarray, thinning_type: int = THINNING_ZHANGSUEN) -> np.ndarray:
    if src.ndim != 2:
        raise ValueError("thinning: input must be a single-channel image")

    processed = src.copy().astype(np.uint8)
    processed = (processed // 255).astype(np.uint8)

    prev = np.zeros_like(processed, dtype=np.uint8)
    diff = np.empty_like(processed, dtype=np.uint8)

    while True:
        _thinning_iteration(processed, 0, thinning_type)
        _thinning_iteration(processed, 1, thinning_type)
        cv2.absdiff(processed, prev, diff)
        if np.count_nonzero(diff) == 0:
            break
        prev[:] = processed

    processed = (processed * 255).astype(np.uint8)
    return processed

def _thinning(src: np.ndarray) -> np.ndarray:
    """
    Choose thinning automatically: use fast ximgproc.thinning if available,
    otherwise fall back to the exact Python implementation (Zhang-Suen).
    """
    if HAVE_XTHIN:
        return x_thinning(src)
    else:
        return _thinning_python(src, thinning_type=THINNING_ZHANGSUEN)

# -------------------------
# removePepperNoise (exact logic)
# -------------------------
def _remove_pepper_noise(mask: np.ndarray) -> None:
    rows, cols = mask.shape
    for y in range(2, rows - 2):
        pThis = mask[y]
        pUp1 = mask[y - 1]
        pUp2 = mask[y - 2]
        pDown1 = mask[y + 1]
        pDown2 = mask[y + 2]

        x = 2
        while x < cols - 2:
            v = pThis[x]
            if v == 0:
                allAbove = (pUp2[x - 2] and pUp2[x - 1] and pUp2[x] and pUp2[x + 1] and pUp2[x + 2])
                allLeft = (pUp1[x - 2] and pThis[x - 2] and pDown1[x - 2])
                allBelow = (pDown2[x - 2] and pDown2[x - 1] and pDown2[x] and pDown2[x + 1] and pDown2[x + 2])
                allRight = (pUp1[x + 2] and pThis[x + 2] and pDown1[x + 2])
                surroundings = allAbove and allLeft and allBelow and allRight
                if surroundings:
                    mask[y - 1, x - 1] = 255
                    mask[y - 1, x + 0] = 255
                    mask[y - 1, x + 1] = 255
                    mask[y + 0, x - 1] = 255
                    mask[y + 0, x + 0] = 255
                    mask[y + 0, x + 1] = 255
                    mask[y + 1, x - 1] = 255
                    mask[y + 1, x + 0] = 255
                    mask[y + 1, x + 1] = 255
                    x += 3
                    continue
            x += 1

# -------------------------
# changeFacialSkinColor (exact logic)
# -------------------------
def _change_facial_skin_color(small_img_bgr: np.ndarray, big_edges: np.ndarray, debug_type: int) -> None:
    yuv = cv2.cvtColor(small_img_bgr, cv2.COLOR_BGR2YCrCb)

    sw = small_img_bgr.shape[1]
    sh = small_img_bgr.shape[0]

    mask_plus_border = np.zeros((sh + 2, sw + 2), dtype=np.uint8)
    mask = mask_plus_border[1:1 + sh, 1:1 + sw]
    resized_edges = cv2.resize(big_edges, (sw, sh), interpolation=cv2.INTER_LINEAR)
    mask[:, :] = resized_edges

    _, mask[:] = cv2.threshold(mask, 80, 255, cv2.THRESH_BINARY)
    cv2.dilate(mask, mask, None)
    cv2.erode(mask, mask, None)

    skin_pts = [
        (sw // 2,          sh // 2 - sh // 6),
        (sw // 2 - sw // 11,  sh // 2 - sh // 6),
        (sw // 2 + sw // 11,  sh // 2 - sh // 6),
        (sw // 2,          sh // 2 + sh // 16),
        (sw // 2 - sw // 9,   sh // 2 + sh // 16),
        (sw // 2 + sw // 9,   sh // 2 + sh // 16),
    ]

    LOWER_Y = 60
    UPPER_Y = 80
    LOWER_Cr = 25
    UPPER_Cr = 15
    LOWER_Cb = 20
    UPPER_Cb = 15
    lower_diff = (LOWER_Y, LOWER_Cr, LOWER_Cb)
    upper_diff = (UPPER_Y, UPPER_Cr, UPPER_Cb)

    edge_mask = mask.copy()

    flags = 4 | cv2.FLOODFILL_FIXED_RANGE | cv2.FLOODFILL_MASK_ONLY
    for pt in skin_pts:
        cv2.floodFill(yuv, mask_plus_border, pt, (0, 0, 0), lower_diff, upper_diff, flags)
        if debug_type >= 1:
            cv2.circle(small_img_bgr, pt, 5, (0, 0, 255), 1, cv2.LINE_AA)

    if debug_type >= 2:
        cv2.imshow("flood mask", (mask * 120).astype(np.uint8))

    skin_mask = cv2.subtract(mask, edge_mask)

    Red = 0
    Green = 70
    Blue = 0
    add_img = np.full_like(small_img_bgr, (Blue, Green, Red), dtype=np.uint8)
    cv2.add(small_img_bgr, add_img, small_img_bgr, mask=skin_mask)


# -------------------------
# cartoonifyImage (optimized A-route, automatic thinning, contour threshold)
# -------------------------
def cartoonify_image(src_color: np.ndarray,
                     sketch_mode: bool = False,
                     alien_mode: bool = False,
                     evil_mode: bool = False,
                     debug_type: int = 0,
                     contour_threshold: float = 0.5) -> np.ndarray:
    """
    Convert the given photo into a cartoon-like or painting-like image.
    contour_threshold: float in [0.0, 1.0] controlling contour visibility (default 0.5).
    Lower -> more edges pass (weaker visible contours). Higher -> fewer edges pass (stronger contours).
    """
    # clamp contour_threshold
    contour_threshold = float(contour_threshold)
    if contour_threshold < 0.0:
        contour_threshold = 0.0
    if contour_threshold > 1.0:
        contour_threshold = 1.0

    size = (src_color.shape[1], src_color.shape[0])  # width, height
    mask = np.zeros((size[1], size[0]), dtype=np.uint8)
    edges = np.zeros((size[1], size[0]), dtype=np.uint8)
    total_flt_edges = np.zeros((size[1], size[0]), dtype=np.float32)

    if not evil_mode:
        bgr = cv2.split(src_color)

        # Vectorized atan2 computation (replaces per-pixel loops)
        for idx in range(3):
            channel = bgr[idx].astype(np.float32)
            ch1 = bgr[(idx + 1) % 3].astype(np.float32)
            ch2 = bgr[(idx + 2) % 3].astype(np.float32)
            mx = np.maximum(ch1, ch2)
            res = np.arctan2(channel + 0.5, mx + 0.5).astype(np.float32)
            res = cv2.medianBlur(res, 5)
            flt_edges = cv2.Laplacian(res, cv2.CV_32F, ksize=5)
            total_flt_edges += flt_edges

        # Use contour_threshold here (original used 0.5)
        mask = (total_flt_edges < contour_threshold).astype(np.uint8) * 255

        # invert, thin, invert
        mask = 255 - mask
        mask = _thinning(mask)
        mask = 255 - mask

        # refine with grayscale Laplacian (log + median blur)
        srcGray = cv2.cvtColor(src_color, cv2.COLOR_BGR2GRAY)
        img = srcGray.astype(np.float32)
        img += 4.0
        cv2.log(img, img)
        img = cv2.medianBlur(img, 5)
        fltEdges = cv2.Laplacian(img, cv2.CV_32F, ksize=3)
        altMask = (fltEdges < contour_threshold).astype(np.uint8) * 255
        mask = cv2.bitwise_and(mask, altMask)

        cv2.normalize(total_flt_edges, total_flt_edges, 0, 255, cv2.NORM_MINMAX)
        edges = total_flt_edges.astype(np.uint8)

        _remove_pepper_noise(mask)

    else:
        srcGray = cv2.cvtColor(src_color, cv2.COLOR_BGR2GRAY)
        srcGray = cv2.medianBlur(srcGray, 3)
        edges1 = cv2.Scharr(srcGray, cv2.CV_8U, 1, 0)
        edges2 = cv2.Scharr(srcGray, cv2.CV_8U, 0, 1)
        edges = cv2.add(edges1, edges2)
        _, mask = cv2.threshold(edges, 12, 255, cv2.THRESH_BINARY_INV)
        mask = cv2.medianBlur(mask, 3)

    if sketch_mode:
        return cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR)

    # --- A-route: faster stylization ---
    smallSize = (max(1, size[0] // 2), max(1, size[1] // 2))
    smallImg = cv2.resize(src_color, smallSize, interpolation=cv2.INTER_LINEAR)

    # Use pyrMeanShiftFiltering for fast stylization, then one bilateral pass
    smallImg = cv2.pyrMeanShiftFiltering(smallImg, sp=10, sr=20, maxLevel=1)
    smallImg = cv2.bilateralFilter(smallImg, d=9, sigmaColor=30, sigmaSpace=15)

    if alien_mode:
        _change_facial_skin_color(smallImg, edges, debug_type)

    src_color_resized = cv2.resize(smallImg, (size[0], size[1]), interpolation=cv2.INTER_LINEAR)

    dst = np.zeros_like(src_color_resized)
    mask_bool = (mask == 255)
    mask3 = np.repeat(mask_bool[:, :, np.newaxis], 3, axis=2)
    dst[mask3] = src_color_resized[mask3]

    return dst
