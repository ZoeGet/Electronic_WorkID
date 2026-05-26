#!/usr/bin/env python3
"""
pcm_to_dac_array.py

Convert signed 16-bit little-endian PCM audio into a C uint16_t array suitable
for STM32 DAC 12-bit right-aligned playback.

Recommended input PCM format:
  - signed 16-bit little-endian
  - mono
  - 16000 Hz

Example ffmpeg command:
  ffmpeg -i start_record.wav -ac 1 -ar 16000 -f s16le start_record.pcm

Example usage:
  python pcm_to_dac_array.py start_record.pcm tts_start_record.c tts_start_record
  python pcm_to_dac_array.py stop_record.pcm tts_stop_record.c tts_stop_record --volume 0.75
  python pcm_to_dac_array.py start_record.pcm tts_start_record.c tts_start_record --header tts_audio_data.h
"""

from __future__ import annotations

import argparse
import os
import re
import struct
from pathlib import Path


DAC_BITS = 12
DAC_MIN = 0
DAC_MAX = (1 << DAC_BITS) - 1
DAC_MID = 1 << (DAC_BITS - 1)


def sanitize_symbol(name: str) -> str:
    symbol = re.sub(r"\W+", "_", name.strip())
    symbol = symbol.strip("_")
    if not symbol:
        symbol = "tts_audio"
    if symbol[0].isdigit():
        symbol = f"_{symbol}"
    return symbol


def pcm_s16le_to_dac12(data: bytes, volume: float) -> list[int]:
    if len(data) % 2 != 0:
        raise ValueError("PCM file size must be even for signed 16-bit little-endian data")

    if volume < 0.0:
        raise ValueError("volume must be greater than or equal to 0")

    values: list[int] = []

    for (sample,) in struct.iter_unpack("<h", data):
        scaled = int(sample * volume)
        dac = DAC_MID + (scaled >> 4)
        if dac < DAC_MIN:
            dac = DAC_MIN
        elif dac > DAC_MAX:
            dac = DAC_MAX
        values.append(dac)

    return values


def format_c_array(values: list[int], symbol: str, samples_per_line: int) -> str:
    lines: list[str] = []
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"const uint16_t {symbol}[] = {{")

    for i in range(0, len(values), samples_per_line):
        chunk = values[i:i + samples_per_line]
        line = ", ".join(f"{value:4d}" for value in chunk)
        if i + samples_per_line < len(values):
            lines.append(f"    {line},")
        else:
            lines.append(f"    {line}")

    lines.append("};")
    lines.append("")
    lines.append(f"const uint32_t {symbol}_len = sizeof({symbol}) / sizeof({symbol}[0]);")
    lines.append("")
    return "\n".join(lines)


def format_header(symbols: list[str], guard: str) -> str:
    lines: list[str] = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")

    for symbol in symbols:
        lines.append(f"extern const uint16_t {symbol}[];")
        lines.append(f"extern const uint32_t {symbol}_len;")
        lines.append("")

    lines.append(f"#endif /* {guard} */")
    lines.append("")
    return "\n".join(lines)


def append_or_write_header(header_path: Path, symbol: str) -> None:
    guard = sanitize_symbol(header_path.stem).upper() + "_H"

    if not header_path.exists():
        header_path.write_text(format_header([symbol], guard), encoding="utf-8")
        return

    content = header_path.read_text(encoding="utf-8")
    if f"extern const uint16_t {symbol}[];" in content:
        return

    insert = f"extern const uint16_t {symbol}[];\nextern const uint32_t {symbol}_len;\n\n"
    endif_match = re.search(r"\n#endif\s*/\*.*?\*/\s*\n?$", content, flags=re.DOTALL)

    if endif_match:
        new_content = content[:endif_match.start() + 1] + insert + content[endif_match.start() + 1:]
    else:
        new_content = content.rstrip() + "\n\n" + insert

    header_path.write_text(new_content, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert signed 16-bit little-endian PCM to STM32 DAC 12-bit C array."
    )
    parser.add_argument("input_pcm", help="Input PCM file, signed 16-bit little-endian")
    parser.add_argument("output_c", help="Output .c file path")
    parser.add_argument(
        "symbol",
        nargs="?",
        help="C array symbol name. Defaults to sanitized input file stem.",
    )
    parser.add_argument(
        "--volume",
        type=float,
        default=0.80,
        help="Software volume multiplier. Default: 0.80",
    )
    parser.add_argument(
        "--samples-per-line",
        type=int,
        default=12,
        help="Number of samples per C array line. Default: 12",
    )
    parser.add_argument(
        "--header",
        help="Optional header file to create/update with extern declarations.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    input_path = Path(args.input_pcm)
    output_path = Path(args.output_c)
    symbol = sanitize_symbol(args.symbol or input_path.stem)

    if args.samples_per_line <= 0:
        raise ValueError("--samples-per-line must be greater than 0")

    data = input_path.read_bytes()
    values = pcm_s16le_to_dac12(data, args.volume)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(format_c_array(values, symbol, args.samples_per_line), encoding="utf-8")

    if args.header:
        append_or_write_header(Path(args.header), symbol)

    duration_seconds = len(values) / 16000.0
    print(f"Input:       {input_path}")
    print(f"Output:      {output_path}")
    print(f"Symbol:      {symbol}")
    print(f"Samples:     {len(values)}")
    print(f"Duration:    {duration_seconds:.3f}s at 16000 Hz")
    print(f"Volume:      {args.volume:.3f}")
    print(f"DAC range:   {min(values) if values else 0}..{max(values) if values else 0}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
