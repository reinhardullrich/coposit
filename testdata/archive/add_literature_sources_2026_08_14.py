#!/usr/bin/env python3
"""Add normalized literature sources and link current corpus matrices to their earliest located source."""

from __future__ import annotations

import csv
import io
import sqlite3
from pathlib import Path


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
EXPECTED_MATRIX_COUNT = 2442

SOURCES_CSV = """source_key,authors,title,reference,comment
hall_newman_1963,Marshall Hall Jr.; Morris Newman,Copositive and completely positive quadratic forms,"Mathematical Proceedings of the Cambridge Philosophical Society 59(2), 329-339 (1963)",Primary printed source for the Horn matrix.
hall_1967,Marshall Hall Jr.,Combinatorial Theory,"Blaisdell Publishing Company, Boston (1967)",Primary source identified by Hiriart-Urruty-Seeger for the printed order-5 doubly-nonnegative but not completely-positive matrix.
hoffman_pereira_1973,Alan J. Hoffman; Francisco Pereira,"On copositive matrices with -1, 0, 1 entries","Journal of Combinatorial Theory, Series A 14 (1973); IBM publication page: https://research.ibm.com/publications/on-copositive-matrices-with-1-0-1-entries","Primary source for the named Hoffman-Pereira matrix and its graph class; later used by Brás et al., Nie et al., Anstreicher, and Peng."
andersson_chang_elfving_1995,Lars-Erik Andersson; G. Chang; Tommy Elfving,Criteria for copositive matrices using simplices and barycentric coordinates,"Linear Algebra and its Applications 220, 9-30 (1995)",Primary printed source for the order-4 copositive example reused by Bomze-Eichfelder.
kaplan_2001,Wilfred Kaplan,A copositivity probe,"Linear Algebra and its Applications 337, 237-251 (2001)",Primary source for Brás matrices M2-M4; M4 is a principal submatrix of Kaplan Example 1.
valiaho_1989,Hannu Väliaho,Quadratic-programming criteria for copositive matrices,"Linear Algebra and its Applications 119, 163-182 (1989)",Primary source for Brás matrix M5 and its order-4 principal submatrix.
baumert_1967,Leonard D. Baumert,Extreme copositive quadratic forms. II,"Pacific Journal of Mathematics 20(1), 1-20 (1967), Theorem 4.6",Prints an exact radical order-5 exceptional extremal matrix; retained symbolically rather than rationalized.
baumert_1966,Leonard D. Baumert,Extreme copositive quadratic forms,"Pacific Journal of Mathematics 19(2), 197-204 (1966), Theorem 3.8",Primary exact variable-splitting extension that lifts any extreme copositive form to every higher order.
baston_1969,Victor Baston,Extreme copositive quadratic forms,"Acta Arithmetica 15(3), 319-327 (1969); doi:10.4064/aa-15-3-319-327",Primary construction for the all-orders and cyclic exceptional families already sampled in the corpus.
hildebrand_2012,Roland Hildebrand,The extreme rays of the 5 x 5 copositive cone,"Linear Algebra and its Applications 437(7), 1538-1547 (2012)",Primary complete parametrization for the non-Horn order-5 exceptional family already sampled in the corpus.
iusem_seeger_2009,Alfredo N. Iusem; Alberto Seeger,Searching for critical angles in a convex cone,"Mathematical Programming 120(1), 3-25 (2009)",Primary source credited for the exact order-2 antipodal copositive pair reprinted by Hiriart-Urruty-Seeger.
burer_anstreicher_duer_2009,Samuel Burer; Kurt M. Anstreicher; Mirjam Dür,The difference between 5 x 5 doubly nonnegative and completely positive matrices,"Linear Algebra and its Applications 431(9), 1539-1552 (2009); doi:10.1016/j.laa.2009.05.021","Prints an exact parametrization of all order-5 extremely bad DNN matrices and several exact finite examples, including Horn separators."
hiriart_urruty_seeger_2010,Jean-Baptiste Hiriart-Urruty; Alberto Seeger,A variational approach to copositive matrices,"SIAM Review 52(4), 593-629 (2010); doi:10.1137/090750391",Survey containing exact finite examples and exact low-order copositive families; earliest identified sources are used where the survey names them.
hildebrand_case34,Roland Hildebrand,Case 34 of the order-6 copositive-cone classification,https://membres-ljk.imag.fr/Roland.Hildebrand/c6classification/case34.pdf,Exact rational half-angle points from this family are already present in the corpus.
afonin_hildebrand_dickinson_2021,Andrey Afonin; Roland Hildebrand; Peter J. C. Dickinson,The extreme rays of the 6 x 6 copositive cone,"Journal of Global Optimization 79(1), 153-190 (2021)",Primary classification source for the Case 13.1 and Case 18 family points already sampled in the corpus.
dickinson_de_zeeuw_2021,Peter J. C. Dickinson; Reinier de Zeeuw,Generating irreducible copositive matrices using the stable set problem,"Discrete Applied Mathematics 296, 103-117 (2021)",Table 2 supplies the graph6 records and stability numbers for the existing cop-irreducible graph matrices.
dickinson_2019,Peter J. C. Dickinson,A new certificate for copositivity,"Linear Algebra and its Applications 569, 15-37 (2019); doi:10.1016/j.laa.2018.12.025",Section 5 prints the Case 9 family from which three exact corpus points were retained.
hildebrand_afonin_2024,Roland Hildebrand; Andrey Afonin,On the structure of the 6 x 6 copositive cone,"Linear Algebra and its Applications 693, 22-38 (2024); doi:10.1016/j.laa.2023.02.004; arXiv:2209.08039",Constructs copositive matrices outside Parrilo's first level; three exact nearby family points are already in the corpus.
laurent_vargas_2023,Monique Laurent; Luis Felipe Vargas,Exactness of Parrilo's conic approximations for copositive matrices and associated low order bounds for the stability number of a graph,Mathematical Programming 2023; arXiv:2109.12876,Primary direct-sum construction for the seven exact corpus samples lying outside every Parrilo level.
hildebrand_2017,Roland Hildebrand,Copositive matrices with circulant zero support set,"Linear Algebra and its Applications 514, 1-46 (2017)",Primary source for the circulant support-n-2 family already sampled at orders 7-25 in the corpus.
dannenberg_schuermann_2023,Valentin Dannenberg; Achill Schürmann,Perfect copositive matrices,"Communications in Mathematics 31(2), 137-156 (2023); doi:10.46298/cm.11141",Prints two seeds and a theorem-backed lift construction; the two finite lift chains are already in the corpus.
kostyukova_tchemisova_2026,Olga Kostyukova; Tatiana Tchemisova,Generation of extremal copositive matrices in higher dimensions,"Axioms 15(6), 414 (2026)",Prints four exact examples and an extension construction; exact examples and selected diagonal congruences are already in the corpus.
bomze_deklerk_2002,Immanuel M. Bomze; Etienne de Klerk,"Solving standard quadratic optimization problems via linear, semidefinite and copositive programming","Journal of Global Optimization 24, 163-185 (2002), Examples 5.1-5.4; https://optimization-online.org/2001/05/329/","Primary printed source for the pentagon, icosahedron, population-genetics, and portfolio StQP matrices later reused by Sponsel et al."
bomze_1992,Immanuel M. Bomze,Detecting all evolutionarily stable strategies,"Journal of Optimization Theory and Applications 75(2), 313-329 (1992), Example 2.1",Prints an exact order-5 payoff matrix used to demonstrate the support traversal and strict-copositivity subproblems inherited by FracESSA.
bundfuss_duer_2008,Stefan Bundfuss; Mirjam Dür,Algorithmic copositivity detection by simplicial partition,"Linear Algebra and its Applications 428, 1511-1523 (2008)",Primary algorithm paper; companion face-test repository retained under upstream/bundfuss_faces.
bundfuss_duer_2009,Stefan Bundfuss; Mirjam Dür,An adaptive linear approximation algorithm for copositive programs,"SIAM Journal on Optimization 20(1), 30-53 (2009); doi:10.1137/070711815","Prints one finite order-2 copositive program; reuses Bomze-de Klerk Q1-Q4, G17, Johnson8-2-4, and Hamming6-4, and reports unseeded random StQPs through order 10000."
zilinskas_2011_programming,Julius Žilinskas,Copositive programming by simplicial partition,"Informatica 22(4), 601-614 (2011); doi:10.3233/INF-2011-22(4)09","Prints two new finite examples, reuses four Bomze-de Klerk matrices, and reports 27 generated and 29 DIMACS graph instances."
bomze_schachinger_uchida_2012,Immanuel M. Bomze; Werner Schachinger; Gabriele Uchida,"Think co(mpletely)positive! Matrix properties, examples and a clustered bibliography on copositive optimization","Journal of Global Optimization 52(3), 423-445 (2012); doi:10.1007/s10898-011-9749-3","Prints exact closure counterexamples and exact copositive-program pathologies, together with two deterministic lift constructions."
zilinskas_duer_2011,Julius Žilinskas; Mirjam Dür,"Depth-first simplicial partition for copositivity detection, with an application to MaxClique","Optimization Methods and Software 26(3), 499-510 (2011), Table 5; doi:10.1080/10556788.2010.544310",Uses 28 named DIMACS graphs through order 3361; the paper's reported lower-bound parameter is retained with each transform.
sponsel_bundfuss_duer_2012,Julia Sponsel; Stefan Bundfuss; Mirjam Dür,An improved algorithm to test copositivity,"Journal of Global Optimization 52, 537-551 (2012), Table 1 and Figure 1; doi:10.1007/s10898-011-9766-2",Primary source for the 16 transformed test matrices; its G8 and G12 drawings are also reused by Tanaka-Yoshise and Safi-Nabavi-Caron.
bomze_eichfelder_2013,Immanuel M. Bomze; Gabriele Eichfelder,Copositivity detection by difference-of-convex decomposition and omega-subdivision,"Mathematical Programming 138, 365-400 (2013)",Primary printed source for several small decimal examples and large random campaigns without published seeds.
deng_fang_jin_xing_2013,Zhibin Deng; Shu-Cherng Fang; Qingwei Jin; Wenxun Xing,Detecting copositivity of a symmetric matrix by an adaptive ellipsoid-based approximation scheme,"European Journal of Operational Research 229(1), 21-28 (2013); doi:10.1016/j.ejor.2013.02.031","The first author's dissertation reproduces the examples. The authors' archived code preserves 19 MATLAB result matrices, but not all random realizations or RNG states: https://web.archive.org/web/20160810004944id_/http://www.ise.ncsu.edu/fangroup/Adaptive_Copositiv_Detection.zip"
tanaka_yoshise_2015,Akihiro Tanaka; Akiko Yoshise,An LP-based algorithm to test copositivity,"Pacific Journal of Optimization 11, 101-120 (2015), Tables 1, 2, and 5; https://optimization-online.org/2014/04/4328/",Uses the Sponsel G8/G12 graphs over dense parameter grids and four strictifying perturbations.
tanaka_yoshise_2018,Akihiro Tanaka; Akiko Yoshise,LP-based tractable subcones of the semidefinite plus nonnegative cone,"Annals of Operations Research 265(1), 155-182 (2018); doi:10.1007/s10479-017-2720-z; arXiv:1601.06878",Extends the earlier G8/G12 parameter grids and reports unseeded random PSD-plus-nonnegative and Nowak-generator campaigns.
bras_eichfelder_judice_2016,Carlos Brás; Gabriele Eichfelder; Joaquim Júdice,Copositivity tests based on the Linear Complementarity Problem,"Computational Optimization and Applications 63, 461-493 (2016), Section 4 and Table 1",Prints M1-M7 and uses generated and DIMACS clique matrices; M1 first appeared in Bomze-Eichfelder.
nie_yang_zhang_2018,Jiawang Nie; Zi Yang; Xinzhen Zhang,A complete semidefinite algorithm for detecting copositive matrices and tensors,"SIAM Journal on Optimization 28(4), 2902-2921 (2018), Examples 4.1-4.8","Contains Horn, Hoffman-Pereira, a symbolic trigonometric extreme matrix, a Horn perturbation, and one exact graph matrix."
bomze_schachinger_ullrich_2014,Immanuel M. Bomze; Werner Schachinger; Reinhard Ullrich,From seven to eleven: completely positive matrices with high cp-rank,"Linear Algebra and its Applications 459, 208-221 (2014)","Prints exact copositive support matrices, their zero generators, and exact completely-positive counterexamples of orders 7-11."
bomze_schachinger_ullrich_2018,Immanuel M. Bomze; Werner Schachinger; Reinhard Ullrich,The complexity of simple models - a study of worst and typical hard cases for the Standard Quadratic Optimization Problem,"Mathematical Methods of Operations Research 88, 171-199 (2018), Tables 1-2",Primary printed source for the 20 cyclic hard StQP matrices and nine non-cyclic lifts.
liuzzi_locatelli_piccialli_2019,Giampaolo Liuzzi; Marco Locatelli; Veronica Piccialli,A new branch-and-bound algorithm for standard quadratic programming problems,"Optimization Methods and Software 34(1), 79-97 (2019)",Paper and public StQP archive; retained archive includes the generator and exact published decimal files through order 1000.
keys_zhou_lange_2019,Kevin L. Keys; Hua Zhou; Kenneth Lange,Proximal Distance Algorithms: Theory and Practice,"Journal of Machine Learning Research 20(66), 1-38 (2019); https://jmlr.org/papers/v20/17-687.html",https://github.com/klkeys/proxdist at commit 7997e81a15c5918b445f114893d624b6d9442c9f preserves the exact Horn generator and a seeded random-matrix script.
badenbroek_deklerk_2019,Rogier Badenbroek; Etienne de Klerk,Simulated annealing with hit-and-run for convex optimization: rigorous complexity analysis and practical perspectives for copositive programming,"arXiv:1907.02368 (2019), Appendix B",First printed source for ten exact order-6 DNN but non-CP matrices; reused by Badenbroek-de Klerk 2021 and Nishijima et al. 2025.
badenbroek_deklerk_2021,Rogier Badenbroek; Etienne de Klerk,A computational study of the copositive cone through its analytic center,INFORMS Journal on Computing 33(4) (2021); companion repository commit 0f1ee669,"https://github.com/rileybadenbroek/CopositiveAnalyticCenter.jl supplies 80 exact decimal matrix files at orders 6, 7, 8, 9, 10, 15, 20, and 25."
anstreicher_2021,Kurt M. Anstreicher,Testing copositivity via mixed-integer linear programming,"Linear Algebra and its Applications 609, 94-113 (2021); https://optimization-online.org/2020/03/7659/","Uses Horn, Hoffman-Pereira, a Horn diagonal perturbation, G17, icosahedron and DIMACS graphs; random SPN/clique campaigns have no published seeds."
gondzio_yildirim_2021,Jacek Gondzio; E. Alper Yildirim,Global solutions of nonconvex standard quadratic programs via mixed integer linear programming reformulations,"Journal of Global Optimization 81, 629-654 (2021)","Uses BLST, ST/Liuzzi, DIMACS, and all 20 BSU hard instances."
peng_2022,Bo Peng,Performance comparison of two recently proposed copositivity tests,"EURO Journal on Computational Optimization 10, 100037 (2022); doi:10.1016/j.ejco.2022.100037","Defines a hard exceptional {-1,0,1} test family from Hoffman-Pereira and reports large unseeded random campaigns."
gtoh_hendrix_casado_2021,B. G.-Tóth; Eligius M. T. Hendrix; Leocadio G. Casado,On monotonicity and search strategies in face-based copositivity detection,"Journal of Global Optimization 80, 1073-1092 (2021), Appendix A-B","Prints Horn, Brock14, 1tc.16.clique, and six exact two-decimal Nowak-derived matrices."
safi_nabavi_caron_2021,Mohammadreza Safi; Seyed Saeed Nabavi; Richard J. Caron,A modified simplex partition algorithm to test copositivity,"Journal of Global Optimization 81, 645-658 (2021), Figure 4 and Tables 1-4; doi:10.1007/s10898-021-01092-1","Defines G8, G10, and G12 by drawings; nine additional random graphs have no seeds or edge lists."
ferreira_gao_nemeth_rigo_2024,Orlando P. Ferreira; Yun Gao; Sándor Z. Németh; Hugo M. Rigo,A gradient projection method for testing copositivity,"Journal paper/preprint (2024), Examples 11-13 and numerical repository Copositivity/Matrices commit 129acd4f","Prints M1, Horn, and Hoffman-Pereira; https://github.com/Copositivity/Matrices preserves 81 graph-derived matrices."
judice_sessa_fukushima_2024,Joaquim Júdice; Tiago Sessa; Masao Fukushima,A two-phase sequential algorithm for standard quadratic programming,"Journal of Global Optimization (2024), Sections 8.2-8.5","Uses QPLIB, Matrix Market, Nowak/BLST generators, Brás M1-M7, and DIMACS graphs."
brown_bernalneira_venturelli_pavone_2024,Nicholas C. Brown; Diego Bernal Neira; Davide Venturelli; Marco Pavone,A quantum-assisted algorithm for copositive optimization,2024 paper and companion repository commit 3fd69d72,https://github.com/StanfordASL/copositive-cutting-plane-max-clique contains generators and experiment code but not the 525 realized random graph matrices.
bomze_peng_qiu_yildirim_2025,Immanuel M. Bomze; Bo Peng; Yuzhou Qiu; E. Alper Yildirim,Tighter yet more tractable relaxations and nontrivial instance generation for sparse standard quadratic optimization,arXiv:2406.01239 (2024) and companion repository commit 624524b2,"Fixed seeds and code reproduce 450 unique matrices used in 1,350 parameterized experimental instances."
nishijima_poirion_takeda_2025,Satoshi Nishijima; Pierre-Louis Poirion; Akiko Takeda,An inexact subgradient method for copositive optimization,"2025 paper, numerical section",Reuses the ten Badenbroek-de Klerk 2019 matrices; its random order-5-through-5000 campaigns and four SDP-generated DNN matrices publish no seeds/solutions.
strekelj_zalar_2025,Tea Štrekelj; Aljaž Zalar,Construction of exceptional copositive matrices,"arXiv:2502.20133 (2025), Section 2.3; companion repository commit 71b485eb5f78a2081ef4cc27acdeb531acdec351",Prints one exact symbolic exceptional DNN matrix and one rational exceptional copositive matrix; the official notebook is retained from https://github.com/ZalarA/Exceptional-Copositive-Matrices.
zischg_2023,Johannes Zischg,Copositivity Testing: A novel decomposition procedure for arbitrary matrices and an investigation of gradient-based search algorithms for finding violating vectors,"Master's thesis, University of Vienna (2023)","Prints exact decomposition examples, evaluates 18,000 unseeded random matrices, and uses all 80 exact Second DIMACS Challenge graphs."
hou_tang_toh_2026,Di Hou; Tianyun Tang; Kim-Chuan Toh,A low-rank augmented Lagrangian method for polyhedral-SDP and moment-SOS relaxations of polynomial optimization,"Mathematical Programming (2026), Sections 5.1, 5.2, and 5.8; doi:10.1007/s10107-026-02395-5","Prints the exact extended-Horn construction. The random Gaussian, BPQY-derived, and matrix-copositivity realizations have no published seeds or files."
johnson_reams_2008,Charles R. Johnson; Robert Reams,Constructing copositive matrices from interior matrices,"Electronic Journal of Linear Algebra 17, 9-20 (2008), Section 4",Primary construction for the odd-order extended Horn family; Hou-Tang-Toh test the order-21 member.
dimacs_1996,David S. Johnson; Michael A. Trick (editors),"Cliques, Coloring, and Satisfiability",DIMACS Series in Discrete Mathematics and Theoretical Computer Science 26 (1996); https://archive.dimacs.rutgers.edu/pub/challenge/graph/benchmarks/clique/,Official Second DIMACS Challenge graph archive; original binary format and translator retained separately.
pena_vera_zuluaga_2007,Javier Peña; Juan Vera; Luis F. Zuluaga,Computing the stability number of a graph via linear and semidefinite programming,"SIAM Journal on Optimization 18, 87-105 (2007)",Primary source for the specially designed G17 graph; archive matrix retained under upstream/G17.
bomze_locatelli_tardella_2008,Immanuel M. Bomze; Marco Locatelli; Fabio Tardella,"New and old bounds for standard quadratic optimization: dominance, equivalence and incomparability","Mathematical Programming 115, 31-64 (2008); public BLST archive",Primary source/archive for 150 order-30 and 150 order-50 triangular-distribution StQP matrices.
scozzari_tardella_2008,Andrea Scozzari; Fabio Tardella,A clique algorithm for standard quadratic programming,"Discrete Applied Mathematics 156, 2439-2448 (2008); public files mirrored by the Liuzzi StQP archive",Primary source for the ST density-controlled instances later used by Gondzio-Yildirim.
vandenbussche_nemhauser_2005,David Vandenbussche; George L. Nemhauser,A branch-and-cut algorithm for nonconvex quadratic programs with box constraints,"Mathematical Programming 102(3), 559-575 (2005)",Primary source for the BoxQP instances; the twelve cases selected by Bomze-Jarre-Rendl are retained from a historical Minotaur repository commit.
bomze_jarre_rendl_2011,Immanuel M. Bomze; Florian Jarre; Franz Rendl,Quadratic factorization heuristics for copositive programming,"Mathematical Programming Computation 3(1), 37-57 (2011); doi:10.1007/s12532-011-0022-z","Uses ten unpublished random copositive-program realizations, twelve named BoxQP cases, and twenty-six named DIMACS graphs."
qplib_2019,Fabio Furini; Emiliano Traversi; Pietro Belotti; Antonio Frangioni; Ambros Gleixner; Nick Gould; Leo Liberti; Andrea Lodi; Ruth Misener; Hans Mittelmann; Nikolaos Sahinidis; Stefan Vigerske; Angelika Wiegele,QPLIB: a library of quadratic programming instances,"Mathematical Programming Computation 11(2), 237-265 (2019)","Primary archive source for QPLIB_0018, QPLIB_0343, and QPLIB_2712 used by Júdice et al."
matrix_market_1997,Ronald F. Boisvert; Roldan Pozo; Karin Remington; Richard Barrett; Jack Dongarra,Matrix Market: a web resource for test matrix collections,"The Quality of Numerical Software, 125-137 (1997); https://math.nist.gov/MatrixMarket/",Primary archive source for the six Harwell-Boeing/Matrix Market matrices used by Júdice et al.
nowak_1998,Ivo Nowak,Some heuristics and test problems for nonconvex quadratic programming over a simplex,"Preprint, 1998",Earliest located source for the random test-problem generators later used by Júdice et al.; no seeds for their realized matrices were published.
yang_li_2009,Shang-jun Yang; Xiao-xin Li,Algorithms for determining the copositivity of a given symmetric matrix,"Linear Algebra and its Applications 430(2-3), 609-618 (2009); doi:10.1016/j.laa.2008.07.028",Reports random unit-diagonal test campaigns at orders 8-10 without seeds or coefficient arrays.
yang_xu_li_2010,Shang-jun Yang; Chang-qing Xu; Xiao-xin Li,A note on algorithms for determining the copositivity of a given symmetric matrix,"Journal of Inequalities and Applications 2010, 498631; doi:10.1155/2010/498631",Tests the Johnson-Reams generalized Horn matrix and a new random unit-diagonal campaign at orders 8-10.
nowak_1999,Ivo Nowak,A new semidefinite programming bound for indefinite quadratic forms over a simplex,"Journal of Global Optimization 14(4), 357-364 (1999)","Primary generator source; exact seeds were not published, but six rounded realized matrices are printed by G.-Tóth et al. 2021."
gokmen_yildirim_2022,Y. Görkem Gökmen; E. Alper Yıldırım,On standard quadratic programs with exact and inexact doubly nonnegative relaxations,Mathematical Programming (2022); doi:10.1007/s10107-020-01611-0,Prints seven distinct exact finite StQP matrices (eight example occurrences) and an all-orders positive-gap construction using the Horn matrix.
muramatsu_waki_tuncel_2016,Masakazu Muramatsu; Hayato Waki; Levent Tunçel,Perturbed sums-of-squares theorem for polynomial optimization and its applications,"Optimization Methods and Software 31(1), 134-156 (2016); doi:10.1080/10556788.2015.1052969",Reports 180 random symmetric PSD-filtered matrices at orders 5-30; no seeds or coefficient arrays are published.
dobre_vera_2015,Cristian Dobre; Juan C. Vera,Exploiting symmetry in copositive programs via semidefinite hierarchies,"Mathematical Programming 151(2), 659-680 (2015); doi:10.1007/s10107-015-0879-0",Primary exact construction of the hard graph family G_k; samples through k=8 are retained for the later quadprogIP use.
xia_vera_zuluaga_2020,Wei Xia; Juan C. Vera; Luis F. Zuluaga,Globally solving nonconvex quadratic programs via linear integer programming techniques,"INFORMS Journal on Computing 32(1), 40-56 (2020); doi:10.1287/ijoc.2018.0883; companion repository commit 84839dbd",https://github.com/xiawei918/quadprogIP includes eight stable-set matrices and the complete QuadProgBB comparison archive.
chen_burer_2012,Jieqiu Chen; Samuel Burer,Globally solving nonconvex quadratic programming problems via completely positive programming,"Mathematical Programming Computation 4(1), 33-52 (2012); doi:10.1007/s12532-011-0033-9",Primary source of the 693 exact quadratic-program instance files redistributed by https://github.com/xiawei918/quadprogIP.
vargas_vera_dickinson_2025,Luis Felipe Vargas; Juan C. Vera; Peter J. C. Dickinson,Low degree sum-of-squares bounds for the stability number: a copositive approach,arXiv:2509.04949v2 (2025),"Introduces the exact hard graph family L_k, reuses Dobre-Vera G_k, and gives an exact star-graph obstruction family."
manainen_seliugin_tarasov_hildebrand_2024,Maxim Manainen; Mikhail Seliugin; Roman Tarasov; Roland Hildebrand,Generating extreme copositive matrices near matrices obtained from COP-irreducible graphs,"Linear Algebra and its Applications 693, 297-323 (2024); doi:10.1016/j.laa.2023.09.026; repository commit 2d8745e4",https://github.com/mamanain/copositive-extreme prints 28 COP-irreducible alpha-3 graphs; all associated extreme boundary matrices are retained.
ahmadi_dash_hua_stellato_2026,Amir Ali Ahmadi; Sanjeeb Dash; Yixuan Hua; Bartolomeo Stellato,Disjunctive sum of squares,arXiv:2605.28674v1 (2026); companion repository commit b4b7cb22,https://github.com/YH7422/DisjunctiveSOS preserves four exact StQP matrices and four exact order-75 random graph adjacency matrices.
de_zeeuw_2018,Reinier de Zeeuw,Which graphs produce irreducible copositive matrices?,"Bachelor project, University of Twente (2018); https://essay.utwente.nl/75600/","Reports a 25,124-graph catalog through order 13 and proves three exact graph operations; the claimed graph file is not attached."
hildebrand_2018_support_two,Roland Hildebrand,Extremal copositive matrices with minimal zero supports of cardinality two,"Electronic Journal of Linear Algebra 34, 28-34 (2018)","Proves that this whole extremal class is, up to positive diagonal scaling, exactly the Hoffman-Pereira {-1,0,1} class."
"""

PRIMARY_MATRIX_IDS = {
    "hall_newman_1963": "9162",
    "hoffman_pereira_1973": "9163,9757-9955",
    "kaplan_2001": "9158-9160",
    "valiaho_1989": "9161,9682-9710",
    "baston_1969": "9985-10056",
    "hildebrand_2012": "9961-9984",
    "iusem_seeger_2009": "5",
    "hiriart_urruty_seeger_2010": "7,731",
    "hildebrand_case34": "9657-9681",
    "afonin_hildebrand_dickinson_2021": "9711-9737",
    "dickinson_de_zeeuw_2021": "9957,10130-10131,10133-10160",
    "dickinson_2019": "10245-10247",
    "hildebrand_afonin_2024": "10248-10250",
    "laurent_vargas_2023": "10251-10257",
    "hildebrand_2017": "10258-10304",
    "dannenberg_schuermann_2023": "10305-10504",
    "kostyukova_tchemisova_2026": "9738-9756,9956,9958-9959",
    "zilinskas_2011_programming": "9218-9224",
    "bomze_schachinger_uchida_2012": "1,71",
    "zilinskas_duer_2011": "9610,9613,9616,9619,9628,9631,9634,9637,9647",
    "sponsel_bundfuss_duer_2012": "9165",
    "bomze_eichfelder_2013": "9157,9192-9194,9196",
    "bras_eichfelder_judice_2016": "9575,9578,9581,9584,9611,9614,9617,9620,9629,9632,9635,9638,9641,9648,9651",
    "anstreicher_2021": "9640",
    "judice_sessa_fukushima_2024": "9574,9577,9580,9583,9650",
    "strekelj_zalar_2025": "9960",
    "johnson_reams_2008": "10057-10129,10161-10244",
}


def expand(spec: str) -> list[int]:
    values: list[int] = []
    for part in spec.split(","):
        bounds = [int(value) for value in part.split("-")]
        values.extend(range(bounds[0], bounds[-1] + 1))
    return values


def main() -> None:
    source_rows = list(csv.DictReader(io.StringIO(SOURCES_CSV)))
    assert len(source_rows) == 78
    source_ids = {row["source_key"]: source_id for source_id, row in enumerate(source_rows, 1)}
    assignments = {
        matrix_id: source_ids[source_key]
        for source_key, spec in PRIMARY_MATRIX_IDS.items()
        for matrix_id in expand(spec)
    }
    assert len(assignments) == 901

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        columns = {row[1] for row in connection.execute("PRAGMA table_info(matrices)")}
        if "source_id" in columns or connection.execute(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='sources'"
        ).fetchone():
            raise SystemExit("literature-source migration is already applied")

        connection.execute("BEGIN IMMEDIATE")
        try:
            assert connection.execute("SELECT count(*) FROM matrices").fetchone()[0] == EXPECTED_MATRIX_COUNT
            existing = {row[0] for row in connection.execute("SELECT matrix_id FROM matrices")}
            assert assignments.keys() <= existing
            connection.execute("""
                CREATE TABLE sources (
                    source_id INTEGER PRIMARY KEY,
                    authors TEXT NOT NULL CHECK(length(authors) > 0),
                    title TEXT NOT NULL CHECK(length(title) > 0),
                    reference TEXT NOT NULL CHECK(length(reference) > 0),
                    comment TEXT
                ) STRICT
            """)
            connection.execute("ALTER TABLE matrices ADD COLUMN source_id INTEGER REFERENCES sources(source_id)")
            connection.executemany(
                "INSERT INTO sources(source_id, authors, title, reference, comment) VALUES (?, ?, ?, ?, ?)",
                [
                    (source_id, row["authors"], row["title"], row["reference"], row["comment"] or None)
                    for source_id, row in enumerate(source_rows, 1)
                ],
            )
            connection.executemany(
                "UPDATE matrices SET source_id=? WHERE matrix_id=?",
                [(source_id, matrix_id) for matrix_id, source_id in assignments.items()],
            )
            assert connection.execute("SELECT count(*) FROM sources").fetchone()[0] == 78
            assert connection.execute("SELECT count(*) FROM matrices WHERE source_id IS NOT NULL").fetchone()[0] == 901
            assert not list(connection.execute("PRAGMA foreign_key_check"))
            connection.commit()
        except Exception:
            connection.rollback()
            raise

    print("added 78 literature sources and linked 901 of 2442 current matrices")


if __name__ == "__main__":
    main()
