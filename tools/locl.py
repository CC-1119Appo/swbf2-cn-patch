# -*- coding: utf-8 -*-
"""locl.py - SWBF2 Locl (localization) chunk parse/build (pure stdlib).

Locl chunk body = NAME chunk (language name) + BODY chunk (string table).
BODY string table: sequence of entries:
    u32 id            (hash, language-independent)
    u16 byte_count    (total entry size: 4 + 2 + text bytes)
    utf16le text      (characters + double null terminator)
Parsing preserves the RAW bytes of every entry and any trailing bytes so that
rebuilding an unmodified Locl is byte-exact (roundtrip diff == 0).
"""
import struct
from dataclasses import dataclass, field

import lvl

_LOCL = b"Locl"
_NAME = b"NAME"
_BODY = b"BODY"


class LoclError(Exception):
    pass


def _find_sub(data: bytes, kind: bytes):
    pos = 0
    while pos + 8 <= len(data):
        if data[pos:pos + 4] == kind:
            size = struct.unpack_from("<I", data, pos + 4)[0]
            return pos, size
        size = struct.unpack_from("<I", data, pos + 4)[0]
        pos += (8 + size + 3) & ~3
    return None


@dataclass
class LoclFile:
    name: str
    entries: list = field(default_factory=list)   # list of dicts: {id, text, raw}
    tail: bytes = b""

    def ids(self):
        return [e["id"] for e in self.entries]


def parse_chunk(chunk: lvl.Chunk) -> LoclFile:
    if chunk.type != _LOCL:
        raise LoclError("not a Locl chunk: %r" % chunk.type)
    body = chunk.data
    n = _find_sub(body, _NAME)
    b = _find_sub(body, _BODY)
    if n is None or b is None:
        raise LoclError("Locl chunk missing NAME/BODY sub-chunks")
    npos, nsize = n
    name = body[npos + 8:npos + 8 + nsize].rstrip(b"\x00").decode("ascii", "replace")
    bpos, bsize = b
    tdata = body[bpos + 8:bpos + 8 + bsize]
    loc = LoclFile(name=name)
    pos = 0
    while pos + 6 <= len(tdata):
        eid = struct.unpack_from("<I", tdata, pos)[0]
        bc = struct.unpack_from("<H", tdata, pos + 4)[0]
        if bc < 6 or pos + bc > len(tdata):
            break
        raw = tdata[pos:pos + bc]
        text = raw[6:].decode("utf-16-le", "replace").rstrip("\x00")
        loc.entries.append({"id": eid, "text": text, "raw": raw})
        pos += bc
    loc.tail = tdata[pos:]
    return loc


def _build_body(loc: LoclFile) -> bytes:
    out = bytearray()
    for e in loc.entries:
        if e.get("raw") is not None:
            out += e["raw"]
        else:
            tb = e["text"].encode("utf-16-le") + b"\x00\x00\x00\x00"
            out += struct.pack("<I", e["id"]) + struct.pack("<H", 6 + len(tb)) + tb
    out += loc.tail
    return bytes(out)


def build_chunk(loc: LoclFile) -> lvl.Chunk:
    body = bytearray()
    nb = loc.name.encode("ascii") + b"\x00"
    body += _NAME + struct.pack("<I", len(nb)) + nb
    body += b"\x00" * ((4 - ((8 + len(nb)) % 4)) % 4)
    table = _build_body(loc)
    body += _BODY + struct.pack("<I", len(table)) + table
    pad = (4 - ((8 + len(body)) % 4)) % 4
    body += b"\x00" * pad
    return lvl.Chunk(_LOCL, len(body), bytes(body), b"")


def find_locl_chunks(l: lvl.LvlFile):
    return [c for c in l.chunks if c.type == _LOCL]
