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
option(COPOSIT_BUILD_PYTHON "Install and test the Python coposit adapter" ${COPOSIT_STANDALONE_DEFAULT})
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
    find_package(Python3 3.11 COMPONENTS Interpreter REQUIRED)
    if(NOT COPOSIT_BUILD_APPS)
        message(FATAL_ERROR "COPOSIT_BUILD_PYTHON requires COPOSIT_BUILD_APPS because Python executes coposit")
    endif()
endif()

find_path(FLINT_INCLUDE_DIR NAMES flint/flint.h REQUIRED)
find_library(FLINT_LIBRARY NAMES flint REQUIRED)
find_library(MPFR_LIBRARY NAMES mpfr REQUIRED)
find_library(GMP_LIBRARY NAMES gmp REQUIRED)

if(COPOSIT_BUILD_APPS OR COPOSIT_BUILD_PYTHON OR (COPOSIT_BUILD_TESTS AND BUILD_TESTING))
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

if(COPOSIT_BUILD_APPS)
    add_executable(coposit coposit_main.cpp)
    target_link_libraries(coposit PRIVATE Threads::Threads)
endif()

set(COPOSIT_COMBINED_CLASSIFICATION_MODELS
    danninger_1990 hadeler_1983 dickinson_2019 dense_bitset_dickinson interval_recursive_dickinson bdd_dickinson zdd_dickinson
    cbdd_dickinson cbdd_halfspace_dickinson upper_endpoint_cbdd_dickinson cbdd_dickinson_improved_1
    wide_certificate_cbdd_dickinson
    multithreaded_cbdd_dickinson ceiling_pruned_dickinson kernel_cone_dickinson affine_companion_dickinson
    layered_singular_lift_dickinson
    breadth_first_singular_lift_dickinson
    czdd_dickinson sat_dickinson sat_halfspace_dickinson sat_halfspace_rays_dickinson sat_halfspace_lp_dickinson
    sat_halfspace_milp_dickinson sat_halfspace_rays_lookahead_dickinson
    sat_halfspace_rays_wide_dickinson
    wide_certificate_sat_dickinson
    xxx xxx_two
    clingo_dickinson clingo_halfspace_dickinson
    support_pruned_dickinson nullity_support_pruned_dickinson rhs_dickinson frank_wolfe_dickinson
    one_step_frank_wolfe_dickinson pairwise_frank_wolfe_dickinson support_polished_frank_wolfe_dickinson
    fracessa zischg_hadeler zischg_dickinson zischg_fracessa)
set(COPOSIT_COPOSITIVE_MODELS
    dutour_2018
    danninger_1990
    copomatrix_2011
    adaptive_dutour_danninger
    adaptive_dutour_copomatrix
    adaptive_sponsel_copomatrix
    adaptive_zischg_sponsel_copomatrix
    hadeler_1983
    dickinson_2019
    dense_bitset_dickinson
    interval_recursive_dickinson
    bdd_dickinson
    zdd_dickinson
    cbdd_dickinson
    cbdd_halfspace_dickinson
    upper_endpoint_cbdd_dickinson
    cbdd_dickinson_improved_1
    wide_certificate_cbdd_dickinson
    multithreaded_cbdd_dickinson
    ceiling_pruned_dickinson
    kernel_cone_dickinson
    affine_companion_dickinson
    layered_singular_lift_dickinson
    breadth_first_singular_lift_dickinson
    czdd_dickinson
    sat_dickinson
    sat_halfspace_dickinson
    sat_halfspace_rays_dickinson
    sat_halfspace_lp_dickinson
    sat_halfspace_milp_dickinson
    sat_halfspace_rays_lookahead_dickinson
    sat_halfspace_rays_wide_dickinson
    wide_certificate_sat_dickinson
    xxx
    xxx_two
    clingo_dickinson
    clingo_halfspace_dickinson
    support_pruned_dickinson
    nullity_support_pruned_dickinson
    rhs_dickinson
    frank_wolfe_dickinson
    one_step_frank_wolfe_dickinson
    pairwise_frank_wolfe_dickinson
    support_polished_frank_wolfe_dickinson
    safi_2021
    bundfuss_2008
    sponsel_2012
    frank_wolfe_sponsel
    fracessa
    zischg_hadeler
    zischg_dickinson
    zischg_fracessa
)
set(COPOSIT_MODEL_TARGETS)
function(add_coposit_model_interfaces model_name)
    set(model_directory experiments/${model_name})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../models/hadeler-based/${model_name}/solver.cpp")
        set(model_directory hadeler-based/${model_name})
    endif()
    if(ARGC GREATER 1)
        set(model_directory ${ARGV1})
    endif()

    if(COPOSIT_BUILD_APPS)
        set(model_target coposit_model_${model_name})
        add_executable(${model_target} model_main.cpp ../models/${model_directory}/solver.cpp)
        set_target_properties(${model_target} PROPERTIES OUTPUT_NAME coposit-${model_name})
        if(model_name IN_LIST COPOSIT_COMBINED_CLASSIFICATION_MODELS)
            target_compile_definitions(${model_target} PRIVATE COPOSIT_HAS_COMBINED_CLASSIFICATION=1)
        endif()
        target_compile_definitions(${model_target} PRIVATE COPOSIT_ENABLE_TIMEOUTS=1)
        if(model_name IN_LIST COPOSIT_COPOSITIVE_MODELS)
            target_compile_definitions(${model_target} PRIVATE COPOSIT_SUPPORTS_COPOSITIVE=1)
        endif()
        if(model_name STREQUAL "wide_certificate_cbdd_dickinson" OR model_name STREQUAL "wide_certificate_sat_dickinson"
           OR model_name STREQUAL "sat_halfspace_rays_wide_dickinson")
            target_compile_definitions(${model_target} PRIVATE COPOSIT_HAS_MODEL_PARAMETER=1)
        endif()
        target_link_libraries(${model_target} PRIVATE coposit_core Threads::Threads)
        if(model_name STREQUAL "sat_dickinson" OR model_name STREQUAL "sat_halfspace_dickinson"
           OR model_name STREQUAL "sat_halfspace_rays_dickinson"
           OR model_name STREQUAL "sat_halfspace_lp_dickinson"
           OR model_name STREQUAL "sat_halfspace_milp_dickinson"
           OR model_name STREQUAL "sat_halfspace_rays_lookahead_dickinson"
           OR model_name STREQUAL "sat_halfspace_rays_wide_dickinson"
           OR model_name STREQUAL "wide_certificate_sat_dickinson"
           OR model_name STREQUAL "xxx"
           OR model_name STREQUAL "xxx_two")
            target_link_libraries(${model_target} PRIVATE coposit_cadical)
        elseif(model_name STREQUAL "clingo_dickinson" OR model_name STREQUAL "clingo_halfspace_dickinson")
            target_link_libraries(${model_target} PRIVATE libclingo)
        endif()
        list(APPEND COPOSIT_MODEL_TARGETS ${model_target})
        set(COPOSIT_MODEL_TARGETS ${COPOSIT_MODEL_TARGETS} PARENT_SCOPE)
    endif()

endfunction()

add_coposit_model_interfaces(dutour_2018 baselines/dutour_2018)
add_coposit_model_interfaces(danninger_1990 baselines/danninger_1990)
add_coposit_model_interfaces(copomatrix_2011 baselines/copomatrix_2011)
add_coposit_model_interfaces(adaptive_dutour_danninger)
add_coposit_model_interfaces(adaptive_dutour_copomatrix)
add_coposit_model_interfaces(adaptive_sponsel_copomatrix)
add_coposit_model_interfaces(adaptive_zischg_sponsel_copomatrix)
add_coposit_model_interfaces(hadeler_1983)
add_coposit_model_interfaces(dickinson_2019)
add_coposit_model_interfaces(dense_bitset_dickinson)
add_coposit_model_interfaces(interval_recursive_dickinson)
add_coposit_model_interfaces(bdd_dickinson)
add_coposit_model_interfaces(zdd_dickinson)
add_coposit_model_interfaces(cbdd_dickinson)
add_coposit_model_interfaces(cbdd_halfspace_dickinson)
add_coposit_model_interfaces(upper_endpoint_cbdd_dickinson)
add_coposit_model_interfaces(cbdd_dickinson_improved_1)
add_coposit_model_interfaces(wide_certificate_cbdd_dickinson)
add_coposit_model_interfaces(multithreaded_cbdd_dickinson)
add_coposit_model_interfaces(ceiling_pruned_dickinson)
add_coposit_model_interfaces(kernel_cone_dickinson)
add_coposit_model_interfaces(affine_companion_dickinson)
add_coposit_model_interfaces(layered_singular_lift_dickinson)
add_coposit_model_interfaces(breadth_first_singular_lift_dickinson)
add_coposit_model_interfaces(czdd_dickinson)
add_coposit_model_interfaces(sat_dickinson)
add_coposit_model_interfaces(sat_halfspace_dickinson)
add_coposit_model_interfaces(sat_halfspace_rays_dickinson)
add_coposit_model_interfaces(sat_halfspace_lp_dickinson)
add_coposit_model_interfaces(sat_halfspace_milp_dickinson)
add_coposit_model_interfaces(sat_halfspace_rays_lookahead_dickinson)
add_coposit_model_interfaces(sat_halfspace_rays_wide_dickinson)
add_coposit_model_interfaces(wide_certificate_sat_dickinson)
add_coposit_model_interfaces(xxx)
add_coposit_model_interfaces(xxx_two)
add_coposit_model_interfaces(clingo_dickinson)
add_coposit_model_interfaces(clingo_halfspace_dickinson)
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
    file(GLOB COPOSIT_MODEL_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../models/baselines/*/solver.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../models/experiments/*/solver.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../models/hadeler-based/*/solver.cpp")
    foreach(model_source IN LISTS COPOSIT_MODEL_SOURCES)
        get_filename_component(model_directory "${model_source}" DIRECTORY)
        get_filename_component(discovered_model "${model_directory}" NAME)
        if(NOT TARGET coposit_model_${discovered_model})
            message(FATAL_ERROR "Model ${discovered_model} has solver.cpp but is not registered with coposit")
        endif()
    endforeach()
    add_dependencies(coposit ${COPOSIT_MODEL_TARGETS})
endif()
if(COPOSIT_BUILD_PYTHON)
    install(TARGETS coposit ${COPOSIT_MODEL_TARGETS} RUNTIME DESTINATION pycoposit)
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
        ../models/experiments/adaptive_sponsel_copomatrix/test_solver.cpp
        ../models/experiments/adaptive_sponsel_copomatrix/solver.cpp)
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

    add_executable(test_hadeler_1983 ../models/hadeler-based/hadeler_1983/test_solver.cpp ../models/hadeler-based/hadeler_1983/solver.cpp)
    target_link_libraries(test_hadeler_1983 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Hadeler1983ModelTests COMMAND test_hadeler_1983)

    add_executable(test_dickinson_2019 ../models/hadeler-based/dickinson_2019/test_solver.cpp ../models/hadeler-based/dickinson_2019/solver.cpp)
    target_link_libraries(test_dickinson_2019 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME Dickinson2019ModelTests COMMAND test_dickinson_2019)

    add_executable(test_dense_bitset_dickinson
        ../models/hadeler-based/dense_bitset_dickinson/test_solver.cpp
        ../models/hadeler-based/dense_bitset_dickinson/solver.cpp)
    target_compile_definitions(test_dense_bitset_dickinson
        PRIVATE COPOSIT_DENSE_BITSET_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_dense_bitset_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME DenseBitsetDickinsonModelTests COMMAND test_dense_bitset_dickinson)

    add_executable(test_interval_recursive_dickinson
        ../models/hadeler-based/interval_recursive_dickinson/test_solver.cpp
        ../models/hadeler-based/interval_recursive_dickinson/solver.cpp)
    target_compile_definitions(test_interval_recursive_dickinson PRIVATE COPOSIT_INTERVAL_RECURSIVE_DICKINSON_TESTING=1)
    target_link_libraries(test_interval_recursive_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME IntervalRecursiveDickinsonModelTests COMMAND test_interval_recursive_dickinson)

    add_executable(test_bdd_dickinson
        ../models/hadeler-based/bdd_dickinson/test_solver.cpp
        ../models/hadeler-based/bdd_dickinson/solver.cpp)
    target_compile_definitions(test_bdd_dickinson
        PRIVATE COPOSIT_BDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_bdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME BddDickinsonModelTests COMMAND test_bdd_dickinson)

    add_executable(test_zdd_dickinson
        ../models/hadeler-based/zdd_dickinson/test_solver.cpp
        ../models/hadeler-based/zdd_dickinson/solver.cpp)
    target_compile_definitions(test_zdd_dickinson
        PRIVATE COPOSIT_ZDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_zdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZddDickinsonModelTests COMMAND test_zdd_dickinson)

    add_executable(test_cbdd_dickinson
        ../models/hadeler-based/cbdd_dickinson/test_solver.cpp
        ../models/hadeler-based/cbdd_dickinson/solver.cpp)
    target_compile_definitions(test_cbdd_dickinson
        PRIVATE COPOSIT_CBDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cbdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CbddDickinsonModelTests COMMAND test_cbdd_dickinson)

    add_executable(test_cbdd_halfspace_dickinson
        ../models/hadeler-based/cbdd_halfspace_dickinson/test_solver.cpp
        ../models/hadeler-based/cbdd_halfspace_dickinson/solver.cpp)
    target_compile_definitions(test_cbdd_halfspace_dickinson
        PRIVATE COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cbdd_halfspace_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CbddHalfspaceDickinsonModelTests COMMAND test_cbdd_halfspace_dickinson)

    add_executable(test_upper_endpoint_cbdd_dickinson
        ../models/hadeler-based/upper_endpoint_cbdd_dickinson/test_solver.cpp
        ../models/hadeler-based/upper_endpoint_cbdd_dickinson/solver.cpp)
    target_compile_definitions(test_upper_endpoint_cbdd_dickinson
        PRIVATE COPOSIT_UPPER_ENDPOINT_CBDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_upper_endpoint_cbdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME UpperEndpointCbddDickinsonModelTests COMMAND test_upper_endpoint_cbdd_dickinson)

    add_executable(test_cbdd_dickinson_improved_1
        ../models/hadeler-based/cbdd_dickinson_improved_1/test_solver.cpp
        ../models/hadeler-based/cbdd_dickinson_improved_1/solver.cpp)
    target_compile_definitions(test_cbdd_dickinson_improved_1
        PRIVATE COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_cbdd_dickinson_improved_1 PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CbddDickinsonImproved1ModelTests COMMAND test_cbdd_dickinson_improved_1)

    add_executable(test_wide_certificate_cbdd_dickinson
        ../models/hadeler-based/wide_certificate_cbdd_dickinson/test_solver.cpp
        ../models/hadeler-based/wide_certificate_cbdd_dickinson/solver.cpp)
    target_compile_definitions(test_wide_certificate_cbdd_dickinson
        PRIVATE COPOSIT_CBDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_wide_certificate_cbdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME WideCertificateCbddDickinsonModelTests COMMAND test_wide_certificate_cbdd_dickinson)

    add_executable(test_multithreaded_cbdd_dickinson
        ../models/hadeler-based/multithreaded_cbdd_dickinson/test_solver.cpp
        ../models/hadeler-based/multithreaded_cbdd_dickinson/solver.cpp)
    target_compile_definitions(test_multithreaded_cbdd_dickinson
        PRIVATE COPOSIT_MULTITHREADED_CBDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_multithreaded_cbdd_dickinson PRIVATE coposit_core GTest::gtest_main Threads::Threads)
    add_test(NAME MultithreadedCbddDickinsonModelTests COMMAND test_multithreaded_cbdd_dickinson)

    add_executable(test_ceiling_pruned_dickinson
        ../models/hadeler-based/ceiling_pruned_dickinson/test_solver.cpp
        ../models/hadeler-based/ceiling_pruned_dickinson/solver.cpp)
    target_compile_definitions(test_ceiling_pruned_dickinson
        PRIVATE COPOSIT_CEILING_PRUNED_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_ceiling_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CeilingPrunedDickinsonModelTests COMMAND test_ceiling_pruned_dickinson)

    add_executable(test_kernel_cone_dickinson
        ../models/hadeler-based/kernel_cone_dickinson/test_solver.cpp
        ../models/hadeler-based/kernel_cone_dickinson/solver.cpp)
    target_compile_definitions(test_kernel_cone_dickinson
        PRIVATE COPOSIT_KERNEL_CONE_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_kernel_cone_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME KernelConeDickinsonModelTests COMMAND test_kernel_cone_dickinson)

    add_executable(test_affine_companion_dickinson
        ../models/hadeler-based/affine_companion_dickinson/test_solver.cpp
        ../models/hadeler-based/affine_companion_dickinson/solver.cpp)
    target_compile_definitions(test_affine_companion_dickinson
        PRIVATE COPOSIT_AFFINE_COMPANION_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_affine_companion_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME AffineCompanionDickinsonModelTests COMMAND test_affine_companion_dickinson)

    add_executable(test_layered_singular_lift_dickinson
        ../models/hadeler-based/layered_singular_lift_dickinson/test_solver.cpp
        ../models/hadeler-based/layered_singular_lift_dickinson/solver.cpp)
    target_compile_definitions(test_layered_singular_lift_dickinson
        PRIVATE COPOSIT_LAYERED_SINGULAR_LIFT_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_layered_singular_lift_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME LayeredSingularLiftDickinsonModelTests COMMAND test_layered_singular_lift_dickinson)

    add_executable(test_breadth_first_singular_lift_dickinson
        ../models/hadeler-based/breadth_first_singular_lift_dickinson/test_solver.cpp
        ../models/hadeler-based/breadth_first_singular_lift_dickinson/solver.cpp)
    target_compile_definitions(test_breadth_first_singular_lift_dickinson
        PRIVATE COPOSIT_BREADTH_FIRST_SINGULAR_LIFT_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_breadth_first_singular_lift_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME BreadthFirstSingularLiftDickinsonModelTests COMMAND test_breadth_first_singular_lift_dickinson)

    add_executable(test_czdd_dickinson
        ../models/hadeler-based/czdd_dickinson/test_solver.cpp
        ../models/hadeler-based/czdd_dickinson/solver.cpp)
    target_compile_definitions(test_czdd_dickinson
        PRIVATE COPOSIT_CZDD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_czdd_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME CzddDickinsonModelTests COMMAND test_czdd_dickinson)

    add_executable(test_sat_dickinson
        ../models/hadeler-based/sat_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_dickinson/solver.cpp)
    target_compile_definitions(test_sat_dickinson
        PRIVATE COPOSIT_SAT_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatDickinsonModelTests COMMAND test_sat_dickinson)

    add_executable(test_sat_halfspace_dickinson
        ../models/hadeler-based/sat_halfspace_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceDickinsonModelTests COMMAND test_sat_halfspace_dickinson)

    add_executable(test_sat_halfspace_rays_dickinson
        ../models/hadeler-based/sat_halfspace_rays_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_rays_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_rays_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_RAYS_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_rays_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceRaysDickinsonModelTests COMMAND test_sat_halfspace_rays_dickinson)

    add_executable(test_xxx
        ../models/hadeler-based/xxx/test_solver.cpp
        ../models/hadeler-based/xxx/solver.cpp)
    target_compile_definitions(test_xxx PRIVATE COPOSIT_XXX_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_xxx PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME XxxModelTests COMMAND test_xxx)

    add_executable(test_xxx_two
        ../models/hadeler-based/xxx_two/test_solver.cpp
        ../models/hadeler-based/xxx_two/solver.cpp)
    target_compile_definitions(test_xxx_two PRIVATE COPOSIT_XXX_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_xxx_two PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME XxxTwoModelTests COMMAND test_xxx_two)

    add_executable(test_xxx_exact_greedy
        ../experiments/exact_greedy_kkt_walk_2026-08-19/test_solver.cpp
        ../experiments/exact_greedy_kkt_walk_2026-08-19/solver.cpp)
    target_compile_definitions(test_xxx_exact_greedy PRIVATE COPOSIT_XXX_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_xxx_exact_greedy PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME XxxExactGreedyModelTests COMMAND test_xxx_exact_greedy)

    add_executable(test_sat_halfspace_lp_dickinson
        ../models/hadeler-based/sat_halfspace_lp_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_lp_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_lp_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_RAYS_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_lp_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceLpDickinsonModelTests COMMAND test_sat_halfspace_lp_dickinson)

    add_executable(test_sat_halfspace_milp_dickinson
        ../models/hadeler-based/sat_halfspace_milp_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_milp_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_milp_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_milp_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceMilpDickinsonModelTests COMMAND test_sat_halfspace_milp_dickinson)

    add_executable(test_sat_halfspace_rays_lookahead_dickinson
        ../models/hadeler-based/sat_halfspace_rays_lookahead_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_rays_lookahead_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_rays_lookahead_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_RAYS_LOOKAHEAD_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_rays_lookahead_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceRaysLookaheadDickinsonModelTests COMMAND test_sat_halfspace_rays_lookahead_dickinson)

    add_executable(test_sat_halfspace_rays_wide_dickinson
        ../models/hadeler-based/sat_halfspace_rays_wide_dickinson/test_solver.cpp
        ../models/hadeler-based/sat_halfspace_rays_wide_dickinson/solver.cpp)
    target_compile_definitions(test_sat_halfspace_rays_wide_dickinson
        PRIVATE COPOSIT_SAT_HALFSPACE_RAYS_WIDE_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_sat_halfspace_rays_wide_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME SatHalfspaceRaysWideDickinsonModelTests COMMAND test_sat_halfspace_rays_wide_dickinson)

    add_executable(test_wide_certificate_sat_dickinson
        ../models/hadeler-based/wide_certificate_sat_dickinson/test_solver.cpp
        ../models/hadeler-based/wide_certificate_sat_dickinson/solver.cpp)
    target_compile_definitions(test_wide_certificate_sat_dickinson
        PRIVATE COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_wide_certificate_sat_dickinson PRIVATE coposit_core coposit_cadical GTest::gtest_main)
    add_test(NAME WideCertificateSatDickinsonModelTests COMMAND test_wide_certificate_sat_dickinson)

    add_executable(test_clingo_dickinson
        ../models/hadeler-based/clingo_dickinson/test_solver.cpp
        ../models/hadeler-based/clingo_dickinson/solver.cpp)
    target_compile_definitions(test_clingo_dickinson
        PRIVATE COPOSIT_CLINGO_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_clingo_dickinson PRIVATE coposit_core libclingo GTest::gtest_main)
    add_test(NAME ClingoDickinsonModelTests COMMAND test_clingo_dickinson)

    add_executable(test_clingo_halfspace_dickinson
        ../models/hadeler-based/clingo_halfspace_dickinson/test_solver.cpp
        ../models/hadeler-based/clingo_halfspace_dickinson/solver.cpp)
    target_compile_definitions(test_clingo_halfspace_dickinson
        PRIVATE COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING=1 COPOSIT_ENABLE_TIMEOUTS=1)
    target_link_libraries(test_clingo_halfspace_dickinson PRIVATE coposit_core libclingo GTest::gtest_main)
    add_test(NAME ClingoHalfspaceDickinsonModelTests COMMAND test_clingo_halfspace_dickinson)

    add_executable(test_support_pruned_dickinson
        ../models/hadeler-based/support_pruned_dickinson/test_solver.cpp
        ../models/hadeler-based/support_pruned_dickinson/solver.cpp)
    target_link_libraries(test_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME SupportPrunedDickinsonModelTests COMMAND test_support_pruned_dickinson)

    add_executable(test_nullity_support_pruned_dickinson
        ../models/hadeler-based/nullity_support_pruned_dickinson/test_solver.cpp
        ../models/hadeler-based/nullity_support_pruned_dickinson/solver.cpp)
    target_compile_definitions(test_nullity_support_pruned_dickinson
        PRIVATE COPOSIT_NULLITY_SUPPORT_PRUNED_DICKINSON_TESTING=1)
    target_link_libraries(test_nullity_support_pruned_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME NullitySupportPrunedDickinsonModelTests COMMAND test_nullity_support_pruned_dickinson)

    add_executable(test_rhs_dickinson ../models/hadeler-based/rhs_dickinson/test_solver.cpp ../models/hadeler-based/rhs_dickinson/solver.cpp)
    target_compile_definitions(test_rhs_dickinson PRIVATE COPOSIT_RHS_DICKINSON_TESTING=1)
    target_link_libraries(test_rhs_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME RhsDickinsonModelTests COMMAND test_rhs_dickinson)

    add_executable(test_frank_wolfe_dickinson
        ../models/hadeler-based/frank_wolfe_dickinson/test_solver.cpp
        ../models/hadeler-based/frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_frank_wolfe_dickinson PRIVATE COPOSIT_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeDickinsonModelTests COMMAND test_frank_wolfe_dickinson)

    add_executable(test_one_step_frank_wolfe_dickinson
        ../models/hadeler-based/one_step_frank_wolfe_dickinson/test_solver.cpp
        ../models/hadeler-based/one_step_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_one_step_frank_wolfe_dickinson
        PRIVATE COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_one_step_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME OneStepFrankWolfeDickinsonModelTests COMMAND test_one_step_frank_wolfe_dickinson)

    add_executable(test_pairwise_frank_wolfe_dickinson
        ../models/hadeler-based/pairwise_frank_wolfe_dickinson/test_solver.cpp
        ../models/hadeler-based/pairwise_frank_wolfe_dickinson/solver.cpp)
    target_compile_definitions(test_pairwise_frank_wolfe_dickinson
        PRIVATE COPOSIT_PAIRWISE_FRANK_WOLFE_DICKINSON_TESTING=1)
    target_link_libraries(test_pairwise_frank_wolfe_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME PairwiseFrankWolfeDickinsonModelTests COMMAND test_pairwise_frank_wolfe_dickinson)

    add_executable(test_support_polished_frank_wolfe_dickinson
        ../models/hadeler-based/support_polished_frank_wolfe_dickinson/test_solver.cpp
        ../models/hadeler-based/support_polished_frank_wolfe_dickinson/solver.cpp)
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
        target_compile_definitions(${target} PRIVATE COPOSIT_BASELINE_SOURCE_DIAGNOSTICS=1)
    endforeach()

    add_executable(test_frank_wolfe_sponsel
        ../models/experiments/frank_wolfe_sponsel/test_solver.cpp
        ../models/experiments/frank_wolfe_sponsel/solver.cpp)
    target_compile_definitions(test_frank_wolfe_sponsel PRIVATE COPOSIT_FRANK_WOLFE_SPONSEL_TESTING=1)
    target_link_libraries(test_frank_wolfe_sponsel PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FrankWolfeSponselModelTests COMMAND test_frank_wolfe_sponsel)

    add_executable(test_fracessa ../models/hadeler-based/fracessa/test_solver.cpp ../models/hadeler-based/fracessa/solver.cpp)
    target_link_libraries(test_fracessa PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME FracessaModelTests COMMAND test_fracessa)

    add_executable(test_zischg_hadeler ../models/hadeler-based/zischg_hadeler/test_solver.cpp ../models/hadeler-based/zischg_hadeler/solver.cpp)
    target_compile_definitions(test_zischg_hadeler PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_hadeler PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgHadelerModelTests COMMAND test_zischg_hadeler)

    add_executable(test_zischg_dickinson
        ../models/hadeler-based/zischg_dickinson/test_solver.cpp
        ../models/hadeler-based/zischg_dickinson/solver.cpp)
    target_compile_definitions(test_zischg_dickinson PRIVATE COPOSIT_ZISCHG_LEVEL_TWO_TESTING=1)
    target_link_libraries(test_zischg_dickinson PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME ZischgDickinsonModelTests COMMAND test_zischg_dickinson)

    add_executable(test_zischg_fracessa ../models/hadeler-based/zischg_fracessa/test_solver.cpp ../models/hadeler-based/zischg_fracessa/solver.cpp)
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

    add_executable(test_diagnostics tests/test_diagnostics.cpp)
    target_link_libraries(test_diagnostics PRIVATE coposit_core GTest::gtest_main)
    add_test(NAME DiagnosticsTests COMMAND test_diagnostics)

    if(COPOSIT_BUILD_APPS)
    add_executable(test_companion_launcher tests/test_companion_launcher.cpp)
    target_link_libraries(test_companion_launcher PRIVATE Threads::Threads)
    add_test(NAME CompanionLauncherTimeoutTest COMMAND test_companion_launcher ${CMAKE_COMMAND})

    add_test(NAME CliStrictSmokeTest COMMAND coposit
        --model dickinson_2019 --mode strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliStrictSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliDiagnosticsSmokeTest COMMAND coposit
        --model dickinson_2019 --mode strict --diagnostics ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliDiagnosticsSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "true")

    add_test(NAME CliMatrixMarketTest COMMAND coposit
        --model dickinson_2019 --mode strict ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.mtx)
    set_tests_properties(CliMatrixMarketTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliDirectMatrixTest COMMAND coposit --model dickinson_2019 --mode non-strict "2#1,-1,1")
    set_tests_properties(CliDirectMatrixTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliPreprocessingOffTest COMMAND coposit
        --model bundfuss_2008 --mode strict --preprocessing off
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliPreprocessingOffTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliDuTourTest COMMAND coposit
        --model dutour_2018 --mode non-strict --preprocessing off
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliDuTourTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliTimeoutOptionSmokeTest COMMAND coposit
        --model dickinson_2019 --mode strict --timeout 2 ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/identity_2.txt)
    set_tests_properties(CliTimeoutOptionSmokeTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliCombinedTest COMMAND coposit
        --model dickinson_2019 --mode both ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/boundary_2.txt)
    set_tests_properties(CliCombinedTest PROPERTIES
        PASS_REGULAR_EXPRESSION "copositive=true[\r\n]+strictly_copositive=false")

    add_test(NAME CliUnknownModelTest COMMAND coposit --model unknown --mode strict "1#1")
    set_tests_properties(CliUnknownModelTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliExperimentTest COMMAND coposit --model frank_wolfe_dickinson --mode strict "1#1")
    set_tests_properties(CliExperimentTest PROPERTIES PASS_REGULAR_EXPRESSION "^true")

    add_test(NAME CliUnsupportedCombinedModeTest COMMAND coposit --model bundfuss_2008 --mode both "1#1")
    set_tests_properties(CliUnsupportedCombinedModeTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliMissingModelTest COMMAND coposit --mode strict "1#1")
    set_tests_properties(CliMissingModelTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliDefaultCombinedModeTest COMMAND coposit --model dickinson_2019 "1#1")
    set_tests_properties(CliDefaultCombinedModeTest PROPERTIES
        PASS_REGULAR_EXPRESSION "copositive=true[\r\n]+strictly_copositive=true")

    add_test(NAME CliMissingUnsupportedModeTest COMMAND coposit --model bundfuss_2008 "1#1")
    set_tests_properties(CliMissingUnsupportedModeTest PROPERTIES WILL_FAIL TRUE)

    add_test(NAME CliParameterizedWideCertificateTest COMMAND coposit
        --model wide_certificate_cbdd_dickinson --model-parameter 75 "1#1")
    set_tests_properties(CliParameterizedWideCertificateTest PROPERTIES
        PASS_REGULAR_EXPRESSION "copositive=true[\r\n]+strictly_copositive=true")

    add_test(NAME CliMissingWideCertificateParameterTest COMMAND coposit
        --model wide_certificate_cbdd_dickinson "1#1")
    set_tests_properties(CliMissingWideCertificateParameterTest PROPERTIES WILL_FAIL TRUE)
    endif()

    if(COPOSIT_BUILD_PYTHON)
        add_test(NAME PythonWrapperTests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/../python/tests -v)
        set_tests_properties(PythonWrapperTests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python")
    endif()
endif()
