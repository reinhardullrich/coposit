cmake_minimum_required(VERSION 3.18)

project(Coposit LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(COPOSIT_BUILD_PYTHON "Build the Python extension modules" ON)

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

add_library(coposit_core INTERFACE)
target_include_directories(coposit_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FLINT_INCLUDE_DIR}
)
target_link_libraries(coposit_core INTERFACE ${FLINT_LIBRARY} ${MPFR_LIBRARY} ${GMP_LIBRARY})

add_executable(coposit model_main.cpp ../models/baselines/dutour_2018/solver.cpp)
target_link_libraries(coposit PRIVATE coposit_core)

add_executable(danninger_1990 model_main.cpp ../models/baselines/danninger_1990/solver.cpp)
target_link_libraries(danninger_1990 PRIVATE coposit_core)

add_executable(copomatrix_2011 model_main.cpp ../models/baselines/copomatrix_2011/solver.cpp)
target_link_libraries(copomatrix_2011 PRIVATE coposit_core)

add_executable(adaptive_dutour_danninger model_main.cpp ../models/adaptive_dutour_danninger/solver.cpp)
target_link_libraries(adaptive_dutour_danninger PRIVATE coposit_core)

add_executable(adaptive_dutour_copomatrix model_main.cpp ../models/adaptive_dutour_copomatrix/solver.cpp)
target_link_libraries(adaptive_dutour_copomatrix PRIVATE coposit_core)

add_executable(adaptive_sponsel_copomatrix model_main.cpp ../models/adaptive_sponsel_copomatrix/solver.cpp)
target_link_libraries(adaptive_sponsel_copomatrix PRIVATE coposit_core)

add_executable(adaptive_zischg_sponsel_copomatrix
    model_main.cpp
    ../models/legacy/adaptive_zischg_sponsel_copomatrix/solver.cpp)
target_link_libraries(adaptive_zischg_sponsel_copomatrix PRIVATE coposit_core)

add_executable(hadeler_1983 model_main.cpp ../models/baselines/hadeler_1983/solver.cpp)
target_link_libraries(hadeler_1983 PRIVATE coposit_core)

add_executable(dickinson_2019 model_main.cpp ../models/baselines/dickinson_2019/solver.cpp)
target_link_libraries(dickinson_2019 PRIVATE coposit_core)

add_executable(support_pruned_dickinson model_main.cpp ../models/support_pruned_dickinson/solver.cpp)
target_link_libraries(support_pruned_dickinson PRIVATE coposit_core)

add_executable(nullity_support_pruned_dickinson model_main.cpp ../models/nullity_support_pruned_dickinson/solver.cpp)
target_link_libraries(nullity_support_pruned_dickinson PRIVATE coposit_core)

add_executable(rhs_dickinson model_main.cpp ../models/rhs_dickinson/solver.cpp)
target_link_libraries(rhs_dickinson PRIVATE coposit_core)

add_executable(frank_wolfe_dickinson model_main.cpp ../models/frank_wolfe_dickinson/solver.cpp)
target_link_libraries(frank_wolfe_dickinson PRIVATE coposit_core)

add_executable(one_step_frank_wolfe_dickinson model_main.cpp ../models/one_step_frank_wolfe_dickinson/solver.cpp)
target_link_libraries(one_step_frank_wolfe_dickinson PRIVATE coposit_core)

add_executable(pairwise_frank_wolfe_dickinson model_main.cpp ../models/pairwise_frank_wolfe_dickinson/solver.cpp)
target_link_libraries(pairwise_frank_wolfe_dickinson PRIVATE coposit_core)

add_executable(support_polished_frank_wolfe_dickinson model_main.cpp ../models/support_polished_frank_wolfe_dickinson/solver.cpp)
target_link_libraries(support_polished_frank_wolfe_dickinson PRIVATE coposit_core)

add_executable(safi_2021 model_main.cpp ../models/baselines/safi_2021/solver.cpp)
target_link_libraries(safi_2021 PRIVATE coposit_core)

add_executable(bundfuss_2008 model_main.cpp ../models/baselines/bundfuss_2008/solver.cpp)
target_link_libraries(bundfuss_2008 PRIVATE coposit_core)

add_executable(sponsel_2012 model_main.cpp ../models/baselines/sponsel_2012/solver.cpp)
target_link_libraries(sponsel_2012 PRIVATE coposit_core)

add_executable(frank_wolfe_sponsel model_main.cpp ../models/frank_wolfe_sponsel/solver.cpp)
target_link_libraries(frank_wolfe_sponsel PRIVATE coposit_core)

add_executable(fracessa model_main.cpp ../models/fracessa/solver.cpp)
target_link_libraries(fracessa PRIVATE coposit_core)

add_executable(zischg_hadeler model_main.cpp ../models/zischg_hadeler/solver.cpp)
target_link_libraries(zischg_hadeler PRIVATE coposit_core)

add_executable(zischg_dickinson model_main.cpp ../models/zischg_dickinson/solver.cpp)
target_link_libraries(zischg_dickinson PRIVATE coposit_core)

add_executable(zischg_fracessa model_main.cpp ../models/zischg_fracessa/solver.cpp)
target_link_libraries(zischg_fracessa PRIVATE coposit_core)

if(COPOSIT_BUILD_PYTHON)
    set(COPOSIT_COMBINED_CLASSIFICATION_MODELS danninger_1990 hadeler_1983 dickinson_2019)

    function(add_coposit_python_module model_name)
        set(target_name coposit_${model_name}_core)
        set(model_directory ${model_name})
        if(ARGC GREATER 1)
            set(model_directory ${ARGV1})
        endif()
        pybind11_add_module(${target_name} MODULE python_module.cpp ../models/${model_directory}/solver.cpp)
        target_compile_definitions(${target_name} PRIVATE COPOSIT_ENABLE_TIMEOUTS=1 COPOSIT_PYTHON_MODULE=${target_name})
        if(model_name IN_LIST COPOSIT_COMBINED_CLASSIFICATION_MODELS)
            target_compile_definitions(${target_name} PRIVATE COPOSIT_HAS_COMBINED_CLASSIFICATION=1)
        endif()
        target_link_libraries(${target_name} PRIVATE coposit_core)
        install(TARGETS ${target_name} LIBRARY DESTINATION . RUNTIME DESTINATION .)
    endfunction()

    add_coposit_python_module(dutour_2018 baselines/dutour_2018)
    add_coposit_python_module(danninger_1990 baselines/danninger_1990)
    add_coposit_python_module(copomatrix_2011 baselines/copomatrix_2011)
    add_coposit_python_module(adaptive_dutour_danninger)
    add_coposit_python_module(adaptive_dutour_copomatrix)
    add_coposit_python_module(adaptive_sponsel_copomatrix)
    add_coposit_python_module(adaptive_zischg_sponsel_copomatrix legacy/adaptive_zischg_sponsel_copomatrix)
    add_coposit_python_module(hadeler_1983 baselines/hadeler_1983)
    add_coposit_python_module(dickinson_2019 baselines/dickinson_2019)
    add_coposit_python_module(support_pruned_dickinson)
    add_coposit_python_module(nullity_support_pruned_dickinson)
    add_coposit_python_module(rhs_dickinson)
    add_coposit_python_module(frank_wolfe_dickinson)
    add_coposit_python_module(one_step_frank_wolfe_dickinson)
    add_coposit_python_module(pairwise_frank_wolfe_dickinson)
    add_coposit_python_module(support_polished_frank_wolfe_dickinson)
    add_coposit_python_module(safi_2021 baselines/safi_2021)
    add_coposit_python_module(bundfuss_2008 baselines/bundfuss_2008)
    add_coposit_python_module(sponsel_2012 baselines/sponsel_2012)
    add_coposit_python_module(frank_wolfe_sponsel)
    add_coposit_python_module(fracessa)
    add_coposit_python_module(zischg_hadeler)
    add_coposit_python_module(zischg_dickinson)
    add_coposit_python_module(zischg_fracessa)
endif()

if(BUILD_TESTING)
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
        ../models/adaptive_dutour_danninger/test_solver.cpp
        ../models/adaptive_dutour_danninger/solver.cpp)
    target_link_libraries(test_adaptive_dutour_danninger PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AdaptiveDutourDanningerModelTests COMMAND test_adaptive_dutour_danninger)

    add_executable(test_adaptive_dutour_copomatrix
        ../models/adaptive_dutour_copomatrix/test_solver.cpp
        ../models/adaptive_dutour_copomatrix/solver.cpp)
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
        ../models/legacy/adaptive_zischg_sponsel_copomatrix/test_solver.cpp
        ../models/legacy/adaptive_zischg_sponsel_copomatrix/solver.cpp)
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

    add_executable(test_support_pruned_dickinson
        ../models/support_pruned_dickinson/test_solver.cpp
        ../models/support_pruned_dickinson/solver.cpp)
    target_link_libraries(test_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SupportPrunedDickinsonModelTests COMMAND test_support_pruned_dickinson)

    add_executable(test_nullity_support_pruned_dickinson
        ../models/nullity_support_pruned_dickinson/test_solver.cpp
        ../models/nullity_support_pruned_dickinson/solver.cpp)
    target_compile_definitions(test_nullity_support_pruned_dickinson
        PRIVATE COPOSIT_NULLITY_SUPPORT_PRUNED_DICKINSON_TESTING=1)
    target_link_libraries(test_nullity_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME NullitySupportPrunedDickinsonModelTests COMMAND test_nullity_support_pruned_dickinson)

    add_executable(test_rhs_dickinson ../models/rhs_dickinson/test_solver.cpp ../models/rhs_dickinson/solver.cpp)
    target_compile_definitions(test_rhs_dickinson PRIVATE COPOSIT_RHS_DICKINSON_TESTING=1)
    target_link_libraries(test_rhs_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME RhsDickinsonModelTests COMMAND test_rhs_dickinson)

    add_executable(test_frank_wolfe_dickinson ../models/frank_wolfe_dickinson/test_solver.cpp ../models/frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_frank_wolfe_dickinson PRIVATE COPOSIT_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeDickinsonModelTests COMMAND test_frank_wolfe_dickinson)

    add_executable(test_one_step_frank_wolfe_dickinson
        ../models/one_step_frank_wolfe_dickinson/test_solver.cpp
        ../models/one_step_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_one_step_frank_wolfe_dickinson
        PRIVATE COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_one_step_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME OneStepFrankWolfeDickinsonModelTests COMMAND test_one_step_frank_wolfe_dickinson)

    add_executable(test_pairwise_frank_wolfe_dickinson
        ../models/pairwise_frank_wolfe_dickinson/test_solver.cpp
        ../models/pairwise_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_pairwise_frank_wolfe_dickinson
        PRIVATE COPOSIT_PAIRWISE_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_pairwise_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME PairwiseFrankWolfeDickinsonModelTests COMMAND test_pairwise_frank_wolfe_dickinson)

    add_executable(test_support_polished_frank_wolfe_dickinson
        ../models/support_polished_frank_wolfe_dickinson/test_solver.cpp
        ../models/support_polished_frank_wolfe_dickinson/solver.cpp)
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
        ../models/frank_wolfe_sponsel/test_solver.cpp
        ../models/frank_wolfe_sponsel/solver.cpp)
    target_compile_definitions(test_frank_wolfe_sponsel PRIVATE COPOSIT_FRANK_WOLFE_SPONSEL_TESTING=1)
    target_link_libraries(test_frank_wolfe_sponsel PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeSponselModelTests COMMAND test_frank_wolfe_sponsel)

    add_executable(test_fracessa ../models/fracessa/test_solver.cpp ../models/fracessa/solver.cpp)
    target_link_libraries(test_fracessa PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FracessaModelTests COMMAND test_fracessa)

    add_executable(test_zischg_hadeler ../models/zischg_hadeler/test_solver.cpp ../models/zischg_hadeler/solver.cpp)
    target_compile_definitions(test_zischg_hadeler PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_hadeler PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgHadelerModelTests COMMAND test_zischg_hadeler)

    add_executable(test_zischg_dickinson ../models/zischg_dickinson/test_solver.cpp ../models/zischg_dickinson/solver.cpp)
    target_compile_definitions(test_zischg_dickinson PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgDickinsonModelTests COMMAND test_zischg_dickinson)

    add_executable(test_zischg_fracessa ../models/zischg_fracessa/test_solver.cpp ../models/zischg_fracessa/solver.cpp)
    target_compile_definitions(test_zischg_fracessa PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_fracessa PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgFracessaModelTests COMMAND test_zischg_fracessa)

    add_executable(test_parse_integer_matrix tests/test_parse_integer_matrix.cpp)
    target_link_libraries(test_parse_integer_matrix PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ParseIntegerMatrixTests COMMAND test_parse_integer_matrix)

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

    add_test(NAME CliSmokeTest COMMAND coposit ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliOrdinaryModeTest
        COMMAND coposit --mode copositive ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliOrdinaryModeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliStrictBoundaryTest
        COMMAND coposit --mode strictly_copositive ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliStrictBoundaryTest PROPERTIES PASS_REGULAR_EXPRESSION "^false")

    if(COPOSIT_BUILD_PYTHON)
        add_test(NAME PythonWrapperTests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/../python/tests -v)
        set_tests_properties(PythonWrapperTests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python")
    endif()
endif()
