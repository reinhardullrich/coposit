#!/usr/bin/env python3
"""Give every maintained matrix a normalized source link."""

from __future__ import annotations

import csv
import io
import sqlite3
from pathlib import Path
from urllib.parse import urlparse


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
FRACESSA_DATABASE = Path(__file__).parents[3] / "fracessa" / "testdata" / "fracessa_testdata.sqlite3"
EXPECTED_MATRIX_COUNT = 2442
EXPECTED_OLD_SOURCE_COUNT = 78
EXPECTED_OLD_LINK_COUNT = 901

NEW_SOURCES_CSV = """source_key,authors,title,reference,comment
suitesparse_2011,Timothy A. Davis; Yifan Hu,The University of Florida Sparse Matrix Collection,"ACM Transactions on Mathematical Software 38(1), Article 1 (2011); https://sparse.tamu.edu/",Primary collection source for the retained SuiteSparse matrices inherited through FracESSA.
qaplib_1998,Eranda Cela; Stefan E. Karisch; Franz Rendl,QAPLIB - A Quadratic Assignment Problem Library,"Journal of Global Optimization 10, 391-403 (1997); data archive doi:10.7488/ds/3428",Primary archive source for the retained QAPLIB matrices inherited through FracESSA.
house_of_graphs,House of Graphs contributors,House of Graphs,https://houseofgraphs.org/,Direct source of the retained deterministically stratified graph sample; exact graph IDs remain with each matrix.
anymatrix_repository,North Numerical Computing contributors,Anymatrix,https://github.com/north-numerical-computing/anymatrix,Exact generator repository used by the retained FracESSA matrix-generator audit; exact revision and generator remain with each matrix.
typedmatrices_repository,TypedMatrices.jl contributors,TypedMatrices.jl,https://github.com/TypedMatrices/TypedMatrices.jl,Exact Julia generator repository used by the retained FracESSA matrix-generator audit; exact revision and generator remain with each matrix.
sdplib_1999,Brian Borchers,SDPLIB 1.2 - A Library of Semidefinite Programming Test Problems,"Optimization Methods and Software 11-12, 683-690 (1999); https://github.com/vsdp/SDPLIB",Primary archive source for retained SDPLIB F0 objective matrices.
network_data_repository_2015,Ryan A. Rossi; Nesreen K. Ahmed,The Network Data Repository with Interactive Graph Analytics and Visualization,"AAAI 2015; https://networkrepository.com/",Primary archive source for the retained deterministic graph sample.
matlab_gallery,The MathWorks,Matrix gallery,https://www.mathworks.com/help/matlab/ref/gallery.html,Documented generator source for retained MATLAB gallery matrices inherited through FracESSA.
konect_2013,Jerome Kunegis,KONECT - The Koblenz Network Collection,"Proceedings of WWW Companion 2013; https://konect.cc/",Primary archive source for the retained network matrices.
magma_hadamard,University of Sydney Computational Algebra Group,Hadamard matrix database,https://magma.maths.usyd.edu.au/magma/download/db/hadamard.tar.gz,Primary archive source for the retained Hadamard representatives.
scipy_gallery,SciPy community,SciPy special and structured matrix generators,https://docs.scipy.org/doc/scipy/reference/linalg.html,Documented generator source for retained SciPy matrices inherited through FracESSA.
or_library_1990,J. E. Beasley,OR-Library - Distributing Test Problems by Electronic Mail,"Journal of the Operational Research Society 41(11), 1069-1072 (1990); https://people.brunel.ac.uk/~mastjjb/jeb/orlib/",Primary archive source for the retained OR-Library matrices.
fracessa_local,Reinhard Ullrich; FracESSA project,FracESSA exact test and regression corpus,"FracESSA local exact corpus; coposit immutable source snapshot testdata/archive/copos_testdata.original.sqlite3.xz",Primary source for deterministic local constructions and historical regression matrices; exact construction text remains with each matrix.
mincop_ldlt_repository,Alexander Oertel; MinCOP_LDLT contributors,MinCOP_LDLT exact copositivity test matrices,https://github.com/AlexOertel/MinCOP_LDLT/tree/7da4b072f135346523e0e14d529ec2511271eb0b/test_matrices,Direct repository source for 329 retained exact rational matrix representatives.
coposit_sparse_generator,Reinhard Ullrich; coposit project,Deterministic high-order sparse small-integer stress matrices,testdata/archive/import_high_order_small_integer_stress_2026_08_09.py,Exact local generator for the 90 sparse boundary strict and non-copositive high-order stress matrices.
coposit_dense_generator,Reinhard Ullrich; coposit project,Deterministic high-order dense randomized stress matrices,testdata/archive/import_high_order_dense_randomized_stress_2026_08_09.py,Exact local generator for the 90 dense boundary strict and non-copositive high-order stress matrices.
"""


def fracessa_source_key(source_url: str | None) -> str:
    if not source_url:
        return "fracessa_local"
    host = urlparse(source_url).netloc.lower()
    if host == "github.com":
        if "/north-numerical-computing/anymatrix/" in source_url:
            return "anymatrix_repository"
        if "/vsdp/SDPLIB/" in source_url:
            return "sdplib_1999"
        if "/TypedMatrices/TypedMatrices.jl/" in source_url:
            return "typedmatrices_repository"
    return {
        "houseofgraphs.org": "house_of_graphs",
        "sparse.tamu.edu": "suitesparse_2011",
        "doi.org": "qaplib_1998",
        "networkrepository.com": "network_data_repository_2015",
        "www.mathworks.com": "matlab_gallery",
        "konect.cc": "konect_2013",
        "magma.maths.usyd.edu.au": "magma_hadamard",
        "docs.scipy.org": "scipy_gallery",
        "qplib.zib.de": "qplib_2019",
        "web.archive.org": "or_library_1990",
    }[host]


def main() -> None:
    new_sources = list(csv.DictReader(io.StringIO(NEW_SOURCES_CSV)))
    assert len(new_sources) == 16
    assert FRACESSA_DATABASE.is_file(), f"missing FracESSA provenance database: {FRACESSA_DATABASE}"

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        source_count = connection.execute("SELECT count(*) FROM sources").fetchone()[0]
        linked_count = connection.execute("SELECT count(*) FROM matrices WHERE source_id IS NOT NULL").fetchone()[0]
        if source_count == 94 and linked_count == EXPECTED_MATRIX_COUNT:
            print("all 2442 matrices already have normalized source links")
            return
        assert source_count == EXPECTED_OLD_SOURCE_COUNT
        assert linked_count == EXPECTED_OLD_LINK_COUNT
        assert connection.execute("SELECT count(*) FROM matrices").fetchone()[0] == EXPECTED_MATRIX_COUNT

        connection.execute("ATTACH DATABASE ? AS fracessa", (str(FRACESSA_DATABASE),))
        connection.execute("BEGIN IMMEDIATE")
        try:
            first_source_id = EXPECTED_OLD_SOURCE_COUNT + 1
            connection.executemany(
                "INSERT INTO sources(source_id, authors, title, reference, comment) VALUES (?, ?, ?, ?, ?)",
                [
                    (first_source_id + offset, row["authors"], row["title"], row["reference"], row["comment"] or None)
                    for offset, row in enumerate(new_sources)
                ],
            )
            source_ids = {
                row["source_key"]: first_source_id + offset
                for offset, row in enumerate(new_sources)
            }
            source_ids.update(
                {
                    "sponsel_bundfuss_duer_2012": 30,
                    "ferreira_gao_nemeth_rigo_2024": 48,
                    "dimacs_1996": 57,
                    "qplib_2019": 63,
                }
            )

            legacy_rows = list(
                connection.execute(
                    """
                    SELECT m.matrix_id, m.source, f.origin, f.source_url
                    FROM matrices AS m
                    JOIN fracessa.matrices AS f ON f.matrix_id=CAST(substr(m.source, 10) AS INTEGER)
                    WHERE m.source_id IS NULL AND m.source LIKE 'FracESSA:%'
                    """
                )
            )
            assert len(legacy_rows) == 910
            connection.executemany(
                "UPDATE matrices SET source_id=?, source=? WHERE matrix_id=?",
                [
                    (
                        source_ids[fracessa_source_key(source_url)],
                        f"{old_source} | {origin}" + (f" <{source_url}>" if source_url else ""),
                        matrix_id,
                    )
                    for matrix_id, old_source, origin, source_url in legacy_rows
                ],
            )

            connection.execute(
                "UPDATE matrices SET source_id=?, source=? WHERE matrix_id=9164 AND source_id IS NULL",
                (
                    source_ids["sponsel_bundfuss_duer_2012"],
                    "Sponsel, Bundfuss and Duer 2012, C5 graph construction, strict side",
                ),
            )
            assert connection.execute("SELECT changes()").fetchone()[0] == 1

            for source_key, pattern, expected in (
                ("ferreira_gao_nemeth_rigo_2024", "Copositivity/Matrices:%", 67),
                ("mincop_ldlt_repository", "AlexOertel/MinCOP_LDLT:%", 329),
            ):
                connection.execute(
                    "UPDATE matrices SET source_id=? WHERE source_id IS NULL AND source LIKE ?",
                    (source_ids[source_key], pattern),
                )
                assert connection.execute("SELECT changes()").fetchone()[0] == expected

            for source_key, first_matrix_id, last_matrix_id, expected in (
                ("dimacs_1996", 9573, 9655, 53),
                ("fracessa_local", 9656, 9656, 1),
                ("coposit_sparse_generator", 10505, 10594, 90),
                ("coposit_dense_generator", 10595, 10684, 90),
            ):
                connection.execute(
                    "UPDATE matrices SET source_id=? WHERE source_id IS NULL AND matrix_id BETWEEN ? AND ?",
                    (source_ids[source_key], first_matrix_id, last_matrix_id),
                )
                assert connection.execute("SELECT changes()").fetchone()[0] == expected

            assert connection.execute("SELECT count(*) FROM sources").fetchone()[0] == 94
            assert connection.execute("SELECT count(*) FROM matrices WHERE source_id IS NULL").fetchone()[0] == 0
            assert not list(connection.execute("PRAGMA foreign_key_check"))
            connection.commit()
        except Exception:
            connection.rollback()
            raise

    print("added 16 corpus sources and linked all 2442 current matrices")


if __name__ == "__main__":
    main()
