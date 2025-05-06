# VTT Tool – Split and Combine WebVTT Subtitle Files

Tool to **split** `.vtt` (WebVTT subtitle) files into chunks or **combine** multiple `.vtt` files back into a single file.

---

## Usage

1. Split a VTT file into chunks of 10 seconds
```bash
python3 splitWebVTT.py <.vvt file>
```

2. Split a VTT file into chunks of n seconds
```bash
python3 splitWebVTT.py <.vvt file> --splitPeriod n
```

where is n is time in seconds

3. Combine multiple VTT files into one
```bash
python3 splitWebVTT.py <directory containing sequential .vtt files>
```