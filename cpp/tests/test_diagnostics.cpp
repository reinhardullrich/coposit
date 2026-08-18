#include <coposit/diagnostics.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>

namespace coposit::diagnostics::testing {

TEST(Diagnostics, FormatsSupportCoverageWithoutCallingTheClock)
{
    snapshot value;
    value.kind = metric::support;
    value.nodes = 3;
    value.resolved = 1;
    value.secondary = 2;
    value.splits = 2;
    value.current = 2;
    value.maximum = 2;
    const std::string output = detail::format(value, std::chrono::seconds(15), 12.5);
    EXPECT_NE(output.find("[00:00:15] stage=model  metric=support  coverage=100.000000%"), std::string::npos);
    EXPECT_NE(output.find("cardinality=2/2"), std::string::npos);
    EXPECT_NE(output.find("visited=3  covered=1  processed=2  certificates=2"), std::string::npos);
    EXPECT_NE(output.find("rate=12.5/s"), std::string::npos);
}

TEST(Diagnostics, FormatsSupportCertificateDistribution)
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

TEST(Diagnostics, RecordsTheCardinalityAndNullityOfFactoredSingularSupports)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    tracker model_diagnostics(metric::support, 8);
    model_diagnostics.stage(5);
    model_diagnostics.singular_support(2);
    model_diagnostics.singular_support(2);
    model_diagnostics.singular_support(4);
    const snapshot value = detail::load();
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();

    EXPECT_EQ(value.singular_cardinality_nullity_counts,
              (std::map<std::pair<size_t, size_t>, uint64_t>{{{5, 2}, 2}, {{5, 4}, 1}}));
    EXPECT_NE(detail::format(value, std::chrono::seconds(1), 0.0).find("singular_k_q_counts=[(5,2,2),(5,4,1)]"),
              std::string::npos);
}

TEST(Diagnostics, FormatsCertificateDistributionWithUpperSetSize)
{
    snapshot value;
    value.kind = metric::decision_diagram;
    value.current = 2;
    value.maximum = 6;
    value.certificate_cardinality_free_index_upper_size_counts = {{{2, 4, 5}, 3}};

    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("certificate_k_d_u_counts=[(2,4,5,3)]"), std::string::npos);
}

TEST(Diagnostics, FormatsPreprocessingPhaseAndWorkWithoutAnEta)
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

TEST(Diagnostics, FormatsPersistentPreprocessingReductionSummary)
{
    snapshot value;
    value.kind = metric::preprocessing;
    value.phase = preprocessing_phase::model_delegation;
    value.depth = 40;
    value.preprocessing_root_dimension = 100;
    value.preprocessing_finished = true;
    value.preprocessing_component_split = true;
    value.preprocessing_components_seen = 3;
    value.preprocessing_largest_component = 40;
    value.preprocessing_pending_components = 2;
    value.preprocessing_largest_pending_component = 40;
    value.preprocessing_reduction_child_checks = 7;
    value.preprocessing_maximum_reduction_depth = 2;
    value.preprocessing_reduction_decisions = 1;
    value.preprocessing_model_delegations = 1;

    const std::string output = detail::format(value, std::chrono::seconds(3), 0.0);
    EXPECT_NE(output.find("preprocessing_root=100  preprocessing_outcome=pending  component_split=yes  "
                          "components_seen=3  largest_component=40"),
              std::string::npos);
    EXPECT_NE(output.find("pending_components=2  largest_pending_component=40"), std::string::npos);
    EXPECT_NE(output.find("reduction_child_checks=7  maximum_reduction_depth=2  "
                          "reduction_decisions=1  model_delegations=1"),
              std::string::npos);

    value.preprocessing_pending_components = 0;
    EXPECT_NE(detail::format(value, std::chrono::seconds(3), 0.0).find("preprocessing_outcome=resolved"), std::string::npos);
    value.preprocessing_finished = false;
    EXPECT_NE(detail::format(value, std::chrono::seconds(3), 0.0).find("preprocessing_outcome=running"), std::string::npos);
}

TEST(Diagnostics, NamesTheZMatrixPreCheck)
{
    snapshot value;
    value.kind = metric::preprocessing;
    value.phase = preprocessing_phase::z_matrix;
    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("stage=preprocessing  phase=Z-matrix"), std::string::npos);
}

TEST(Diagnostics, NamesTheMotzkinStrausPreCheck)
{
    snapshot value;
    value.kind = metric::preprocessing;
    value.phase = preprocessing_phase::motzkin_straus;
    const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
    EXPECT_NE(output.find("stage=preprocessing  phase=Motzkin-Straus"), std::string::npos);
}

TEST(Diagnostics, FormatsCircularBraceletWorkWithoutInventingCoverage)
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

TEST(Diagnostics, FormatsDecisionDiagramCardinalityAndWorkWithoutInventingCoverage)
{
    snapshot value;
    value.kind = metric::decision_diagram;
    value.diagram_phase = decision_diagram_phase::certificate_subtract;
    value.nodes = 17;
    value.resolved = 16;
    value.secondary = 230;
    value.splits = 4096;
    value.current = 6;
    value.maximum = 45;
    value.certificate_cardinality_free_index_counts = {{{1, 44}, 2}, {{6, 16}, 3}};
    const std::string output = detail::format(value, std::chrono::seconds(12), 2.0, std::chrono::seconds(4));
    EXPECT_NE(output.find("metric=decision-diagram  phase=certificate subtract  "
                          "cardinality=6/45"),
              std::string::npos);
    EXPECT_NE(output.find("cardinality_elapsed=00:00:04"), std::string::npos);
    EXPECT_NE(output.find("emitted_supports=17  certificates=16"), std::string::npos);
    EXPECT_NE(output.find("dd_nodes_allocated=230  dd_operations=4096"), std::string::npos);
    EXPECT_NE(output.find("certificate_k_d_counts=[(1,44,2),(6,16,3)]"), std::string::npos);
    EXPECT_EQ(output.find("coverage="), std::string::npos);
}

TEST(Diagnostics, PublishesPreprocessingOnlyWhenReportingIsEnabled)
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

TEST(Diagnostics, ModelTrackerPreservesPreprocessingReductionSummary)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    preprocessing_begin(100);
    preprocessing_top_component(40, true);
    preprocessing_top_component(35, true);
    preprocessing_complete(2, 40);
    preprocessing_reduction_child(1);
    preprocessing_reduction_child(2);
    preprocessing_reduction_decision();
    preprocessing_model_delegation();
    tracker model_diagnostics(metric::support, 40);
    const snapshot value = detail::load();
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();

    EXPECT_EQ(value.kind, metric::support);
    EXPECT_EQ(value.preprocessing_root_dimension, 100U);
    EXPECT_TRUE(value.preprocessing_finished);
    EXPECT_TRUE(value.preprocessing_component_split);
    EXPECT_EQ(value.preprocessing_components_seen, 2U);
    EXPECT_EQ(value.preprocessing_largest_component, 40U);
    EXPECT_EQ(value.preprocessing_pending_components, 2U);
    EXPECT_EQ(value.preprocessing_largest_pending_component, 40U);
    EXPECT_EQ(value.preprocessing_reduction_child_checks, 2U);
    EXPECT_EQ(value.preprocessing_maximum_reduction_depth, 2U);
    EXPECT_EQ(value.preprocessing_reduction_decisions, 1U);
    EXPECT_EQ(value.preprocessing_model_delegations, 1U);
}

TEST(Diagnostics, NamesTheDanningerPreprocessingPhase)
{
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::root_checks), "root checks");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::negative_part_diagonal_dominance), "negative-part diagonal dominance");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::negative_part_factorization), "negative-part factorization");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::all_ones), "all-ones");
    EXPECT_STREQ(detail::phase_text(preprocessing_phase::danninger), "Danninger");
}

TEST(Diagnostics, UsesTheFixedOneSecondInterval)
{
    EXPECT_EQ(report_interval, std::chrono::seconds(1));
    EXPECT_EQ(decision_diagram_publish_interval, 200U);
}

TEST(Diagnostics, PrintsTheExactFinalSnapshotWithoutWaitingOneSecond)
{
    std::ostringstream output;
    reporter diagnostics_reporter(true, output);
    {
        tracker model_diagnostics(metric::support, 4);
        model_diagnostics.stage(1);
        model_diagnostics.visit_support();
        model_diagnostics.secondary();
        model_diagnostics.certificate();
        model_diagnostics.finish();
    }
    diagnostics_reporter.stop();

    EXPECT_NE(output.str().find("visited=1  covered=0  processed=1  certificates=1"), std::string::npos);
}

TEST(Diagnostics, PublishesTheFirstModelNodeImmediately)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_diagnostics(metric::proof, 16);
        model_diagnostics.visit(16, 0);
        const snapshot value = detail::load();
        EXPECT_EQ(value.kind, metric::proof);
        EXPECT_EQ(value.nodes, 1U);
        EXPECT_EQ(value.current, 16U);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Diagnostics, PublishesEverySupportCounterImmediately)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_diagnostics(metric::support, 8);
        model_diagnostics.stage(2);
        model_diagnostics.visit_support();
        model_diagnostics.secondary();
        model_diagnostics.certificate();
        model_diagnostics.visit_support();
        model_diagnostics.covered_support();
        const snapshot value = detail::load();
        EXPECT_EQ(value.nodes, 2U);
        EXPECT_EQ(value.resolved, 1U);
        EXPECT_EQ(value.secondary, 1U);
        EXPECT_EQ(value.splits, 1U);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Diagnostics, SeparatesOuterAndLiftedSupportWork)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_diagnostics(metric::support, 8);
        model_diagnostics.secondary();
        model_diagnostics.support_lift_system(5, 2, 7);
        model_diagnostics.support_lift_duplicate(5, 2, 7);
        model_diagnostics.support_lift_covered(6, 3, 7);
        model_diagnostics.support_lift_frontier(9);
        model_diagnostics.finish();
        const snapshot value = detail::load();
        EXPECT_EQ(value.secondary, 2U);
        EXPECT_EQ(value.lifted_processed, 1U);
        EXPECT_EQ(value.lift_duplicate_skips, 1U);
        EXPECT_EQ(value.lift_covered_skips, 1U);
        EXPECT_EQ(value.lift_dimension, 6U);
        EXPECT_EQ(value.lift_depth, 3U);
        EXPECT_EQ(value.lift_maximum_dimension, 6U);
        EXPECT_EQ(value.lift_maximum_depth, 3U);
        EXPECT_EQ(value.lift_cache_size, 7U);
        EXPECT_EQ(value.lift_frontier_size, 9U);
        EXPECT_EQ(value.lift_maximum_frontier_size, 9U);
        const std::string output = detail::format(value, std::chrono::seconds(1), 0.0);
        EXPECT_NE(output.find("processed=2"), std::string::npos);
        EXPECT_NE(output.find("outer_processed=1"), std::string::npos);
        EXPECT_NE(output.find("lifted_processed=1"), std::string::npos);
        EXPECT_NE(output.find("lift_duplicate_skips=1"), std::string::npos);
        EXPECT_NE(output.find("lift_covered_skips=1"), std::string::npos);
        EXPECT_NE(output.find("lift_cache_size=7"), std::string::npos);
        EXPECT_NE(output.find("lift_dimension=6  lift_depth=3"), std::string::npos);
        EXPECT_NE(output.find("lift_maximum_dimension=6  lift_maximum_depth=3"), std::string::npos);
        EXPECT_NE(output.find("lift_frontier=9  lift_maximum_frontier=9"), std::string::npos);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Diagnostics, RecordsRootAndLiftedCertificateCardinalities)
{
    detail::reset();
    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_diagnostics(metric::support, 45);
        model_diagnostics.support_cardinality(3);
        model_diagnostics.lifted_certificate(8, 45, 6);
        const snapshot value = detail::load();
        const auto entry = value.certificate_root_lifted_upper_lower_counts.find({3, 8, 45, 6});
        ASSERT_NE(entry, value.certificate_root_lifted_upper_lower_counts.end());
        EXPECT_EQ(entry->second, 1U);
        EXPECT_NE(detail::format(value, std::chrono::seconds(1), 0.0)
                      .find("certificate_root_k_lifted_k_u_l_counts=[(3,8,45,6,1)]"),
                  std::string::npos);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Diagnostics, PublishesDecisionDiagramCountersOnlyWhenEnabled)
{
    detail::reset();
    {
        tracker model_diagnostics(metric::decision_diagram, 45);
        model_diagnostics.decision_diagram_cardinality(6, decision_diagram_phase::cardinality_build);
        model_diagnostics.decision_diagram_support();
        EXPECT_EQ(detail::load().kind, metric::none);
    }

    detail::state.enabled.store(true, std::memory_order_relaxed);
    {
        tracker model_diagnostics(metric::decision_diagram, 45);
        model_diagnostics.decision_diagram_cardinality(6, decision_diagram_phase::cardinality_build);
        model_diagnostics.decision_diagram_phase_change(decision_diagram_phase::support_solve);
        model_diagnostics.decision_diagram_support();
        model_diagnostics.decision_diagram_certificate();
        model_diagnostics.decision_diagram_work(230, 4096);
        const snapshot value = detail::load();
        EXPECT_EQ(value.current, 6U);
        EXPECT_EQ(value.nodes, 1U);
        EXPECT_EQ(value.resolved, 1U);
        EXPECT_EQ(value.secondary, 230U);
        EXPECT_EQ(value.splits, 4096U);
        EXPECT_EQ(value.diagram_phase, decision_diagram_phase::support_solve);
        EXPECT_GT(value.cardinality_started_ns, 0);
    }
    detail::state.enabled.store(false, std::memory_order_relaxed);
    detail::reset();
}

TEST(Diagnostics, FormatsSeparateAdaptiveEngineCounters)
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

} // namespace coposit::diagnostics::testing
