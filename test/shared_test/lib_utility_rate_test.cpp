#include <gtest/gtest.h>
#include <lib_utility_rate.h>
#include <lib_utility_rate_equations.h>

#include "shared_rate_data.h"


TEST(lib_utility_rate_test, test_copy)
{
	ssc_number_t p_ur_ec_sched_weekday[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1 };
	ssc_number_t p_ur_ec_sched_weekend[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    ssc_number_t p_ur_ec_tou_mat[24] = { 1, 1, 9.9999999999999998e+37, 0, 0.10000000000000001, 0,
        2, 1, 9.9999999999999998e+37, 0, 0.050000000000000003, 0,
        3, 1, 9.9999999999999998e+37, 0, 0.20000000000000001, 0,
        4, 1, 9.9999999999999998e+37, 0, 0.25, 0 };
	size_t tou_rows = 4;
	bool sell_eq_buy = false;

	rate_data data;
	data.init(8760);
	data.setup_energy_rates(&p_ur_ec_sched_weekday[0], &p_ur_ec_sched_weekend[0], tou_rows, &p_ur_ec_tou_mat[0], sell_eq_buy);

	int steps_per_hour = 1;
	// TODO - fill these out
	std::vector<double> monthly_load_forecast;
	std::vector<double> monthly_gen_forecast;
	std::vector<double> monthly_peak_forecast;
	UtilityRateForecast rate_forecast_1(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);



}

TEST(lib_utility_rate_test, test_tiered_tou_cost_estimates)
{
	// Rate is PG&E's EV9A from https://www.pge.com/includes/docs/pdfs/shared/environment/pge/cleanair/electricdrivevehicles/PEV_rate_options.pdf
	// TODO: schedule is not accurate. Update the weekday/weekend schedules if you're going to use this to test any other functions
	ssc_number_t p_ur_ec_sched_weekday[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 1, 1, 1, 1 };
	ssc_number_t p_ur_ec_sched_weekend[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	ssc_number_t p_ur_ec_tou_mat[90] = { 1, 1, 8.3, 0, 0.03461, 0,
		1, 2, 10.8, 0, 0.05140, 0,
		1, 3, 16.6, 0, 0.14698, 0,
		1, 4, 24.9, 0, 0.18727, 0,
		1, 5, 9.9999999999999998e+37, 0, 0.18727, 0,
		2, 1, 8.3, 0, 0.09132, 0,
		2, 2, 10.8, 0, 0.10811, 0,
		2, 3, 16.6, 0, 0.26397, 0,
		2, 4, 24.9, 0, 0.3733, 0,
		2, 5, 9.9999999999999998e+37, 0, 0.3733, 0,
		3, 1, 8.3, 0, 0.27904, 0,
		3, 2, 10.8, 0, 0.29583, 0,
		3, 3, 16.6, 0, 0.45169, 0,
		3, 4, 24.9, 0, 0.5611, 0,
		3, 5, 9.9999999999999998e+37, 0, 0.5611, 0, };
	size_t tou_rows = 15;
	bool sell_eq_buy = false;

	rate_data data;
	data.init(8760);
	data.setup_energy_rates(&p_ur_ec_sched_weekday[0], &p_ur_ec_sched_weekend[0], tou_rows, &p_ur_ec_tou_mat[0], sell_eq_buy);
	data.rate_scale.push_back(1.0);

	int steps_per_hour = 1;
	std::vector<double> monthly_load_forecast;
	monthly_load_forecast.push_back(25); // Rates above are kWh/day, but don't want to test that just yet
	std::vector<double> monthly_gen_forecast;
	monthly_gen_forecast.push_back(0);
	std::vector<double> monthly_peak_forecast;
	monthly_peak_forecast.push_back(7);

	UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

	rate_forecast.compute_next_composite_tou(0, 0);

	ASSERT_EQ(3, rate_forecast.next_buy_rates.size());

	EXPECT_NEAR(0.11365, rate_forecast.next_buy_rates[0], 0.0001);
	EXPECT_NEAR(0.22782, rate_forecast.next_buy_rates[1], 0.0001);
	EXPECT_NEAR(0.41554, rate_forecast.next_buy_rates[2], 0.0001);

	ASSERT_EQ(3, rate_forecast.next_sell_rates.size());

	EXPECT_NEAR(0.0, rate_forecast.next_sell_rates[0], 0.0001);
	EXPECT_NEAR(0.0, rate_forecast.next_sell_rates[1], 0.0001);
	EXPECT_NEAR(0.0, rate_forecast.next_sell_rates[2], 0.0001);
}

TEST(lib_utility_rate_test, test_tiered_sell_rates)
{
	// Rate is PG&E's EV9A from https://www.pge.com/includes/docs/pdfs/shared/environment/pge/cleanair/electricdrivevehicles/PEV_rate_options.pdf
	// TODO: schedule is not accurate. Update the weekday/weekend schedules if you're going to use this to test any other functions
	ssc_number_t p_ur_ec_sched_weekday[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1 };
	ssc_number_t p_ur_ec_sched_weekend[288] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	ssc_number_t p_ur_ec_tou_mat[90] = { 1, 1, 8.2, 0, 0.03461, 0.02,
		1, 2, 10.8, 0, 0.05140, 0.02,
		1, 3, 9.9999999999999998e+37, 0, 0.18727, 0.02,
		2, 1, 8.3, 0, 0.09132, 0.03,
		2, 2, 10.8, 0, 0.10811, 0.01,
		2, 3, 9.9999999999999998e+37, 0, 0.3733, 0.01 };
	size_t tou_rows = 6;
	bool sell_eq_buy = false;

	rate_data data;
	data.init(8760);
	data.setup_energy_rates(&p_ur_ec_sched_weekday[0], &p_ur_ec_sched_weekend[0], tou_rows, &p_ur_ec_tou_mat[0], sell_eq_buy);
	data.rate_scale.push_back(1.0);

	int steps_per_hour = 1;
	std::vector<double> monthly_load_forecast;
	monthly_load_forecast.push_back(0); // Rates above are kWh/day, but don't want to test that just yet
	std::vector<double> monthly_gen_forecast;
	monthly_gen_forecast.push_back(10);
	std::vector<double> monthly_peak_forecast;
	monthly_peak_forecast.push_back(7);

	UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

	rate_forecast.compute_next_composite_tou(0, 0);

	ASSERT_EQ(2, rate_forecast.next_buy_rates.size());

	EXPECT_NEAR(0.03461, rate_forecast.next_buy_rates[0], 0.0001);
	EXPECT_NEAR(0.09132, rate_forecast.next_buy_rates[1], 0.0001);

	ASSERT_EQ(2, rate_forecast.next_sell_rates.size());

	EXPECT_NEAR(0.02, rate_forecast.next_sell_rates[0], 0.0001);
	EXPECT_NEAR(0.0266, rate_forecast.next_sell_rates[1], 0.0001);
}

TEST(lib_utility_rate_test, test_demand_charges_crossing_months)
{
	rate_data data;
	set_up_default_commercial_rate_data(data);

	int steps_per_hour = 1;
	std::vector<double> monthly_load_forecast = { 150, 75 };
	std::vector<double> monthly_gen_forecast = { 0, 0 };
	std::vector<double> monthly_peak_forecast = { 100, 50 };

	UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

	// - is load
	std::vector<double> forecast = {-100, -50, -50, -25};
	rate_forecast.initializeMonth(0, 0);
	rate_forecast.copyTOUForecast();

	int hour_of_year = 742; // 10 pm on Jan 31st
	double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

	// Total cost for the months would be $1511.25, but this subtracts off the peaks predicted by average load ($3.12)
	ASSERT_NEAR(1508.11, cost, 0.02);
}

TEST(lib_utility_rate_test, test_changing_rates_crossing_months)
{
	rate_data data;
	set_up_default_commercial_rate_data(data);

	int steps_per_hour = 1;
	std::vector<double> monthly_load_forecast = { 0, 0, 0, 150, 75 };
	std::vector<double> monthly_gen_forecast = { 0, 0, 0, 0, 0 };
	std::vector<double> monthly_peak_forecast = { 0, 0, 0, 100, 50 };

	UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

	// - is load
	std::vector<double> forecast = { -100, -50, -50, -25 };
	rate_forecast.initializeMonth(3, 0);
	rate_forecast.copyTOUForecast();

	int hour_of_year = 2878; // 10 pm on Apr 30th
	double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

	// Total cost for the months would be $1513.13, but this subtracts off the peaks predicted by average load ($3.09)
	ASSERT_NEAR(1510.03, cost, 0.02);
}

TEST(lib_utility_rate_test, test_demand_charges_crossing_year)
{
	rate_data data;
	set_up_default_commercial_rate_data(data);

	int steps_per_hour = 1;
	std::vector<double> monthly_load_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 150, 75 };
	std::vector<double> monthly_gen_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	std::vector<double> monthly_peak_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 50 };

	UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

	// - is load
	std::vector<double> forecast = { -100, -50, -50, -25 };
	rate_forecast.initializeMonth(11, 0);
	rate_forecast.copyTOUForecast();

	int hour_of_year = 8758; // 10 pm on Dec 31st
	double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

	ASSERT_NEAR(1520.79, cost, 0.02);
}

TEST(lib_utility_rate_test, test_sell_rates)
{
    rate_data data;
    ssc_number_t p_ur_ec_sched_weekday[288] = { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 4 };
    ssc_number_t p_ur_ec_sched_weekend[288] = { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
    ssc_number_t p_ur_ec_tou_mat[24] = { 1, 1, 9.9999999999999998e+37, 0, 0.050000000000000003, 0.02,
        2, 1, 9.9999999999999998e+37, 0, 0.074999999999999997, 0.02,
        3, 1, 9.9999999999999998e+37, 0, 0.059999999999999998, 0.02,
        4, 1, 9.9999999999999998e+37, 0, 0.050000000000000003, 0.02};
    size_t tou_rows = 4;
    bool sell_eq_buy = false;

    ssc_number_t p_load_escalation[1] = { 0 };
    ssc_number_t p_rate_escalation[1] = { 0 };

    ssc_number_t  ur_ts_sell_rate[1] = { 0 };

    ssc_number_t p_ur_dc_sched_weekday[288] = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2 };
    ssc_number_t p_ur_dc_sched_weekend[288] = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
    ssc_number_t p_ur_dc_tou_mat[16] = { 1, 1, 100, 20,
                                         1, 2, 9.9999999999999998e+37, 15,
                                         2, 1, 100, 10,
                                         2, 2, 9.9999999999999998e+37, 5 };
    ssc_number_t p_ur_dc_flat_mat[48] = { 0, 1, 9.9999999999999998e+37, 0,
                                        1, 1, 9.9999999999999998e+37, 0,
                                        2, 1, 9.9999999999999998e+37, 0,
                                        3, 1, 9.9999999999999998e+37, 0,
                                        4, 1, 9.9999999999999998e+37, 0,
                                        5, 1, 9.9999999999999998e+37, 0,
                                        6, 1, 9.9999999999999998e+37, 0,
                                        7, 1, 9.9999999999999998e+37, 0,
                                        8, 1, 9.9999999999999998e+37, 0,
                                        9, 1, 9.9999999999999998e+37, 0,
                                        10, 1, 9.9999999999999998e+37, 0,
                                        11, 1, 9.9999999999999998e+37, 0 };
    size_t dc_flat_rows = 12;

    data.rate_scale = { 1, 1.025 };
    data.init(8760);
    data.setup_demand_charges(&p_ur_dc_sched_weekday[0], &p_ur_dc_sched_weekend[0], tou_rows, &p_ur_dc_tou_mat[0], dc_flat_rows, &p_ur_dc_flat_mat[0]);
    data.setup_energy_rates(&p_ur_ec_sched_weekday[0], &p_ur_ec_sched_weekend[0], tou_rows, &p_ur_ec_tou_mat[0], sell_eq_buy);
    data.init_energy_rates(false);

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 75 };
    std::vector<double> monthly_gen_forecast = { 0, 100 };
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -100, -50, -50, -25, 25, 50, 25 };
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 742; // 10 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    
    ASSERT_NEAR(1506.11, cost, 0.02);
}

TEST(lib_utility_rate_test, test_net_metering_one_tou_period)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 75 };
    std::vector<double> monthly_gen_forecast = { 0, 0 };
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -100, -50, 50, 100 }; // Net zero load
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 1; // 1 am on Jan 1st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0, cost, 0.001);

    hour_of_year += 4;

    std::vector<double> second_forecast = { 100, 50, -50, -100 }; // Net zero load
    cost = rate_forecast.forecastCost(second_forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0, cost, 0.001);
}

TEST(lib_utility_rate_test, test_net_metering_multiple_tou_periods)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 75 };
    std::vector<double> monthly_gen_forecast = { 0, 0 }; // Test unexpected generation as well
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -100, -50, 50, 100 }; // Net zero load
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 10; // 10 am on Jan 1st, tou rates kick over at noon
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(13.55, cost, 0.01);

    hour_of_year += 4;

    std::vector<double> second_forecast = { 100, 50, -50, -100 }; // Net zero load
    cost = rate_forecast.forecastCost(second_forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0.0, cost, 0.01);
}

TEST(lib_utility_rate_test, test_net_metering_end_of_month_carryover)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 0, 150 };
    std::vector<double> monthly_gen_forecast = { 150, 0 };
    std::vector<double> monthly_peak_forecast = { 0, 100 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { 100, 50, -50, -100 }; // Net zero load
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 742; // 10 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0, cost, 0.001);
}

TEST(lib_utility_rate_test, test_net_metering_end_of_month_cashout)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges
    data.net_metering_credit_month = 0;

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 0, 150 };
    std::vector<double> monthly_gen_forecast = { 150, 0 };
    std::vector<double> monthly_peak_forecast = { 0, 100 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { 100, 50, -50, -100 }; // Net zero load
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 742; // 10 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(13.55, cost, 0.01);
}

TEST(lib_utility_rate_test, test_net_metering_end_of_month_charges)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 0 };
    std::vector<double> monthly_gen_forecast = { 0, 150 };
    std::vector<double> monthly_peak_forecast = { 100, 0 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -100, -50, 50, 100 }; // Net zero load, but charged at end of Jan
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 742; // 10 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(13.55, cost, 0.01);
}

TEST(lib_utility_rate_test, test_net_metering_charges_crossing_year)
{
    rate_data data;
    set_up_pge_residential_rate_data(data);

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 25 };
    std::vector<double> monthly_gen_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 75, 0 };
    std::vector<double> monthly_peak_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -25, 25, 50, -25, -25 }; // Cash out credits at end of year, get charged for Jan at escalated rate (inflation)
    rate_forecast.initializeMonth(11, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 8757; // 9 pm on Dec 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(4.66, cost, 0.01);
}

TEST(lib_utility_rate_test, test_net_metering_charges_crossing_year_other_cash_out_month)
{
    rate_data data;
    set_up_pge_residential_rate_data(data);
    data.net_metering_credit_month = 4;

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 25 };
    std::vector<double> monthly_gen_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 75, 0 };
    std::vector<double> monthly_peak_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { -25, 25, 50, -25, -25 };
    rate_forecast.initializeMonth(11, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 8757; // 9 pm on Dec 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0, cost, 0.01);
}

TEST(lib_utility_rate_test, test_multiple_forecast_calls)
{
    rate_data data;
    set_up_pge_residential_rate_data(data); // No demand charges

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 100, 75 };
    std::vector<double> monthly_gen_forecast = { 175, 0 };
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { 25, 25, 25, 25 };
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 1; // 1 am on Jan 1st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(-2.8, cost, 0.001);

    hour_of_year += 4;

    std::vector<double> second_forecast = { -100, 25, 25, 25 };
    cost = rate_forecast.forecastCost(second_forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0.7, cost, 0.001); // Accounts for using up credits from previous period. Buy rate is not used due to net generation
}

TEST(lib_utility_rate_test, test_one_at_a_time_vs_full_vector)
{
    rate_data data;
    set_up_default_commercial_rate_data(data);

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 75 };
    std::vector<double> monthly_gen_forecast = { 50, 175 };
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { 50, -100, -50, -50, -25, 25, 50, 100 };
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    UtilityRateForecast forecast_copy(rate_forecast);

    int hour_of_year = 741; // 9 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    // Total cost for the months would be $1511.25, but this subtracts off the peaks predicted by average load ($3.12)
    ASSERT_NEAR(1508.11, cost, 0.02);

    cost = 0;
    for (int i = 0; i < forecast.size(); i++)
    {
        std::vector<double> single_forecast = { forecast[i] };
        cost += forecast_copy.forecastCost(single_forecast, 0, hour_of_year + i, 0);
    }

    ASSERT_NEAR(1508.11, cost, 0.02);
}

TEST(lib_utility_rate_test, test_one_at_a_time_vs_full_vector_nm_credits)
{
    rate_data data;
    set_up_pge_residential_rate_data(data);

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 150, 75 };
    std::vector<double> monthly_gen_forecast = { 50, 175 };
    std::vector<double> monthly_peak_forecast = { 100, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 2);

    // - is load
    std::vector<double> forecast = { 50, -100, -50, -50, -25, 25, 50, 100 };
    rate_forecast.initializeMonth(0, 0);
    rate_forecast.copyTOUForecast();

    UtilityRateForecast forecast_copy(rate_forecast);

    int hour_of_year = 741; // 9 pm on Jan 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(9.04, cost, 0.02);

    cost = 0;
    for (int i = 0; i < forecast.size(); i++)
    {
        std::vector<double> single_forecast = { forecast[i] };
        cost += forecast_copy.forecastCost(single_forecast, 0, hour_of_year + i, 0);
    }

    ASSERT_NEAR(9.04, cost, 0.02);
}

TEST(lib_utility_rate_test, test_end_of_analyis_period)
{
    rate_data data;
    set_up_pge_residential_rate_data(data);
    data.net_metering_credit_month = 4;
    data.rate_scale = { 1 };

    int steps_per_hour = 1;
    std::vector<double> monthly_load_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25 };
    std::vector<double> monthly_gen_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 75 };
    std::vector<double> monthly_peak_forecast = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50 };

    UtilityRateForecast rate_forecast(&data, steps_per_hour, monthly_load_forecast, monthly_gen_forecast, monthly_peak_forecast, 1);

    // - is load
    std::vector<double> forecast = { -25, -25, 50 };
    rate_forecast.initializeMonth(11, 0);
    rate_forecast.copyTOUForecast();

    int hour_of_year = 8757; // 9 pm on Dec 31st
    double cost = rate_forecast.forecastCost(forecast, 0, hour_of_year, 0);

    ASSERT_NEAR(0, cost, 0.01);
}