cmake_minimum_required(VERSION 3.18)

project(Coposit LANGUAGES CXX)

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
option(COPOSIT_BUILD_TESTS "Build the Coposit test suite" ${COPOSIT_STANDALONE_DEFAULT})

if(COPOSIT_BUILD_TESTS)
    include(CTest)
endif()

if(COPOSIT_BUILD_APPS)
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

add_library(coposit_core INTERFACE)
target_include_directories(coposit_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FLINT_INCLUDE_DIR}
)
target_link_libraries(coposit_core INTERFACE ${FLINT_LIBRARY} ${MPFR_LIBRARY} ${GMP_LIBRARY})
add_library(Coposit::core ALIAS coposit_core)

add_library(coposit_safe STATIC ../models/dickinson_final/solver.cpp)
set_target_properties(coposit_safe PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(coposit_safe PUBLIC coposit_core)
add_library(Coposit::safe ALIAS coposit_safe)

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

set(COPOSIT_COMBINED_CLASSIFICATION_MODELS danninger_1990 hadeler_1983 dickinson_2019 dickinson_final)
set(COPOSIT_COPOSITIVE_MODELS
    dutour_2018
    danninger_1990
    copomatrix_2011
    adaptive_sponsel_copomatrix
    hadeler_1983
    dickinson_2019
    dickinson_final
    safi_2021
    bundfuss_2008
    sponsel_2012
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
        target_link_libraries(${python_target} PRIVATE coposit_core)
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
    target_link_libraries(test_safe PRIVATE Coposit::safe GTest::gtest_main)
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
    set_tests_properties(CliProgressSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

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

    add_test(NAME CliAnalyzeFineTunedTest COMMAND coposit_analyze
        --model bundfuss_2008 --mode strict
        --connected-components off --pre-checks on
        --pre-check frank-wolfe off --pre-check positive-definiteness off
        --principal-submatrices-up-to 2
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliAnalyzeFineTunedTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliAnalyzeDuTourTest COMMAND coposit_analyze
        --model dutour_2018 --mode non-strict --connected-components off --pre-checks off
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
