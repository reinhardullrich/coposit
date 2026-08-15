#include <coposit/progress.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>

namespace coposit::progress::testing {

TEST(Progress, FormatsSupportCoverageWithoutCallingTheClock)
{
    snapshot value;
    value.kind = metric::support;
    value.nodes = 3;
    value.resolved = 1;
    value.secondary = 2;
    value.splits = 2;
    value.open = 1;
    value.current = 2;
    value.maximum = 2;
    const std::string output = detail::format(value, std::chrono::seconds(15), 12.5);
    EXPECT_NE(output.find("[00:00:15] stage=model  metric=support  coverage=100.000000%"), std::string::npos);
    EXPECT_NE(output.find("cardinality=2/2"), std::string::npos);
    EXPECT_NE(output.find("visited=3  covered=1  processed=2  certificates=2"), std::string::npos);
    EXPECT_NE(output.find("zed_blocks_tested=1"), std::string::npos);
    EXPECT_NE(output.find("rate=12.5/s"), std::string::npos);
}

TEST(Progress, FormatsSupportCertificateDistribution)
{
    snapshot value;
    value.kind = metric::support;
    value.nodes = 17;
    value.resolved = 4;
    value.secondary = 13;
    value.splits = 2;
    value.current = 2;
    value.maximum = 6;
    value.certificate_cardinality_free_index_counts = {{{2, 4}, 2}};

    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("certificate_k_d_counts=[(2,4,2)]"), std::string::npos);
}

TEST(Progress, FormatsCertificateDistributionWithUpperSetSize)
{
    snapshot value;
    value.kind = metric::decision_diagram;
    value.current = 2;
    value.maximum = 6;
    value.certificate_cardinality_free_index_upper_size_counts = {{{2, 4, 5}, 3}};

    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("certificate_k_d_u_counts=[(2,4,5,3)]"), std::string::npos);
}

TEST(Progress, FormatsPreprocessingPhaseAndWorkWithoutAnEta)
{
    snapshot value;
    value.kind = metric::preprocessing;
    value.phase = preprocessing_phase::exact_factorization;
    value.current = 7;
    value.maximum = 20;
    value.depth = 20;
    const std::string output = detail::format(value, std::chrono::seconds(30), 99.0);
    EXPECT_NE(output.find("[00:00:30] stage=preprocessing  phase=exact factorization"), std::string::npos);
    EXPECT_NE(output.find("work=7/20"), std::string::npos);
    EXPECT_NE(output.find("dimension=20"), std::string::npos);
    EXPECT_EQ(output.find("rate="), std::string::npos);
}

TEST(Progress, NamesTheZMatrixPreCheck)
{
    snapshot value;
    value.kind = metric::preprocessing;
    value.phase = preprocessing_phase::z_matrix;
    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("stage=preprocessing  phase=Z-matrix"), std::string::npos);
}

TEST(Progress, FormatsCircularBraceletWorkWithoutInventingCoverage)
{
    snapshot value;
    value.kind = metric::bracelet;
    value.nodes = 120;
    value.resolved = 20;
    value.secondary = 100;
    value.splits = 7;
    value.current = 9;
    value.maximum = 18;
    const std::string output = detail::format(value, std::chrono::seconds(30), 4.0);
    EXPECT_NE(output.find("metric=bracelet  cardinality=9/18"), std::string::npos);
    EXPECT_NE(output.find("bracelets=120  affine_skipped=20  exact_systems=100  candidates=7"), std::string::npos);
    EXPECT_EQ(output.find("coverage="), std::string::npos);
}

TEST(Progress, FormatsDecisionDiagramCardinalityAndWorkWithoutInventingCoverage)
{
    snapshot value;
    value.kind = metric::decision_diagram;
    value.diagram_phase = decision_diagram_phase::certificate_subtract;
    value.nodes = 17;
    value.resolved = 16;
    value.secondary = 230;
    value.splits = 4096;
    value.open = 3;
    value.current = 6;
    value.maximum = 45;
    value.certificate_cardinality_free_index_counts = {{{1, 44}, 2}, {{6, 16}, 3}};
    const std::string output = detail::format(value, std::chrono::seconds(12), 2.0, std::chrono::seconds(4));
    EXPECT_NE(output.find("metric=decision-diagram  phase=certificate subtract  cardinality=6/45"), std::string::npos);
    EXPECT_NE(output.find("cardinality_elapsed=00:00:04"), std::string::npos);
    EXPECT_NE(output.find("emitted_supports=17  certificates=16  zed_blocks_tested=3"), std::string::npos);
    EXPECT_NE(output.find("dd_nodes_allocated=230  dd_operations=4096"), std::string::npos);
    EXPECT_NE(output.find("certificate_k_d_counts=[(1,44,2),(6,16,3)]"), std::string::npos);
    EXPECT_EQ(output.find("coverage="), std::string::npos);
}

TEST(Progress, PublishesPreprocessingOnlyWhenReportingIsEnabled)
{
    detail::reset();
    preprocessing_stage(preprocessing_phase::frank_wolfe, 12, 2, 12);
    EXPECT_EQ(detail::load().kind, metric::none);

    detail::state.enabled.store(true, std::memory_order_relaxed);
    preprocessing_stage(preprocessing_phase::frank_wolfe, 12, 2, 12);
    advance_preprocessing(3, 12);
    const snapshot value = detail::load();
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();

    EXPECT_EQ(value.kind, metric::preprocessing);
    EXPECT_EQ(value.phase, preprocessing_phase::frank_wolfe);
    EXPECT_EQ(value.current, 3U);
    EXPECT_EQ(value.maximum, 12U);
    EXPECT_EQ(value.depth, 12U);
}

TEST(Progress, NamesTheDanningerPreprocessingPhase)
{
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::root_checks), "root checks");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::negative_part_diagonal_dominance), "negative-part diagonal dominance");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::all_ones), "all-ones");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::danninger), "Danninger");
}

TEST(Progress, UsesTheFixedOneSecondInterval)
{
    EXPECT_EQ(report_interval, std::chrono::seconds(1));
    EXPECT_EQ(decision_diagram_publish_interval, 200U);
}

TEST(Progress, PrintsTheExactFinalSnapshotWithoutWaitingOneSecond)
{
    std::ostringstream output;
    reporter progress_reporter(true, output);
    {
        tracker model_progress(metric::support, 4);
        model_progress.stage(1);
        model_progress.visit_support();
        model_progress.secondary();
        model_progress.certificate();
        model_progress.finish();
    }
    progress_reporter.stop();

    EXPECT_NE(output.str().find("visited=1  covered=0  processed=1  certificates=1"), std::string::npos);
}

TEST(Progress, PublishesTheFirstModelNodeImmediately)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_progress(metric::proof, 16);
        model_progress.visit(16, 0);
        const snapshot value = detail::load();
        EXPECT_EQ(value.kind, metric::proof);
        EXPECT_EQ(value.nodes, 1U);
        EXPECT_EQ(value.current, 16U);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Progress, PublishesEverySupportCounterImmediately)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_progress(metric::support, 8);
        model_progress.stage(2);
        model_progress.visit_support();
        model_progress.secondary();
        model_progress.certificate();
        model_progress.visit_support();
        model_progress.covered_support();
        const snapshot value = detail::load();
        EXPECT_EQ(value.nodes, 2U);
        EXPECT_EQ(value.resolved, 1U);
        EXPECT_EQ(value.secondary, 1U);
        EXPECT_EQ(value.splits, 1U);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Progress, PublishesDecisionDiagramCountersOnlyWhenEnabled)
{
    detail::reset();
    {
        tracker model_progress(metric::decision_diagram, 45);
        model_progress.decision_diagram_cardinality(6, decision_diagram_phase::cardinality_build);
        model_progress.decision_diagram_support();
        EXPECT_EQ(detail::load().kind, metric::none);
    }

    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_progress(metric::decision_diagram, 45);
        model_progress.decision_diagram_cardinality(6, decision_diagram_phase::cardinality_build);
        model_progress.decision_diagram_phase_change(decision_diagram_phase::support_solve);
        model_progress.decision_diagram_support();
        model_progress.decision_diagram_certificate();
        model_progress.decision_diagram_zed_block();
        model_progress.decision_diagram_work(230, 4096);
        const snapshot value = detail::load();
        EXPECT_EQ(value.current, 6U);
        EXPECT_EQ(value.nodes, 1U);
        EXPECT_EQ(value.resolved, 1U);
        EXPECT_EQ(value.open, 1U);
        EXPECT_EQ(value.secondary, 230U);
        EXPECT_EQ(value.splits, 4096U);
        EXPECT_EQ(value.diagram_phase, decision_diagram_phase::support_solve);
        EXPECT_GT(value.cardinality_started_ns, 0);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Progress, FormatsSeparateAdaptiveEngineCounters)
{
    snapshot value;
    value.kind = metric::adaptive;
    value.engine = adaptive_engine::sponsel;
    value.model_phase = adaptive_phase::h_factorization;
    value.route = adaptive_route::sponsel;
    value.nodes = 7;
    value.sponsel_nodes = 5;
    value.sponsel_splits = 4;
    value.copomatrix_nodes = 2;
    value.copomatrix_children = 3;
    value.copomatrix_staircase = 9;
    value.streak = 4;
    value.current = 16;
    value.maximum = 16;
    value.work_current = 6;
    value.work_maximum = 16;
    const std::string output = detail::format(value, std::chrono::seconds(3), 2.0);
    EXPECT_NE(output.find("metric=adaptive  engine=sponsel  phase=H factorization"), std::string::npos);
    EXPECT_NE(output.find("sponsel_nodes=5  sponsel_splits=4"), std::string::npos);
    EXPECT_NE(output.find("copomatrix_nodes=2  copomatrix_children=3  staircase=9"), std::string::npos);
    EXPECT_NE(output.find("work=6/16"), std::string::npos);
}

} // namespace coposit::progress::testing
