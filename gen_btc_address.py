"""
Bitcoin 地址离线生成器
- 无需安装任何第三方库，只用 Python 标准库
- 完全离线，私钥永远不离开本机
- 实现了完整的 secp256k1 椭圆曲线运算
"""

import secrets
import hashlib


# ============================================================
# 基础哈希函数
# ============================================================

def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def ripemd160(data: bytes) -> bytes:
    try:
        h = hashlib.new('ripemd160')
    except ValueError:
        # OpenSSL 3.x 某些发行版禁用了 ripemd160
        h = hashlib.new('ripemd160', usedforsecurity=False)
    h.update(data)
    return h.digest()


def hash160(data: bytes) -> bytes:
    """SHA256 然后 RIPEMD160"""
    return ripemd160(sha256(data))


# ============================================================
# Base58Check 编码
# ============================================================

BASE58 = b'123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'


def base58_encode(data: bytes) -> str:
    leading_zeros = len(data) - len(data.lstrip(b'\x00'))
    num = int.from_bytes(data, 'big')
    result = []
    while num > 0:
        num, rem = divmod(num, 58)
        result.append(BASE58[rem:rem + 1])
    result.extend([BASE58[0:1]] * leading_zeros)
    return b''.join(reversed(result)).decode()


def base58check(version: int, payload: bytes) -> str:
    data = bytes([version]) + payload
    checksum = sha256(sha256(data))[:4]
    return base58_encode(data + checksum)


# ============================================================
# secp256k1 椭圆曲线参数
# ============================================================

P  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F
N  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
Gx = 0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
Gy = 0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8
G  = (Gx, Gy)


def point_add(p1, p2):
    if p1 is None:
        return p2
    if p2 is None:
        return p1
    x1, y1 = p1
    x2, y2 = p2
    if x1 == x2:
        if y1 != y2:
            return None  # 无穷远点
        m = (3 * x1 * x1 * pow(2 * y1, P - 2, P)) % P
    else:
        m = ((y2 - y1) * pow(x2 - x1, P - 2, P)) % P
    x3 = (m * m - x1 - x2) % P
    y3 = (m * (x1 - x3) - y1) % P
    return (x3, y3)


def scalar_mult(k: int, point):
    result = None
    addend = point
    while k:
        if k & 1:
            result = point_add(result, addend)
        addend = point_add(addend, addend)
        k >>= 1
    return result


# ============================================================
# 生成密钥对
# ============================================================

def generate_private_key() -> bytes:
    """用操作系统级 CSPRNG 生成 32 字节私钥"""
    while True:
        key = secrets.token_bytes(32)
        k = int.from_bytes(key, 'big')
        if 1 <= k < N:
            return key


def private_to_public(priv: bytes) -> bytes:
    """私钥 -> 压缩公钥 (33 字节)"""
    k = int.from_bytes(priv, 'big')
    point = scalar_mult(k, G)
    prefix = b'\x02' if point[1] % 2 == 0 else b'\x03'
    return prefix + point[0].to_bytes(32, 'big')


def public_to_p2pkh(pub: bytes) -> str:
    """压缩公钥 -> P2PKH 地址 (1开头，传统格式)"""
    return base58check(0x00, hash160(pub))


def private_to_wif(priv: bytes) -> str:
    """私钥 -> WIF 格式 (K/L 开头，压缩公钥)"""
    return base58check(0x80, priv + b'\x01')


# ============================================================
# 主程序
# ============================================================

if __name__ == '__main__':
    print()
    print("=" * 62)
    print("  Bitcoin 地址离线生成器  |  纯 Python 标准库，无网络")
    print("=" * 62)

    priv = generate_private_key()
    pub  = private_to_public(priv)
    addr = public_to_p2pkh(pub)
    wif  = private_to_wif(priv)

    print()
    print(f"  私钥 (十六进制，32字节):")
    print(f"    {priv.hex()}")
    print()
    print(f"  私钥 (WIF格式，导入钱包用):")
    print(f"    {wif}")
    print()
    print(f"  比特币地址 (填入矿机配置):")
    print(f"    {addr}")
    print()
    print("=" * 62)
    print("  ⚠️  安全须知：")
    print("  1. 将私钥 WIF 抄在纸上，离线存放（不要截图/云同步）")
    print("  2. 丢失私钥 = 永久无法取回资金")
    print("  3. 地址可以公开，私钥绝对不能给任何人")
    print("  4. 建议断网后运行此脚本，运行完立即关闭终端")
    print("=" * 62)
    print()
