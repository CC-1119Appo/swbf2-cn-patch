# -*- coding: utf-8 -*-
"""lvl.py - SWBF2 (2005) ucfb LVL container reader/writer (pure stdlib).

Grammar verified against the real game files:
  Standard header (8 bytes):  magic b'ucfb' + u32 size_field (= total file size - 8)
  Indexed header (16 bytes):  magic b'ucfb' + u32 size_field + u32 date + u32 index_size
                              + <index_size> index bytes, then the chunk stream.
  Chunk: 4CC type (4 bytes) + u32 size + data[size] + pad to 4-byte alignment.
  The chunk size field is the DATA length only (padding excluded).
"""
import os
import struct
from dataclasses import dataclass, field

MAGIC = b"ucfb"


class LvlFormatError(Exception):
    pass


def _is_print4(b: bytes) -> bool:
    return len(b) == 4 and all(32 <= x < 127 for x in b)


@dataclass
class Chunk:
    type: bytes
    size: int
    data: bytes
    pad: bytes = b""

    def serialized(self) -> bytes:
        assert len(self.type) == 4 and len(self.data) == self.size
        return self.type + struct.pack("<I", self.size) + self.data + self.pad

    def name(self) -> str:
        return self.type.decode("ascii", "replace")


@dataclass
class LvlFile:
    chunks: list = field(default_factory=list)
    indexed: bool = False
    date: int = 0
    index: bytes = b""

    def serialize(self) -> bytes:
        body = b"".join(c.serialized() for c in self.chunks)
        if self.indexed:
            total = 16 + len(self.index) + len(body)
            hdr = MAGIC + struct.pack("<III", total - 8, self.date, len(self.index))
            return hdr + self.index + body
        total = 8 + len(body)
        return MAGIC + struct.pack("<I", total - 8) + body


def iter_chunks(data: bytes, start: int = 0, end: int = None) -> list:
    if end is None:
        end = len(data)
    pos = start
    chunks = []
    while pos + 8 <= end:
        typ = data[pos:pos + 4]
        if not _is_print4(typ):
            raise LvlFormatError("non-printable chunk type at offset %d: %r" % (pos, typ))
        size = struct.unpack_from("<I", data, pos + 4)[0]
        if pos + 8 + size > end:
            raise LvlFormatError("chunk %r overruns stream at %d (size %d)" % (typ, pos, size))
        d = data[pos + 8:pos + 8 + size]
        total = 8 + size
        nxt = pos + ((total + 3) & ~3)
        pad = data[pos + total:nxt]
        chunks.append(Chunk(typ, size, d, pad))
        pos = nxt
    if pos != end:
        raise LvlFormatError("chunk stream leaves %d trailing bytes" % (end - pos))
    return chunks


def _try_parse_stream(data: bytes, start: int):
    try:
        return iter_chunks(data, start, len(data))
    except LvlFormatError:
        return None


_KNOWN_TYPES = {b"snd_", b"font", b"Locl", b"tex_", b"SHDR", b"lvl_", b"scr_", b"mcfg",
                b"fx__", b"modl", b"skel", b"gmod", b"entc", b"coll", b"prim", b"CLTH"}


def _plausible_dos_date(v: int) -> bool:
    sec2 = v & 0x1F
    minute = (v >> 5) & 0x3F
    hour = (v >> 11) & 0x1F
    day = (v >> 16) & 0x1F
    month = (v >> 21) & 0x0F
    year = (v >> 25) & 0x7F
    return (1 <= month <= 12 and 1 <= day <= 31 and hour <= 23 and minute <= 59
            and sec2 <= 29 and year >= 1)


def parse_lvl(data: bytes) -> LvlFile:
    if len(data) < 8 or data[:4] != MAGIC:
        raise LvlFormatError("not an ucfb lvl (magic %r)" % data[:4])
    size_field = struct.unpack_from("<I", data, 4)[0]
    std_chunks = _try_parse_stream(data, 8)
    idx_chunks = None
    idx_size = 0
    if len(data) >= 16:
        idx_size = struct.unpack_from("<I", data, 12)[0]
        if 0 <= idx_size <= len(data) - 16:
            idx_chunks = _try_parse_stream(data, 16 + idx_size)
    lvl = LvlFile()
    date = struct.unpack_from("<I", data, 8)[0] if len(data) >= 12 else 0
    std_is_known = bool(std_chunks) and std_chunks[0].type in _KNOWN_TYPES
    indexed = (idx_chunks is not None
               and _plausible_dos_date(date)
               and not std_is_known)
    if indexed:
        lvl.indexed = True
        lvl.date = date
        lvl.index = data[16:16 + idx_size]
        lvl.chunks = idx_chunks
    elif std_chunks is not None:
        lvl.chunks = std_chunks
    else:
        raise LvlFormatError("could not parse chunk stream")
    if size_field != len(data) - 8:
        raise LvlFormatError("size field mismatch: field=%d actual=%d" % (size_field, len(data) - 8))
    return lvl


def load(path: str) -> LvlFile:
    with open(path, "rb") as f:
        return parse_lvl(f.read())


def save(path: str, lvl: LvlFile) -> None:
    with open(path, "wb") as f:
        f.write(lvl.serialize())


if __name__ == "__main__":
    import sys
    for p in sys.argv[1:]:
        lvl = load(p)
        print("%s: indexed=%s chunks=%d date=%s index=%d" % (
            os.path.basename(p), lvl.indexed, len(lvl.chunks), hex(lvl.date), len(lvl.index)))
