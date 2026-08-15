#!/usr/bin/env python3
"""Import the 1+3+6+7+34 finite literature matrices selected after the main catalog migration."""

from __future__ import annotations

import hashlib
import json
import math
import os
import sqlite3
from functools import reduce
from pathlib import Path

from deduplicate_literature_import_2026_08_14 import fingerprints, load_values, permutation_equivalent


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
KEYS_DATA = Path(__file__).with_name("keys_zhou_lange_random_symmetric_seed1234_julia06.json")
KEYS_SHA256 = "c5bca4910e223d5fb8aa035ba73ef5a9d897eb9b05e67476df0e9b2cb1258fdf"
MARKER = "import_batch=small-literature-addendum-2026-08-14;"
EXTERNAL_THRESHOLD = 500_000
VUONG_ORDERS = tuple(range(1000, 5001, 250))


SOURCES = {
    "keys": (
        "Kevin L. Keys; Hua Zhou; Kenneth Lange",
        "Proximal Distance Algorithms: Theory and Practice",
        2019,
        "Journal of Machine Learning Research 20(66), 1-38 (2019); https://jmlr.org/papers/v20/17-687.html",
        "Companion code at commit 7997e81a15c5918b445f114893d624b6d9442c9f publishes srand(1234) and the sequential random-matrix recipe.",
    ),
    "dur": (
        "Mirjam Dür; Jean-Baptiste Hiriart-Urruty",
        "Testing copositivity with the help of difference-of-convex optimization",
        2013,
        "Mathematical Programming 140(1), 31-43 (2013); doi:10.1007/s10107-012-0625-9; https://www.math.univ-toulouse.fr/~jbhu/Duer-HU-final.2012.pdf",
        "Table 2 tests B_alpha=alpha(E-C5)-E at alpha=0.5, 1.7, and 1.9 from 1000 random starts per parameter setting.",
    ),
    "kuzmanovic": (
        "Dejan Kuzmanovic",
        "Copositivity detection - a preprocessing study",
        2022,
        "Master's thesis, University of Vienna (2022); https://utheses.univie.ac.at/detail/63296; https://github.com/Inferno29/CopositivityDetectionAlgorithm",
        "Primary source for six separately printed exact examples; the unseeded 100000-matrix result archive remains excluded.",
    ),
    "vuong": (
        "Francisco J. Aragón-Artacho; Rubén Campoy; Phan T. Vuong",
        "The Boosted DC Algorithm for Linearly Constrained DC Programming",
        2022,
        "Set-Valued and Variational Analysis 30, 1265-1289 (2022); doi:10.1007/s11228-022-00656-x",
        "Section 5.1 tests Q_n^mu=mu(E-A_cycle)-E for n=1000,1250,...,5000 at mu=2 and mu=1.9.",
    ),
    "zischg": (
        "Johannes Zischg; Immanuel M. Bomze",
        "Novel shortcut strategies in copositivity detection: Decomposition for quicker positive certificates",
        2025,
        "Operations Research Perspectives 14, 100324 (2025); doi:10.1016/j.orp.2024.100324",
        "Appendix B prints the order-5 premature-termination example and an explicit negative witness.",
    ),
}


def pack(rows: list[list[int]]) -> tuple[int, ...]:
    n = len(rows)
    assert n and all(len(row) == n for row in rows)
    assert all(rows[i][j] == rows[j][i] for i in range(n) for j in range(n))
    values = tuple(rows[i][j] for i in range(n) for j in range(i, n))
    divisor = reduce(math.gcd, map(abs, values)) or 1
    return tuple(value // divisor for value in values)


def cycle_values(n: int, positive: int, edge: int) -> tuple[int, ...]:
    return tuple(
        edge if i != j and (j == i + 1 or (i == 0 and j == n - 1)) else positive
        for i in range(n) for j in range(i, n)
    )


def matrix_source(instance: str, evidence: str) -> str:
    return f"{MARKER} instance={instance}; {evidence}"


def ensure_source(connection: sqlite3.Connection, key: str) -> int:
    authors, title, year, reference, comment = SOURCES[key]
    row = connection.execute(
        "SELECT source_id,authors,publication_year,reference FROM sources WHERE title=?", (title,)
    ).fetchone()
    if row:
        if row[1:] != (authors, year, reference):
            raise ValueError(f"source metadata conflict for {title}")
        return row[0]
    source_id = connection.execute("SELECT coalesce(max(source_id),0)+1 FROM sources").fetchone()[0]
    connection.execute(
        "INSERT INTO sources(source_id,authors,title,publication_year,reference,comment) VALUES (?,?,?,?,?,?)",
        (source_id, authors, title, year, reference, comment),
    )
    return source_id


def solved(source_id: int, comment: str) -> str:
    return json.dumps([{"source_id": source_id, "comment": comment}], separators=(",", ":"))


def unsolved(source_id: int, comment: str) -> str:
    return json.dumps([{"source_id": source_id, "comment": comment}], separators=(",", ":"))


def write_file(path: Path, chunks) -> tuple[str, bool]:
    path.parent.mkdir(exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    digest = hashlib.sha256()
    with temporary.open("wb") as output:
        for chunk in chunks:
            digest.update(chunk)
            output.write(chunk)
    expected = digest.hexdigest()
    if path.exists():
        with path.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        temporary.unlink()
        if actual != expected:
            raise ValueError(f"existing matrix payload differs: {path}")
        return actual, False
    os.replace(temporary, path)
    return expected, True


def values_storage(name: str, n: int, values: tuple[int, ...] | list[int]) -> tuple[str, str | None, Path | None]:
    text = ",".join(map(str, values))
    if len(text) <= EXTERNAL_THRESHOLD:
        return text, None, None
    path = DATABASE.parent / "matrices" / f"{name}.mtx"
    header = f"%%MatrixMarket matrix array integer symmetric\n{n} {n}\n".encode()
    digest, created = write_file(path, (header, *(f"{value}\n".encode() for value in values)))
    return f"file:matrices/{path.name}", digest, path if created else None


def cycle_storage(n: int, positive: int, edge: int, label: str) -> tuple[str, str, Path | None]:
    path = DATABASE.parent / "matrices" / f"vuong_cycle_{label}_n{n}.mtx"
    positive_line = f"{positive}\n".encode()
    edge_line = f"{edge}\n".encode()

    def chunks():
        yield f"%%MatrixMarket matrix array integer symmetric\n{n} {n}\n".encode()
        for column in range(n):
            cursor = column
            negative_rows = {column + 1} if column + 1 < n else set()
            if column == 0:
                negative_rows.add(n - 1)
            for row in sorted(negative_rows):
                if row > cursor:
                    yield positive_line * (row - cursor)
                yield edge_line
                cursor = row + 1
            if cursor < n:
                yield positive_line * (n - cursor)

    digest, created = write_file(path, chunks())
    return f"file:matrices/{path.name}", digest, path if created else None


def assert_not_duplicate(connection: sqlite3.Connection, candidates: list[tuple[str, int, tuple[int, ...]]]) -> None:
    by_dimension: dict[int, list[tuple[int, tuple[int, ...], tuple]]] = {}
    for n in sorted({candidate[1] for candidate in candidates}):
        rows = connection.execute("SELECT matrix_id,matrix FROM matrices WHERE dimension=?", (n,)).fetchall()
        by_dimension[n] = []
        for matrix_id, storage in rows:
            values = load_values(n, storage)
            by_dimension[n].append((matrix_id, values, fingerprints(n, values)[1]))
    for name, n, values in candidates:
        invariant = fingerprints(n, values)[1]
        for matrix_id, other, other_invariant in by_dimension[n]:
            if invariant == other_invariant and permutation_equivalent(n, values, other):
                raise ValueError(f"new {name} duplicates existing matrix {matrix_id} under scaling/permutation")


def main() -> None:
    if hashlib.sha256(KEYS_DATA.read_bytes()).hexdigest() != KEYS_SHA256:
        raise ValueError("Keys-Zhou-Lange materialization artifact hash mismatch")
    keys_matrices = json.loads(KEYS_DATA.read_text())
    if [item["dimension"] for item in keys_matrices] != [4, 8, 16, 32, 64, 128, 256]:
        raise ValueError("wrong Keys-Zhou-Lange dimensions")

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("BEGIN IMMEDIATE")
        existing = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]
        if existing == 51:
            print("already_imported=1 matrices=51")
            return
        if existing:
            raise ValueError(f"partial small-literature import: {existing}/51 rows")

        source_ids = {key: ensure_source(connection, key) for key in SOURCES}
        dur_solved = solved(source_ids["dur"], "Table 2 reports negative-witness outcomes from the 1000-start DC heuristic")
        kuz_solved = solved(source_ids["kuzmanovic"], "the thesis explicitly classifies the printed example")
        keys_solved = solved(source_ids["keys"], "PD, accelerated PD, and Mosek numerical simplex-minimum experiment; not an exact positive certificate")
        zischg_solved = solved(source_ids["zischg"], "Appendix B gives an explicit violating vector")

        records = [
            ("zischg_bomze_appendix_b", 5, pack([
                [10, -5, 4, 0, 0], [-5, 10, -9, -7, 0], [4, -9, 10, 9, 3],
                [0, -7, 9, 10, -9], [0, 0, 3, -9, 10],
            ]), 0, 0, "zischg", "Zischg-Bomze Appendix B premature-termination example",
             "decimal matrix scaled by 10; x=(7/40,7/20,0,3/4,3/4) gives x^T(10A)x=-261/160", zischg_solved, "[]"),
        ]
        for alpha, positive, edge in (("0.5", -1, -2), ("1.7", 7, -10), ("1.9", 9, -10)):
            failure = "[]" if alpha == "0.5" else unsolved(
                source_ids["dur"], "most of the 1000 randomized DC starts in Table 2 ended without a decision; other starts found a negative witness"
            )
            records.append((
                f"dur_hiriart_urruty_B_alpha_{alpha}", 5, cycle_values(5, positive, edge), 0, 0, "dur",
                f"Dür-Hiriart-Urruty C5 B_alpha family, alpha={alpha}",
                f"exact scaling of B_alpha=alpha(E-C5)-E; two adjacent coordinates 1/2 give alpha/2-1<0",
                dur_solved, failure,
            ))

        kuzmanovic = (
            ("case2_order2", [[0, -1], [-1, 1]], 0, 0, "x=(1,1) gives -1"),
            ("remark3_1_order3", [[0, -1, 1], [-1, 1, 1], [1, 1, 1]], 0, 0, "x=(1,1,0) gives -1"),
            ("inverse_example_order2", [[-2, 1], [1, -2]], 0, 0, "e1 gives -2"),
            ("spectral_example_order2", [[7, 2], [2, 1]], 1, 1, "positive definite: determinant 3 and leading entry 7"),
            ("section5_zero_diagonal_order5", [[0, -1, 1, 1, 1], [-1, 1, 1, 1, 1], [1, 1, 1, 1, 1],
                                                [1, 1, 1, 1, 1], [1, 1, 1, 1, 1]], 0, 0, "x=(2,1,0,0,0) gives -3"),
            ("section5_random_order5", [[32, 5, 9, 29, 43], [5, 0, 21, -12, 40], [9, 21, 13, 45, 14],
                                        [29, -12, 45, 20, 20], [43, 40, 14, 20, 50]], 0, 0,
             "x=(0,21,0,12,0) gives -3168"),
        )
        for name, rows, strict, copositive, evidence in kuzmanovic:
            records.append((f"kuzmanovic_{name}", len(rows), pack(rows), strict, copositive, "kuzmanovic",
                            "Kuzmanovic thesis printed example", evidence, kuz_solved, "[]"))

        for item in keys_matrices:
            n, values = item["dimension"], tuple(item["values"])
            if len(values) != n * (n + 1) // 2:
                raise ValueError(f"wrong Keys packed length at n={n}")
            negative = next(i for i in range(n) if values[i * n - i * (i - 1) // 2] < 0)
            records.append((
                f"keys_random_symmetric_n{n}", n, values, 0, 0, "keys",
                "Keys-Zhou-Lange seeded random symmetric Gaussian",
                f"published sequential srand(1234) Julia 0.6 recipe; exact Float64 primitive scaling; diagonal {negative + 1} is negative",
                keys_solved, "[]",
            ))

        assert len(records) == 17
        assert_not_duplicate(connection, [(row[0], row[1], row[2]) for row in records])

        created_files: list[Path] = []
        first_id = connection.execute("SELECT max(matrix_id)+1 FROM matrices").fetchone()[0]
        try:
            for offset, (name, n, values, strict, copositive, source_key, family, evidence, solved_json, unsolved_json) in enumerate(records):
                storage, digest, created = values_storage(name, n, values)
                if created:
                    created_files.append(created)
                connection.execute(
                    """INSERT INTO matrices(
                           matrix_id,dimension,matrix,file_sha256,is_strictly_copositive,is_copositive,source,source_id,family,
                           references_solved,references_unsolved
                       ) VALUES (?,?,?,?,?,?,?,?,?,?,?)""",
                    (first_id + offset, n, storage, digest, strict, copositive, matrix_source(name, evidence), source_ids[source_key],
                     family, solved_json, unsolved_json),
                )

            vuong_solved = {
                "mu2": solved(source_ids["vuong"], "100-start DCA/BDCA numerical experiment on the copositive boundary; not an exact positive certificate"),
                "mu1p9": solved(source_ids["vuong"], "100-start DCA/BDCA numerical experiment on the near-boundary non-copositive family"),
            }
            offset = len(records)
            for label, positive, edge, strict, copositive, truth in (
                ("mu2", 1, -1, 0, 1, "mu=2 is copositive but not strict: the cycle clique number is 2 and the simplex minimum is zero"),
                ("mu1p9", 9, -10, 0, 0, "10Q_n^1.9 has simplex minimum 10(1.9/2-1)=-1/2<0"),
            ):
                for n in VUONG_ORDERS:
                    storage, digest, created = cycle_storage(n, positive, edge, label)
                    if created:
                        created_files.append(created)
                    name = f"vuong_cycle_{label}_n{n}"
                    connection.execute(
                        """INSERT INTO matrices(
                               matrix_id,dimension,matrix,file_sha256,is_strictly_copositive,is_copositive,source,source_id,family,
                               references_solved,references_unsolved
                           ) VALUES (?,?,?,?,?,?,?,?,?,?,?)""",
                        (first_id + offset, n, storage, digest, strict, copositive, matrix_source(name, truth), source_ids["vuong"],
                         f"Vuong-Aragón-Artacho-Campoy cycle Q_n^mu, {label}", vuong_solved[label], "[]"),
                    )
                    offset += 1

            if offset != 51:
                raise ValueError(f"wrong imported count: {offset}")
            if connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0] != 51:
                raise ValueError("wrong persisted import count")
            if list(connection.execute("PRAGMA foreign_key_check")):
                raise ValueError("foreign-key check failed")
            connection.commit()
        except Exception:
            connection.rollback()
            for path in created_files:
                path.unlink(missing_ok=True)
            raise

    print(f"imported=51 matrix_ids={first_id}-{first_id + 50} sources={source_ids}")


if __name__ == "__main__":
    main()
