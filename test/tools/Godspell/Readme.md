# Godspell

Godspell tool recursively looks for typos inside a directory and generates a CSV report "typos.csv".

## Usage

```
python3 godSpell.py
```

Above command will default to use root directory as "../../.." (Current AAMP directory).

```
python3 godSpell.py /path/to/directory
```

Use the above command to change default root directory.

## Whitelist
1. The script uses words_alpha.txt (an English dictionary) and extra.txt (supplemental project-specific dictionary). To whitelist more word(s), please add custom word(s) to extra.txt.
2. To ignore any lines containing specific text, add that text to ignore_lines.txt
3. To ignore all words containing a specific prefix, add that prefix to ignore_prefixes.txt