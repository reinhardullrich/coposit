cmake_minimum_required(VERSION 3.18)

project(coposit LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(COPOSIT_STANDALONE_DEFAULT ON)
else()
    set(COPOSIT_STANDALONE_DEFAULT OFF)
endif()

option(COPOSIT_BUILD_APPS "Build the command-line applications" ${COPOSIT_STANDALONE_DEFAULT})
option(COPOSIT_BUILD_PYTHON "Build the Python extension modules" ${COPOSIT_STANDALONE_DEFAULT})
option(COPOSIT_BUILD_TESTS "Build the coposit test suite" ${COPOSIT_STANDALONE_DEFAULT})

if(COPOSIT_BUILD_TESTS)
    include(CTest)
endif()

if(COPOSIT_BUILD_APPS OR COPOSIT_BUILD_PYTHON OR COPOSIT_BUILD_TESTS)
    find_package(Threads REQUIRED)
endif()

if(COPOSIT_BUILD_PYTHON)
    if(NOT Python3_EXECUTABLE)
        find_program(Python3_EXECUTABLE NAMES python3 python REQUIRED)
    endif()
    find_package(Python3 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
    set(PYBIND11_FINDPYTHON ON CACHE BOOL "Use CMake FindPython for pybind11" FORCE)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v3.0.4
    )
    FetchContent_MakeAvailable(pybind11)
endif()

find_path(FLINT_INCLUDE_DIR NAMES flint/flint.h REQUIRED)
find_library(FLINT_LIBRARY NAMES flint REQUIRED)
find_library(MPFR_LIBRARY NAMES mpfr REQUIRED)
find_library(GMP_LIBRARY NAMES gmp REQUIRED)

if(COPOSIT_BUILD_PYTHON OR (COPOSIT_BUILD_TESTS AND BUILD_TESTING))
    FetchContent_Declare(cadical
        GIT_REPOSITORY https://github.com/arminbiere/cadical.git
        GIT_TAG rel-2.2.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(cadical)
    if(NOT cadical_POPULATED)
        cmake_policy(PUSH)
        if(POLICY CMP0169)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(cadical)
        cmake_policy(POP)
    endif()
    file(GLOB CADICAL_LIBRARY_SOURCES CONFIGURE_DEPENDS "${cadical_SOURCE_DIR}/src/*.cpp")
    list(FILTER CADICAL_LIBRARY_SOURCES EXCLUDE REGEX "/(cadical|mobical)\\.cpp$")
    add_library(coposit_cadical STATIC ${CADICAL_LIBRARY_SOURCES} ${cadical_SOURCE_DIR}/src/kitten.c)
    set_target_properties(coposit_cadical PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(coposit_cadical PRIVATE NBUILD)
    target_include_directories(coposit_cadical PUBLIC ${cadical_SOURCE_DIR}/src)

    set(CLINGO_BUILD_APPS OFF CACHE BOOL "Build clingo applications" FORCE)
    set(CLINGO_BUILD_EXAMPLES OFF CACHE BOOL "Build clingo examples" FORCE)
    set(CLINGO_BUILD_TESTS OFF CACHE BOOL "Build clingo tests" FORCE)
    set(CLINGO_BUILD_SHARED OFF CACHE BOOL "Build shared libclingo" FORCE)
    set(CLINGO_BUILD_STATIC ON CACHE BOOL "Build static libclingo" FORCE)
    set(CLINGO_BUILD_WITH_LUA OFF CACHE STRING "Build clingo Lua support" FORCE)
    set(CLINGO_BUILD_WITH_PYTHON OFF CACHE STRING "Build clingo Python support" FORCE)
    FetchContent_Declare(clingo
        GIT_REPOSITORY https://github.com/potassco/clingo.git
        GIT_TAG v5.8.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(clingo)
    foreach(clingo_target IN ITEMS libclingo libgringo libclasp libpotassco libreify)
        if(TARGET ${clingo_target})
            set_target_properties(${clingo_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endforeach()
endif()

add_library(coposit_core INTERFACE)
target_include_directories(coposit_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FLINT_INCLUDE_DIR}
)
target_link_libraries(coposit_core INTERFACE ${FLINT_LIBRARY} ${MPFR_LIBRARY} ${GMP_LIBRARY})
add_library(coposit::core ALIAS coposit_core)

add_library(coposit_safe STATIC ../models/dickinson_final/solver.cpp)
set_target_properties(coposit_safe PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(coposit_safe PUBLIC coposit_core)
add_library(coposit::safe ALIAS coposit_safe)

if(COPOSIT_BUILD_APPS)
    add_executable(coposit coposit_main.cpp)
    target_link_libraries(coposit PRIVATE Threads::Threads)

    add_executable(coposit_fast_solver model_main.cpp ../models/adaptive_sponsel_copomatrix/solver.cpp)
    set_target_properties(coposit_fast_solver PROPERTIES OUTPUT_NAME coposit-fast)
    target_compile_definitions(coposit_fast_solver PRIVATE COPOSIT_PUBLIC_FAST=1)
    target_link_libraries(coposit_fast_solver PRIVATE coposit_core)

    add_executable(coposit_safe_solver model_main.cpp)
    set_target_properties(coposit_safe_solver PROPERTIES OUTPUT_NAME coposit-safe)
    target_compile_definitions(coposit_safe_solver PRIVATE COPOSIT_PUBLIC_SAFE=1)
    target_link_libraries(coposit_safe_solver PRIVATE coposit_safe)

    add_dependencies(coposit coposit_fast_solver coposit_safe_solver)

    add_executable(coposit_analyze coposit_analyze_main.cpp)
    set_target_properties(coposit_analyze PROPERTIES OUTPUT_NAME coposit-analyze)
    target_link_libraries(coposit_analyze PRIVATE Threads::Threads)
endif()

set(COPOSIT_COMBINED_CLASSIFICATION_MODELS
    danninger_1990 hadeler_1983 dickinson_2019 dickinson_final dense_bitset_dickinson cbdd_zed_dickinson wide_certificate_cbdd_zed_dickinson
    wide_75_certificate_cbdd_zed_dickinson wide_90_certificate_cbdd_zed_dickinson wide_95_certificate_cbdd_zed_dickinson
    multithreaded_cbdd_zed_dickinson ceiling_pruned_dickinson sat_zed_dickinson clingo_sat_zed_dickinson fracessa_circular)
set(COPOSIT_COPOSITIVE_MODELS
    dutour_2018
    danninger_1990
    copomatrix_2011
    adaptive_sponsel_copomatrix
    hadeler_1983
    dickinson_2019
    dickinson_final
    dense_bitset_dickinson
    cbdd_zed_dickinson
    wide_certificate_cbdd_zed_dickinson
    wide_75_certificate_cbdd_zed_dickinson
    wide_90_certificate_cbdd_zed_dickinson
    wide_95_certificate_cbdd_zed_dickinson
    multithreaded_cbdd_zed_dickinson
    ceiling_pruned_dickinson
    sat_zed_dickinson
    clingo_sat_zed_dickinson
    safi_2021
    bundfuss_2008
    sponsel_2012
    fracessa_circular
)
set(COPOSIT_CXX_ANALYSIS_MODELS
    dutour_2018
    danninger_1990
    copomatrix_2011
    adaptive_sponsel_copomatrix
    hadeler_1983
    dickinson_final
    safi_2021
    bundfuss_2008
    sponsel_2012
)

set(COPOSIT_ANALYSIS_TARGETS)
function(add_coposit_model_interfaces model_name)
    set(model_directory experiments/${model_name})
    if(ARGC GREATER 1)
        set(model_directory ${ARGV1})
    endif()

    if(COPOSIT_BUILD_APPS AND model_name IN_LIST COPOSIT_CXX_ANALYSIS_MODELS)
        set(analysis_target coposit_analyze_${model_name})
        add_executable(${analysis_target} model_main.cpp ../models/${model_directory}/solver.cpp)
        set_target_properties(${analysis_target} PROPERTIES OUTPUT_NAME coposit-analyze-${model_name})
        target_compile_definitions(${analysis_target} PRIVATE COPOSIT_ANALYSIS_COMPANION=1)
        if(model_name IN_LIST COPOSIT_COMBINED_CLASSIFICATION_MODELS)
            target_compile_definitions(${analysis_target} PRIVATE COPOSIT_HAS_COMBINED_CLASSIFICATION=1)
        endif()
        if(model_name IN_LIST COPOSIT_COPOSITIVE_MODELS)
            target_compile_definitions(${analysis_target} PRIVATE COPOSIT_SUPPORTS_COPOSITIVE=1)
        endif()
        target_link_libraries(${analysis_target} PRIVATE coposit_core)
        list(APPEND COPOSIT_ANALYSIS_TARGETS ${analysis_target})
        set(COPOSIT_ANALYSIS_TARGETS ${COPOSIT_ANALYSIS_TARGETS} PARENT_SCOPE)
    endif()

    if(COPOSIT_BUILD_PYTHON)
        set(python_target coposit_${model_name}_core)
        pybind11_add_module(${python_target} MODULE python_module.cpp ../models/${model_directory}/solver.cpp)
        target_compile_definitions(${python_target} PRIVATE COPOSIT_ENABLE_TIMEOUTS=1 COPOSIT_PYTHON_MODULE=${python_target})
        if(model_name IN_LIST COPOSIT_COMBINED_CLASSIFICATION_MODELS)
            target_compile_definitions(${python_target} PRIVATE COPOSIT_HAS_COMBINED_CLASSIFICATION=1)
        endif()
        if(model_name IN_LIST COPOSIT_COPOSITIVE_MODELS)
            target_compile_definitions(${python_target} PRIVATE COPOSIT_SUPPORTS_COPOSITIVE=1)
        endif()
        target_link_libraries(${python_target} PRIVATE coposit_core Threads::Threads)
        if(model_name STREQUAL "sat_zed_dickinson")
            target_link_libraries(${python_target} PRIVATE coposit_cadical)
        elseif(model_name STREQUAL "clingo_sat_zed_dickinson")
            target_link_libraries(${python_target} PRIVATE libclingo)
        endif()
        install(TARGETS ${python_target} LIBRARY DESTINATION . RUNTIME DESTINATION .)
    endif()
endfunction()

add_coposit_model_interfaces(dutour_2018 baselines/dutour_2018)
add_coposit_model_interfaces(danninger_1990 baselines/danninger_1990)
add_coposit_model_interfaces(copomatrix_2011 baselines/copomatrix_2011)
add_coposit_model_interfaces(adaptive_dutour_danninger)
add_coposit_model_interfaces(adaptive_dutour_copomatrix)
add_coposit_model_interfaces(adaptive_sponsel_copomatrix adaptive_sponsel_copomatrix)
add_coposit_model_interfaces(adaptive_zischg_sponsel_copomatrix)
add_coposit_model_interfaces(hadeler_1983 baselines/hadeler_1983)
add_coposit_model_interfaces(dickinson_2019 baselines/dickinson_2019)
add_coposit_model_interfaces(dickinson_final dickinson_final)
add_coposit_model_interfaces(dense_bitset_dickinson)
add_coposit_model_interfaces(interval_recursive_dickinson)
add_coposit_model_interfaces(interval_bdd_dickinson)
add_coposit_model_interfaces(interval_zdd_dickinson)
add_coposit_model_interfaces(cardinality_bdd_dickinson)
add_coposit_model_interfaces(cardinality_zdd_dickinson)
add_coposit_model_interfaces(dickinson_zed)
add_coposit_model_interfaces(bdd_zed_dickinson)
add_coposit_model_interfaces(zdd_zed_dickinson)
add_coposit_model_interfaces(cbdd_zed_dickinson)
add_coposit_model_interfaces(wide_certificate_cbdd_zed_dickinson)
add_coposit_model_interfaces(wide_75_certificate_cbdd_zed_dickinson)
add_coposit_model_interfaces(wide_90_certificate_cbdd_zed_dickinson)
add_coposit_model_interfaces(wide_95_certificate_cbdd_zed_dickinson)
add_coposit_model_interfaces(multithreaded_cbdd_zed_dickinson)
add_coposit_model_interfaces(ceiling_pruned_dickinson)
add_coposit_model_interfaces(czdd_zed_dickinson)
add_coposit_model_interfaces(sat_zed_dickinson)
add_coposit_model_interfaces(clingo_sat_zed_dickinson)
add_coposit_model_interfaces(support_pruned_dickinson)
add_coposit_model_interfaces(nullity_support_pruned_dickinson)
add_coposit_model_interfaces(rhs_dickinson)
add_coposit_model_interfaces(frank_wolfe_dickinson)
add_coposit_model_interfaces(one_step_frank_wolfe_dickinson)
add_coposit_model_interfaces(pairwise_frank_wolfe_dickinson)
add_coposit_model_interfaces(support_polished_frank_wolfe_dickinson)
add_coposit_model_interfaces(safi_2021 baselines/safi_2021)
add_coposit_model_interfaces(bundfuss_2008 baselines/bundfuss_2008)
add_coposit_model_interfaces(sponsel_2012 baselines/sponsel_2012)
add_coposit_model_interfaces(frank_wolfe_sponsel)
add_coposit_model_interfaces(fracessa)
add_coposit_model_interfaces(fracessa_circular)
add_coposit_model_interfaces(zischg_hadeler)
add_coposit_model_interfaces(zischg_dickinson)
add_coposit_model_interfaces(zischg_fracessa)

if(COPOSIT_BUILD_APPS)
    add_dependencies(coposit_analyze ${COPOSIT_ANALYSIS_TARGETS})
endif()

if(COPOSIT_BUILD_TESTS AND BUILD_TESTING)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(test_dutour_2018 ../models/baselines/dutour_2018/test_solver.cpp ../models/baselines/dutour_2018/solver.cpp)
    target_link_libraries(test_dutour_2018 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Dutour2018ModelTests COMMAND test_dutour_2018)

    add_executable(test_danninger_1990 ../models/baselines/danninger_1990/test_solver.cpp ../models/baselines/danninger_1990/solver.cpp)
    target_link_libraries(test_danninger_1990 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Danninger1990ModelTests COMMAND test_danninger_1990)

    add_executable(test_copomatrix_2011 ../models/baselines/copomatrix_2011/test_solver.cpp ../models/baselines/copomatrix_2011/solver.cpp)
    target_link_libraries(test_copomatrix_2011 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Copomatrix2011ModelTests COMMAND test_copomatrix_2011)

    add_executable(test_adaptive_dutour_danninger
        ../models/experiments/adaptive_dutour_danninger/test_solver.cpp
        ../models/experiments/adaptive_dutour_danninger/solver.cpp)
    target_link_libraries(test_adaptive_dutour_danninger PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AdaptiveDutourDanningerModelTests COMMAND test_adaptive_dutour_danninger)

    add_executable(test_adaptive_dutour_copomatrix
        ../models/experiments/adaptive_dutour_copomatrix/test_solver.cpp
        ../models/experiments/adaptive_dutour_copomatrix/solver.cpp)
    target_compile_definitions(test_adaptive_dutour_copomatrix
        PRIVATE COPOSIT_ADAPTIVE_DUTOUR_COPOMATRIX_TESTING=1)
    target_link_libraries(test_adaptive_dutour_copomatrix PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AdaptiveDutourCopomatrixModelTests COMMAND test_adaptive_dutour_copomatrix)

    add_executable(test_adaptive_sponsel_copomatrix
        ../models/adaptive_sponsel_copomatrix/test_solver.cpp
        ../models/adaptive_sponsel_copomatrix/solver.cpp)
    target_compile_definitions(test_adaptive_sponsel_copomatrix
        PRIVATE COPOSIT_ADAPTIVE_SPONSEL_COPOMATRIX_TESTING=1)
    target_link_libraries(test_adaptive_sponsel_copomatrix PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AdaptiveSponselCopomatrixModelTests COMMAND test_adaptive_sponsel_copomatrix)

    add_executable(test_adaptive_zischg_sponsel_copomatrix
        ../models/experiments/adaptive_zischg_sponsel_copomatrix/test_solver.cpp
        ../models/experiments/adaptive_zischg_sponsel_copomatrix/solver.cpp)
    target_compile_definitions(test_adaptive_zischg_sponsel_copomatrix
        PRIVATE COPOSIT_ADAPTIVE_ZISCHG_SPONSEL_COPOMATRIX_TESTING=1)
    target_link_libraries(test_adaptive_zischg_sponsel_copomatrix PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AdaptiveZischgSponselCopomatrixModelTests COMMAND test_adaptive_zischg_sponsel_copomatrix)

    add_executable(test_hadeler_1983 ../models/baselines/hadeler_1983/test_solver.cpp ../models/baselines/hadeler_1983/solver.cpp)
    target_link_libraries(test_hadeler_1983 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Hadeler1983ModelTests COMMAND test_hadeler_1983)

    add_executable(test_dickinson_2019 ../models/baselines/dickinson_2019/test_solver.cpp ../models/baselines/dickinson_2019/solver.cpp)
    target_link_libraries(test_dickinson_2019 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Dickinson2019ModelTests COMMAND test_dickinson_2019)

    add_executable(test_dickinson_final ../models/dickinson_final/test_solver.cpp ../models/dickinson_final/solver.cpp)
    target_compile_definitions(test_dickinson_final PRIVATE COPOSIT_BASELINE_SOURCE_TRACE=1)
    target_link_libraries(test_dickinson_final PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME DickinsonFinalModelTests COMMAND test_dickinson_final)

    add_executable(test_dense_bitset_dickinson
        ../models/experiments/dense_bitset_dickinson/test_solver.cpp
        ../models/experiments/dense_bitset_dickinson/solver.cpp)
    target_compile_definitions(test_dense_bitset_dickinson
        PRIVATE COPOSIT_DENSE_BITSET_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_dense_bitset_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME DenseBitsetDickinsonModelTests COMMAND test_dense_bitset_dickinson)

    add_executable(test_interval_recursive_dickinson
        ../models/experiments/interval_recursive_dickinson/test_solver.cpp
        ../models/experiments/interval_recursive_dickinson/solver.cpp)
    target_compile_definitions(test_interval_recursive_dickinson PRIVATE COPOSIT_INTERVAL_RECURSIVE_DICKINSON_TESTING=1)
    target_link_libraries(test_interval_recursive_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME IntervalRecursiveDickinsonModelTests COMMAND test_interval_recursive_dickinson)

    add_executable(test_interval_bdd_dickinson
        ../models/experiments/interval_bdd_dickinson/test_solver.cpp
        ../models/experiments/interval_bdd_dickinson/solver.cpp)
    target_compile_definitions(test_interval_bdd_dickinson
        PRIVATE COPOSIT_INTERVAL_BDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_interval_bdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME IntervalBddDickinsonModelTests COMMAND test_interval_bdd_dickinson)

    add_executable(test_interval_zdd_dickinson
        ../models/experiments/interval_zdd_dickinson/test_solver.cpp
        ../models/experiments/interval_zdd_dickinson/solver.cpp)
    target_compile_definitions(test_interval_zdd_dickinson
        PRIVATE COPOSIT_INTERVAL_ZDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_interval_zdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME IntervalZddDickinsonModelTests COMMAND test_interval_zdd_dickinson)

    add_executable(test_cardinality_bdd_dickinson
        ../models/experiments/cardinality_bdd_dickinson/test_solver.cpp
        ../models/experiments/cardinality_bdd_dickinson/solver.cpp)
    target_compile_definitions(test_cardinality_bdd_dickinson
        PRIVATE COPOSIT_CARDINALITY_BDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cardinality_bdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CardinalityBddDickinsonModelTests COMMAND test_cardinality_bdd_dickinson)

    add_executable(test_cardinality_zdd_dickinson
        ../models/experiments/cardinality_zdd_dickinson/test_solver.cpp
        ../models/experiments/cardinality_zdd_dickinson/solver.cpp)
    target_compile_definitions(test_cardinality_zdd_dickinson
        PRIVATE COPOSIT_CARDINALITY_ZDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cardinality_zdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CardinalityZddDickinsonModelTests COMMAND test_cardinality_zdd_dickinson)

    add_executable(test_dickinson_zed
        ../models/experiments/dickinson_zed/test_solver.cpp
        ../models/experiments/dickinson_zed/solver.cpp)
    target_compile_definitions(test_dickinson_zed PRIVATE COPOSIT_DICKINSON_ZED_TESTING=1)
    target_link_libraries(test_dickinson_zed PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME DickinsonZedModelTests COMMAND test_dickinson_zed)

    add_executable(test_bdd_zed_dickinson
        ../models/experiments/bdd_zed_dickinson/test_solver.cpp
        ../models/experiments/bdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_bdd_zed_dickinson
        PRIVATE COPOSIT_BDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_bdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME BddZedDickinsonModelTests COMMAND test_bdd_zed_dickinson)

    add_executable(test_zdd_zed_dickinson
        ../models/experiments/zdd_zed_dickinson/test_solver.cpp
        ../models/experiments/zdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_zdd_zed_dickinson
        PRIVATE COPOSIT_ZDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_zdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZddZedDickinsonModelTests COMMAND test_zdd_zed_dickinson)

    add_executable(test_cbdd_zed_dickinson
        ../models/experiments/cbdd_zed_dickinson/test_solver.cpp
        ../models/experiments/cbdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_cbdd_zed_dickinson
        PRIVATE COPOSIT_CBDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cbdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CbddZedDickinsonModelTests COMMAND test_cbdd_zed_dickinson)

    add_executable(test_wide_certificate_cbdd_zed_dickinson
        ../models/experiments/wide_certificate_cbdd_zed_dickinson/test_solver.cpp
        ../models/experiments/wide_certificate_cbdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_wide_certificate_cbdd_zed_dickinson
        PRIVATE COPOSIT_CBDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_wide_certificate_cbdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME WideCertificateCbddZedDickinsonModelTests COMMAND test_wide_certificate_cbdd_zed_dickinson)

    foreach(percentage IN ITEMS 75 90 95)
        add_executable(test_wide_${percentage}_certificate_cbdd_zed_dickinson
            ../models/experiments/wide_${percentage}_certificate_cbdd_zed_dickinson/test_solver.cpp
            ../models/experiments/wide_${percentage}_certificate_cbdd_zed_dickinson/solver.cpp)
        target_compile_definitions(test_wide_${percentage}_certificate_cbdd_zed_dickinson
            PRIVATE COPOSIT_CBDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
        target_link_libraries(test_wide_${percentage}_certificate_cbdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
        add_test(NAME Wide${percentage}CertificateCbddZedDickinsonModelTests
            COMMAND test_wide_${percentage}_certificate_cbdd_zed_dickinson)
    endforeach()

    add_executable(test_multithreaded_cbdd_zed_dickinson
        ../models/experiments/multithreaded_cbdd_zed_dickinson/test_solver.cpp
        ../models/experiments/multithreaded_cbdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_multithreaded_cbdd_zed_dickinson
        PRIVATE COPOSIT_MULTITHREADED_CBDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_multithreaded_cbdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main Threads::Threads)
    add_test(NAME MultithreadedCbddZedDickinsonModelTests COMMAND test_multithreaded_cbdd_zed_dickinson)

    add_executable(test_ceiling_pruned_dickinson
        ../models/experiments/ceiling_pruned_dickinson/test_solver.cpp
        ../models/experiments/ceiling_pruned_dickinson/solver.cpp)
    target_compile_definitions(test_ceiling_pruned_dickinson
        PRIVATE COPOSIT_CEILING_PRUNED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_ceiling_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CeilingPrunedDickinsonModelTests COMMAND test_ceiling_pruned_dickinson)

    add_executable(test_czdd_zed_dickinson
        ../models/experiments/czdd_zed_dickinson/test_solver.cpp
        ../models/experiments/czdd_zed_dickinson/solver.cpp)
    target_compile_definitions(test_czdd_zed_dickinson
        PRIVATE COPOSIT_CZDD_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_czdd_zed_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CzddZedDickinsonModelTests COMMAND test_czdd_zed_dickinson)

    add_executable(test_sat_zed_dickinson
        ../models/experiments/sat_zed_dickinson/test_solver.cpp
        ../models/experiments/sat_zed_dickinson/solver.cpp)
    target_compile_definitions(test_sat_zed_dickinson
        PRIVATE COPOSIT_SAT_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_zed_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatZedDickinsonModelTests COMMAND test_sat_zed_dickinson)

    add_executable(test_clingo_sat_zed_dickinson
        ../models/experiments/clingo_sat_zed_dickinson/test_solver.cpp
        ../models/experiments/clingo_sat_zed_dickinson/solver.cpp)
    target_compile_definitions(test_clingo_sat_zed_dickinson
        PRIVATE COPOSIT_CLINGO_SAT_ZED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_clingo_sat_zed_dickinson PRIVATE coposit_core libclingo GTest::gtest_main)
    add_test(NAME ClingoSatZedDickinsonModelTests COMMAND test_clingo_sat_zed_dickinson)

    add_executable(test_support_pruned_dickinson
        ../models/experiments/support_pruned_dickinson/test_solver.cpp
        ../models/experiments/support_pruned_dickinson/solver.cpp)
    target_link_libraries(test_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SupportPrunedDickinsonModelTests COMMAND test_support_pruned_dickinson)

    add_executable(test_nullity_support_pruned_dickinson
        ../models/experiments/nullity_support_pruned_dickinson/test_solver.cpp
        ../models/experiments/nullity_support_pruned_dickinson/solver.cpp)
    target_compile_definitions(test_nullity_support_pruned_dickinson
        PRIVATE COPOSIT_NULLITY_SUPPORT_PRUNED_DICKINSON_TESTING=1)
    target_link_libraries(test_nullity_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME NullitySupportPrunedDickinsonModelTests COMMAND test_nullity_support_pruned_dickinson)

    add_executable(test_rhs_dickinson ../models/experiments/rhs_dickinson/test_solver.cpp ../models/experiments/rhs_dickinson/solver.cpp)
    target_compile_definitions(test_rhs_dickinson PRIVATE COPOSIT_RHS_DICKINSON_TESTING=1)
    target_link_libraries(test_rhs_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME RhsDickinsonModelTests COMMAND test_rhs_dickinson)

    add_executable(test_frank_wolfe_dickinson
        ../models/experiments/frank_wolfe_dickinson/test_solver.cpp
        ../models/experiments/frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_frank_wolfe_dickinson PRIVATE COPOSIT_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeDickinsonModelTests COMMAND test_frank_wolfe_dickinson)

    add_executable(test_one_step_frank_wolfe_dickinson
        ../models/experiments/one_step_frank_wolfe_dickinson/test_solver.cpp
        ../models/experiments/one_step_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_one_step_frank_wolfe_dickinson
        PRIVATE COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_one_step_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME OneStepFrankWolfeDickinsonModelTests COMMAND test_one_step_frank_wolfe_dickinson)

    add_executable(test_pairwise_frank_wolfe_dickinson
        ../models/experiments/pairwise_frank_wolfe_dickinson/test_solver.cpp
        ../models/experiments/pairwise_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_pairwise_frank_wolfe_dickinson
        PRIVATE COPOSIT_PAIRWISE_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_pairwise_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME PairwiseFrankWolfeDickinsonModelTests COMMAND test_pairwise_frank_wolfe_dickinson)

    add_executable(test_support_polished_frank_wolfe_dickinson
        ../models/experiments/support_polished_frank_wolfe_dickinson/test_solver.cpp
        ../models/experiments/support_polished_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_support_polished_frank_wolfe_dickinson
        PRIVATE COPOSIT_SUPPORT_POLISHED_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_support_polished_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SupportPolishedFrankWolfeDickinsonModelTests COMMAND test_support_polished_frank_wolfe_dickinson)

    add_executable(test_safi_2021 ../models/baselines/safi_2021/test_solver.cpp ../models/baselines/safi_2021/solver.cpp)
    target_link_libraries(test_safi_2021 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Safi2021ModelTests COMMAND test_safi_2021)

    add_executable(test_bundfuss_2008 ../models/baselines/bundfuss_2008/test_solver.cpp ../models/baselines/bundfuss_2008/solver.cpp)
    target_link_libraries(test_bundfuss_2008 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Bundfuss2008ModelTests COMMAND test_bundfuss_2008)

    add_executable(test_sponsel_2012 ../models/baselines/sponsel_2012/test_solver.cpp ../models/baselines/sponsel_2012/solver.cpp)
    target_link_libraries(test_sponsel_2012 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Sponsel2012ModelTests COMMAND test_sponsel_2012)

    foreach(target IN ITEMS
        test_dutour_2018
        test_danninger_1990
        test_copomatrix_2011
        test_hadeler_1983
        test_dickinson_2019
        test_safi_2021
        test_bundfuss_2008
        test_sponsel_2012)
        target_compile_definitions(${target} PRIVATE COPOSIT_BASELINE_SOURCE_TRACE=1)
    endforeach()

    add_executable(test_frank_wolfe_sponsel
        ../models/experiments/frank_wolfe_sponsel/test_solver.cpp
        ../models/experiments/frank_wolfe_sponsel/solver.cpp)
    target_compile_definitions(test_frank_wolfe_sponsel PRIVATE COPOSIT_FRANK_WOLFE_SPONSEL_TESTING=1)
    target_link_libraries(test_frank_wolfe_sponsel PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeSponselModelTests COMMAND test_frank_wolfe_sponsel)

    add_executable(test_fracessa ../models/experiments/fracessa/test_solver.cpp ../models/experiments/fracessa/solver.cpp)
    target_link_libraries(test_fracessa PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FracessaModelTests COMMAND test_fracessa)

    add_executable(test_fracessa_circular
        ../models/experiments/fracessa_circular/test_solver.cpp
        ../models/experiments/fracessa_circular/solver.cpp)
    target_link_libraries(test_fracessa_circular PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FracessaCircularModelTests COMMAND test_fracessa_circular)

    add_executable(test_zischg_hadeler ../models/experiments/zischg_hadeler/test_solver.cpp ../models/experiments/zischg_hadeler/solver.cpp)
    target_compile_definitions(test_zischg_hadeler PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_hadeler PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgHadelerModelTests COMMAND test_zischg_hadeler)

    add_executable(test_zischg_dickinson
        ../models/experiments/zischg_dickinson/test_solver.cpp
        ../models/experiments/zischg_dickinson/solver.cpp)
    target_compile_definitions(test_zischg_dickinson PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgDickinsonModelTests COMMAND test_zischg_dickinson)

    add_executable(test_zischg_fracessa ../models/experiments/zischg_fracessa/test_solver.cpp ../models/experiments/zischg_fracessa/solver.cpp)
    target_compile_definitions(test_zischg_fracessa PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_fracessa PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgFracessaModelTests COMMAND test_zischg_fracessa)

    add_executable(test_fracessa_matrix_parser tests/test_fracessa_matrix_parser.cpp)
    target_link_libraries(test_fracessa_matrix_parser PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FracessaMatrixParserTests COMMAND test_fracessa_matrix_parser)

    add_executable(test_integer tests/test_integer.cpp)
    target_link_libraries(test_integer PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME IntegerTests COMMAND test_integer)

    add_executable(test_exact_number_parser tests/test_exact_number_parser.cpp)
    target_link_libraries(test_exact_number_parser PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ExactNumberParserTests COMMAND test_exact_number_parser)

    add_executable(test_matrix_market_parser tests/test_matrix_market_parser.cpp)
    target_link_libraries(test_matrix_market_parser PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME MatrixMarketParserTests COMMAND test_matrix_market_parser)

    add_executable(test_matrix_parser tests/test_matrix_parser.cpp)
    target_link_libraries(test_matrix_parser PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME MatrixParserTests COMMAND test_matrix_parser)

    add_executable(test_support tests/test_support.cpp)
    target_link_libraries(test_support PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SupportTests COMMAND test_support)

    add_executable(test_fraction_free_ldlt tests/test_fraction_free_ldlt.cpp)
    target_link_libraries(test_fraction_free_ldlt PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FractionFreeLdltTests COMMAND test_fraction_free_ldlt)

    add_executable(test_small_copositivity tests/test_small_copositivity.cpp)
    target_link_libraries(test_small_copositivity PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SmallCopositivityTests COMMAND test_small_copositivity)

    add_executable(test_pre_check tests/test_pre_check.cpp)
    target_link_libraries(test_pre_check PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME PreCheckTests COMMAND test_pre_check)

    add_executable(test_connected_components tests/test_connected_components.cpp)
    target_link_libraries(test_connected_components PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ConnectedComponentsTests COMMAND test_connected_components)

    add_executable(test_component_pipeline tests/test_component_pipeline.cpp)
    target_link_libraries(test_component_pipeline PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ComponentPipelineTests COMMAND test_component_pipeline)

    add_executable(test_safe tests/test_safe.cpp)
    target_link_libraries(test_safe PRIVATE coposit::safe GTest::gtest_main)
    add_test(NAME SafeApiTests COMMAND test_safe)

    add_executable(test_progress tests/test_progress.cpp)
    target_link_libraries(test_progress PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ProgressTests COMMAND test_progress)

    if(COPOSIT_BUILD_APPS)
    add_executable(test_companion_launcher tests/test_companion_launcher.cpp)
    target_link_libraries(test_companion_launcher PRIVATE Threads::Threads)
    add_test(NAME CompanionLauncherTimeoutTest COMMAND test_companion_launcher ${CMAKE_COMMAND})

    add_test(NAME CliFastSmokeTest COMMAND coposit fast strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliFastSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliSafeSmokeTest COMMAND coposit safe strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliSafeSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliProgressSmokeTest COMMAND coposit safe strict --progress ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliProgressSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "true")

    add_test(NAME CliTimeoutOptionSmokeTest
        COMMAND coposit safe strict --timeout 2 ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliTimeoutOptionSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliMatrixMarketTest COMMAND coposit safe strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.mtx)
    set_tests_properties(CliMatrixMarketTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliDirectMatrixTest COMMAND coposit safe non-strict "2#1,-1,1")
    set_tests_properties(CliDirectMatrixTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliFastNonStrictModeTest
        COMMAND coposit fast non-strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliFastNonStrictModeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliSafeNonStrictModeTest
        COMMAND coposit safe non-strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliSafeNonStrictModeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliSafeCombinedModeTest
        COMMAND coposit safe both ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliSafeCombinedModeTest PROPERTIES
        PASS_REGULAR_EXPRESSION "copositive=true[\r\n]+strictly_copositive=false")

    add_test(NAME CliFastCombinedModeRejectedTest
        COMMAND coposit fast both ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliFastCombinedModeRejectedTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliAnalyzePreprocessingOffTest COMMAND coposit_analyze
        --model bundfuss_2008 --mode strict --preprocessing off
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliAnalyzePreprocessingOffTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliAnalyzeDuTourTest COMMAND coposit_analyze
        --model dutour_2018 --mode non-strict --preprocessing off
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliAnalyzeDuTourTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliAnalyzeTimeoutOptionSmokeTest COMMAND coposit_analyze
        --model dickinson_final --mode strict --timeout 2 ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliAnalyzeTimeoutOptionSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliAnalyzeCombinedTest COMMAND coposit_analyze
        --model dickinson_final --mode both ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliAnalyzeCombinedTest PROPERTIES
        PASS_REGULAR_EXPRESSION "copositive=true[\r\n]+strictly_copositive=false")

    add_test(NAME CliAnalyzeUnknownModelTest COMMAND coposit_analyze --model unknown --mode strict "1#1")
    set_tests_properties(CliAnalyzeUnknownModelTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliAnalyzeExcludedExperimentTest COMMAND coposit_analyze --model frank_wolfe_dickinson --mode strict "1#1")
    set_tests_properties(CliAnalyzeExcludedExperimentTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliAnalyzeUnsupportedCombinedModeTest COMMAND coposit_analyze --model bundfuss_2008 --mode both "1#1")
    set_tests_properties(CliAnalyzeUnsupportedCombinedModeTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliFastStrictBoundaryTest
        COMMAND coposit fast strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliFastStrictBoundaryTest PROPERTIES PASS_REGULAR_EXPRESSION "^false")

    add_test(NAME CliSafeStrictBoundaryTest
        COMMAND coposit safe strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliSafeStrictBoundaryTest PROPERTIES PASS_REGULAR_EXPRESSION "^false")

    add_test(NAME CliFastIdentityTest COMMAND coposit_fast_solver --help)
    set_tests_properties(CliFastIdentityTest PROPERTIES PASS_REGULAR_EXPRESSION "Adaptive Sponsel-COPOMATRIX")

    add_test(NAME CliSafeIdentityTest COMMAND coposit_safe_solver --help)
    set_tests_properties(CliSafeIdentityTest PROPERTIES PASS_REGULAR_EXPRESSION "Dickinson Final")

    add_test(NAME CliMissingMethodTest COMMAND coposit)
    set_tests_properties(CliMissingMethodTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliUnknownMethodTest COMMAND coposit unknown)
    set_tests_properties(CliUnknownMethodTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliMissingPredicateTest COMMAND coposit fast)
    set_tests_properties(CliMissingPredicateTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliUnknownPredicateTest COMMAND coposit fast copositive)
    set_tests_properties(CliUnknownPredicateTest PROPERTIES WILL_FAIL TRUE)
    endif()

    if(COPOSIT_BUILD_PYTHON)
        add_test(NAME PythonWrapperTests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/../python/tests -v)
        set_tests_properties(PythonWrapperTests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python")
    endif()
endif()
