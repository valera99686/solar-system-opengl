# -*- coding: utf-8 -*-
"""Генератор ресурсів лабораторної роботи №3: текстури планет, небо, моделі, атлас інтерфейсу.

Усе створюється процедурно (шум, кратери, смуги атмосфер) — зовнішніх зображень і моделей
програма не завантажує з інтернету. Запуск:

    python tools/make_assets.py [каталог assets]

Виходи (формат TGA без стиснення, рядки згори вниз):
    textures/<id>.tga       кольорова карта (рівнопроміжна розгортка сфери, 2:1)
    textures/<id>_s.tga     карта дзеркальності (сірі відтінки), для Землі
    textures/<id>_n.tga     карта нормалей у дотичному просторі
    textures/saturn_ring.tga, ship.tga, rock.tga
    textures/sky_{px,nx,py,ny,pz,nz}.tga     грані кубічної карти неба
    textures/hud_atlas.tga + hud_atlas.txt   тексти інтерфейсу
    models/ship.obj, asteroid.obj
"""

import math
import os
import struct
import sys
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")
if len(sys.argv) > 1:
    ROOT = sys.argv[1]
TEX = os.path.join(ROOT, "textures")
MOD = os.path.join(ROOT, "models")
os.makedirs(TEX, exist_ok=True)
os.makedirs(MOD, exist_ok=True)

U = np.uint32


# ---------------------------------------------------------------------------
# Файли
# ---------------------------------------------------------------------------
def write_tga(name, arr):
    """arr: H×W (сірий), H×W×3 або H×W×4, uint8; рядки згори вниз."""
    arr = np.ascontiguousarray(arr)
    h, w = arr.shape[:2]
    ch = 1 if arr.ndim == 2 else arr.shape[2]
    header = bytearray(18)
    header[2] = 3 if ch == 1 else 2
    struct.pack_into("<HH", header, 12, w, h)
    header[16] = ch * 8
    header[17] = 0x20 | (8 if ch == 4 else 0)
    if ch >= 3:
        data = arr[..., [2, 1, 0] + ([3] if ch == 4 else [])]
    else:
        data = arr
    with open(os.path.join(TEX, name), "wb") as f:
        f.write(bytes(header))
        f.write(data.tobytes())


def to_u8(a):
    return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)


# ---------------------------------------------------------------------------
# Шум: значеннєвий тривимірний із хешем решітки (безшовний на сфері)
# ---------------------------------------------------------------------------
def _hash(ix, iy, iz, seed):
    h = (ix * U(73856093)) ^ (iy * U(19349663)) ^ (iz * U(83492791)) ^ U((seed * 2654435761) & 0xFFFFFFFF)
    h ^= h >> U(13)
    h *= U(1274126177)
    h ^= h >> U(16)
    return (h & U(0xFFFFFF)).astype(np.float32) / np.float32(16777216.0)


def noise3(x, y, z, seed=0):
    xf, yf, zf = np.floor(x), np.floor(y), np.floor(z)
    fx, fy, fz = (x - xf).astype(np.float32), (y - yf).astype(np.float32), (z - zf).astype(np.float32)
    ix, iy, iz = xf.astype(np.int64).astype(U), yf.astype(np.int64).astype(U), zf.astype(np.int64).astype(U)
    sx, sy, sz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    one = U(1)
    c000 = _hash(ix, iy, iz, seed)
    c100 = _hash(ix + one, iy, iz, seed)
    c010 = _hash(ix, iy + one, iz, seed)
    c110 = _hash(ix + one, iy + one, iz, seed)
    c001 = _hash(ix, iy, iz + one, seed)
    c101 = _hash(ix + one, iy, iz + one, seed)
    c011 = _hash(ix, iy + one, iz + one, seed)
    c111 = _hash(ix + one, iy + one, iz + one, seed)
    a = c000 + (c100 - c000) * sx
    b = c010 + (c110 - c010) * sx
    c = c001 + (c101 - c001) * sx
    d = c011 + (c111 - c011) * sx
    return (a + (b - a) * sy) + ((c + (d - c) * sy) - (a + (b - a) * sy)) * sz


def fbm(x, y, z, octaves=5, seed=0, lac=2.0, gain=0.5):
    total = np.zeros(x.shape, np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        total += amp * noise3(x, y, z, seed + o * 17)
        norm += amp
        x, y, z = x * lac, y * lac, z * lac
        amp *= gain
    return total / norm


def ridged(x, y, z, octaves=4, seed=0):
    return 1.0 - np.abs(2.0 * fbm(x, y, z, octaves, seed) - 1.0)


def sphere_grid(w, h):
    """Напрямки на одиничній сфері для кожного пікселя рівнопроміжної розгортки (збігаються з makeSphere)."""
    lat = (0.5 - (np.arange(h, dtype=np.float32) + 0.5) / h) * math.pi
    lon = (np.arange(w, dtype=np.float32) + 0.5) / w * 2.0 * math.pi
    lon, lat = np.meshgrid(lon, lat)
    x = np.cos(lat) * np.cos(lon)
    y = np.sin(lat)
    z = -np.cos(lat) * np.sin(lon)
    return x.astype(np.float32), y.astype(np.float32), z.astype(np.float32), lat.astype(np.float32), lon.astype(np.float32)


def ramp(t, stops):
    t = np.clip(t, 0.0, 1.0)
    pos = [s[0] for s in stops]
    out = np.zeros(t.shape + (3,), np.float32)
    for c in range(3):
        out[..., c] = np.interp(t, pos, [s[1][c] for s in stops])
    return out


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def crater_height(x, y, z, count, seed, rmin, rmax, depth=1.0):
    """Сума кратерів (кутові радіуси в радіанах): чаша всередині, валик по краю."""
    rng = np.random.default_rng(seed)
    h = np.zeros(x.shape, np.float32)
    for _ in range(count):
        c = rng.normal(size=3)
        c /= np.linalg.norm(c)
        r = rmin * (rmax / rmin) ** (rng.random() ** 2.2)
        d = np.arccos(np.clip(x * c[0] + y * c[1] + z * c[2], -1.0, 1.0))
        t = d / r
        m = t < 2.4
        if not m.any():
            continue
        tt = t[m]
        bowl = np.where(tt < 1.0, -(1.0 - tt * tt) * depth, 0.0)
        rim = 0.45 * depth * np.exp(-((tt - 1.0) / 0.22) ** 2)
        ejecta = 0.08 * depth * np.exp(-np.maximum(tt - 1.0, 0.0) * 2.2) * (tt >= 1.0)
        h[m] += (bowl + rim + ejecta).astype(np.float32) * min(1.0, r / 0.02) ** 0.3
    return h


def normal_map(height, strength):
    """Карта нормалей у дотичному просторі: R — праворуч (u), G — на північ (v), B — назовні."""
    h, w = height.shape
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    dy[0, :] = 0.0
    dy[-1, :] = 0.0
    nx = -dx * strength
    ny = dy * strength          # рядок зростає вниз, а «на північ» — угору
    nz = np.ones_like(nx)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / n, ny / n, nz / n], axis=-1) * 0.5 + 0.5


# ---------------------------------------------------------------------------
# Планети
# ---------------------------------------------------------------------------
def tex_sun(w=1024, h=512):
    x, y, z, lat, lon = sphere_grid(w, h)
    big = fbm(x * 2, y * 2, z * 2, 4, 1)
    gran = fbm(x * 14, y * 14, z * 14, 5, 2)
    t = 0.45 + 0.35 * big + 0.35 * (gran - 0.5)
    col = ramp(t, [(0.0, (0.55, 0.16, 0.02)), (0.35, (0.92, 0.38, 0.06)), (0.6, (1.0, 0.62, 0.14)),
                   (0.85, (1.0, 0.82, 0.35)), (1.0, (1.0, 0.95, 0.7))])
    spots = smoothstep(0.66, 0.74, fbm(x * 3.2, y * 3.2, z * 3.2, 3, 5)) * smoothstep(0.55, 0.2, np.abs(lat))
    col *= (1.0 - 0.7 * spots)[..., None]
    write_tga("sun.tga", to_u8(col))


def tex_rocky(name, base, dark, lat_ice=None, craters=300, seed=3, rmin=0.012, rmax=0.16, strength=5.0,
              normal=True, maria=0.0, w=1024, h=512, tint=None):
    x, y, z, lat, lon = sphere_grid(w, h)
    n1 = fbm(x * 3, y * 3, z * 3, 5, seed)
    n2 = fbm(x * 12, y * 12, z * 12, 4, seed + 7)
    ch = crater_height(x, y, z, craters, seed, rmin, rmax)
    shade = 0.66 + 0.5 * n1 + 0.2 * (n2 - 0.5) + 0.26 * ch
    if maria > 0:
        m = smoothstep(0.5, 0.58, fbm(x * 1.6, y * 1.6, z * 1.6, 4, seed + 3))
        shade *= 1.0 - maria * m
    col = np.zeros((h, w, 3), np.float32)
    for c in range(3):
        col[..., c] = base[c] * np.clip(shade, 0, 2) * (1 - 0.0) + dark[c] * 0.0
    if tint is not None:
        t2 = fbm(x * 2.2, y * 2.2, z * 2.2, 3, seed + 21)
        col = col * (1 - 0.35 * t2[..., None]) + np.array(tint, np.float32) * 0.35 * t2[..., None] * shade[..., None]
    if lat_ice is not None:
        ice = smoothstep(lat_ice, lat_ice + 0.12, np.abs(lat)) * (0.6 + 0.4 * n1)
        col = col * (1 - ice[..., None]) + np.array([0.92, 0.94, 0.96], np.float32) * ice[..., None]
    write_tga(name + ".tga", to_u8(col))
    if normal:
        height = ch * 0.8 + (n1 - 0.5) * 0.35 + (n2 - 0.5) * 0.12
        write_tga(name + "_n.tga", to_u8(normal_map(height, strength * 12)))


def tex_venus(w=1024, h=512):
    x, y, z, lat, lon = sphere_grid(w, h)
    wx = fbm(x * 2 + 5, y * 2, z * 2, 4, 11)
    wy = fbm(x * 2, y * 2 + 7, z * 2, 4, 12)
    wz = fbm(x * 2, y * 2, z * 2 + 3, 4, 13)
    q = fbm(x * 3 + wx * 1.6, y * 3 + wy * 1.6, z * 3 + wz * 1.6, 5, 14)
    t = 0.2 + 0.8 * q
    col = ramp(t, [(0.0, (0.66, 0.46, 0.2)), (0.4, (0.84, 0.66, 0.36)), (0.75, (0.93, 0.82, 0.55)),
                   (1.0, (0.98, 0.93, 0.78))])
    write_tga("venus.tga", to_u8(col))


def tex_earth(w=2048, h=1024):
    x, y, z, lat, lon = sphere_grid(w, h)
    elev = 0.62 * fbm(x * 1.7 + 3, y * 1.7, z * 1.7, 7, 21) + 0.38 * fbm(x * 4.5, y * 4.5, z * 4.5, 4, 22)
    sea = 0.50
    land = elev > sea
    depth = np.clip((sea - elev) / 0.2, 0, 1)
    ocean = ramp(depth, [(0.0, (0.10, 0.42, 0.62)), (0.35, (0.05, 0.25, 0.50)), (1.0, (0.02, 0.10, 0.32))])
    hgt = np.clip((elev - sea) / 0.2, 0, 1)
    dry = smoothstep(0.35, 0.65, fbm(x * 2.5 + 9, y * 2.5, z * 2.5, 4, 23)) * np.exp(-((np.abs(lat) - 0.45) / 0.35) ** 2)
    green = ramp(hgt, [(0.0, (0.16, 0.38, 0.12)), (0.35, (0.10, 0.28, 0.08)), (0.7, (0.38, 0.33, 0.22)),
                       (1.0, (0.55, 0.52, 0.48))])
    desert = ramp(hgt, [(0.0, (0.78, 0.65, 0.38)), (0.5, (0.66, 0.5, 0.3)), (1.0, (0.5, 0.42, 0.34))])
    landc = green * (1 - dry[..., None]) + desert * dry[..., None]
    tundra = smoothstep(0.82, 1.1, np.abs(lat) + 0.08 * (fbm(x * 6, y * 6, z * 6, 3, 24) - 0.5))
    landc = landc * (1 - tundra[..., None] * 0.7) + np.array([0.82, 0.86, 0.88], np.float32) * tundra[..., None] * 0.7
    col = np.where(land[..., None], landc, ocean)
    ice = smoothstep(1.12, 1.22, np.abs(lat) + 0.1 * (fbm(x * 5, y * 5, z * 5, 3, 25) - 0.5))
    col = col * (1 - ice[..., None]) + np.array([0.93, 0.96, 0.98], np.float32) * ice[..., None]
    cloud = fbm(x * 3.3 + 1.7, y * 4.6, z * 3.3, 6, 26)
    cl = smoothstep(0.58, 0.80, cloud * (0.8 + 0.4 * np.cos(lat * 2.5)))
    col = col * (1 - 0.8 * cl[..., None]) + np.array([0.97, 0.98, 1.0], np.float32) * 0.8 * cl[..., None]
    write_tga("earth.tga", to_u8(col))
    spec = np.where(land, 0.07, 0.95) * (1 - 0.9 * cl) * (1 - 0.7 * ice)
    write_tga("earth_s.tga", to_u8(spec))


def band_pattern(lat, x, y, z, freq, warp, seed):
    w = fbm(x * 3, y * 11, z * 3, 5, seed)
    return 0.5 + 0.5 * np.sin(lat * freq + warp * (w - 0.5) * 8.0)


def tex_gas(name, stops, freq, warp, seed, contrast=1.0, storm=None, w=1024, h=512):
    x, y, z, lat, lon = sphere_grid(w, h)
    b = band_pattern(lat, x, y, z, freq, warp, seed)
    fine = fbm(x * 9, y * 26, z * 9, 5, seed + 5)
    t = 0.5 + (b - 0.5) * contrast + (fine - 0.5) * 0.14
    col = ramp(t, stops)
    if storm is not None:
        slat, slon, sr, scol = storm
        dl = (lon - slon + math.pi) % (2 * math.pi) - math.pi
        d = np.sqrt((dl * math.cos(slat) / (sr * 1.7)) ** 2 + ((lat - slat) / sr) ** 2)
        m = smoothstep(1.0, 0.55, d)
        swirl = 0.85 + 0.3 * fbm(x * 12, y * 12, z * 12, 4, seed + 9)
        col = col * (1 - m[..., None] * 0.9) + np.array(scol, np.float32) * m[..., None] * 0.9 * swirl[..., None]
    write_tga(name + ".tga", to_u8(col))


def tex_saturn_ring(w=1024, h=16):
    u = (np.arange(w, dtype=np.float32) + 0.5) / w     # 0 — внутрішній край, 1 — зовнішній
    rng = np.random.default_rng(7)
    fine = np.zeros(w, np.float32)
    for k in range(1, 9):
        fine += rng.random() * np.sin(u * w * 0.06 * k + rng.random() * 6.28) / k
    fine = 0.5 + 0.25 * fine
    alpha = np.zeros(w, np.float32)
    alpha += 0.16 * smoothstep(0.0, 0.05, u) * smoothstep(0.17, 0.13, u)           # слабке кільце D/C
    alpha += 0.42 * smoothstep(0.14, 0.2, u) * smoothstep(0.34, 0.3, u)            # кільце C
    alpha += 0.92 * smoothstep(0.3, 0.36, u) * smoothstep(0.64, 0.6, u)            # яскраве кільце B
    alpha += 0.72 * smoothstep(0.69, 0.72, u) * smoothstep(0.93, 0.9, u)           # кільце A
    alpha *= 1.0 - 0.85 * np.exp(-((u - 0.855) / 0.012) ** 2)                      # щілина Енке
    alpha += 0.18 * np.exp(-((u - 0.975) / 0.01) ** 2)                             # кільце F
    alpha = np.clip(alpha * (0.78 + 0.45 * fine), 0, 0.97)
    shade = np.clip(0.55 + 0.6 * fine, 0, 1)
    col = np.zeros((h, w, 4), np.float32)
    col[..., 0] = (0.86 * shade)[None, :]
    col[..., 1] = (0.76 * shade)[None, :]
    col[..., 2] = (0.60 * shade)[None, :]
    col[..., 3] = alpha[None, :]
    write_tga("saturn_ring.tga", to_u8(col))


def tex_io(w=512, h=256):
    x, y, z, lat, lon = sphere_grid(w, h)
    n = fbm(x * 4, y * 4, z * 4, 5, 41)
    col = ramp(n, [(0.0, (0.75, 0.55, 0.12)), (0.5, (0.93, 0.82, 0.30)), (1.0, (0.98, 0.93, 0.6))])
    red = smoothstep(0.62, 0.75, fbm(x * 5 + 2, y * 5, z * 5, 4, 42))
    col = col * (1 - red[..., None] * 0.6) + np.array([0.75, 0.25, 0.08], np.float32) * red[..., None] * 0.6
    rng = np.random.default_rng(43)
    for _ in range(14):
        c = rng.normal(size=3); c /= np.linalg.norm(c)
        d = np.arccos(np.clip(x * c[0] + y * c[1] + z * c[2], -1, 1))
        m = smoothstep(0.07, 0.03, d)
        col *= (1 - 0.85 * m)[..., None]
    write_tga("io.tga", to_u8(col))


def tex_europa(w=512, h=256):
    x, y, z, lat, lon = sphere_grid(w, h)
    base = 0.82 + 0.12 * fbm(x * 6, y * 6, z * 6, 4, 51)
    crack = smoothstep(0.86, 0.95, ridged(x * 5, y * 5, z * 5, 3, 52)) + 0.6 * smoothstep(0.9, 0.97, ridged(x * 11 + 3, y * 11, z * 11, 3, 53))
    col = np.stack([base * 0.95, base * 0.9, base * 0.82], axis=-1)
    brown = np.array([0.55, 0.32, 0.18], np.float32)
    col = col * (1 - np.clip(crack, 0, 1)[..., None] * 0.7) + brown * np.clip(crack, 0, 1)[..., None] * 0.7
    write_tga("europa.tga", to_u8(col))


def tex_titan(w=512, h=256):
    x, y, z, lat, lon = sphere_grid(w, h)
    n = fbm(x * 2.2, y * 3.5, z * 2.2, 4, 61)
    col = ramp(0.35 + 0.5 * n + 0.15 * np.sin(lat * 6), [(0.0, (0.62, 0.36, 0.1)), (0.5, (0.86, 0.58, 0.2)), (1.0, (0.95, 0.76, 0.4))])
    write_tga("titan.tga", to_u8(col))


def tex_ship(size=512):
    rng = np.random.default_rng(71)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    base = 0.58 + 0.06 * rng.random((size // 8, size // 8)).repeat(8, 0).repeat(8, 1)
    panel = 64
    lines = ((xx % panel) < 2) | ((yy % panel) < 2)
    sub = ((xx % 16) < 1) & ((yy % panel) > 4)
    img = np.stack([base * 0.96, base * 1.0, base * 1.06], axis=-1)
    img[lines] *= 0.42
    img[sub] *= 0.8
    # заклепки
    for py in range(panel // 2, size, panel):
        for px in range(panel // 2, size, panel):
            r2 = (xx - px) ** 2 + (yy - py) ** 2
            img[r2 < 5] *= 1.25
    # кольорові смуги
    stripe = (yy > 3 * panel) & (yy < 3 * panel + 20)
    img[stripe] = np.array([0.75, 0.16, 0.12], np.float32)
    stripe2 = (yy > 5 * panel + 12) & (yy < 5 * panel + 26)
    img[stripe2] = np.array([0.15, 0.35, 0.78], np.float32)
    img *= (0.88 + 0.24 * fbm(xx / 60.0, yy / 60.0, np.zeros_like(xx), 4, 72))[..., None]
    write_tga("ship.tga", to_u8(np.clip(img, 0, 1)))


def tex_rock(w=512, h=256):
    x, y, z, lat, lon = sphere_grid(w, h)
    n = fbm(x * 5, y * 5, z * 5, 6, 81)
    col = ramp(n, [(0.0, (0.2, 0.18, 0.16)), (0.5, (0.42, 0.38, 0.34)), (1.0, (0.65, 0.6, 0.54))])
    ch = crater_height(x, y, z, 60, 82, 0.05, 0.3, 0.6)
    col *= np.clip(0.85 + 0.6 * ch, 0.3, 1.4)[..., None]
    write_tga("rock.tga", to_u8(np.clip(col, 0, 1)))


# ---------------------------------------------------------------------------
# Небо: кубічна карта
# ---------------------------------------------------------------------------
def face_dirs(face, n):
    j = (np.arange(n, dtype=np.float32) + 0.5) / n * 2 - 1
    u, v = np.meshgrid(j, j)          # u — стовпець, v — рядок
    one = np.ones_like(u)
    if face == 0: d = (one, -v, -u)
    elif face == 1: d = (-one, -v, u)
    elif face == 2: d = (u, one, v)
    elif face == 3: d = (u, -one, -v)
    elif face == 4: d = (u, -v, one)
    else: d = (-u, -v, -one)
    d = np.stack(d, axis=-1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)


def dir_to_face(s):
    """Обернене відображення: напрямок → (грань, i, j) у координатах пікселя."""
    a = np.abs(s)
    m = np.argmax(a, axis=1)
    sx, sy, sz = s[:, 0], s[:, 1], s[:, 2]
    face = np.zeros(len(s), np.int32)
    u = np.zeros(len(s)); v = np.zeros(len(s))
    for f, cond, uu, vv in [
        (0, (m == 0) & (sx > 0), -sz / np.abs(sx), -sy / np.abs(sx)),
        (1, (m == 0) & (sx <= 0), sz / np.abs(sx), -sy / np.abs(sx)),
        (2, (m == 1) & (sy > 0), sx / np.abs(sy), sz / np.abs(sy)),
        (3, (m == 1) & (sy <= 0), sx / np.abs(sy), -sz / np.abs(sy)),
        (4, (m == 2) & (sz > 0), sx / np.abs(sz), -sy / np.abs(sz)),
        (5, (m == 2) & (sz <= 0), -sx / np.abs(sz), -sy / np.abs(sz)),
    ]:
        face[cond] = f
        u[cond] = uu[cond]
        v[cond] = vv[cond]
    return face, v, u


def make_sky(n=1024):
    t0 = time.time()
    rng = np.random.default_rng(91)
    gal = np.array([0.28, 0.86, 0.42]); gal /= np.linalg.norm(gal)
    # зорі: рівномірно по сфері + згущення до площини Галактики
    def sample(count, band):
        s = rng.normal(size=(count * 3, 3)); s /= np.linalg.norm(s, axis=1, keepdims=True)
        if band:
            b = s @ gal
            keep = rng.random(len(s)) < np.exp(-(b / 0.22) ** 2)
            s = s[keep]
        return s[:count]
    stars = np.concatenate([sample(26000, False), sample(52000, True)])
    mag = 0.07 + 0.93 * rng.random(len(stars)) ** 14
    temp = rng.random(len(stars))[:, None]
    tint = (1 - temp) * np.array([0.72, 0.82, 1.0]) + temp * np.array([1.0, 0.82, 0.62])
    colors = tint * mag[:, None]
    face, ii, jj = dir_to_face(stars)
    sigma = 0.45 + 0.45 * mag
    faces = [np.zeros((n, n, 3), np.float32) for _ in range(6)]
    for f in range(6):
        sel = face == f
        pi = (ii[sel] + 1) / 2 * n - 0.5
        pj = (jj[sel] + 1) / 2 * n - 0.5
        sg = sigma[sel]
        col = colors[sel]
        for di in range(-2, 3):
            for dj in range(-2, 3):
                yi = np.floor(pi).astype(int) + di
                xj = np.floor(pj).astype(int) + dj
                ok = (yi >= 0) & (yi < n) & (xj >= 0) & (xj < n)
                w = np.exp(-((yi - pi) ** 2 + (xj - pj) ** 2) / (2 * sg ** 2))
                for c in range(3):
                    np.add.at(faces[f][..., c], (yi[ok], xj[ok]), (col[ok, c] * w[ok]).astype(np.float32))
    names = ["px", "nx", "py", "ny", "pz", "nz"]
    for f in range(6):
        d = face_dirs(f, n)
        x, y, z = d[..., 0] * 1.0, d[..., 1] * 1.0, d[..., 2] * 1.0
        b = d @ gal
        band = np.exp(-(b / 0.17) ** 2)
        dust = smoothstep(0.55, 0.75, fbm(x * 5 + 1, y * 5, z * 5, 5, 95))
        glow = band * (0.35 + 0.65 * fbm(x * 4, y * 4, z * 4, 5, 96)) * (1 - 0.65 * dust)
        neb = fbm(x * 1.7, y * 1.7, z * 1.7, 4, 97)
        purple = smoothstep(0.55, 0.8, neb) * 0.07
        teal = smoothstep(0.58, 0.82, fbm(x * 1.5 + 4, y * 1.5, z * 1.5, 4, 98)) * 0.05
        img = faces[f].copy()
        img[..., 0] += glow * 0.20 + purple * 0.9 + teal * 0.1
        img[..., 1] += glow * 0.18 + purple * 0.25 + teal * 0.8
        img[..., 2] += glow * 0.24 + purple * 1.0 + teal * 0.9
        write_tga("sky_%s.tga" % names[f], to_u8(np.clip(img, 0, 1)))
    print("  небо: %.1f с" % (time.time() - t0))


# ---------------------------------------------------------------------------
# Моделі OBJ
# ---------------------------------------------------------------------------
class ObjBuilder:
    def __init__(self):
        self.v, self.vt, self.vn, self.f = [], [], [], []

    def _add(self, lst, item):
        lst.append(item)
        return len(lst)

    def tri(self, p, n, uv):
        idx = []
        for k in range(3):
            idx.append((self._add(self.v, p[k]), self._add(self.vt, uv[k]), self._add(self.vn, n[k])))
        self.f.append(idx)

    def save(self, path, title):
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("# %s\n" % title)
            for p in self.v:
                f.write("v %.5f %.5f %.5f\n" % tuple(p))
            for t in self.vt:
                f.write("vt %.5f %.5f\n" % tuple(t))
            for n in self.vn:
                f.write("vn %.5f %.5f %.5f\n" % tuple(n))
            for face in self.f:
                f.write("f " + " ".join("%d/%d/%d" % t for t in face) + "\n")


def unit(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v) + 1e-12)


def make_asteroid():
    # Ікосфера з двома підрозбиттями (320 граней), зміщена шумом
    t = (1 + 5 ** 0.5) / 2
    verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
             (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    verts = [unit(v) for v in verts]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
             (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11),
             (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    cache = {}

    def mid(a, b):
        key = (min(a, b), max(a, b))
        if key not in cache:
            verts.append(unit((np.array(verts[a]) + np.array(verts[b])) / 2))
            cache[key] = len(verts) - 1
        return cache[key]

    for _ in range(2):
        new = []
        for a, b, c in faces:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            new += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new
    V = np.array(verts)
    n1 = fbm(V[:, 0:1] * 1.6 + 3, V[:, 1:2] * 1.6, V[:, 2:3] * 1.6, 4, 101)[:, 0]
    n2 = fbm(V[:, 0:1] * 5, V[:, 1:2] * 5, V[:, 2:3] * 5, 3, 102)[:, 0]
    r = 0.72 + 0.5 * n1 + 0.12 * (n2 - 0.5)
    P = V * r[:, None] * np.array([1.0, 0.82, 0.92])
    # напрям обходу: нормаль грані має дивитися від центру
    sign = 0.0
    for a, b, c in faces:
        sign += np.dot(np.cross(P[b] - P[a], P[c] - P[a]), P[a] + P[b] + P[c])
    if sign < 0:
        faces = [(a, c, b) for a, b, c in faces]
    N = np.zeros_like(P)
    for a, b, c in faces:
        fn = np.cross(P[b] - P[a], P[c] - P[a])
        N[a] += fn
        N[b] += fn
        N[c] += fn
    N = N / np.linalg.norm(N, axis=1, keepdims=True)
    ob = ObjBuilder()
    for a, b, c in faces:
        uvs = []
        for i in (a, b, c):
            d = unit(V[i])
            uvs.append([0.5 + math.atan2(-d[2], d[0]) / (2 * math.pi), 0.5 + math.asin(np.clip(d[1], -1, 1)) / math.pi])
        us = [u[0] for u in uvs]
        if max(us) - min(us) > 0.5:             # грань перетинає шов розгортки
            for u in uvs:
                if u[0] < 0.5:
                    u[0] += 1.0
        ob.tri([P[a], P[b], P[c]], [N[a], N[b], N[c]], uvs)
    ob.save(os.path.join(MOD, "asteroid.obj"), "астероїд: зміщена ікосфера, 320 граней")
    print("  asteroid.obj: %d граней" % len(ob.f))


def make_ship():
    ob = ObjBuilder()

    def tri(p, n, uv):
        """Додає трикутник; порядок вершин узгоджується з нормалями (проти годинникової стрілки ззовні)."""
        p = [np.array(q, float) for q in p]
        n = [unit(q) for q in n]
        g = np.cross(p[1] - p[0], p[2] - p[0])
        if np.linalg.norm(g) < 1e-12:
            return
        if np.dot(g, n[0] + n[1] + n[2]) < 0:
            p = [p[0], p[2], p[1]]
            n = [n[0], n[2], n[1]]
            uv = [uv[0], uv[2], uv[1]]
        ob.tri(p, n, uv)

    def box_uv(p, n, scale=0.35):
        an = np.abs(n)
        if an[1] >= an[0] and an[1] >= an[2]:
            return (p[0] * scale + 0.5, p[2] * scale + 0.5)
        if an[0] >= an[2]:
            return (p[2] * scale + 0.5, p[1] * scale + 0.5)
        return (p[0] * scale + 0.5, p[1] * scale + 0.5)

    def flat(p0, p1, p2, center):
        p0, p1, p2 = np.array(p0, float), np.array(p1, float), np.array(p2, float)
        n = unit(np.cross(p1 - p0, p2 - p0))
        if np.dot(n, (p0 + p1 + p2) / 3 - center) < 0:
            n = -n
        tri([p0, p1, p2], [n, n, n], [box_uv(p, n) for p in (p0, p1, p2)])

    def prism(poly, y0, y1):
        """Призма над опуклим многокутником в площині xz; плоскі грані."""
        pts = [(p[0], y1, p[1]) for p in poly] + [(p[0], y0, p[1]) for p in poly]
        center = np.mean(np.array(pts, float), axis=0)
        n = len(poly)
        top = [(p[0], y1, p[1]) for p in poly]
        bot = [(p[0], y0, p[1]) for p in poly]
        for i in range(1, n - 1):
            flat(top[0], top[i], top[i + 1], center)
            flat(bot[0], bot[i], bot[i + 1], center)
        for i in range(n):
            j = (i + 1) % n
            flat(bot[i], bot[j], top[j], center)
            flat(bot[i], top[j], top[i], center)

    # фюзеляж: еліптичні перерізи, ніс у −z, плавний профіль; нормалі усереднені
    R, S = 20, 26
    zs = np.linspace(-1.9, 1.15, S)

    def profile(z):
        t = (z + 1.9) / 3.05
        nose = math.sqrt(max(0.0, 1 - ((0.35 - t) / 0.35) ** 2)) if t < 0.35 else 1.0
        tail = 1.0 - 0.28 * float(smoothstep(0.8, 1.0, np.array(t)))
        return 0.34 * nose * tail, 0.26 * nose * tail

    P = np.zeros((S, R, 3))
    for i, z in enumerate(zs):
        w, h = profile(z)
        for k in range(R):
            a = 2 * math.pi * k / R
            sy = math.sin(a)
            P[i, k] = (w * math.cos(a), h * sy * (1.0 if sy > 0 else 0.8), z)
    N = np.zeros_like(P)
    for i in range(S - 1):
        for k in range(R):
            k2 = (k + 1) % R
            fn = np.cross(P[i, k2] - P[i, k], P[i + 1, k] - P[i, k])
            for ii, kk in ((i, k), (i, k2), (i + 1, k2), (i + 1, k)):
                N[ii, kk] += fn
    for i in range(S):
        for k in range(R):
            radial = np.array([P[i, k, 0], P[i, k, 1], 0.0])
            if np.linalg.norm(radial) < 1e-6:
                N[i, k] = (0.0, 0.0, -1.0 if i == 0 else 1.0)
                continue
            if np.dot(N[i, k], radial) < 0:
                N[i, k] = -N[i, k]
            N[i, k] = unit(N[i, k])
    for i in range(S - 1):
        for k in range(R):
            k2 = (k + 1) % R
            corner = [(i, k), (i, k2), (i + 1, k2), (i + 1, k)]
            pts = [P[a, b] for a, b in corner]
            nrm = [N[a, b] for a, b in corner]
            uv = [(2.0 * (k + (1 if q in (1, 2) else 0)) / R, 0.35 * (pts[q][2] + 2.0)) for q in range(4)]
            tri([pts[0], pts[1], pts[2]], [nrm[0], nrm[1], nrm[2]], [uv[0], uv[1], uv[2]])
            tri([pts[0], pts[2], pts[3]], [nrm[0], nrm[2], nrm[3]], [uv[0], uv[2], uv[3]])
    for k in range(R):                           # кормовий зріз
        k2 = (k + 1) % R
        c = np.array([0.0, 0.0, zs[-1]])
        tri([c, P[-1, k], P[-1, k2]], [(0, 0, 1)] * 3, [(0.5, 0.5), (0.5, 0.5), (0.5, 0.5)])

    # крила (стрілоподібні призми) і законцівки-шайби
    for s in (-1, 1):
        wing = [(0.24, -0.35), (1.55, 0.55), (1.55, 0.85), (0.24, 0.9)]
        prism([(s * px, pz) for px, pz in wing], -0.035, 0.035)
        prism([(s * px, pz) for px, pz in [(1.55, 0.5), (1.62, 0.55), (1.62, 0.9), (1.55, 0.85)]], -0.02, 0.22)
    # кіль
    prism([(-0.025, 0.7), (0.025, 0.7), (0.025, 1.15), (-0.025, 1.15)], 0.2, 0.68)

    # двигуни: два конічних циліндри в кормі
    for s in (-1, 1):
        cx, rr, segs = s * 0.52, 0.17, 14
        for k in range(segs):
            a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
            p00 = (cx + rr * math.cos(a0), rr * math.sin(a0), 0.15)
            p01 = (cx + rr * math.cos(a1), rr * math.sin(a1), 0.15)
            p10 = (cx + rr * 0.8 * math.cos(a0), rr * 0.8 * math.sin(a0), 1.32)
            p11 = (cx + rr * 0.8 * math.cos(a1), rr * 0.8 * math.sin(a1), 1.32)
            n0, n1 = (math.cos(a0), math.sin(a0), 0.0), (math.cos(a1), math.sin(a1), 0.0)
            u0, u1 = k / segs * 2, (k + 1) / segs * 2
            tri([p00, p10, p11], [n0, n0, n1], [(u0, 0.0), (u0, 0.5), (u1, 0.5)])
            tri([p00, p11, p01], [n0, n1, n1], [(u0, 0.0), (u1, 0.5), (u1, 0.0)])
            tri([(cx, 0.0, 1.32), p11, p10], [(0, 0, 1)] * 3, [(0.5, 0.5)] * 3)
    ob.save(os.path.join(MOD, "ship.obj"), "космічний корабель: фюзеляж, крила, кіль, два двигуни")
    print("  ship.obj: %d граней" % len(ob.f))


# ---------------------------------------------------------------------------
# Атлас інтерфейсу
# ---------------------------------------------------------------------------
def find_font(size):
    for name in ("segoeui.ttf", "arial.ttf", "calibri.ttf", "tahoma.ttf", "DejaVuSans.ttf"):
        for base in (r"C:\Windows\Fonts", "/usr/share/fonts/truetype/dejavu"):
            path = os.path.join(base, name)
            if os.path.exists(path):
                return ImageFont.truetype(path, size)
    raise RuntimeError("не знайдено шрифту з кирилицею")


def make_hud():
    font = find_font(34)
    big = find_font(34)
    items = {}

    def render(text, f=font, lines=None):
        lines = lines or [text]
        dummy = ImageDraw.Draw(Image.new("L", (1, 1)))
        widths = [dummy.textlength(l, font=f) for l in lines]
        asc, desc = f.getmetrics()
        lh = asc + desc + 6
        w = int(max(widths)) + 8
        h = lh * len(lines) + 6
        img = Image.new("L", (w, h), 0)
        d = ImageDraw.Draw(img)
        for i, l in enumerate(lines):
            d.text((4, 3 + i * lh), l, font=f, fill=255)
        return np.array(img)

    items["mode_free"] = render("Камера: вільна")
    items["mode_orbit"] = render("Камера: орбіта")
    items["selected"] = render("Вибрано:")
    items["speed"] = render("Швидкість:")
    items["days"] = render("діб/с")
    items["paused"] = render("ПАУЗА")
    items["hint"] = render("H — довідка")
    names = {"sun": "Сонце", "mercury": "Меркурій", "venus": "Венера", "earth": "Земля", "moon": "Місяць",
             "mars": "Марс", "jupiter": "Юпітер", "io": "Іо", "europa": "Європа", "ganymede": "Ганімед",
             "callisto": "Каллісто", "saturn": "Сатурн", "titan": "Титан", "uranus": "Уран", "neptune": "Нептун",
             "ship": "Корабель"}
    for k, v in names.items():
        items["body_" + k] = render(v)
    for ch in "0123456789":
        items["glyph_" + ch] = render(ch)
    items["glyph_dot"] = render(".")
    items["help"] = render("", lines=[
        "Керування",
        "W A S D — політ,  Пробіл / Ctrl — вгору / вниз,  Q E — крен,  Shift — швидше",
        "Права кнопка миші — оглядатися;  колесо — швидкість польоту (в орбіті — відстань)",
        "Tab / Shift+Tab — вибір тіла;  1…9, 0 — політ до тіла;  C — орбіта / вільна камера;  R — загальний вид",
        ", .  — повільніше / швидше;  P — пауза;  F12 — знімок екрана;  Esc — вихід",
        "G тіні · F ліхтар · N нормалі · T текстури · O орбіти · B пояс · L Сонце · X контур · I інтерфейс"])
    # шеляфна упаковка в смугу шириною 2048
    W = 2048
    x = y = rowh = 0
    pos = {}
    for name, arr in items.items():
        h, w = arr.shape
        if x + w + 2 > W:
            x, y, rowh = 0, y + rowh + 2, 0
        pos[name] = (x, y, w, h)
        x += w + 2
        rowh = max(rowh, h)
    H = 1
    while H < y + rowh + 2:
        H *= 2
    atlas = np.zeros((H, W, 4), np.uint8)
    atlas[..., :3] = 255
    for name, arr in items.items():
        px, py, w, h = pos[name]
        atlas[py:py + h, px:px + w, 3] = arr
    write_tga("hud_atlas.tga", atlas)
    with open(os.path.join(TEX, "hud_atlas.txt"), "w", encoding="utf-8", newline="\n") as f:
        for name, (px, py, w, h) in pos.items():
            f.write("%s %d %d %d %d\n" % (name, px, py, w, h))
    print("  атлас інтерфейсу: %dx%d, %d спрайтів" % (W, H, len(items)))


# ---------------------------------------------------------------------------
def step(label, fn, *a, **k):
    only = os.environ.get("ONLY")
    if only and not any(s and s in label for s in only.split(",")):
        return
    t0 = time.time()
    fn(*a, **k)
    print("%-24s %.1f с" % (label, time.time() - t0))


def main():
    step("Сонце", tex_sun)
    step("Меркурій", tex_rocky, "mercury", (0.62, 0.58, 0.54), (0.3, 0.28, 0.26), craters=520, seed=11, rmin=0.01, rmax=0.14)
    step("Венера", tex_venus)
    step("Земля", tex_earth)
    step("Місяць", tex_rocky, "moon", (0.60, 0.59, 0.57), (0.3, 0.3, 0.3), craters=420, seed=12, rmin=0.01, rmax=0.2, maria=0.5)
    step("Марс", tex_rocky, "mars", (0.76, 0.40, 0.22), (0.4, 0.2, 0.1), lat_ice=1.30, craters=240, seed=13, rmin=0.01, rmax=0.12, tint=(0.35, 0.2, 0.12))
    step("Юпітер", tex_gas, "jupiter",
         [(0.0, (0.45, 0.28, 0.17)), (0.3, (0.68, 0.46, 0.30)), (0.55, (0.88, 0.78, 0.62)), (0.8, (0.93, 0.88, 0.78)), (1.0, (0.62, 0.40, 0.26))],
         freq=24, warp=0.55, seed=31, contrast=1.0, storm=(-0.38, 2.4, 0.12, (0.72, 0.28, 0.16)))
    step("Іо", tex_io)
    step("Європа", tex_europa)
    step("Ганімед", tex_rocky, "ganymede", (0.50, 0.46, 0.42), (0.3, 0.28, 0.25), craters=260, seed=14, rmin=0.012, rmax=0.15, normal=False, w=512, h=256)
    step("Каллісто", tex_rocky, "callisto", (0.33, 0.30, 0.27), (0.2, 0.18, 0.16), craters=520, seed=15, rmin=0.008, rmax=0.1, normal=False, w=512, h=256)
    step("Сатурн", tex_gas, "saturn",
         [(0.0, (0.66, 0.55, 0.36)), (0.4, (0.80, 0.70, 0.48)), (0.75, (0.90, 0.82, 0.62)), (1.0, (0.72, 0.62, 0.42))],
         freq=20, warp=0.7, seed=32, contrast=0.8)
    step("кільце Сатурна", tex_saturn_ring)
    step("Титан", tex_titan)
    step("Уран", tex_gas, "uranus",
         [(0.0, (0.50, 0.78, 0.82)), (0.6, (0.60, 0.86, 0.88)), (1.0, (0.70, 0.92, 0.92))],
         freq=14, warp=0.3, seed=33, contrast=0.55)
    step("Нептун", tex_gas, "neptune",
         [(0.0, (0.07, 0.15, 0.55)), (0.5, (0.14, 0.30, 0.82)), (0.85, (0.30, 0.50, 0.95)), (1.0, (0.7, 0.8, 1.0))],
         freq=16, warp=0.5, seed=34, contrast=0.6, storm=(-0.35, 3.3, 0.14, (0.04, 0.08, 0.3)))
    step("корабель (текстура)", tex_ship)
    step("камінь", tex_rock)
    step("небо", make_sky)
    step("астероїд (модель)", make_asteroid)
    step("корабель (модель)", make_ship)
    step("атлас інтерфейсу", make_hud)
    print("Готово:", os.path.abspath(ROOT))


if __name__ == "__main__":
    main()
