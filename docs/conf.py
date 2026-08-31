from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

project = "coposit"
author = "Reinhard Ullrich"
copyright = "2026, Reinhard Ullrich"

extensions = ["sphinx.ext.autodoc", "sphinx.ext.napoleon", "breathe"]
autodoc_typehints = "description"
napoleon_google_docstring = True
breathe_projects = {"coposit": str(ROOT / "docs" / "_build" / "xml")}
breathe_default_project = "coposit"

html_theme = "alabaster"
html_title = "coposit"
html_short_title = "coposit"
html_baseurl = "https://reinhardullrich.github.io/coposit/"
html_static_path = ["_static"]
html_extra_path = ["robots.txt", "sitemap.xml"]
html_css_files = ["coposit.css"]
html_show_sphinx = False
html_sidebars = {"**": ["about.html", "searchfield.html", "navigation.html", "localtoc.html"]}
html_theme_options = {
    "description": "Exact copositivity testing for symmetric matrices",
    "fixed_sidebar": True,
    "github_user": "reinhardullrich",
    "github_repo": "coposit",
    "github_button": False,
    "sidebar_collapse": False,
}
