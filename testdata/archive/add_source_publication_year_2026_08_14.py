#!/usr/bin/env python3
"""Add the earliest documented public year to every normalized corpus source."""

from __future__ import annotations

import sqlite3
from pathlib import Path


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"

PUBLICATION_YEARS = {
    1: 1963, 2: 1967, 3: 1973, 4: 1995, 5: 2001, 6: 1989, 7: 1967, 8: 1966, 9: 1969, 10: 2012,
    11: 2009, 12: 2009, 13: 2010, 14: 2014, 15: 2021, 16: 2021, 17: 2019, 18: 2022, 19: 2021, 20: 2017,
    21: 2023, 22: 2026, 23: 2001, 24: 1992, 25: 2008, 26: 2009, 27: 2011, 28: 2012, 29: 2011, 30: 2012,
    31: 2013, 32: 2013, 33: 2014, 34: 2016, 35: 2016, 36: 2018, 37: 2014, 38: 2018, 39: 2019, 40: 2019,
    41: 2019, 42: 2021, 43: 2020, 44: 2021, 45: 2022, 46: 2021, 47: 2021, 48: 2024, 49: 2024, 50: 2024,
    51: 2024, 52: 2025, 53: 2025, 54: 2023, 55: 2026, 56: 2008, 57: 1996, 58: 2007, 59: 2008, 60: 2008,
    61: 2005, 62: 2011, 63: 2019, 64: 1997, 65: 1998, 66: 2009, 67: 2010, 68: 1999, 69: 2022, 70: 2016,
    71: 2015, 72: 2020, 73: 2012, 74: 2025, 75: 2023, 76: 2026, 77: 2018, 78: 2018, 79: 2011, 80: 1997,
    81: 2012, 82: 2021, 83: 2024, 84: 1999, 85: 2015, 86: 1991, 87: 2013, 88: 2004, 89: 2010, 90: 1990,
    91: 2013, 92: 2025, 93: 2026, 94: 2026,
}


def main() -> None:
    assert set(PUBLICATION_YEARS) == set(range(1, 95))

    with sqlite3.connect(DATABASE) as connection:
        columns = {row[1] for row in connection.execute("PRAGMA table_info(sources)")}
        if "publication_year" in columns:
            assert dict(connection.execute("SELECT source_id, publication_year FROM sources")) == PUBLICATION_YEARS
            print("all 94 sources already have publication years")
            return

        rows = list(connection.execute("SELECT source_id, authors, title, reference, comment FROM sources ORDER BY source_id"))
        assert [row[0] for row in rows] == list(PUBLICATION_YEARS)

        connection.execute("PRAGMA foreign_keys=OFF")
        connection.execute("BEGIN IMMEDIATE")
        try:
            connection.execute("""
                CREATE TABLE sources_with_publication_year (
                    source_id INTEGER PRIMARY KEY,
                    authors TEXT NOT NULL CHECK(length(authors) > 0),
                    title TEXT NOT NULL CHECK(length(title) > 0),
                    publication_year INTEGER NOT NULL CHECK(publication_year BETWEEN 1000 AND 9999),
                    reference TEXT NOT NULL CHECK(length(reference) > 0),
                    comment TEXT
                ) STRICT
            """)
            connection.executemany(
                "INSERT INTO sources_with_publication_year VALUES (?, ?, ?, ?, ?, ?)",
                [(source_id, authors, title, PUBLICATION_YEARS[source_id], reference, comment)
                 for source_id, authors, title, reference, comment in rows],
            )
            connection.execute("DROP TABLE sources")
            connection.execute("ALTER TABLE sources_with_publication_year RENAME TO sources")
            connection.commit()
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.execute("PRAGMA foreign_keys=ON")

        assert dict(connection.execute("SELECT source_id, publication_year FROM sources")) == PUBLICATION_YEARS
        assert not list(connection.execute("PRAGMA foreign_key_check"))

    print("added publication years to all 94 sources")


if __name__ == "__main__":
    main()
