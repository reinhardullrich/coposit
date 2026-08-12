# Corpus Archive

This directory preserves one-time corpus construction and migration material. None of it is used by the maintained C++ or Python
runtime.

- `copos_testdata.original.sqlite3.xz` is the immutable byte-exact FracESSA source database.
- `FRACESSA_TESTDATA_README.md` is its historical documentation.
- `import_*.py` and `corpus_matrix.py` preserve the exact matrix-generation and verification procedures used for dated corpus imports.
- `externalize_large_matrices.py` and `add_matrix_file_hashes_2026_08_11.py` preserve the completed external-file migration.
- The dated `.sql` files preserve applied schema, classification, result, and benchmark-set migrations.

The maintained corpus is `../copos_testdata.sqlite3`; its current schema is `../schema.sql`, and externally stored matrices are under
`../matrices/`.
