#include <coposit/progress.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace coposit::progress::testing {

TEST(Progress, FormatsSupportCoverageWithoutCallingTheClock)
{
    snapshot value;
    value.kind = metric::support;
    value.nodes = 3;
    value.resolved = 1;
    value.secondary = 2;
    value.current = 2;
    value.maximum = 2;
    const std::string output = detail::format(value, std::chrono::seconds(15), 12.5);
    EXPECT_NE(output.find("[00:00:15] stage=model  metric=support  coverage=100.000000%"), std::string::npos);
    EXPECT_NE(output.find("cardinality=2/2"), std::string::npos);
    EXPECT_NE(output.find("rate=12.5/s"), std::string::npos);
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

TEST(Progress, UsesTheFixedOneSecondInterval)
{
    EXPECT_EQ(report_interval, std::chrono::seconds(1));
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
