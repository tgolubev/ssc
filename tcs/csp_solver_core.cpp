#include "csp_solver_core.h"
#include "csp_solver_util.h"

#include "lib_util.h"
#include "csp_dispatch.h"

#include <sstream>

C_csp_solver::C_csp_solver(C_csp_weatherreader &weather,
	C_csp_collector_receiver &collector_receiver,
	C_csp_power_cycle &power_cycle,
	C_csp_tes &tes,
	C_csp_tou &tou,
	S_csp_system_params &system) : 
	mc_weather(weather), 
	mc_collector_receiver(collector_receiver), 
	mc_power_cycle(power_cycle),
	mc_tes(tes),
	mc_tou(tou),
	ms_system_params(system)
{

	// Hierarchy logic
	reset_hierarchy_logic();

	// Inititalize non-reference member data
	m_T_htf_cold_des = m_q_dot_rec_on_min = m_q_dot_rec_des =
		
		m_cycle_W_dot_des = m_cycle_eta_des = m_cycle_q_dot_des = m_cycle_max_frac = m_cycle_cutoff_frac =
		m_cycle_sb_frac_des = m_cycle_T_htf_hot_des = m_m_dot_pc_des = std::numeric_limits<double>::quiet_NaN();

	// Reporting and Output Tracking
	m_i_reporting = -1;
	m_sim_time_start = m_sim_time_end = m_sim_step_size_baseline =
		m_report_time_start = m_report_time_end = m_report_step = std::numeric_limits<double>::quiet_NaN();

	m_op_mode_tracking.resize(0);

	error_msg = "";

	// Initializie temporary output 2D vector
	mvv_outputs_temp.resize(N_END, std::vector<double>(0,std::numeric_limits<double>::quiet_NaN()));
	for( int i = 0; i < C_csp_solver::N_END; i++ )
		mvv_outputs_temp[i].reserve(10);

	// Initialize ssc output reporting arrays
	mp_reporting_array = 0;

	// Solved Controller Variables
	m_defocus = std::numeric_limits<double>::quiet_NaN();
}

void C_csp_solver::reset_hierarchy_logic()
{
	m_is_CR_SU__PC_OFF__TES_OFF__AUX_OFF_avail = true;
	m_is_CR_ON__PC_SB__TES_OFF__AUX_OFF_avail = true;
	m_is_CR_ON__PC_SU__TES_OFF__AUX_OFF_avail = true;
	m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail = true;
	m_is_CR_OFF__PC_SU__TES_DC__AUX_OFF_avail = true;
	m_is_CR_DF__PC_MAX__TES_OFF__AUX_OFF_avail = true;

	m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_HI_SIDE = true;
	m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_LO_SIDE = true;

	m_is_CR_ON__PC_RM_LO__TES_OFF__AUX_OFF_avail = true;

	m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_HI_SIDE = true;
	m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_LO_SIDE = true;

	m_is_CR_ON__PC_TARGET__TES_DC__AUX_OFF_avail = true;
	m_is_CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = true;

	m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail = true;

	m_is_CR_OFF__PC_SB__TES_DC__AUX_OFF_avail = true;
	m_is_CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF_avail = true;
	m_is_CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = true;
	
	m_is_CR_ON__PC_SB__TES_CH__AUX_OFF_avail = true;
	m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = true;
	m_is_CR_SU__PC_SB__TES_DC__AUX_OFF_avail = true;
	m_is_CR_ON__PC_SB__TES_DC__AUX_OFF_avail = true;

	m_is_CR_OFF__PC_TARGET__TES_DC__AUX_OFF_avail = true;
	m_is_CR_SU__PC_TARGET__TES_DC__AUX_OFF_avail = true;
	m_is_CR_ON__PC_RM_HI__TES_FULL__AUX_OFF_avail = true;

	m_is_CR_ON__PC_MIN__TES_EMPTY__AUX_OFF_avail = true;

	m_is_CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = true;

	m_is_CR_DF__PC_MAX__TES_FULL__AUX_OFF_avail = true;

	m_is_CR_ON__PC_SB__TES_FULL__AUX_OFF_avail = true;

	m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = true;

	m_is_CR_ON__PC_SU__TES_CH__AUX_OFF_avail = true;

	m_is_CR_DF__PC_SU__TES_FULL__AUX_OFF_avail = true;

	m_is_CR_DF__PC_SU__TES_OFF__AUX_OFF_avail = true;
} 

void C_csp_solver::init()
{
	// First, initialize each component and update solver-level membe data as necessary
		// Weather reader
	mc_weather.init();
		// Collector-receiver
	C_csp_collector_receiver::S_csp_cr_init_inputs init_inputs;
	init_inputs.m_latitude = mc_weather.ms_solved_params.m_lat;		//[deg]
	init_inputs.m_longitude = mc_weather.ms_solved_params.m_lon;	//[deg]
	init_inputs.m_shift = mc_weather.ms_solved_params.m_shift;		//[deg]
	C_csp_collector_receiver::S_csp_cr_solved_params cr_solved_params;
	mc_collector_receiver.init(init_inputs, cr_solved_params);
	m_T_htf_cold_des = cr_solved_params.m_T_htf_cold_des;		//[K]
	m_q_dot_rec_on_min = cr_solved_params.m_q_dot_rec_on_min;	//[MW]
	m_q_dot_rec_des = cr_solved_params.m_q_dot_rec_des;			//[MW]
		// Power cycle
	C_csp_power_cycle::S_solved_params pc_solved_params;
	mc_power_cycle.init(pc_solved_params);
	m_cycle_W_dot_des = pc_solved_params.m_W_dot_des;					//[MW]
	m_cycle_eta_des = pc_solved_params.m_eta_des;						//[-]
	m_cycle_q_dot_des = pc_solved_params.m_q_dot_des;					//[MW]
	m_cycle_max_frac = pc_solved_params.m_max_frac;					//[-]
	m_cycle_cutoff_frac = pc_solved_params.m_cutoff_frac;				//[-]
	m_cycle_sb_frac_des = pc_solved_params.m_sb_frac;					//[-]
	m_cycle_T_htf_hot_des = pc_solved_params.m_T_htf_hot_ref + 273.15;	//[K] convert from C
	m_m_dot_pc_des = pc_solved_params.m_m_dot_design;					//[kg/hr]
		// TES
	mc_tes.init();
		// TOU
	mc_tou.init();
	mc_tou.init_parent();
		// Thermal Storage
	m_is_tes = mc_tes.does_tes_exist();

    /* 
    If no TES exists, initialize values to zero. They won't be touched again
    */

	if(!m_is_tes)
	{	// Set constant values for tes HTF states
	
		mc_tes_ch_htf_state.m_m_dot = 0.0;		//[kg/hr]
		mc_tes_ch_htf_state.m_temp_in = 0.0;	//[C]
		mc_tes_ch_htf_state.m_temp_out =0.0;	//[C]

		mc_tes_dc_htf_state.m_m_dot = 0.0;		//[kg/hr]
		mc_tes_dc_htf_state.m_temp_in = 0.0;	//[C]
		mc_tes_dc_htf_state.m_temp_out = 0.0;	//[C]

		mc_tes_outputs.m_q_heater = 0.0;		//[MW]
		mc_tes_outputs.m_W_dot_rhtf_pump = 0.0;	//[MWe]
		mc_tes_outputs.m_q_dot_loss = 0.0;		//[MW]
		mc_tes_outputs.m_q_dot_dc_to_htf = 0.0;	//[MW]
		mc_tes_outputs.m_q_dot_ch_from_htf = 0.0;	//[MW]
		mc_tes_outputs.m_T_hot_ave = 0.0;		//[K]
		mc_tes_outputs.m_T_cold_ave = 0.0;		//[K]
		mc_tes_outputs.m_T_hot_final = 0.0;		//[K]
		mc_tes_outputs.m_T_cold_final = 0.0;	//[K]
	}
}


void C_csp_solver::Ssimulate(C_csp_solver::S_sim_setup & sim_setup, 
								bool(*mf_callback)(void *data, double percent, C_csp_messages *csp_messages, float time_sec), void *m_cdata,
								float **ptr_array,
								float **post_proc_array)
{
	// Load ssc arrays here, for now...
	mp_reporting_array = ptr_array;
	mp_post_proc_array = post_proc_array;

	for( int i = 0; i < C_csp_solver::N_END; i++ )
	{
		if( mp_reporting_array[i] == 0 )
		{
			throw(C_csp_exception("Not all required reported outputs are allocated", "CSP Solver"));
		}
	}
	
	/* 
    This function is called once and manages the time series simulation.
    */

    //Set up initial simulation information.
	m_sim_time_start = 0.0;			//[s] hardcode simulation to start at first of year, for now
	m_sim_time_start = sim_setup.m_sim_time_start;
	
	//m_sim_time_end = 1000.*3600;		//[s] hardcode simulation to run through entire year, for now
	m_sim_time_end = sim_setup.m_sim_time_end;
	
	// Hardcode simulation step size until we build in some weather file checks
	m_sim_step_size_baseline = 3600.0;			//[s]
	mc_sim_info.m_step = m_sim_step_size_baseline;		//[s] hardcode steps = 1 hr, for now

    //initialize control flags
	/*bool is_rec_su_allowed = true;
	bool is_pc_su_allowed = true;
	bool is_pc_sb_allowed = true;*/

    //instantiate dispatch optimization object
    csp_dispatch_opt dispatch;
    //load parameters used by dispatch algorithm
    //-------------------------------
    dispatch.copy_weather_data( mc_weather );
    dispatch.params.col_rec = &mc_collector_receiver;
    dispatch.params.siminfo = &mc_sim_info;
    dispatch.params.messages = &mc_csp_messages;
    
    dispatch.params.dt = mc_sim_info.m_step/3600.;  //hr
    dispatch.params.dt_pb_startup_cold = mc_power_cycle.get_cold_startup_time();
    dispatch.params.dt_pb_startup_hot = mc_power_cycle.get_hot_startup_time();
    dispatch.params.q_pb_standby = mc_power_cycle.get_standby_energy_requirement()*1000.;
    dispatch.params.e_pb_startup_cold = mc_power_cycle.get_cold_startup_energy(m_sim_step_size_baseline)*1000.;
    dispatch.params.e_pb_startup_hot = mc_power_cycle.get_hot_startup_energy(m_sim_step_size_baseline)*1000.;
    
    dispatch.params.dt_rec_startup = mc_collector_receiver.get_startup_time()/3600.;
    dispatch.params.e_rec_startup = mc_collector_receiver.get_startup_energy(m_sim_step_size_baseline)*1000;
    dispatch.params.q_rec_min = mc_collector_receiver.get_min_power_delivery()*1000.;
    dispatch.params.w_rec_pump = mc_collector_receiver.get_pumping_parasitic_coef();


    dispatch.params.e_tes_init = mc_tes.get_initial_charge_energy()*1000;
    dispatch.params.e_tes_min = mc_tes.get_min_charge_energy()*1000;
    dispatch.params.e_tes_max = mc_tes.get_max_charge_energy()*1000;
    dispatch.params.tes_degrade_rate = mc_tes.get_degradation_rate();

    dispatch.params.q_pb_max = mc_power_cycle.get_max_thermal_power()*1000;
    dispatch.params.q_pb_min = mc_power_cycle.get_min_thermal_power()*1000;
    dispatch.params.q_pb_des = m_cycle_q_dot_des*1000.;
    dispatch.params.eta_cycle_ref = m_cycle_eta_des;

    //Cycle efficiency
    dispatch.params.eff_table_load.clear();
    //add zero point
    dispatch.params.eff_table_load.add_point(0., 0.);    //this is required to allow the model to converge
    
    int neff = 2;
    for(int i=0; i<neff; i++)
    {
        double x = dispatch.params.q_pb_min + (dispatch.params.q_pb_max - dispatch.params.q_pb_min)/(double)(neff - 1)*i;
        double xf = x * 1.e-3/m_cycle_q_dot_des;  //MW

        double eta;
        
        //eta = 0.86 + xf * 0.28 - xf*xf * 0.14;  //Equation from curve fit of power tower steam rankine Type 224
        //eta *= m_cycle_eta_des;
        eta = mc_power_cycle.get_efficiency_at_load(xf);

        dispatch.params.eff_table_load.add_point(x, eta);
    }

    //cycle efficiency vs temperature
    dispatch.params.eff_table_Tdb.clear();
    int neffT = 40;

    for(int i=0; i<neffT; i++)
    {
        double T = -10. + 60./(double)(neffT - 1) * i;

        double eta = mc_power_cycle.get_efficiency_at_TPH(T, 1., 30.) / m_cycle_eta_des;  

        dispatch.params.eff_table_Tdb.add_point(T, eta);
    }
    

        //solver parameters
    dispatch.solver_params.max_bb_iter = mc_tou.mc_dispatch_params.m_max_iterations;
    dispatch.solver_params.mip_gap = mc_tou.mc_dispatch_params.m_mip_gap;
    dispatch.solver_params.solution_timeout = mc_tou.mc_dispatch_params.m_solver_timeout;
    dispatch.solver_params.bb_type = mc_tou.mc_dispatch_params.m_bb_type;
    dispatch.solver_params.scaling_type = mc_tou.mc_dispatch_params.m_scaling_type;
    dispatch.solver_params.presolve_type = mc_tou.mc_dispatch_params.m_presolve_type;
    dispatch.solver_params.is_write_ampl_dat = mc_tou.mc_dispatch_params.m_is_write_ampl_dat;
    dispatch.solver_params.is_ampl_engine = mc_tou.mc_dispatch_params.m_is_ampl_engine;
    dispatch.solver_params.ampl_data_dir = mc_tou.mc_dispatch_params.m_ampl_data_dir;
    dispatch.solver_params.ampl_exec_call = mc_tou.mc_dispatch_params.m_ampl_exec_call;
    //-------------------------------

        
	int cr_operating_state = C_csp_collector_receiver::OFF;
	int pc_operating_state = C_csp_power_cycle::OFF;

	//bool is_est_rec_output_useful = false;

	double tol_mode_switching = 0.05;		// Give buffer to account for uncertainty in estimates

	double step_local = mc_sim_info.m_step;		//[hr] Step size might adjust during receiver and/or power cycle startup
	bool is_sim_timestep_complete = true;		//[-] Are we running serial simulations at partial timesteps inside of one typical timestep?

	double time_previous = m_sim_time_start;		//[s]

	double time_sim_step_next = m_sim_time_start + m_sim_step_size_baseline;	//[s]

	double step_tolerance = 10.0;	//[s] For adjustable timesteps, if within 10 seconds, assume it equals baseline timesteps

	mc_sim_info.m_step = step_local;						//[s]
	mc_sim_info.m_time = time_previous + step_local;		//[s]

	// Reset vector that tracks operating modes
	m_op_mode_tracking.resize(0);

	// Reset Controller Variables to Defaults
	m_defocus = 1.0;		//[-]  

	m_i_reporting = 0;
	m_report_time_start = m_sim_time_start;			//[s]
	m_report_step = sim_setup.m_report_step;		//[s]
	m_report_time_end = m_report_time_start + m_report_step;	//[s]

	double reporting_interval_frac = 0.02;
	double reporting_frac_current = reporting_interval_frac;

    /* 
    ************************** MAIN TIME-SERIES LOOP **************************
    */

    double disp_time_last = -9999.;

    //values to report later on the dispatch algorithm
    double disp_qsf_expect = 0.;
    double disp_qsfprod_expect = 0.;
    double disp_qsfsu_expect = 0.;
    double disp_tes_expect = 0.;
    double disp_etasf_expect = 0.;
    double disp_etapb_expect = 0.;
    double disp_qpbsu_expect = 0.;
    double disp_wpb_expect = 0.;
    double disp_rev_expect = 0.;
    //field efficiency learning parameters
    double disp_qsf_last = 0.;
    double disp_qsf_effadj = 1.;
    double disp_effadj_weight = 0.;
    int disp_effadj_count = 0;

	// Block dispatch saved variables
	bool is_q_dot_pc_target_overwrite = false;

	while( mc_sim_info.m_time <= m_sim_time_end )
	{
		// Report simulation progress
		double calc_frac_current = (mc_sim_info.m_time - m_sim_time_start) / (m_sim_time_end - m_sim_time_start);
		if( calc_frac_current > reporting_frac_current )
		{
			if(! 
                mf_callback(m_cdata, (float) calc_frac_current*100.f, &mc_csp_messages, mc_sim_info.m_time)
                )
                return;     //user cancelled the simulation

			reporting_frac_current += reporting_interval_frac;
		}

		// Store mc_sim_info at start of timestep, use in case it needs to be reset if variable timestep modes fail
		double step_ts_start = mc_sim_info.m_step;		//[s]
		double time_ts_start = mc_sim_info.m_time;		//[s]
		
		// Get tou for timestep
		mc_tou.call(mc_sim_info.m_time, mc_tou_outputs);
		size_t tou_period = mc_tou_outputs.m_csp_op_tou;	//[-]
		double f_turbine_tou = mc_tou_outputs.m_f_turbine;	//[-]
		double pricing_mult = mc_tou_outputs.m_price_mult;	//[-]

		// Get collector/receiver & power cycle operating states at start of time step (last time step)
		cr_operating_state = mc_collector_receiver.get_operating_state();
		if( cr_operating_state < C_csp_collector_receiver::E_csp_cr_modes::OFF ||
			cr_operating_state > C_csp_collector_receiver::E_csp_cr_modes::ON )
		{
			std::string msg = util::format("The collector-receiver operating state at time %lg [hr] is %d. Recognized"
				" values are from %d to %d\n", mc_sim_info.m_step / 3600.0, cr_operating_state, C_csp_collector_receiver::E_csp_cr_modes::OFF, C_csp_collector_receiver::E_csp_cr_modes::ON);
			throw(C_csp_exception(msg,"CSP Solver Core"));
		}
		pc_operating_state = mc_power_cycle.get_operating_state();

		// Get TES operating state info at end of last time step
		double q_dot_tes_dc, q_dot_tes_ch;
		q_dot_tes_dc = q_dot_tes_ch = std::numeric_limits<double>::quiet_NaN();
		if( m_is_tes )
		{
            //predict estimated amount of charge/discharge available
			double m_dot_field_dc_est, T_hot_field_dc_est;	//[kg/s, K]
			m_dot_field_dc_est = T_hot_field_dc_est = std::numeric_limits<double>::quiet_NaN();
			mc_tes.discharge_avail_est(m_T_htf_cold_des, mc_sim_info.m_step, q_dot_tes_dc, m_dot_field_dc_est, T_hot_field_dc_est);

			double m_dot_field_ch_est, T_cold_field_ch_est;	//[kg/s, K]
			m_dot_field_ch_est = T_cold_field_ch_est = std::numeric_limits<double>::quiet_NaN();
			mc_tes.charge_avail_est(m_cycle_T_htf_hot_des, mc_sim_info.m_step, q_dot_tes_ch, m_dot_field_ch_est, T_cold_field_ch_est);
		}
		else
		{
			q_dot_tes_dc = q_dot_tes_ch = 0.0;
		}

		// Calculate maximum thermal power to power cycle for startup. This will be zero if power cycle is on.
		double q_dot_pc_su_max = mc_power_cycle.get_max_q_pc_startup();		//[MWt]
		
		// Can add the following code to simulate with no storage charge/discharge, but IDLE calcs
		//q_dot_tes_dc = q_dot_tes_ch = 0.0;
        


		// Get weather at this timestep. Should only be called once per timestep. (Except converged() function)
		mc_weather.timestep_call(mc_sim_info);

		// Get or set decision variables
		bool is_rec_su_allowed = true;
		bool is_pc_su_allowed = true;
		bool is_pc_sb_allowed = true;
		mc_sim_info.m_tou = 1;	    //[base 1] used ONLY by power cycle model for hybrid cooling - may also want to move this to controller

		// Get standby fraction and min operating fraction
			// Could eventually be a method in PC class...
		double cycle_sb_frac = m_cycle_sb_frac_des;				//[-]
			
			// *** If standby not allowed, then reset q_pc_sb = q_pc_min ?? *** 
                //or is this too confusing and not helpful enough?
		double q_pc_sb = cycle_sb_frac * m_cycle_q_dot_des;		//[MW]
		double q_pc_min = m_cycle_cutoff_frac * m_cycle_q_dot_des;	//[MW]
		double q_pc_max = m_cycle_max_frac * m_cycle_q_dot_des;		//[MW]
		double q_pc_target = q_pc_max;							//[MW]

		q_pc_target = f_turbine_tou * m_cycle_q_dot_des;	//[MW]

		
		// Solve collector/receiver at steady state with design inputs and weather to estimate output
		// May replace this call with a simple proxy model later...
		mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
		C_csp_collector_receiver::S_csp_cr_est_out est_out;
		mc_collector_receiver.estimates(mc_weather.ms_outputs,
			mc_cr_htf_state_in,
			est_out,
			mc_sim_info);
		double q_dot_cr_startup = est_out.m_q_startup_avail;
		double q_dot_cr_on = est_out.m_q_dot_avail;
		if( q_dot_cr_on <= m_q_dot_rec_on_min*1.01 )
		{
			q_dot_cr_on = 0.0;
		}

		// Optional rules for TOD Block Plant Control
		if( mc_tou.mc_dispatch_params.m_is_block_dispatch )
		{

			// Rule 1: if the sun sets (or does not rise) in __ [hours], then do not allow power cycle standby
				//double standby_time_buffer = 2.0;
			if( mc_tou.mc_dispatch_params.m_use_rule_1 &&
				(mc_sim_info.m_time + mc_tou.mc_dispatch_params.m_standby_off_buffer <= mc_weather.ms_outputs.m_time_rise || 
				mc_sim_info.m_time + mc_tou.mc_dispatch_params.m_standby_off_buffer >= mc_weather.ms_outputs.m_time_set) )
			{
				is_pc_sb_allowed = false;
			}

			// Rule 2:
			if( mc_tou.mc_dispatch_params.m_use_rule_2 &&
				((q_pc_target < q_pc_min && q_dot_tes_ch < m_q_dot_rec_des*mc_tou.mc_dispatch_params.m_q_dot_rec_des_mult) ||
				is_q_dot_pc_target_overwrite) )
			{
				// If overwrite was previously true, but now power cycle is off, set to false
				if( is_q_dot_pc_target_overwrite && 
				(pc_operating_state == C_csp_power_cycle::OFF || q_pc_target >= q_pc_min) )
				{
					is_q_dot_pc_target_overwrite = false;
				}
				else
				{
					is_q_dot_pc_target_overwrite = true;
				}

				if( is_q_dot_pc_target_overwrite )
				{
					q_pc_target = mc_tou.mc_dispatch_params.m_f_q_dot_pc_overwrite*m_cycle_q_dot_des;
				}
			}
		}



		// After rules, reset booleans if necessary
		if( q_pc_target < q_pc_min )
		{
			is_pc_su_allowed = false;
			is_pc_sb_allowed = false;
			q_pc_target = 0.0;
		}



        bool opt_complete = false;

        //Run dispatch optimization?
        if(mc_tou.mc_dispatch_params.m_dispatch_optimize)
        {

            //time to reoptimize
            int opt_horizon = mc_tou.mc_dispatch_params.m_optimize_horizon;



            if( (int)mc_sim_info.m_time % (int)(m_sim_step_size_baseline*mc_tou.mc_dispatch_params.m_optimize_frequency) == m_sim_step_size_baseline 
                && mc_sim_info.m_time < (8760 - opt_horizon)*mc_sim_info.m_step 
                && disp_time_last != mc_sim_info.m_time
                )
            {

                //message
                stringstream ss;
                ss << "Optimizing thermal energy dispatch profile for time window " 
                    << (int)(mc_sim_info.m_time/3600.) << " - " 
                    << (int)(mc_sim_info.m_time/3600.) + mc_tou.mc_dispatch_params.m_optimize_frequency;
                
                mc_csp_messages.add_message(C_csp_messages::NOTICE, ss.str());
			    if(! 
                    mf_callback(m_cdata, (float) calc_frac_current*100.f, &mc_csp_messages, mc_sim_info.m_time)
                    ) 
                    return;

                ss.flush();

                //get the new price signal
                dispatch.price_signal.clear();
                dispatch.price_signal.resize(opt_horizon, 1.);

                for(int t=0; t<opt_horizon; t++)
                {
                    mc_tou.call(mc_sim_info.m_time + t * mc_sim_info.m_step, mc_tou_outputs);
		            dispatch.price_signal.at(t) = mc_tou_outputs.m_price_mult;
                }

                //note the states of the power cycle and receiver
                dispatch.params.is_pb_operating0 = mc_power_cycle.get_operating_state() == 1;
                dispatch.params.is_pb_standby0 = mc_power_cycle.get_operating_state() == 2;
                dispatch.params.is_rec_operating0 = mc_collector_receiver.get_operating_state() == C_csp_collector_receiver::ON;
            
                //time
                dispatch.params.info_time = mc_sim_info.m_time; //s

                //Note the state of the thermal energy storage system
                double q_disch, m_dot_disch, T_tes_return;
                mc_tes.discharge_avail_est(m_T_htf_cold_des, mc_sim_info.m_step, q_disch, m_dot_disch, T_tes_return);
                dispatch.params.e_tes_init = q_disch * 1000. * mc_sim_info.m_step/3600. + dispatch.params.e_tes_min;        //kWh
		        if(dispatch.params.e_tes_init < dispatch.params.e_tes_min )
                    dispatch.params.e_tes_init = dispatch.params.e_tes_min;
                if(dispatch.params.e_tes_init > dispatch.params.e_tes_max )
                    dispatch.params.e_tes_init = dispatch.params.e_tes_max;

                //predict performance for the time horizon
                if( 
                    dispatch.predict_performance(mc_sim_info.m_time / mc_sim_info.m_step - 1, opt_horizon) 
                    )
                {
                    
                    //call the optimize method
                    opt_complete = dispatch.m_last_opt_successful = 
                        dispatch.optimize();

                    dispatch.m_current_read_step = 0;   //reset
                }

                //call again to go back to original state
                mc_tou.call(mc_sim_info.m_time, mc_tou_outputs);

            }

            //running from the optimized profile 
            if(
                dispatch.m_last_opt_successful 
                && dispatch.m_current_read_step < dispatch.outputs.q_pb_target.size()
                )
            {

                //update the learned field efficiency adjustment factor
                if(disp_qsf_last > 0.)
                {
                    double qopt_last = dispatch.outputs.q_sf_expected.at( dispatch.m_current_read_step )*1.e-3;     //mw

                    double etanew = disp_qsf_last / qopt_last;

                    disp_effadj_weight += disp_qsf_last;
                    //disp_effadj_count ++;

                    //double wfact = disp_effadj_weight / (double)disp_effadj_count;
                    
                    disp_qsf_effadj =+ (1. - etanew)/(min(disp_effadj_weight/disp_qsf_last, 5.));
                }

                //read in other values
                dispatch.m_current_read_step = (int)floor(mc_sim_info.m_time / m_sim_step_size_baseline - 0.99999) % mc_tou.mc_dispatch_params.m_optimize_frequency;

                is_rec_su_allowed = dispatch.outputs.rec_operation.at( dispatch.m_current_read_step );
                is_pc_sb_allowed = dispatch.outputs.pb_standby.at( dispatch.m_current_read_step );
                is_pc_su_allowed = dispatch.outputs.pb_operation.at( dispatch.m_current_read_step ) || is_pc_sb_allowed;

                q_pc_target = dispatch.outputs.q_pb_target.at( dispatch.m_current_read_step ) / 1000. ;
                
                //quality checks
                if(!is_pc_sb_allowed && (q_pc_target + 1.e-5 < q_pc_min))
                    is_pc_su_allowed = false;
                if(is_pc_sb_allowed)
                    q_pc_target = dispatch.params.q_pb_standby*1.e-3; 
                
                //q_pc_sb = dispatch.outputs.q_pb_standby.at( dispatch.m_current_read_step ) / 1000. ;

                disp_etapb_expect = dispatch.outputs.eta_pb_expected.at( dispatch.m_current_read_step ) 
                                    * m_cycle_eta_des * ( dispatch.outputs.pb_operation.at( dispatch.m_current_read_step ) ? 1. : 0. );
                disp_etasf_expect = dispatch.outputs.eta_sf_expected.at( dispatch.m_current_read_step );
                disp_qsf_expect = dispatch.outputs.q_sfavail_expected.at( dispatch.m_current_read_step )*1.e-3;
                disp_qsfprod_expect = dispatch.outputs.q_sf_expected.at( dispatch.m_current_read_step )*1.e-3;
                disp_qsfsu_expect = dispatch.outputs.q_rec_startup.at( dispatch.m_current_read_step )*1.e-3;
                disp_tes_expect = dispatch.outputs.tes_charge_expected.at( dispatch.m_current_read_step )*1.e-3;
                disp_qpbsu_expect = dispatch.outputs.q_pb_startup.at( dispatch.m_current_read_step )*1.e-3;
                disp_wpb_expect = dispatch.outputs.q_pb_target.at(dispatch.m_current_read_step ) * disp_etapb_expect *1.e-3;  
                disp_rev_expect = disp_wpb_expect * dispatch.price_signal.at( dispatch.m_current_read_step );

                //if( is_sim_timestep_complete ) // disp_time_last != mc_sim_info.m_time )
                //    dispatch.m_current_read_step++;

                if(dispatch.m_current_read_step > mc_tou.mc_dispatch_params.m_optimize_frequency)
                    throw C_csp_exception("Counter synchronization error in dispatch optimization routine.", "dispatch");
            }
            
            disp_time_last = mc_sim_info.m_time;
                        
        }

        /* 
        ------------ Controller/Solver iteration loop -------------
        */

		int operating_mode = ENTRY_MODE;
		bool are_models_converged = false;
		reset_hierarchy_logic();
		// Reset operating mode tracker		
		m_op_mode_tracking.resize(0);
					
		// Check if CR startup should be solved before entering hierarchy
		double q_dot_tes_dc_t_CR_su = 0.0;
		if( (cr_operating_state == C_csp_collector_receiver::OFF || cr_operating_state == C_csp_collector_receiver::STARTUP) &&
			q_dot_cr_startup &&
			is_rec_su_allowed && 
			m_is_tes )
		{
			// Set startup conditions
			mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

			mc_collector_receiver.startup(mc_weather.ms_outputs,
				mc_cr_htf_state_in,
				mc_cr_out_solver,
				mc_cr_out_report,
				mc_sim_info);

			// Check that startup happened
			// Because for all modes w/ startup, the startup occurs under the same conditions for any given timestep
			// then if startup fails here, it won't succeed downstream
			// so set is_rec_su_allowed = false
			if( mc_cr_out_solver.m_q_startup == 0.0 || mc_cr_out_solver.m_time_required_su != mc_cr_out_solver.m_time_required_su )
			{	// Collector/receiver can't produce useful energy
				
				is_rec_su_allowed = false;
			}
			else
			{
				double t_CR_su = mc_cr_out_solver.m_time_required_su;		//[s] Receiver model returns MIN(time required to completely startup, full timestep duration)

				// Use minimum of CR startup timestep and initial simulation timestep
				t_CR_su = fmin(t_CR_su, mc_sim_info.m_step);			//[s]

				// Predict estimated amount of discharage available with new timestep
				if( m_is_tes )
				{
					double m_dot_field_dc_est, T_hot_field_dc_est;	//[kg/s, K]
					m_dot_field_dc_est = T_hot_field_dc_est = std::numeric_limits<double>::quiet_NaN();
					mc_tes.discharge_avail_est(m_T_htf_cold_des, t_CR_su, q_dot_tes_dc_t_CR_su, m_dot_field_dc_est, T_hot_field_dc_est);
				}
				else
				{
					q_dot_tes_dc_t_CR_su = 0.0;
				}
			} 
		}

		while(!are_models_converged)		// Solve for correct operating mode and performance in following loop:
		{

			// Reset timestep info for iterations on the operating mode...
			mc_sim_info.m_step = step_ts_start;		//[s]
			mc_sim_info.m_time = time_ts_start;		//[s]
			is_sim_timestep_complete = true;

			if( (cr_operating_state == C_csp_collector_receiver::OFF || cr_operating_state == C_csp_collector_receiver::STARTUP)
				&& (pc_operating_state == C_csp_power_cycle::OFF || pc_operating_state == C_csp_power_cycle::STARTUP) )
			{	// At start of this timestep, are power cycle AND collector/receiver off?

				if( q_dot_cr_startup > 0.0 && is_rec_su_allowed &&
					m_is_CR_SU__PC_OFF__TES_OFF__AUX_OFF_avail )
				{	// Receiver startup is allowed and possible (will generate net energy)

					if( q_dot_tes_dc > 0.0 && is_pc_su_allowed &&
						m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail )
					{
						operating_mode = CR_SU__PC_SU__TES_DC__AUX_OFF;
					}
					else
					{
						operating_mode = CR_SU__PC_OFF__TES_OFF__AUX_OFF;
					}
				}
				else
				{
					if( q_dot_tes_dc > 0.0 && is_pc_su_allowed &&
						m_is_CR_OFF__PC_SU__TES_DC__AUX_OFF_avail )
					{
						operating_mode = CR_OFF__PC_SU__TES_DC__AUX_OFF;
					}
					else
					{
						operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					}
				}
			}	// End logic for CR_state == OFF or STARTUP    AND     PC_state == OFF or STARTUP

			else if( cr_operating_state == C_csp_collector_receiver::ON &&
				(pc_operating_state == C_csp_power_cycle::OFF || pc_operating_state == C_csp_power_cycle::STARTUP) )
			{
				if( q_dot_cr_on > 0.0 && is_rec_su_allowed )
				{	// Receiver is allowed to remain on, and it can produce useful energy. Now, need to find a home for it

					if( is_pc_su_allowed &&
						m_is_CR_ON__PC_SU__TES_OFF__AUX_OFF_avail )	// Can receiver output go to power cycle?
					{
						if( q_dot_tes_ch > 0.0 )
						{
							if( (q_dot_cr_on - q_dot_tes_ch)*(1.0+tol_mode_switching) > q_dot_pc_su_max &&
								m_is_CR_DF__PC_SU__TES_FULL__AUX_OFF_avail )
							{
								operating_mode = CR_DF__PC_SU__TES_FULL__AUX_OFF;								
							}
							else if( q_dot_cr_on*(1.0+tol_mode_switching) > q_dot_pc_su_max &&
										m_is_CR_ON__PC_SU__TES_CH__AUX_OFF_avail )
							{
								operating_mode = CR_ON__PC_SU__TES_CH__AUX_OFF;								
							}
							else
							{
								operating_mode = CR_ON__PC_SU__TES_OFF__AUX_OFF;
							}
						}
						else
						{
							if( q_dot_cr_on*(1.0+tol_mode_switching) > q_dot_pc_su_max &&
								m_is_CR_DF__PC_SU__TES_OFF__AUX_OFF_avail )
							{
								operating_mode = CR_DF__PC_SU__TES_OFF__AUX_OFF;								
							}
							else
							{
								operating_mode = CR_ON__PC_SU__TES_OFF__AUX_OFF;
							}							
						}						
					}
					else if( q_dot_tes_ch > 0.0 )
					{
						if( q_dot_cr_on*(1.0 - tol_mode_switching) < q_dot_tes_ch &&
							m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail )
						{
							operating_mode = CR_ON__PC_OFF__TES_CH__AUX_OFF;
						}
						else if(m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail)
						{
							operating_mode = CR_DF__PC_OFF__TES_FULL__AUX_OFF;														
						}
						else
						{
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}
					}
					else
					{
						operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					}
				}
				else if( q_dot_tes_dc && is_pc_su_allowed &&
					m_is_CR_OFF__PC_SU__TES_DC__AUX_OFF_avail )
				{	// Can power cycle startup using TES?

					operating_mode = CR_OFF__PC_SU__TES_DC__AUX_OFF;
				}
				else
				{
					operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
				}
			}

			else if( (cr_operating_state == C_csp_collector_receiver::OFF || cr_operating_state == C_csp_collector_receiver::STARTUP) &&
				(pc_operating_state == C_csp_power_cycle::ON || pc_operating_state == C_csp_power_cycle::STANDBY) )
			{
				if( q_dot_cr_startup > 0.0 && is_rec_su_allowed )
				{	// Receiver startup is allowed and possible (will generate net energy) - determine if power cycle can remain on

					if( is_pc_su_allowed || is_pc_sb_allowed )
					{					
						if( q_dot_tes_dc_t_CR_su*(1.0 + tol_mode_switching) > q_pc_target && is_pc_su_allowed &&
							m_is_CR_SU__PC_TARGET__TES_DC__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to matching target, the controller tries that mode

							operating_mode = CR_SU__PC_TARGET__TES_DC__AUX_OFF;
						}
						else if( q_dot_tes_dc_t_CR_su*(1.0 + tol_mode_switching) > q_pc_min && is_pc_su_allowed &&
							m_is_CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to reaching min fraction, the controller tries that mode

							operating_mode = CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF;
						}
						else if( is_pc_sb_allowed && q_dot_tes_dc_t_CR_su*(1.0 + tol_mode_switching) > q_pc_sb &&
							m_is_CR_SU__PC_SB__TES_DC__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to reaching min fraction, the controller tries that mode

							operating_mode = CR_SU__PC_SB__TES_DC__AUX_OFF;
						}
						else if( q_dot_tes_dc_t_CR_su > 0.0 && is_pc_su_allowed &&
							m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail )
						{
							operating_mode = CR_SU__PC_MIN__TES_EMPTY__AUX_OFF;														
						}
						else if( m_is_CR_SU__PC_OFF__TES_OFF__AUX_OFF_avail )
						{
							operating_mode = CR_SU__PC_OFF__TES_OFF__AUX_OFF;
						}
						else
						{
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}
					}	// End 'is_pc_su_allowed' logic
					else
					{	// power cycle startup/operation not allowed
						
						if( m_is_CR_SU__PC_OFF__TES_OFF__AUX_OFF_avail )
						{
							operating_mode = CR_SU__PC_OFF__TES_OFF__AUX_OFF;
						}
						else
						{
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}
					}
				}
				else	// Receiver remains OFF - determine if power cycle can remain on
				{
					if( is_pc_su_allowed || is_pc_sb_allowed )
					{
					
						if( q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_target && is_pc_su_allowed &&
							m_is_CR_OFF__PC_TARGET__TES_DC__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to matching target, the controller tries that mode

							operating_mode = CR_OFF__PC_TARGET__TES_DC__AUX_OFF;
						}
						else if( q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_min && is_pc_su_allowed &&
								m_is_CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to reaching min fraction, the controller tries that mode

							operating_mode = CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF;						
						}
						else if( is_pc_sb_allowed && q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_sb &&
								m_is_CR_OFF__PC_SB__TES_DC__AUX_OFF_avail )
						{	// Tolerance is applied so that if TES is *close* to reaching min fraction, the controller tries that mode

							operating_mode = CR_OFF__PC_SB__TES_DC__AUX_OFF;						
						}
						else if( q_dot_tes_dc > 0.0 && is_pc_su_allowed &&
								m_is_CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF_avail )
						{
							operating_mode = CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF;												
						}
						else
						{
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}
					}	// end logic on 'is_pc_su_allowed'
					else
					{

						operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					}
				}
			}

			else if( cr_operating_state == C_csp_collector_receiver::ON &&
				(pc_operating_state == C_csp_power_cycle::ON || pc_operating_state == C_csp_power_cycle::STANDBY) )
			{
				if( q_dot_cr_on > 0.0 && is_rec_su_allowed )
				{	// Receiver operation is allowed and possible - find a home for output

					if( is_pc_su_allowed || is_pc_sb_allowed )
					{
						if( q_dot_cr_on*(1.0 + tol_mode_switching) > q_pc_target && is_pc_su_allowed &&
							m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_LO_SIDE && m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_LO_SIDE )
						{	// The power cycle cannot accept the entire receiver output
							// Tolerance is applied so that if CR is *close* to reaching the PC target, the controller tries modes that fill TES

							// Is storage available to discharge to power cycle?
							if( q_dot_tes_ch > 0.0 )
							{
								// 1) Try to fill storage while hitting power cycle target
								if( (q_dot_cr_on - q_dot_tes_ch)*(1.0 - tol_mode_switching) < q_pc_target &&
									m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_HI_SIDE )
								{	// Storage can accept the remaining receiver output
									// Tolerance is applied so that if CR + TES is *close* to reaching PC target, the controller tries that mode

									operating_mode = CR_ON__PC_TARGET__TES_CH__AUX_OFF;
								}

								// 2) Try operating power cycle at maximum capacity
								// Assume we want to completely fill storage, so the power cycle operation should float to meet that condition
								else if( (q_dot_cr_on - q_dot_tes_ch)*(1.0 - tol_mode_switching) < q_pc_max &&
									m_is_CR_ON__PC_RM_HI__TES_FULL__AUX_OFF_avail )
								{	// Storage and the power cycle operating between target and max can accept the remaining receiver output
									// Tolerance is applied so that if CR + TES is *close* to reaching PC  max, the controller tries that mode

									operating_mode = CR_ON__PC_RM_HI__TES_FULL__AUX_OFF;
								}

								// 3) Try defocusing the CR and operating the power cycle at maximum capacity
								else if( m_is_CR_DF__PC_MAX__TES_FULL__AUX_OFF_avail )
								{
									
									operating_mode = CR_DF__PC_MAX__TES_FULL__AUX_OFF;
								}
								else
								{
									operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;;
								}
							}	// End if(q_dot_tes_ch > 0.0) logic

							else
							{	// No storage available for dispatch

								// 1) Try operating power cycle at maximum capacity
								if( q_dot_cr_on*(1.0 - tol_mode_switching) < q_pc_max &&
									m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_HI_SIDE )
								{	// Tolerance is applied so that if CR + TES is *close* to reaching PC  max, the controller tries that mode

									operating_mode = CR_ON__PC_RM_HI__TES_OFF__AUX_OFF;
								}
								else if( m_is_CR_DF__PC_MAX__TES_OFF__AUX_OFF_avail )
								{
									operating_mode = CR_DF__PC_MAX__TES_OFF__AUX_OFF;
								}
								else
								{
									operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
								}
							}	// End else 'no storage available for dispatch'
						}
						else
						{	// Power cycle is asking for more output than the receiver can supply

							if( q_dot_tes_dc > 0.0 )
							{	// Storage dispatch is available

								if( (q_dot_cr_on + q_dot_tes_dc)*(1.0 + tol_mode_switching) > q_pc_target && is_pc_su_allowed &&
									m_is_CR_ON__PC_TARGET__TES_DC__AUX_OFF_avail )
								{	// Storage can provide enough dispatch to reach power cycle target
									// Tolerance is applied so that if CR + TES is *close* to reaching PC target, the controller tries that mode

									operating_mode = CR_ON__PC_TARGET__TES_DC__AUX_OFF;									
								}
								else if( (q_dot_cr_on + q_dot_tes_dc)*(1.0 + tol_mode_switching) > q_pc_min && is_pc_su_allowed &&
									m_is_CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF_avail )
								{	// Storage can provide enough dispatch to at least meet power cycle minimum operation fraction
									// Run at highest possible PC fraction by dispatch all remaining storage
									// Tolerance is applied so that if CR + TES is *close* to reaching PC min, the controller tries that mode

									operating_mode = CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF;									
								}
								else if( q_dot_cr_on*(1.0 + tol_mode_switching) > q_pc_sb && is_pc_sb_allowed &&
									m_is_CR_ON__PC_SB__TES_OFF__AUX_OFF_avail && m_is_CR_ON__PC_SB__TES_CH__AUX_OFF_avail )
								{
									if( q_dot_tes_ch > 0.0 )
									{
										if( (q_dot_cr_on - q_dot_tes_ch)*(1.0 + tol_mode_switching) > q_pc_sb &&
											m_is_CR_ON__PC_SB__TES_FULL__AUX_OFF_avail )
										{	// Tolerance is applied so that if CR output is *close* to operating at standby AND completely filling storage, controller tries that mode

											operating_mode = CR_ON__PC_SB__TES_FULL__AUX_OFF;
										}
										else
										{
											operating_mode = CR_ON__PC_SB__TES_CH__AUX_OFF;
										}
									}
									else
									{
										// This could *technically* use defocus, but can argue the energy is just being thrown away in power cycle anyway
										operating_mode = CR_ON__PC_SB__TES_OFF__AUX_OFF;
									}
								}
								else if( (q_dot_cr_on + q_dot_tes_dc)*(1.0 + tol_mode_switching) > q_pc_sb && is_pc_sb_allowed &&
									m_is_CR_ON__PC_SB__TES_DC__AUX_OFF_avail )
								{
									operating_mode = CR_ON__PC_SB__TES_DC__AUX_OFF;
								}
								else if( is_pc_su_allowed &&
										m_is_CR_ON__PC_MIN__TES_EMPTY__AUX_OFF_avail )
								{
									operating_mode = CR_ON__PC_MIN__TES_EMPTY__AUX_OFF;
								}	
								else
								{
									operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
								}
							}
							else
							{	// Storage dispatch is not available

								// Can the power cycle operate at or above the minimum operation fraction?
								if( q_dot_cr_on*(1.0 + tol_mode_switching) > q_pc_min && is_pc_su_allowed &&
									m_is_CR_ON__PC_RM_LO__TES_OFF__AUX_OFF_avail )
								{	// Tolerance is applied so that if CR is *close* to reaching PC min, the controller tries that mode

									operating_mode = CR_ON__PC_RM_LO__TES_OFF__AUX_OFF;
								}
								else if( is_pc_sb_allowed && q_dot_cr_on*(1.0 + tol_mode_switching) > q_pc_sb &&
									m_is_CR_ON__PC_SB__TES_OFF__AUX_OFF_avail && m_is_CR_ON__PC_SB__TES_CH__AUX_OFF_avail )
								{	// Receiver can likely operate in standby
									// Tolerance is applied so that if CR is *close* to reaching PC standby, the controller tries that mode

									if( q_dot_cr_on*(1.0 + tol_mode_switching) > q_pc_sb &&
										m_is_CR_ON__PC_SB__TES_OFF__AUX_OFF_avail )
									{	// Tolerance is applied so that if CR output is *close* to reaching standby, the controller tries that mode

										if( q_dot_tes_ch > 0.0 )
										{
											if( (q_dot_cr_on - q_dot_tes_ch)*(1.0+tol_mode_switching) > q_pc_sb &&
												m_is_CR_ON__PC_SB__TES_FULL__AUX_OFF_avail )
											{	// Tolerance is applied so that if CR output is *close* to operating at standby AND completely filling storage, controller tries that mode

												operating_mode = CR_ON__PC_SB__TES_FULL__AUX_OFF;
											}
											else
											{
												operating_mode = CR_ON__PC_SB__TES_CH__AUX_OFF;
											}
										}
										else
										{
											// This could *technically* use defocus, but can argue the energy is just being thrown away in power cycle anyway
											operating_mode = CR_ON__PC_SB__TES_OFF__AUX_OFF;
										}
									}
								}
								else if( q_dot_tes_ch > 0.0 )
								{	// Charge storage with receiver output

									if( q_dot_cr_on*(1.0 - tol_mode_switching) < q_dot_tes_ch &&
										m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail )
									{	// Tolerance is applied so that if CR is *close* to being less than a full TES charge, the controller tries normal operation (no defocus)


										operating_mode = CR_ON__PC_OFF__TES_CH__AUX_OFF;
									}
									else if( m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail )
									{	// The CR output will overcharge storage, so it needs to defocus.
										// However, because the CR output is already part-load, it may be close to shutting down before defocus...

										operating_mode = CR_DF__PC_OFF__TES_FULL__AUX_OFF;
									}
									else
									{
										operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
									}
								}
								else
								{	// No home for receiver output, and not enough thermal power for power cycle

									operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
								}
							}	// End logic else 'storage dispatch not available'
						}	// End logic else 'power cycle requires more q_dot than receiver can supply'				
					}	// End logic if(is_rec_su_allowed)
					else
					{	// Power cycle startup is not allowed - see if receiver output can go to storage

						if( q_dot_tes_ch > 0.0 )
						{
							if( q_dot_cr_on*(1.0 - tol_mode_switching) < q_dot_tes_ch &&
								m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail )
							{
								operating_mode = CR_ON__PC_OFF__TES_CH__AUX_OFF;
							}
							else if( m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail )
							{
								operating_mode = CR_DF__PC_OFF__TES_FULL__AUX_OFF;
							}
							else
							{
								operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
							}
						}
						else
						{
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}

					}	// End logic else 'pc su is NOT allowed'		
				}	// End logic if(q_dot_cr_output > 0.0 && is_rec_su_allowed)

				else	// Receiver is off - determine if power cycle can remain on
				{
					if( is_pc_su_allowed || is_pc_sb_allowed )
					{
						if( q_dot_tes_dc > 0.0 )
						{	// Storage dispatch is available

							if( q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_target && is_pc_su_allowed &&
								m_is_CR_OFF__PC_TARGET__TES_DC__AUX_OFF_avail )
							{	// Storage can provide enough dispatch to reach power cycle target
								// Tolerance is applied so that if TES is *close* to reaching PC target, the controller tries that mode

								operating_mode = CR_OFF__PC_TARGET__TES_DC__AUX_OFF;
							}
							else if( q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_min && is_pc_su_allowed &&
									m_is_CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF_avail )
							{	// Storage can provide enough dispatch to at least meet power cycle minimum operation fraction
								// Run at highest possible PC fraction by dispatching all remaining storage
								// Tolerance is applied so that if CR + TES is *close* to reaching PC min, the controller tries that mode

								operating_mode = CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF;
							}
							else if( is_pc_sb_allowed && q_dot_tes_dc*(1.0 + tol_mode_switching) > q_pc_sb &&
									m_is_CR_OFF__PC_SB__TES_DC__AUX_OFF_avail )
							{	// Tolerance is applied so that if CR + TES is *close* to reaching standby, the controller tries that mode
								
								operating_mode = CR_OFF__PC_SB__TES_DC__AUX_OFF;
							}
							else if( is_pc_su_allowed && 
									m_is_CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF_avail )
							{	// If not enough thermal power to stay in standby, then run at min PC load until TES is fully discharged

								operating_mode = CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF;
							}
							else
							{
								operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
							}
						}	// End logic for if( q_dot_tes_dc > 0.0 )
						else
						{	// Storage dispatch is not available

							// No thermal power available to power cycle
							operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
						}
					}	// End logic if( is_pc_su_allowed )
					else
					{	// If neither receiver nor power cycle operation is allowed, then shut everything off

						operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					}
				}	// End logic for else 'receiver not on'

			}
			// End operating state mode for CR ON, PC ON/STANDBY


			// Store operating mode
			m_op_mode_tracking.push_back(operating_mode);


			switch( operating_mode )
			{
			case CR_DF__PC_MAX__TES_OFF__AUX_OFF:
			{
				// Running CR at full power results in too much thermal power to power cycle
				// Therefore, must defocus CR and operating PC at FULL POWER

				// Assuming here that partial defocus is allowed, so should always be able to reach full power to PC
				// If CR and PC for some reason don't solve or produce power, will shut down CR and PC

				
				// Get collector-receiver performance with no defocus
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				mc_cr_inputs.m_field_control = 1.0;						//[-] no defocusing for initial simulation
				mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;
				mc_collector_receiver.call(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_inputs,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);


				double defocus_guess_ini = std::numeric_limits<double>::quiet_NaN();
				if( mc_cr_out_solver.m_q_thermal > 0.0 )
				{
					// Controller hierarchy doesn't allow to go back to No Defocus and PC_RM, so check that defocus is <= 1
					defocus_guess_ini = fmin(1.0, q_pc_max / mc_cr_out_solver.m_q_thermal);
				}
				else
				{					
					// CR not producing power at design inlet temperature

					// Weird that controller chose Defocus operating mode, so report message and shut down CR and PC
					error_msg = util::format("At time = %lg the controller chose Defocus operating mode, but the collector/receiver"
						"did not produce power with the design inlet temperature. Controller will shut-down CR and PC",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

					//operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					are_models_converged = false;

					m_is_CR_DF__PC_MAX__TES_OFF__AUX_OFF_avail = false;

					break;
				}

				// Solve for inner loop: cr-pc convergence tolerances

				double tol_cr_pc_C = 2.0;
				double tol_cr_pc = tol_cr_pc_C / m_T_htf_cold_des;

				double relaxed_tol_cr_pc_mult = 5.0;
				double relaxed_tol_cr_pc = relaxed_tol_cr_pc_mult*tol_cr_pc;

				double defocus_guess = defocus_guess_ini;

				// Consider upper and lower bounds on defocus
				// Know that upper bound on defocus = 1, so can set that
				// Some combination of CR & PC methods (existing or otherwise) could *possibly* be used to guess lower, but let's assume we don't know it
				// Upper bound, error, and booleans
				double defocus_upper = 1.0;
				double y_defocus_uppper = std::numeric_limits<double>::quiet_NaN();
				bool is_upper_bound = true;
				bool is_upper_error = false;
				// Lower bound, error, and booleans
				double defocus_lower = std::numeric_limits<double>::quiet_NaN();
				double y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_lower_bound = false;
				bool is_lower_error = false;


				// Iterating on defocus until q_rec_thermal = q_pc_max AND CR_to_PC iteration solves successfully
				// Tolerance, in this case, probably doesn't need to be larger than inner nest tolerance
				double tol = 0.001;
				double relax_tol_mult = 5.0;
				double relax_tol = relax_tol_mult*tol;
				double bounds_tol = tol / 2.0;			// (upper - lower)/upper if no solution, when upper and lower get this close, make decision and get out

				// Defocus: 1 = full power, 0 = no power
				double diff_q_dot = 999.9*tol;			// (Rec - q_pc_max)/q_pc_max: (+) q_dot too large, decrease defocus, (-) q_dot too small, increase defocus fraction

				// CR-PC solver outputs are needed to determine whether defocus iteration solved within convergence
				double cr_pc_exit_tol = std::numeric_limits<double>::quiet_NaN();
				int cr_pc_exit_mode = -1;

				// Will be reset in while() loop under other outcomes
				int defocus_exit_mode = CONVERGED;		// Need this because have to use 'break' to exit the while() iteration loop

				int iter_defocus = 0;

				// Start iteration loop
				while( fabs(diff_q_dot) > tol || diff_q_dot != diff_q_dot )
				{
					iter_defocus++;			// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_defocus_bounds = defocus_upper - defocus_lower;
					if( diff_defocus_bounds / defocus_upper < bounds_tol )
					{
						if( diff_q_dot != diff_q_dot )
						{	// CR-PC aren't converging, so need to shut them down

							diff_q_dot = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = NO_SOLUTION;
							break;		// Get out of while()					
						}
						else
						{	// Poor convergence between power delivered to PC and power requested

							defocus_exit_mode = POOR_CONVERGENCE;
							break;		// Get out of while()
						}
					}

					// Subsequent iterations need to re-calculate defocus
					if( iter_defocus > 1 )
					{
						if( diff_q_dot != diff_q_dot )		// Check if solution was found
						{	// CR-PC model did not converge, so we don't know anything about this defocus
							// However, we know that we should now have an upper or lower bound (else code would have exited from logic below)
							// But, check that bounds exist, just to be careful
							if( !is_lower_bound || !is_upper_bound )
							{

								diff_q_dot = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							defocus_guess = 0.5*(defocus_lower + defocus_upper);
						}
						else if( diff_q_dot > 0.0 )		// q_dot was too high, decrease defocus
						{
							is_upper_bound = true;
							is_upper_error = true;
							defocus_upper = defocus_guess;		// Set upper bound
							y_defocus_uppper = diff_q_dot;		// Set upper convergence error

							if( is_lower_bound && is_lower_error )	// False-position method
							{
								defocus_guess = y_defocus_uppper / (y_defocus_uppper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_lower_bound )
							{
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmax(0.01, defocus_guess - 0.05);			// Could perhaps use last solution to make a smarter guess...
							}

						}
						else							// q_dot was too low, increase defocus 
						{
							is_lower_bound = true;
							is_lower_error = true;
							defocus_lower = defocus_guess;	// Set lower bound
							y_defocus_lower = diff_q_dot;	// Set lower convergence error

							if( is_upper_bound && is_upper_error )
							{
								defocus_guess = y_defocus_uppper / (y_defocus_uppper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_upper_bound )
							{	// should always have upper bound, but keep this framework for consistency...
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmin(1.0, defocus_guess + 0.05);
							}
						}
					}

					// Use defocus_guess and call method to solve CR-PC iteration
					cr_pc_exit_tol = std::numeric_limits<double>::quiet_NaN();
					solver_cr_to_pc_to_cr(defocus_guess, tol_cr_pc, cr_pc_exit_mode, cr_pc_exit_tol);

					// Process results from CR-PC iteration:
					if( cr_pc_exit_mode == NO_SOLUTION )
					{	// CR and PC did not produce power or did not solve

						if( iter_defocus == 1 )
						{	// If this happened on first iteration, assume guess is lower bound and try again?

							defocus_lower = defocus_guess;
							is_lower_bound = true;
							is_lower_error = false;
							diff_q_dot = std::numeric_limits<double>::quiet_NaN();
						}
						else
						{	// CR_PC has solved successfully at least once
							// And assume upper bound always solves
							// So assume that NO_SOLUTION corresponds to lower bound
							// So if a lower bound is already known, then nowhere to go
							if( is_lower_bound )
							{
								diff_q_dot = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							else
							{
								defocus_lower = defocus_guess;
								is_lower_bound = true;
								is_lower_error = false;
								diff_q_dot = std::numeric_limits<double>::quiet_NaN();
							}
						}	// end logic on iteration count for NO SOLUTION					
					}	// end code for NO SOLUTION
					else
					{
						// CR-PC iteration found a solution (though perhaps at POOR CONVERGENCE)
						// Calculate the difference between thermal power delivered to PC and thermal power requested
						// (Rec - q_pc_max)/q_pc_max: (+) q_dot too large, decrease defocus, (-) q_dot too small, increase defocus fraction

						diff_q_dot = (mc_cr_out_solver.m_q_thermal - q_pc_max) / q_pc_max;
					}

				}	// end iteration on CR defocus

				// Set Member Defocus Here
				m_defocus = defocus_guess;

				// Reached convergence on defocus, but it is *possible* that the CR-PC iteration only solved at POOR CONVERGENCE
				// Check here...?
				if( cr_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(cr_pc_exit_tol) > relaxed_tol_cr_pc )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						cr_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg the collector/receiver and power cycle solution only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, cr_pc_exit_mode);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						// update 'exit_mode' for following logic branches
						cr_pc_exit_mode = CONVERGED;
					}
				}
				if( defocus_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(diff_q_dot) > relax_tol )
					{	// Defocus did not converge within Relaxed Tolerance, shut off CR & PC

						// update defocus Exit Mode
						defocus_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg the defocus iteration only reached a convergence"
							" = &lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, cr_pc_exit_mode);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						// update Exit Mode
						defocus_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode == NO_SOLUTION || cr_pc_exit_mode == NO_SOLUTION )
				{
					error_msg = util::format("At time = %lg the controller chose Defocus operating mode, but the solver failed to reach convergence "
						"Controller will shut-down CR and PC",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

					// Shut down CR and PC
					//operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					are_models_converged = false;

					m_is_CR_DF__PC_MAX__TES_OFF__AUX_OFF_avail = false;

					break;
				}
				else if( defocus_exit_mode == CONVERGED && cr_pc_exit_mode == CONVERGED )
				{
					// If defocus solution has converged, then q_pc = q_pc_max, and shouldn't need to double-check anything...

					// Solve for idle storage
					if( m_is_tes )
					{
						mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);
					
						// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
						mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

						// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
						mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					}

					are_models_converged = true;
				}
				else
				{
					throw(C_csp_exception("Solver tried mode 'CR_DF__PC_MAX__TES_OFF__AUX_OFF' and did not receive useful exit instructions", "CSP Solver"));
				}

				break;		
			}

			//case CR_ON__PC_RM__TES_OFF__AUX_OFF:
			case CR_ON__PC_RM_LO__TES_OFF__AUX_OFF:
			case CR_ON__PC_RM_HI__TES_OFF__AUX_OFF:
			{
				// Collector/Receiver in ON, and only place for HTF to go is power cycle.
				// Therefore, power cycle must operate at Resource Match and use w/e is provided
				// (in cases with storage or field defocus, power cycle will try to hit an exact thermal input)
				// 'Failure Modes'
				// 1) Receiver provides too much power
				//		* Go to defocus
				// 2) Receiver cannot maintain minimum operation fraction
				//		* Go to power cycle standby or shutdown

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				double tol_C = 2.0;
				double tol = tol_C / m_T_htf_cold_des;

				double relaxed_tol_multiplier = 5.0;
				double relaxed_tol = relaxed_tol_multiplier*tol;

				// Call CR-PC_CR Solver
				int exit_mode = -1;
				double exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				double field_control = 1.0;
				solver_cr_to_pc_to_cr(field_control, tol, exit_mode, exit_tolerance);

				// If CR and PC models solved and produced power, but did not converge within tolerance,
				// check whether achieved convergence is "good enough" to report and continue
				if( exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC						

						// update 'exit_mode'
						exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg the collector/receiver and power cycle solution only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						// update 'exit_mode' for following logic branches
						exit_mode = CONVERGED;
					}
				}

				if( exit_mode == NO_SOLUTION )
				{	// Either CR & PC did not solve/produce power, or did not solve within Relaxed Tolerance: shut off CR and PC

					if( operating_mode == CR_ON__PC_RM_LO__TES_OFF__AUX_OFF )
					{
						m_is_CR_ON__PC_RM_LO__TES_OFF__AUX_OFF_avail = false;
						are_models_converged = false;
					}
					else if( operating_mode == CR_ON__PC_RM_HI__TES_OFF__AUX_OFF )
					{
						m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_LO_SIDE = false;
						are_models_converged = false;
					}
					else
					{
						throw(C_csp_exception("Operating mode not recognized", "CSP Solver"));
					}

					operating_mode = CR_OFF__PC_OFF__TES_OFF__AUX_OFF;
					are_models_converged = false;

					break;		// exits switch(operating mode)
				}

				else if( exit_mode == CONVERGED )
				{
					// If the CR and PC models converged, check whether the power cycle thermal input is within bounds

					if( operating_mode == CR_ON__PC_RM_LO__TES_OFF__AUX_OFF )
					{	// In this mode, the power cycle thermal input needs to be greater than the minimum power cycle fraction

						if( mc_cr_out_solver.m_q_thermal < q_pc_min )
						{
							m_is_CR_ON__PC_RM_LO__TES_OFF__AUX_OFF_avail = false;
							are_models_converged = false;
							break;						
						}

					}
					else if( operating_mode == CR_ON__PC_RM_HI__TES_OFF__AUX_OFF )
					{	// In this mode, the power cycle thermal input needs to be greater than the target cycle fraction
						// ... and less than the maximum cycle fraction

						if( mc_cr_out_solver.m_q_thermal > q_pc_max )
						{
							m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_HI_SIDE = false;
							are_models_converged = false;
							break;
						}
						else if( mc_cr_out_solver.m_q_thermal < q_pc_target )
						{
							m_is_CR_ON__PC_RM_HI__TES_OFF__AUX_OFF_avail_LO_SIDE = false;
							are_models_converged = false;
							break;
						}

					}
					else
					{
						throw(C_csp_exception("Operating mode not recognized", "CSP Solver"));
					}


					if(m_is_tes)
					{
						mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);
					
					
						// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
						mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

						// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
						mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					}

					
					are_models_converged = true;
					break;

				}
				else
				{
					throw(C_csp_exception("Solver tried mode 'CR_ON__PC_RM__TES_OFF__AUX_OFF' and did not receive exit instructions", "CSP Solver"));
				}


			}	// end case{} to allow compilation with local (w/r/t case) variables

				break;


			case CR_ON__PC_SB__TES_OFF__AUX_OFF:
				// Collector/receiver is ON
				// Power cycle is running in standby
				// During standby, assume power cycle HTF return temperature is constant and = m_T_htf_cold_des
				// so shouldn't need to iterate between CR and PC
				// Assume power cycle can remain in standby the entirety of the timestep

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// First, solve the CR. Again, we're assuming HTF inlet temperature is always = m_T_htf_cold_des
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				mc_cr_inputs.m_field_control = 1.0;						//[-] no defocusing for initial simulation
				mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

				mc_collector_receiver.call(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_inputs,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				if( mc_cr_out_solver.m_q_thermal < q_pc_sb )
				{	// Collector/receiver can't produce useful energy
					
					m_is_CR_ON__PC_SB__TES_OFF__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// If receiver is indeed producing power, then try power cycle at standby
				// Power cycle: STANDBY
				mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
				mc_pc_inputs.m_m_dot = mc_cr_out_solver.m_m_dot_salt_tot;	//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::STANDBY;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);

				if( m_is_tes )
				{
					mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);


					// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

					// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
				}

				are_models_converged = true;

				break;


			case CR_ON__PC_SU__TES_OFF__AUX_OFF:
				// Collector/receiver is ON
				// Startup power cycle
				// During startup, assume power cycle HTF return temperature is constant and = m_T_htf_cold_des
				// so shouldn't need to iterate between collector/receiver and power cycle
				// This will probably result in a local timestep shorter than the baseline simulation timestep (governed by weather file)

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// CR: ON
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				mc_cr_inputs.m_field_control = 1.0;						//[-] no defocusing for initial simulation
				mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

				mc_collector_receiver.call(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_inputs,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				if( mc_cr_out_solver.m_q_thermal == 0.0 )
				{	// Collector/receiver can't produce useful energy
					
					m_is_CR_ON__PC_SU__TES_OFF__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// If receiver IS producing energy, try starting up power cycle
				// Power Cycle: STARTUP
				mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
				mc_pc_inputs.m_m_dot = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);				


				// Check for new timestep
				step_local = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)
				if( step_local < mc_sim_info.m_step - step_tolerance )
				{
					is_sim_timestep_complete = false;
				}

				// Reset sim_info values
				if( !is_sim_timestep_complete )
				{
					mc_sim_info.m_step = step_local;						//[s]
					mc_sim_info.m_time = time_previous + step_local;		//[s]
				}

				if( m_is_tes )
				{
					mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);


					// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

					// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
				}

				// Compare q_dot_to_pc to q_dot_pc_su_max
				if( mc_cr_out_solver.m_q_thermal > q_dot_pc_su_max && q_dot_tes_ch > 0.0 )
				{
					error_msg = util::format("At time = %lg CR_ON__PC_SU__TES_OFF__AUX_OFF method converged to a power cycle"
						" thermal input greater than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);
				}

				are_models_converged = true;

				break;

			case CR_SU__PC_OFF__TES_OFF__AUX_OFF:
				// Run the collector/receiver under startup mode
				// **************
				// This will probably result in a local timestep shorter than the baseline simulation timestep (governed by weather file)

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

				mc_collector_receiver.startup(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				// Check that startup happened
				if( mc_cr_out_solver.m_q_startup == 0.0 )
				{	// Collector/receiver can't produce useful energy
					
					m_is_CR_SU__PC_OFF__TES_OFF__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// Check for new timestep
				step_local = mc_cr_out_solver.m_time_required_su;		//[s] Receiver model returns MIN(time required to completely startup, full timestep duration)
				if( step_local < mc_sim_info.m_step - step_tolerance )
				{
					is_sim_timestep_complete = false;
				}

				// Reset sim_info values
				if( !is_sim_timestep_complete )
				{
					mc_sim_info.m_step = step_local;						//[s]
					mc_sim_info.m_time = time_previous + step_local;		//[s]
				}

				// Power Cycle: OFF
				mc_pc_htf_state_in.m_temp = m_cycle_T_htf_hot_des - 273.15;	//[C]
				mc_pc_inputs.m_m_dot = 0.0;		//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::OFF;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);

				if( m_is_tes )
				{
					mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);


					// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

					// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
				}

				are_models_converged = true;

				break;

			case CR_OFF__PC_OFF__TES_OFF__AUX_OFF:
				// Solve all models as 'off' or 'idle'
				// Collector/receiver

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				
				mc_collector_receiver.off(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				// Power Cycle: OFF
				// HTF State
				mc_pc_htf_state_in.m_temp = m_cycle_T_htf_hot_des - 273.15;	//[C]
				mc_pc_inputs.m_m_dot = 0.0;		//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::OFF;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);

				if( m_is_tes )
				{
					mc_tes.idle(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, mc_tes_outputs);


					// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

					// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
				}

				are_models_converged = true;

				break;		// exit switch() after CR_OFF__PC_OFF__TES_OFF__AUX_OFF:

			case CR_OFF__PC_SU__TES_DC__AUX_OFF:
			{
				// Use thermal storage to startup power cycle
				// This solver iterates to find the thermal storage outlet temperature to the power cycle
				//    and the power cycle demand mass flow rate that reach system equilibrium

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				double step_tol = step_tolerance;		//[s]
				double step_pc_su = std::numeric_limits<double>::quiet_NaN();

				int exit_mode = CONVERGED;
				double T_pc_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				solver_pc_su_controlled__tes_dc(step_tol, 
					step_pc_su,
					exit_mode, T_pc_in_exit_tolerance);

				// Check exit mode
				if(exit_mode != CONVERGED)
				{
					are_models_converged = false;
					m_is_CR_OFF__PC_SU__TES_DC__AUX_OFF_avail = false;
					break;
				}

				// Check reported timestep against initial timestep
				if(step_pc_su < mc_sim_info.m_step - step_tolerance)
				{
					is_sim_timestep_complete = false;
					mc_sim_info.m_step = step_pc_su;
					mc_sim_info.m_time = time_previous + step_pc_su;
				}

				// Now run CR at 'OFF'
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				
				mc_collector_receiver.off(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				are_models_converged = true; 
			}

				break;

			case CR_ON__PC_OFF__TES_CH__AUX_OFF:
			{
				// Method to solve operating mode where the CR is on (under some fixed operating conditions, i.e. defocus)
				// and charging TES. No PC operating or AUX, so the output of the CR connects directly to TES

				// (the following is modeled after 'solver_cr_to_pc_to_cr'... perhaps this could be generalized in the future)

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// *****************************
				// *****************************

				// Need to step through and validate this operating mode!!!!

				// *****************************
				// *****************************

				// Guess the receiver inlet temperature = cold storage tank temperature
				double T_rec_in_guess_ini = mc_tes.get_cold_temp() - 273.15;	//[C]
				double T_rec_in_guess = T_rec_in_guess_ini;						//[C]

				// Initialize upper and lower bounds and booleans
				double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
				double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
				double y_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
				double y_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
				// Booleans for bounds and convergence error
				bool is_upper_bound = false;
				bool is_lower_bound = false;
				bool is_upper_error = false;
				bool is_lower_error = false;

				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				double diff_T_rec_in = 999.9*tol;

				int iter_T_rec_in = 0;

				int exit_mode = CONVERGED;
				double exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Start iteration loop
				while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
				{
					iter_T_rec_in++;		// First iteration = 1

					// Check if distance between bounds is "too small"
					//***********************
					// Check if distance between bounds is "too small"
					double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
					if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
					{
						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in
				
							exit_mode = NO_SOLUTION;
							exit_tolerance = diff_T_rec_in;
							return;
						}
						else
						{	// Models are producing power, but convergence errors are not within Tolerance

							exit_mode = POOR_CONVERGENCE;
							exit_tolerance = diff_T_rec_in;
							return;
						}
					}


					if(iter_T_rec_in > 1)
					{
						// Subsequent iterations need to re-calculate T_rec_in_guess
						//***********************
						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models did not solve such that a convergence error could be generated
							// However, we know that upper and lower bounds are set, so we can calculate a new guess via bisection method
							// but check that bounds exist, to be careful
							if( !is_lower_bound || !is_upper_bound )
							{
								exit_mode = NO_SOLUTION;
								exit_tolerance = diff_T_rec_in;
								return;
							}
							T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
						}
						else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
						{
							is_lower_bound = true;
							is_lower_error = true;
							T_rec_in_lower = T_rec_in_guess;		// Set lower bound
							y_rec_in_lower = diff_T_rec_in;				// Set lower convergence error

							if( is_upper_bound && is_upper_error )		// False-position method
							{
								T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;	//[C]
							}
							else if( is_upper_bound )						// Bisection method
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else				// Constant adjustment
							{
								T_rec_in_guess += 2.5;			//[C]
							}
						}
						else							// Guess receiver inlet temperature was too high
						{
							is_upper_bound = true;
							is_upper_error = true;
							T_rec_in_upper = T_rec_in_guess;		// Set upper bound
							y_rec_in_upper = diff_T_rec_in;				// Set upper convergence error

							if( is_lower_bound && is_lower_error )		// False-position method
							{
								T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;	//[C]
							}
							else if( is_lower_bound )
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else
							{
								T_rec_in_guess -= 2.5;			//[C] 
							}
						}
					}	// End iter > 1 loop to reset guess values or get out

					// Solve the collector-receiver model
					// CR ON
					mc_cr_htf_state_in.m_temp = T_rec_in_guess;					//[C]
					mc_cr_inputs.m_field_control = 1.0;							//[-] assuming no defocus
					mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;		// Receiver is operating and producing useful output

					mc_collector_receiver.call(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_inputs,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check if receiver is OFF or model didn't solve
					// ... if that is the case, then can't send useful information to TES and need to branch off here
					if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
					{
						
						// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
						if( iter_T_rec_in == 1 )
						{	
							exit_mode = NO_SOLUTION;
							exit_tolerance = diff_T_rec_in;
							break;
						}
						else
						{	// Set this T_rec_in_guess as either upper or lower bound, depending on which end of DESIGN temp it falls
							// Assumption here is that receiver solved at first guess temperature
							// and that the failure wouldn't occur between established bounds
							if( T_rec_in_guess < T_rec_in_guess_ini )
							{
								if( is_lower_bound || !is_upper_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = diff_T_rec_in;
									break;
								}
								T_rec_in_lower = T_rec_in_guess;
								is_lower_bound = true;
								is_lower_error = false;
								// At this point, both and upper and lower bound should exist, so can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}
							else
							{
								if( is_upper_bound || !is_lower_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = diff_T_rec_in;
									break;
								}
								T_rec_in_upper = T_rec_in_guess;
								is_upper_bound = true;
								is_upper_error = false;
								// At this point, both and upper and lower bound should exist, so can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}
						}															
					}	// End logic to handle CR off or failure

					// Now, solved TES charge with CR outputs
					double T_htf_tes_cold_out = std::numeric_limits<double>::quiet_NaN();
					bool tes_charge_success = mc_tes.charge(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry+273.15, mc_cr_out_solver.m_m_dot_salt_tot/3600.0, mc_cr_out_solver.m_T_salt_hot+273.15,
						T_htf_tes_cold_out, mc_tes_outputs);
					T_htf_tes_cold_out -= 273.15;		//[C] convert back from K

					// Set charge htf state
					mc_tes_ch_htf_state.m_m_dot = mc_cr_out_solver.m_m_dot_salt_tot;	//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;		//[C]
					mc_tes_ch_htf_state.m_temp_out = T_htf_tes_cold_out;			//[C]

					// Set discharge htf state
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K

					if( !tes_charge_success )
					{	// If receiver output overcharges storage during iteration, then assume we need some defocus and break loop
						// Receiver thermal output is *roughly* constant for varying receiver inlet temperatures,
						// ... and we don't want to try to throttle thermal power output by controlling this value

						exit_mode = KNOW_NEXT_MODE;
						exit_tolerance = std::numeric_limits<double>::quiet_NaN();						
						break;											
					}

					diff_T_rec_in = (T_htf_tes_cold_out - T_rec_in_guess) / T_rec_in_guess;
				}	// while () iteration on diff_T_rec_in

				if(exit_mode == KNOW_NEXT_MODE)
				{
					m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// Check exit_mode to determine how while loop exited
				// Reached convergence on defocus, but it is *possible* that the CR-PC iteration only solved at POOR CONVERGENCE
				if( exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg the collector/receiver and thermal storage charging method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						// update 'exit_mode' for following logic branches
						exit_mode = CONVERGED;
					}
				}
			
				if( exit_mode == NO_SOLUTION )
				{	// This mode did not solve, and did not provide enough information to try other operating mode. Shut plant off
		
					m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail = false;

					are_models_converged = false;
					break;								
				}
			
				if( exit_mode != CONVERGED )
				{	// All other options should be exhausted, so if not CONVERGED, something is wrong. Shut down plant

					m_is_CR_ON__PC_OFF__TES_CH__AUX_OFF_avail = false;

					are_models_converged = false;
					break;			
				}

				// If CR ON, TES CH solved, then solve powerblock OFF and get out
				// Power Cycle: OFF
				// HTF State
				mc_pc_htf_state_in.m_temp = m_cycle_T_htf_hot_des - 273.15;	//[C]
				mc_pc_inputs.m_m_dot = 0.0;		//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::OFF;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);

				are_models_converged = true;
			
			}	// End brace after code for this operating mode - brace required to avoid compiler error for local variables

				break;

			case CR_ON__PC_TARGET__TES_CH__AUX_OFF:
			{
				// CR is on (no defocus)
				// PC is on and hitting specified target
				// TES is charging

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Use 'general' solver for this operating mode
				// Define solver method inputs
				double q_dot_pc_fixed = q_pc_target;		//[MW]
				int power_cycle_mode = C_csp_power_cycle::ON;	//[-] power cycle is ON
				double field_control_in = 1.0;				//[-] No defocus 
				double tol = 1.0 / m_cycle_T_htf_hot_des;	//[-] Relative error corresponding to 1 C temperature difference

				// Initialize variables that solver will calculate
				double T_rec_in_exit_tolerance, q_pc_exit_tolerance;
				T_rec_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				int T_rec_in_exit_mode, q_pc_exit_mode;
				T_rec_in_exit_mode = q_pc_exit_mode = -1;

				// Call the solver
				solver_cr_on__pc_fixed__tes_ch(q_dot_pc_fixed, power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				// Define relaxed tolerance
				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				// Process solver results

				// Handle exit modes from outer and inner loops
					// If inner nest (power cycle thermal power iteration) causes exit, then we know CR solved with *some* inputs
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_TARGET__TES_CH__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
					}
				}

				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_TARGET__TES_CH__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
					}
				}
				
				if(q_pc_exit_mode == UNDER_TARGET_PC || q_pc_exit_mode == NO_SOLUTION || T_rec_in_exit_mode == NO_SOLUTION )
				{
					m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_LO_SIDE = false;
					are_models_converged = false;
					break;
				}
				else if( q_pc_exit_mode == OVER_TARGET_PC || T_rec_in_exit_mode == OVER_TARGET_PC )
				{
					m_is_CR_ON__PC_TARGET__TES_CH__AUX_OFF_avail_HI_SIDE = false;
					are_models_converged = false;
					break;
				}
								
				// If convergence was successful, finalize this timestep and get out
					// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			
			}
				break;

			case CR_ON__PC_TARGET__TES_DC__AUX_OFF:
			{
				// The collector receiver is on and returning hot HTF to the PC
				// TES is discharging hot HTF that is mixed with the CR HTF
				// to operating the PC at its target value

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Define arguments to solver method
				double q_dot_pc_fixed = q_pc_target;		//[MWt]
				int power_cycle_mode = C_csp_power_cycle::ON;
				double field_control_in = 1.0;
				
				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-] 

				int T_rec_in_exit_mode, q_pc_exit_mode;
				T_rec_in_exit_mode = q_pc_exit_mode = -1;

				double T_rec_in_exit_tolerance, q_pc_exit_tolerance;
				T_rec_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				solver_cr_on__pc_fixed__tes_dc(q_dot_pc_fixed, power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				// Handle exit modes from outer and inner loops
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						q_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
						
						q_pc_exit_mode = CONVERGED;
					}					
				}

				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( q_pc_exit_mode == OVER_TARGET_PC )
				{
					error_msg = util::format("At time = %lg CR_ON__PC_TARGET__TES_DC__AUX_OFF method converged to a power cycle"
					" thermal input greater than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);

					q_pc_exit_mode = CONVERGED;
				}

				if( q_pc_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_TARGET__TES_DC__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			
			}	// end outer bracket for case CR_ON__PC_OFF__TES_CH__AUX_OFF
				
				break;	// break case CR_ON__PC_OFF__TES_CH__AUX_OFF

			case CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF:
			{
				// The collector receiver is on and return hot HTF to the Pc
				// TES is discharging hot HTF that is then mixed with the CR HTF
				// The power cycle operates between its minimum operation fraction and target operation

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
				double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

				// Lower bound could be freeze protection temperature...
				double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
				double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
				double y_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
				double y_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
				// Booleans for bounds and convergence error
				bool is_upper_bound = false;
				bool is_lower_bound = false;
				bool is_upper_error = false;
				bool is_lower_error = false;

				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				double diff_T_rec_in = 999.9*tol;			// (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

				int iter_T_rec_in = 0;

				int exit_mode = CONVERGED;
				double exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Start iteration loop
				while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
				{
					iter_T_rec_in++;		// First iteration = 1

					// Check if distance between bounds is "too small"
					double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
					if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
					{
						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

							exit_mode = NO_SOLUTION;
							exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else
						{
							exit_mode = POOR_CONVERGENCE;
							exit_tolerance = diff_T_rec_in;
							break;
						}
					}

					// Subsequent iterations need to re-calculate T_in
					if( iter_T_rec_in > 1 )
					{	// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models did not solve such that a convergence error could be calculated
							// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
							// But, check that bounds exist
							if( !is_lower_bound || !is_upper_bound )
							{
								exit_mode = NO_SOLUTION;
								exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;
							}
							T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]						
						}
						else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
						{
							is_lower_bound = true;
							is_lower_error = true;
							T_rec_in_lower = T_rec_in_guess;		//[C]
							y_rec_in_lower = diff_T_rec_in;			//[-]

							if( is_upper_bound && is_upper_error )
							{
								T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
							}
							else if( is_upper_bound )
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
							}
							else
							{
								T_rec_in_guess += 2.5;			//[C]
							}
						}
						else
						{
							is_upper_bound = true;
							is_upper_error = true;
							T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
							y_rec_in_upper = diff_T_rec_in;			//[-]

							if( is_lower_bound && is_upper_bound )
							{
								T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
							}
							else if( is_lower_bound )
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else
							{
								T_rec_in_guess -= 2.5;		//[C]
							}
						}
					} // end logic to determine new T_rec_in

					// Solve the collector-receiver model
					mc_cr_htf_state_in.m_temp = T_rec_in_guess;		//[C]
					mc_cr_inputs.m_field_control = 1.0;				//[-] no defocusing for initial simulation
					mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

					mc_collector_receiver.call(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_inputs,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check if receiver is OFF or model didn't solve
					if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
					{
						// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
						if( iter_T_rec_in == 1 )
						{
							exit_mode = NO_SOLUTION;
							exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;	// exit while() on diff_T_rec_in
						}
						else
						{	// If collector-receiver model has solved with results previously in this loop, then try to find another guess value
							// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
							// Also, assume that if both upper and lower bounds exist, then can't generate a new guess because don't know which way to move
							if( T_rec_in_guess < T_rec_in_guess_ini )
							{	// If current guess value is less than the initial guess value, then:

								// If lower bound is already set OR upper bound is not set, can't generate a new guess value
								if( is_lower_bound || !is_upper_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;	// exit while() on diff_T_rec_in
								}

								// Else, set lower bound and flags and 'continue' to start of while()
								T_rec_in_lower = T_rec_in_guess;
								is_lower_bound = true;
								is_lower_error = false;

								// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}
							else
							{	// If current guess value is greater than initial value, then:

								// If upper bound is already set OR lower bound is not set, can't generate a new guess value
								if( is_upper_bound || !is_lower_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;	// exit while() on diff_T_rec_in
								}

								// Else, set upper bound and flags and 'continue' to start of while()
								T_rec_in_upper = T_rec_in_guess;
								is_upper_bound = true;
								is_upper_error = false;

								// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}

						}	// end else on if(iter_T_rec_in == 1)

					}	// end logic to determine path if receiver is off or did not solve
				
					// Now solve TES full discharge
					double T_tes_htf_hot, m_dot_tes_dc;
					mc_tes.discharge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_rec_in_guess+273.15, T_tes_htf_hot, m_dot_tes_dc, mc_tes_outputs);
					T_tes_htf_hot -= 273.15;	//[C] convert from K
					m_dot_tes_dc *= 3600.0;		//[kg/hr] convert from kg/s

					// HTF discharging state
					mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc;			//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = T_rec_in_guess;		//[C]
					mc_tes_dc_htf_state.m_temp_out = T_tes_htf_hot;		//[C]

					// HTF charging state
					mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;//[C] convert from K

					double m_dot_pc = mc_cr_out_solver.m_m_dot_salt_tot + m_dot_tes_dc;		//[kg/hr]

					double T_pc_htf_in = (m_dot_tes_dc*T_tes_htf_hot + mc_cr_out_solver.m_m_dot_salt_tot*mc_cr_out_solver.m_T_salt_hot) / (m_dot_pc);	//[C]

					// Solve power cycle model
					mc_pc_htf_state_in.m_temp = T_pc_htf_in;		//[C]
					mc_pc_inputs.m_m_dot = m_dot_pc;				//[kg/hr]

					// Inputs
					mc_pc_inputs.m_standby_control = C_csp_power_cycle::ON;

					// Performance Call
					mc_power_cycle.call(mc_weather.ms_outputs,
						mc_pc_htf_state_in,
						mc_pc_inputs,
						mc_pc_out_solver,
						mc_pc_out_report,
						mc_sim_info);

					// Check that power cycle is producing power or model didn't solve
					if(mc_pc_out_solver.m_P_cycle == 0.0)
					{
						// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
						// Go to Receiver OFF power cycle OFF
						if( iter_T_rec_in == 1 )
						{
							exit_mode = NO_SOLUTION;
							exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else
						{
							// Set T_rec_in_guess as either upper or lower bound, depending on which end of DESIGN temp it falls
							// Assumption here is that receiver solved at first guess temperature
							// But if both upper and lower bounds are established, then don't have a direction to try
							// So this reguess is only good for 1 PC failure on the 2nd iteration...
							if( T_rec_in_guess < T_rec_in_guess_ini )
							{
								if( is_lower_bound || !is_upper_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;
								}
								T_rec_in_lower = T_rec_in_guess;
								is_lower_bound = true;
								is_lower_error = false;
								// At this point, both and upper and lower bound should exist, so can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
							}
							else
							{
								if( is_upper_bound || !is_lower_bound )
								{
									exit_mode = NO_SOLUTION;
									exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;
								}
								T_rec_in_upper = T_rec_in_guess;
								is_upper_bound = true;
								is_upper_error = false;
								// At this point, both and upper and lower bound should exist, so can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
							}
						}
					}	// End logic for PC off or not producing power

					// Get HTF temperature out from the power cycle and compare to guess value (T_rec_in)
					double T_rec_in_calc = mc_pc_out_solver.m_T_htf_cold;	//[C]

					diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;		//[-]
				}

				// *********************************
				// Check PC q_dot is >= MIN!!!!!!!!
				// *********************************
				if(mc_pc_out_solver.m_q_dot_htf < q_pc_min)
				{
					exit_mode = NO_SOLUTION;
				}

				if(mc_pc_out_solver.m_q_dot_htf > q_pc_target)
				{
					error_msg = util::format("At time = %lg CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF method converged to a power cycle"
						" thermal input greater than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);
				}

				// Handle exit modes
				if( exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						// update 'exit_mode'
						exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						exit_mode = CONVERGED;
					}
				}
				
				if(exit_mode != CONVERGED)
				{
					m_is_CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;

			}

				break;	// break case CR_ON__PC_RM_LO__TES_EMPTY__AUX_OFF
			

			case CR_DF__PC_OFF__TES_FULL__AUX_OFF:
			{
				// Running the CR at full power results in too much thermal power to TES
				// Power cycle operation is either not allowed or not possible under the timestep conditions

				// Assuming here that partial defocus is allowed, so should always be able to reach full power to PC
				
				// Get collector-receiver performance with no defocus
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				mc_cr_inputs.m_field_control = 1.0;						//[-] no defocusing for initial simulation
				mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;
				mc_collector_receiver.call(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_inputs,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				
				if( mc_cr_out_solver.m_q_thermal == 0.0 )
				{
					// CR not producing power at design inlet temperature

					// Weird that controller chose Defocus operating mode, so report message and shut down CR and PC
					error_msg = util::format("At time = %lg the controller chose CR_DF__PC_OFF__TES_FULL__AUX_OFF, but the collector/receiver"
						"did not produce power with the design inlet temperature. Controller will shut-down CR and PC",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

					are_models_converged = false;

					m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail = false;

					break;
				}


				// Set up iteration to find defocus that results in completely filled storage
				// Upper bound, error, and booleans
				double defocus_upper = 1.0;
				double y_defocus_uppper = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_upper_bound = true;
				bool is_defocus_upper_error = false;
				// Lower bound, error, and booleans
				double defocus_lower = std::numeric_limits<double>::quiet_NaN();
				double y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_lower_bound = false;
				bool is_defocus_lower_error = false;
				
				double tol = 0.001;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				// Controller hierarchy doesn't allow to go back to No Defocus and PC_RM, so check that defocus is <= 1
					// *********************************************
					// **** Need a better defocus estimate here ****
					// *********************************************
				// 1) estimate FULL charge m_dot over timestep
				// 2) compare to design point receiver output (rough estimate because of DNI, field eta, etc...)
				// Solve TES for FULL charge
				double T_htf_tes_out_est, m_dot_htf_tes_out_est;
				T_htf_tes_out_est = m_dot_htf_tes_out_est = std::numeric_limits<double>::quiet_NaN();
				mc_tes.charge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_T_htf_cold_des,
					T_htf_tes_out_est, m_dot_htf_tes_out_est, mc_tes_outputs);
				m_dot_htf_tes_out_est *= 3600.0;

				double defocus_guess_ini = fmin(1.0, m_dot_htf_tes_out_est / mc_cr_out_solver.m_m_dot_salt_tot);				
				double defocus_guess = defocus_guess_ini;

				// Defocus: 1 = full power, 0 = no power
				double diff_m_dot = 999.9*tol;			// (m_dot_rec - m_dot_tes)/m_dot_tes: (+) q_dot too large, decrease defocus, (-) q_dot too small, increase defocus fraction

				int defocus_exit_mode = CONVERGED;
				double defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int iter_defocus = 0;

				// Exit information for outer loop on T_rec_in (needs to be accessible in this scope)
				int T_rec_in_exit_mode = CONVERGED;
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Iterate on defocus to fully charge storage
				while( fabs(diff_m_dot) > tol || diff_m_dot != diff_m_dot )
				{
					iter_defocus++;			// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_defocus_bounds = defocus_upper - defocus_lower;
					if( diff_defocus_bounds / defocus_upper < tol / 2.0 )
					{
						if( diff_m_dot != diff_m_dot )
						{	// CR-PC aren't converging, so need to shut them down

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = NO_SOLUTION;
							break;		// Get out of while()					
						}
						else
						{	// Poor convergence between power delivered to PC and power requested

							defocus_exit_tolerance = diff_m_dot;
							defocus_exit_mode = POOR_CONVERGENCE;
							break;		// Get out of while()
						}
					}

					// Subsequent iterations need to re-calculate defocus
					if( iter_defocus > 1 )
					{
						if( diff_m_dot != diff_m_dot )		// Check if solution was found
						{	// CR-PC model did not converge, so we don't know anything about this defocus
							// However, we know that we should now have an upper or lower bound (else code would have exited from logic below)
							// But, check that bounds exist, just to be careful
							if( !is_defocus_lower_bound || !is_defocus_upper_bound )
							{

								defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							defocus_guess = 0.5*(defocus_lower + defocus_upper);
						}
						else if( diff_m_dot > 0.0 )		// q_dot was too high, decrease defocus
						{
							is_defocus_upper_bound = true;
							is_defocus_upper_error = true;
							defocus_upper = defocus_guess;		// Set upper bound
							y_defocus_uppper = diff_m_dot;		// Set upper convergence error

							if( is_defocus_lower_bound && is_defocus_lower_error )	// False-position method
							{
								defocus_guess = y_defocus_uppper / (y_defocus_uppper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_lower_bound )
							{
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmax(0.01, defocus_guess - 0.05);			// Could perhaps use last solution to make a smarter guess...
							}

						}
						else							// q_dot was too low, increase defocus 
						{
							is_defocus_lower_bound = true;
							is_defocus_lower_error = true;
							defocus_lower = defocus_guess;	// Set lower bound
							y_defocus_lower = diff_m_dot;	// Set lower convergence error

							if( is_defocus_upper_bound && is_defocus_upper_error )
							{
								defocus_guess = y_defocus_uppper / (y_defocus_uppper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_upper_bound )
							{	// should always have upper bound, but keep this framework for consistency...
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmin(1.0, defocus_guess + 0.05);
							}
						}
					}


					// Inner loop: iterate on T_rec_in until it matches charge_full T_cold
						// Maybe want initial guess as function of cold tank temperature as it shouldn't be getting any warmer?
						//double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
						// ok...
					double T_rec_in_guess_ini = mc_tes.get_cold_temp() - 273.15;
					double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

					// Lower bound could be freeze protection temperature...
					double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					// Booleans for bounds and convergence error
					bool is_T_rec_upper_bound = false;
					bool is_T_rec_lower_bound = false;
					bool is_T_rec_upper_error = false;
					bool is_T_rec_lower_error = false;
					
					double tol_T_rec_in = 0.9*tol;

					double diff_T_rec_in = 999.9*tol_T_rec_in;

					int iter_T_rec_in = 0;

					// Exit information for outer loop on T_rec_in
					T_rec_in_exit_mode = CONVERGED;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

					// Start iteration on T_rec_in
					while( fabs(diff_T_rec_in) > tol_T_rec_in || diff_T_rec_in != diff_T_rec_in )
					{
						iter_T_rec_in++;
						
						// Check if distance between bounds is "too small"
						double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
						if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
						{
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;	// exit while() on diff_T_rec_in and enter while() on defocus
							}
							else
							{
								T_rec_in_exit_mode = POOR_CONVERGENCE;
								T_rec_in_exit_tolerance = diff_T_rec_in;
								break;	// exit while() on diff_T_rec_in and enter while() on defocus
							}
						}
				
						// Subsequent iterations need to re-calculate T_in
						if( iter_T_rec_in > 1 )
						{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models did not solve such that a convergence error could be calculated
								// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
								// But, check that bounds exist
								if( !is_T_rec_lower_bound || !is_T_rec_upper_bound )
								{
									T_rec_in_exit_mode = NO_SOLUTION;
									T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;	// exit while() on diff_T_rec_in and enter while() on defocus
								}
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
							{
								is_T_rec_lower_bound = true;
								is_T_rec_lower_error = true;
								T_rec_in_lower = T_rec_in_guess;		//[C]
								y_T_rec_in_lower = diff_T_rec_in;			//[-]

								if( is_T_rec_upper_bound && is_T_rec_upper_error )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_T_rec_upper_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
								}
								else
								{
									T_rec_in_guess += 2.5;			//[C]
								}
							}
							else
							{
								is_T_rec_upper_bound = true;
								is_T_rec_upper_error = true;
								T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
								y_T_rec_in_upper = diff_T_rec_in;			//[-]

								if( is_T_rec_lower_bound && is_T_rec_upper_bound )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_T_rec_lower_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
								}
								else
								{
									T_rec_in_guess -= 2.5;		//[C]
								}
							}
						}
				
						// Solve the collector-receiver, *using defocus from outer loop*
						// ... and T_rec_in_guess from this loop
						// CR ON
						mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
						mc_cr_inputs.m_field_control = defocus_guess;	//[-] no defocusing for initial simulation
						mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

						mc_collector_receiver.call(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_inputs,
							mc_cr_out_solver,
							mc_cr_out_report,
							mc_sim_info);

						// Check if receiver is OFF or model didn't solve
						if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
						{
							// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
							if( iter_T_rec_in == 1 )
							{
								T_rec_in_exit_mode = REC_IS_OFF;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;  // exit while() on diff_T_rec_in and enter while() on defocus
							}
							else
							{	// If collector-receiver model has solved with results previously, then try to find another guess value
								// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
								// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
								if( T_rec_in_guess < T_rec_in_guess_ini )
								{	// If current guess value is less than initial value, then:

									// If lower bound is already set OR upper bound is not set, can't generate new guess
									if( is_T_rec_lower_bound || !is_T_rec_upper_bound )
									{
										T_rec_in_exit_mode = REC_IS_OFF;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in and enter while() on defocus
									}

									T_rec_in_lower = T_rec_in_guess;
									is_T_rec_lower_bound = true;
									is_T_rec_lower_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
								else
								{	// If current guess value is greater than initial value, then:

									// If upper bound is already set OR lower bound is not set, can't generate new guess
									if( is_T_rec_upper_bound || !is_T_rec_lower_bound )
									{
										T_rec_in_exit_mode = REC_IS_OFF;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in and enter while() on defocus
									}

									T_rec_in_upper = T_rec_in_guess;
									is_T_rec_upper_bound = true;
									is_T_rec_upper_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
							}	// end else on if(iter_T_rec_in == 1)					
						}	// end logic to determine path if receiver is off or did not solve

						// Get receiver HTF outlet temperature
						double T_htf_rec_out = mc_cr_out_solver.m_T_salt_hot;		//[C]

						// Solve TES for FULL charge
						double T_htf_tes_out, m_dot_htf_tes_out;
						T_htf_tes_out = m_dot_htf_tes_out = std::numeric_limits<double>::quiet_NaN();
						mc_tes.charge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_htf_rec_out + 273.15,
							T_htf_tes_out, m_dot_htf_tes_out, mc_tes_outputs);

						T_htf_tes_out -= 273.15;		//[C] convert from K
						m_dot_htf_tes_out *= 3600.0;	//[kg/hr] convert from kg/s

						// HTF charging state
						mc_tes_ch_htf_state.m_m_dot = m_dot_htf_tes_out;				//[kg/hr]
						mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;		//[C]
						mc_tes_ch_htf_state.m_temp_out = T_htf_tes_out;					//[C] convert from K

						// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
						mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K

						diff_T_rec_in = (T_htf_tes_out - T_rec_in_guess) / T_rec_in_guess;		//[-]

					}	// end while() on T_rec_in

					// Check receiver exit codes...
					if( T_rec_in_exit_mode == NO_SOLUTION )
					{
						break;
					}

					if( T_rec_in_exit_mode == REC_IS_OFF )
					{
						// Assume this means we've hit the lower bound on defocus
						is_defocus_lower_bound = true;
						is_defocus_lower_error = false;
						defocus_lower = defocus_guess;
						y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
						diff_m_dot = std::numeric_limits<double>::quiet_NaN();

						continue;
					}

					// Calculate defocus error
					// (m_dot_rec - m_dot_tes)/m_dot_tes: (+) q_dot too large, decrease defocus, (-) q_dot too small, increase defocus fraction
					diff_m_dot = (mc_cr_out_solver.m_m_dot_salt_tot - mc_tes_ch_htf_state.m_m_dot) / mc_tes_ch_htf_state.m_m_dot;

				}	// end while() on defocus

				// Handle exit modes
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_OFF__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(defocus_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						defocus_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_OFF__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, defocus_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						defocus_exit_mode = CONVERGED;
					}				
				}

				if( defocus_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					are_models_converged = false;

					m_is_CR_DF__PC_OFF__TES_FULL__AUX_OFF_avail = false;

					break;				
				}

				// Set member data defocus
				m_defocus = defocus_guess;

				// Need to call power cycle at 'OFF'
				// HTF State
				mc_pc_htf_state_in.m_temp = m_cycle_T_htf_hot_des - 273.15;	//[C]
				mc_pc_inputs.m_m_dot = 0.0;		//[kg/hr] no mass flow rate to power cycle
				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::OFF;
				//mc_pc_inputs.m_tou = tou_timestep;
				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;

			}
				break;	// break case CR_DF__PC_OFF__TES_FULL__AUX_OFF


			case CR_OFF__PC_SB__TES_DC__AUX_OFF:
			case CR_SU__PC_SB__TES_DC__AUX_OFF:
			{
				// Collector-receiver is OFF
				// Power cycle is running in standby with thermal power input from TES discharge

				// Assume that power cycle HTF return temperature is constant and = m_T_htf_cold_des
				// Assume power cycle can remain in standby the entirety of the timestep

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Solve CR in 'OFF' or 'SU' mode depending on Operating Mode

				if( operating_mode == CR_OFF__PC_SB__TES_DC__AUX_OFF )
				{
					// Run CR at 'OFF'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
					
					mc_collector_receiver.off(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

				}
				else if( operating_mode == CR_SU__PC_SB__TES_DC__AUX_OFF )
				{
					// Run CR at 'Start Up'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

					mc_collector_receiver.startup(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check that startup happened
					if( mc_cr_out_solver.m_q_startup == 0.0 )
					{	// Collector/receiver can't produce useful energy

						m_is_CR_SU__PC_SB__TES_DC__AUX_OFF_avail = false;
						are_models_converged = false;
						break;
					}

					// Check for new timestep
					step_local = mc_cr_out_solver.m_time_required_su;		//[s] Receiver model returns MIN(time required to completely startup, full timestep duration)
					if( step_local < mc_sim_info.m_step - step_tolerance )
					{
						is_sim_timestep_complete = false;
					}

					// Reset sim_info values
					if( !is_sim_timestep_complete )
					{
						mc_sim_info.m_step = step_local;						//[s]
						mc_sim_info.m_time = time_previous + step_local;		//[s]
					}

				}
					
				// First, get the maximum possible max flow rate from TES discharge
				double T_htf_hot_out, m_dot_htf_max;
				T_htf_hot_out = m_dot_htf_max = std::numeric_limits<double>::quiet_NaN();
				mc_tes.discharge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry+273.15, m_T_htf_cold_des, T_htf_hot_out, m_dot_htf_max, mc_tes_outputs);

				// Get solved TES HTF info and determine if enough q_dot can be supplied to the power cycle to maintain standby
				double q_dot_dc_max = mc_tes_outputs.m_q_dot_dc_to_htf;		//[MW]

				if(q_dot_dc_max < q_pc_sb)
				{
					if( operating_mode == CR_OFF__PC_SB__TES_DC__AUX_OFF )
					{
						m_is_CR_OFF__PC_SB__TES_DC__AUX_OFF_avail = false;
						
					}
					else if( operating_mode == CR_SU__PC_SB__TES_DC__AUX_OFF )
					{
						m_is_CR_SU__PC_SB__TES_DC__AUX_OFF_avail = false;
					}
					
					are_models_converged = false;
					break;
				}


				// Set up iteration on discharge mass flow rate
				double tol = 0.001;		//[-] tolerance on convergence of PC required/delivered standby thermal power

				double diff_q_dot = 999.9*tol;	//[-] (q_dot_calc - q_dot_sb)/q_dot_sb

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]
				double bounds_tol = tol / 2.0;				

				double m_dot_upper = m_dot_htf_max;							//[kg/s]
				double y_m_dot_upper = (q_dot_dc_max - q_pc_sb)/q_pc_sb;	//[-]
				bool is_upper_bound = true;
				bool is_upper_error = true;

				double m_dot_lower = std::numeric_limits<double>::quiet_NaN();
				double y_m_dot_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_lower_bound = false;
				bool is_lower_error = false;

				double m_dot_dc_guess = m_dot_htf_max*(q_pc_sb/q_dot_dc_max);	//[kg/s]

				int q_dot_exit_mode = CONVERGED;
				int iter_q_dot = 0;
				double exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Start iteration on discharge mass flow rate
				while( fabs(diff_q_dot) > tol || diff_q_dot != diff_q_dot )
				{
					iter_q_dot++;		// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_q_dot_bounds = (m_dot_upper - m_dot_lower)/m_dot_upper;		//[-]
					if( diff_q_dot_bounds < bounds_tol )
					{
						if( diff_q_dot != diff_q_dot )
						{	// Unable to solve TES model...

							exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							q_dot_exit_mode = NO_SOLUTION;
							break;		// get out of while()
						}
						else
						{	// Poor convergence between power discharged from TES and PC standby

							exit_tolerance = diff_q_dot;
							q_dot_exit_mode = POOR_CONVERGENCE;
							break;		// get out of while()
						}
					}

					// Subsequent iterations need to recalculate defocus
					if(iter_q_dot > 1)
					{
						if( diff_q_dot != diff_q_dot )		// Check if solution was found during previous iteration
						{	// TES did not solve, so don't know how result of previous m_dot_guess
							// However, we can check whether we can generate a new guess using established bounds and the bisection method

							if( !is_lower_bound || !is_upper_bound )
							{
								exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								q_dot_exit_mode = NO_SOLUTION;
								break;	// get out of while()
							}
							m_dot_dc_guess = 0.5*(m_dot_lower + m_dot_upper);
						}
						else if( diff_q_dot > 0.0 )		// q_dot calculated was too large, decrease mass flow rate
						{
							is_upper_bound = true;
							is_upper_error = true;
							m_dot_upper = m_dot_dc_guess;
							y_m_dot_upper = diff_q_dot;

							if( is_lower_bound && is_lower_error )	// False-position method
							{
								m_dot_dc_guess = y_m_dot_upper/(y_m_dot_upper-y_m_dot_lower)*(m_dot_lower-m_dot_upper) + m_dot_upper;
							}
							else if( is_lower_bound )
							{
								m_dot_dc_guess = 0.5*(m_dot_lower + m_dot_upper);
							}
							else
							{
								m_dot_dc_guess *= 0.5;
							}
						}
						else		// q_dot calculated was too small, decrease mass flow rate
						{
							is_lower_bound = true;
							is_upper_error = true;
							m_dot_lower = m_dot_dc_guess;
							y_m_dot_lower = diff_q_dot;

							if( is_upper_bound && is_upper_error )	// False-position method
							{
								m_dot_dc_guess = y_m_dot_upper / (y_m_dot_upper - y_m_dot_lower)*(m_dot_lower - m_dot_upper) + m_dot_upper;
							}
							else if( is_upper_bound )
							{
								m_dot_dc_guess = 0.5*(m_dot_lower + m_dot_upper);
							}
							else
							{
								m_dot_dc_guess = 0.5*(m_dot_lower + m_dot_htf_max);
							}
						}
					}	

					// Solve TES discharge
					bool is_tes_success = mc_tes.discharge(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_dc_guess, m_T_htf_cold_des, T_htf_hot_out, mc_tes_outputs);
					
					// Check that TES solved successfully
					if( !is_tes_success )
					{	// TES did not solve with this iteration's mass flow rate

						// No explanation why TES failed, so get out of while() loop
						exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						q_dot_exit_mode = NO_SOLUTION;
						break;	// get out of while()
					}
					
					// Set TES HTF states (this needs to be less bulky...)
					// HTF discharging state
					mc_tes_dc_htf_state.m_m_dot = m_dot_dc_guess*3600.0;		//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = m_T_htf_cold_des-273.15;	//[C]
					mc_tes_dc_htf_state.m_temp_out = T_htf_hot_out-273.15;		//[C]

					// HTF charging state
					mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;//[C] convert from K

					// Calculate diff_q_dot
					diff_q_dot = (mc_tes_outputs.m_q_dot_dc_to_htf - q_pc_sb)/q_pc_sb;

				}	// End iteration on discharge mass flow rate

				// Handle exit modes
				if( q_dot_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						// update 'exit_mode'
						q_dot_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						
						if( operating_mode == CR_OFF__PC_SB__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_OFF__PC_SB__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, exit_tolerance);
						}
						else if( operating_mode == CR_SU__PC_SB__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_SU__PC_SB__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, exit_tolerance);
						}

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_dot_exit_mode = CONVERGED;
					}
				}

				if( q_dot_exit_mode != CONVERGED )
				{
					if( operating_mode == CR_OFF__PC_SB__TES_DC__AUX_OFF )
					{
						m_is_CR_OFF__PC_SB__TES_DC__AUX_OFF_avail = false;

					}
					else if( operating_mode == CR_SU__PC_SB__TES_DC__AUX_OFF )
					{
						m_is_CR_SU__PC_SB__TES_DC__AUX_OFF_avail = false;
					}

					are_models_converged = false;
					break;
				}

				// Now solve PC at Standby
				mc_pc_htf_state_in.m_temp = T_htf_hot_out - 273.15;		//[C], convert from K: average storage discharge temperature
				mc_pc_inputs.m_m_dot = m_dot_dc_guess*3600.0;		//[kg/hr], convert from kg/s

				// Inputs
				mc_pc_inputs.m_standby_control = C_csp_power_cycle::STANDBY;

				// Performance Call
				mc_power_cycle.call(mc_weather.ms_outputs,
					mc_pc_htf_state_in,
					mc_pc_inputs,
					mc_pc_out_solver,
					mc_pc_out_report,
					mc_sim_info);
													

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			}
				break;	// break case CR_OFF__PC_SB__TES_DC__AUX_OFF


			case CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF:
			{
				// The collector receiver is off
				// The power cycle runs at its minimum operating fraction until storage is depleted
				// A new, shorter timestep is calculated here

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				double q_dot_pc_fixed = q_pc_min;			//[MWt]

				double time_tes_dc, T_tes_in_exit_tolerance, q_pc_exit_tolerance;
				time_tes_dc = T_tes_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int T_tes_in_exit_mode, q_pc_exit_mode;

				solver_pc_fixed__tes_empty(q_dot_pc_fixed,
					tol,
					time_tes_dc,
					T_tes_in_exit_mode, T_tes_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				// Handle exit modes from outer and inner loops
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
						
						q_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_pc_exit_mode = CONVERGED;
					}
				}
				
				if( T_tes_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_tes_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						T_tes_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_tes_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_tes_in_exit_mode = CONVERGED;
					}
				}

				if( T_tes_in_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
				{
					m_is_CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				if( time_tes_dc > mc_sim_info.m_step )
				{
					error_msg = util::format("At time = %lg CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF method calculated a timestep"
						"that was longer than the baseline timestep. Controller moved to the next timestep in the"
						"controller hierarchy",
						mc_sim_info.m_time / 3600.0);

					m_is_CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// Update mc_sim_info
				mc_sim_info.m_step = time_tes_dc;
				mc_sim_info.m_time = time_previous + time_tes_dc;

				// Know that simulation timestep is not complete
				is_sim_timestep_complete = false;

				// Now run CR at 'OFF'
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				
				mc_collector_receiver.off(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			}
				break;	// break case CR_OFF__PC_MIN__TES_EMPTY__AUX_OFF


			case CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF:
			case CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF:
			{
				// The collector-receiver is off
				// The power cycle runs somewhere between its minimum operating fraction and target operation, with thermal input from TES, which is depleted at the end of the timestep

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// First, solve the CR
				if( operating_mode == CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF )
				{
					// Now run CR at 'OFF'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
					
					mc_collector_receiver.off(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

				}
				else if( operating_mode == CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF )
				{
					// Run CR at 'Start Up'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

					mc_collector_receiver.startup(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check that startup happened
					if( mc_cr_out_solver.m_q_startup == 0.0 )
					{	// Collector/receiver can't produce useful energy

						m_is_CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = false;
						are_models_converged = false;
						break;
					}

					// Check for new timestep
					step_local = mc_cr_out_solver.m_time_required_su;		//[s] Receiver model returns MIN(time required to completely startup, full timestep duration)
					if( step_local < mc_sim_info.m_step - step_tolerance )
					{
						is_sim_timestep_complete = false;
					}

					// Reset sim_info values
					if( !is_sim_timestep_complete )
					{
						mc_sim_info.m_step = step_local;						//[s]
						mc_sim_info.m_time = time_previous + step_local;		//[s]
					}

				}

				double T_tes_cold_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
				double T_tes_cold_guess = T_tes_cold_ini;				//[C]

				double T_tes_cold_lower = std::numeric_limits<double>::quiet_NaN();
				double y_T_tes_cold_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_lower_bound = false;
				bool is_lower_error = false;

				double T_tes_cold_upper = std::numeric_limits<double>::quiet_NaN();
				double y_T_tes_cold_upper = std::numeric_limits<double>::quiet_NaN();
				bool is_upper_bound = false;
				bool is_upper_error = false;

				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				double diff_T_tes_cold = 999.9*tol;			//[-] (T_tes_cold_calc - T_tes_cold_guess)/T_tes_cold_guess

				int iter_T_tes_cold = 0;

				int exit_mode = CONVERGED;
				double exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Start iteration loop
				while( fabs(diff_T_tes_cold) > tol || diff_T_tes_cold != diff_T_tes_cold )
				{
					iter_T_tes_cold++;

					// Check if distance between bounds is "too small"
					double diff_T_bounds = T_tes_cold_upper - T_tes_cold_lower;
					if( diff_T_bounds / T_tes_cold_upper < tol / 2.0 )
					{
						if( diff_T_tes_cold != diff_T_tes_cold )
						{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_tes_cold

							exit_mode = NO_SOLUTION;
							exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else
						{

							exit_mode = POOR_CONVERGENCE;
							exit_tolerance = diff_T_tes_cold;
							break;
						}
					}

					// Subsequent iterations need to re-calculate T_tes_cold
					if( iter_T_tes_cold > 1 )
					{	// diff_T_tes_cold = (T_tes_cold_calc - T_tes_cold_guess)/T_tes_cold_guess

						if( diff_T_tes_cold != diff_T_tes_cold )
						{	// Models did not solve such that a convergence error could be calculated
							// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
							// But, check that bounds exist
							if( !is_lower_bound || !is_upper_bound )
							{

								exit_mode = NO_SOLUTION;
								exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;
							}
							T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
						}
						else if( diff_T_tes_cold > 0.0 )		// Guess cold temperature was too low
						{
							is_lower_bound = true;
							is_lower_error = true;
							T_tes_cold_lower = T_tes_cold_guess;	//[C]
							y_T_tes_cold_lower = diff_T_tes_cold;	//[-]

							if( is_upper_bound && is_upper_error )
							{
								T_tes_cold_guess = y_T_tes_cold_upper / (y_T_tes_cold_upper - y_T_tes_cold_lower)*(T_tes_cold_lower - T_tes_cold_upper) + T_tes_cold_upper;		//[C]
							}
							else if( is_upper_bound )
							{
								T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
							}
							else
							{
								T_tes_cold_guess += 2.5;			//[C]
							}
						}
						else
						{
							is_upper_bound = true;
							is_upper_error = true;
							T_tes_cold_upper = T_tes_cold_guess;	//[C]
							y_T_tes_cold_upper = diff_T_tes_cold;	//[-]

							if( is_lower_bound && is_lower_error )
							{
								T_tes_cold_guess = y_T_tes_cold_upper / (y_T_tes_cold_upper - y_T_tes_cold_lower)*(T_tes_cold_lower - T_tes_cold_upper) + T_tes_cold_upper;		//[C]
							}
							else if( is_lower_bound )
							{
								T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
							}
							else
							{
								T_tes_cold_guess -= 2.5;		//[C]
							}
						}
					}	// end logic to determine new T_tes_cold

					// First, get the maximum possible max flow rate from TES discharge
					double T_htf_tes_hot, m_dot_tes_dc;
					T_htf_tes_hot = m_dot_tes_dc = std::numeric_limits<double>::quiet_NaN();
					mc_tes.discharge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_tes_cold_guess + 273.15, T_htf_tes_hot, m_dot_tes_dc, mc_tes_outputs);

					// Set TES HTF states (this needs to be less bulky...)
					// HTF discharging state
					mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc*3600.0;		//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = T_tes_cold_guess;		//[C]
					mc_tes_dc_htf_state.m_temp_out = T_htf_tes_hot - 273.15;	//[C]

					// HTF charging state
					mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;//[C] convert from K

					// Solve PC model
					mc_pc_htf_state_in.m_temp = T_htf_tes_hot - 273.15;		//[C]
					mc_pc_inputs.m_m_dot = m_dot_tes_dc*3600.0;			//[kg/hr]

					// Inputs
					mc_pc_inputs.m_standby_control = C_csp_power_cycle::ON;

					// Performance Call
					mc_power_cycle.call(mc_weather.ms_outputs,
						mc_pc_htf_state_in,
						mc_pc_inputs,
						mc_pc_out_solver,
						mc_pc_out_report,
						mc_sim_info);			

					diff_T_tes_cold = (mc_pc_out_solver.m_T_htf_cold - T_tes_cold_guess) / T_tes_cold_guess;
				}

				// *********************************
				// Check PC q_dot is >= MIN!!!!!!!!
				// *********************************
				if( mc_pc_out_solver.m_q_dot_htf < q_pc_min )
				{
					exit_mode = NO_SOLUTION;
				}

				if( exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						
						if( operating_mode == CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF )
						{						
							error_msg = util::format("At time = %lg CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, exit_tolerance);
						}
						else if( operating_mode == CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, exit_tolerance);
						}
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						exit_mode = CONVERGED;
					}
				}

				if( exit_mode != CONVERGED )
				{
					if( operating_mode == CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF )
						m_is_CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = false;
					else if( operating_mode == CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF )
						m_is_CR_SU__PC_RM_LO__TES_EMPTY__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;

			}
				break;	// break case CR_OFF__PC_RM_LO__TES_EMPTY__AUX_OFF

			case CR_ON__PC_SB__TES_CH__AUX_OFF:
			{
				// CR is on (no defocus)
				// PC is in standby and requires standby fraction of design thermal input
				// TES is charging with balance of CR output

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Use 'general' solver for this operating mode
				// Define solver method inputs
				double q_dot_pc_fixed = q_pc_sb;			//[MW]
				int power_cycle_mode = C_csp_power_cycle::STANDBY;	//[-] Power cycle in Standby!!
				double field_control_in = 1.0;				//[-]
				double tol = 1.0 / m_cycle_T_htf_hot_des;	//[-] Relative error corresponding to 1 C temperature difference

				// Initialize variables that solver will calculate
				double T_rec_in_exit_tolerance, q_pc_exit_tolerance;
				T_rec_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				int T_rec_in_exit_mode, q_pc_exit_mode;
				T_rec_in_exit_mode = q_pc_exit_mode = -1;

				// Call the solver
				solver_cr_on__pc_fixed__tes_ch(q_dot_pc_fixed, power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				// Define relaxed tolerance
				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				// Process solver results

				// Inner nest exit modes...
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC
					
						// update T_rec_in_exit_mode
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_SB__TES_CH__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_pc_exit_mode = CONVERGED;
					}
				}

				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC
					
						// update T_rec_in_exit_mode
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_SB__TES_CH__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				
				}

				if( T_rec_in_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_SB__TES_CH__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			}
				break;	// break case CR_ON__PC_SB__TES_CH__AUX_OFF

			case CR_SU__PC_MIN__TES_EMPTY__AUX_OFF:
			{
				// The collector-receiver is in startup
				// The power cycle runs at its minimum fraction until storage is depleted
				// A new, shorter timestep is calculated here

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;


				// First, startup the collector-receiver and get the time required
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

				mc_collector_receiver.startup(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				// Check that startup happened
				if( mc_cr_out_solver.m_q_startup == 0.0 )
				{	// Collector/receiver can't produce useful energy

					m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// Get startup time
				double step_cr = fmin(mc_sim_info.m_step, mc_cr_out_solver.m_time_required_su);	//[s]


				// Now, run PC at MIN until storage is discharged
				// Get the time required and compare to same value for CR
				// Need to setup parameters for 'solver_pc_fixed__tes_empty' method
				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				double q_dot_pc_fixed = q_pc_min;			//[MWt]

				double time_tes_dc, T_tes_in_exit_tolerance, q_pc_exit_tolerance;
				time_tes_dc = T_tes_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int T_tes_in_exit_mode, q_pc_exit_mode;

				solver_pc_fixed__tes_empty(q_dot_pc_fixed,
					tol,
					time_tes_dc,
					T_tes_in_exit_mode, T_tes_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				// Handle exit modes from outer and inner loops
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						q_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_pc_exit_mode = CONVERGED;
					}
				}

				if( T_tes_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_tes_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_tes_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_tes_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_tes_in_exit_mode = CONVERGED;
					}
				}

				if( T_tes_in_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
				{
					m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				if( time_tes_dc > mc_sim_info.m_step )
				{
					error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method calculated a timestep"
						"that was longer than the baseline timestep. Controller moved to the next timestep in the"
						"controller hierarchy",
						mc_sim_info.m_time / 3600.0);

					m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}


				// Now compare calculated CR_SU timestep w/ TES_DC timestep
				if(step_cr > time_tes_dc)
				{	// If the time required for CR startup is longer than time to discharge thermal storage at PC MIN
					//     then rerun CR_SU with new timestep (and CR_SU will continue in the next timestep w/ PC OFF...)

					// Check if shortest timestep is close to end of initial timestep
					if(time_tes_dc < mc_sim_info.m_step - step_tolerance)
					{
						// Know that timestep is not complete
						is_sim_timestep_complete = false;

						// Update simulation time info
						mc_sim_info.m_step = time_tes_dc;					//[s]
						mc_sim_info.m_time = time_previous + time_tes_dc;	//[s]
				
						// Rerun CR_SU
						// First, startup the collector-receiver and get the time required
						mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

						mc_collector_receiver.startup(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_out_solver,
							mc_cr_out_report,
							mc_sim_info);

						// Check that startup happened
						if( mc_cr_out_solver.m_q_startup == 0.0 )
						{	// Collector/receiver can't produce useful energy

							m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;

							are_models_converged = false;
							break;
						}
					}
				}
				else if(time_tes_dc > step_cr)
				{	// If the time required to discharge TES at PC MIN is longer than CR startup
					//     then rerun PC MIN but don't fully discharge storage

					// Check if shortest timestep is close to end of initial timestep
					if(step_cr < mc_sim_info.m_step - step_tolerance)
					{
						// Know that timestep is not complete
						is_sim_timestep_complete = false;

						// Update simulation time info
						mc_sim_info.m_step = step_cr;					//[s]
						mc_sim_info.m_time = time_previous + step_cr;	//[s]

						// Rerun PC MIN and TES DC
						int pc_mode = C_csp_power_cycle::ON;
					
						solver_pc_fixed__tes_dc(q_dot_pc_fixed, pc_mode,
							tol,
							T_tes_in_exit_mode, T_tes_in_exit_tolerance,
							q_pc_exit_mode, q_pc_exit_tolerance);

						// Handle exit modes from outer and inner loops
						if( q_pc_exit_mode == POOR_CONVERGENCE )
						{
							if( fabs(q_pc_exit_tolerance) > relaxed_tol )
							{	// Did not converge within Relaxed Tolerance

								q_pc_exit_mode = NO_SOLUTION;
							}
							else
							{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
								error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
									"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
									mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
								mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

								q_pc_exit_mode = CONVERGED;
							}
						}

						if( T_tes_in_exit_mode == POOR_CONVERGENCE )
						{
							if( fabs(T_tes_in_exit_tolerance) > relaxed_tol )
							{	// Did not converge within Relaxed Tolerance

								T_tes_in_exit_mode = NO_SOLUTION;
							}
							else
							{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
								error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
									"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
									mc_sim_info.m_time / 3600.0, T_tes_in_exit_tolerance);
								mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

								T_tes_in_exit_mode = CONVERGED;
							}
						}

						if( T_tes_in_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
						{
							m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
							are_models_converged = false;
							break;
						}
					}
				}
				else if(time_tes_dc == step_cr)		// Guess both times could be equal
				{
					// Check whether, improbably, both CR_SU and TES_DC are equal but less than the initial simulation time

					if(time_tes_dc < mc_sim_info.m_step - step_tolerance)
					{
						is_sim_timestep_complete = false;

						mc_sim_info.m_step = time_tes_dc;
						mc_sim_info.m_time = time_previous + time_tes_dc;
					}

				}
				else
				{	// Catch if time_tes_dc or step_cr return NaN

					error_msg = util::format("At time = %lg CR_SU__PC_MIN__TES_EMPTY__AUX_OFF method calculated a NaN timestep",
						mc_sim_info.m_time / 3600.0);

					m_is_CR_SU__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}
				
				
				are_models_converged = true;
				
			}
				break;


			case CR_ON__PC_SB__TES_DC__AUX_OFF:
			{
				// The collector receiver is on and returning hot HTF to the PC
				// TES is discharging hot HTF that is mixed with the CR HTF
				// to operate the PC at standby

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Define arguments to solver method
				double q_dot_pc_fixed = q_pc_sb;	//[MWt]
				int power_cycle_mode = C_csp_power_cycle::STANDBY;
				double field_control_in = 1.0;

				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-] 

				int T_rec_in_exit_mode, q_pc_exit_mode;
				T_rec_in_exit_mode = q_pc_exit_mode = -1;

				double T_rec_in_exit_tolerance, q_pc_exit_tolerance;
				T_rec_in_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				solver_cr_on__pc_fixed__tes_dc(q_dot_pc_fixed, power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance,
					q_pc_exit_mode, q_pc_exit_tolerance);

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				// Handle exit modes from outer and inner loops
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						q_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_SB__TES_DC__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_pc_exit_mode = CONVERGED;
					}
				}

				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance, shut off CR and PC

						// update 'exit_mode'
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						error_msg = util::format("At time = %lg CR_ON__PC_SB__TES_DC__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( q_pc_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_SB__TES_DC__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;
			
			}	// end 'CR_ON__PC_SB__TES_DC__AUX_OFF'
				
				break;


			case CR_OFF__PC_TARGET__TES_DC__AUX_OFF:
			case CR_SU__PC_TARGET__TES_DC__AUX_OFF:
			{
				// The collector receiver is off
				// The power cycle run at the target thermal input level
				// The TES supplies the thermal power to the power cycle
				
				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// First, solve the CR
				if( operating_mode == CR_OFF__PC_TARGET__TES_DC__AUX_OFF )
				{
					// Now run CR at 'OFF'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
					
					mc_collector_receiver.off(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

				}
				else if( operating_mode == CR_SU__PC_TARGET__TES_DC__AUX_OFF )
				{
					// Run CR at 'Start Up'
					mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

					mc_collector_receiver.startup(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check that startup happened
					if( mc_cr_out_solver.m_q_startup == 0.0 )
					{	// Collector/receiver can't produce useful energy

						m_is_CR_SU__PC_TARGET__TES_DC__AUX_OFF_avail = false;
						are_models_converged = false;
						break;
					}

					// Check for new timestep
					step_local = mc_cr_out_solver.m_time_required_su;		//[s] Receiver model returns MIN(time required to completely startup, full timestep duration)
					if( step_local < mc_sim_info.m_step - step_tolerance )
					{
						is_sim_timestep_complete = false;
					}

					// Reset sim_info values
					if( !is_sim_timestep_complete )
					{
						mc_sim_info.m_step = step_local;						//[s]
						mc_sim_info.m_time = time_previous + step_local;		//[s]
					}
				}


				double q_dot_pc_fixed = q_pc_target;	//[MWt]
				int power_cycle_mode = C_csp_power_cycle::ON;
				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]


				double T_cold_exit_tolerance, q_pc_exit_tolerance;
				T_cold_exit_tolerance = q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int T_cold_exit_mode, q_pc_exit_mode;
				T_cold_exit_mode = q_pc_exit_mode = -1;

				solver_pc_fixed__tes_dc(q_dot_pc_fixed, power_cycle_mode,
				tol,
				T_cold_exit_mode, T_cold_exit_tolerance,
				q_pc_exit_mode, q_pc_exit_tolerance);

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				// Check if solver converged or a new operating mode is required
				// Handle exit modes from outer and inner loops
				if( q_pc_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(q_pc_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						q_pc_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						
						if( operating_mode == CR_OFF__PC_TARGET__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_OFF__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						}
						else if( operating_mode == CR_SU__PC_TARGET__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_SU__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, q_pc_exit_tolerance);
						}
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						q_pc_exit_mode = CONVERGED;
					}
				}

				if( T_cold_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_cold_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_cold_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode
						
						if( operating_mode == CR_OFF__PC_TARGET__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_OFF__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, T_cold_exit_tolerance);
						}
						else if( operating_mode == CR_SU__PC_TARGET__TES_DC__AUX_OFF )
						{
							error_msg = util::format("At time = %lg CR_SU__PC_TARGET__TES_DC__AUX_OFF method only reached a convergence"
								"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
								mc_sim_info.m_time / 3600.0, T_cold_exit_tolerance);
						}
						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_cold_exit_mode = CONVERGED;
					}
				}

				if( T_cold_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
				{
					if( operating_mode == CR_OFF__PC_TARGET__TES_DC__AUX_OFF )
						m_is_CR_OFF__PC_TARGET__TES_DC__AUX_OFF_avail = false;
					else if( operating_mode == CR_SU__PC_TARGET__TES_DC__AUX_OFF )
						m_is_CR_SU__PC_TARGET__TES_DC__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}				

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;

			}	// end 'CR_OFF__PC_TARGET__TES_DC__AUX_OFF'
				break;


			case CR_ON__PC_RM_HI__TES_FULL__AUX_OFF:
			{
				// The collector receiver is on and delivering hot HTF to the TES and PC
				// The PC is operating between its target and maximum thermal power
				// The TES is fully charging over the timestep

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				int power_cycle_mode = C_csp_power_cycle::ON;
				double field_control_in = 1.0;
				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				int T_rec_in_exit_mode = -1;

				solver_cr_on__pc_float__tes_full(power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance);


				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]


				if( mc_pc_out_solver.m_q_dot_htf > q_pc_max )
				{
					T_rec_in_exit_mode = NO_SOLUTION;
				}

				if( mc_pc_out_solver.m_q_dot_htf < q_pc_target )
				{
					error_msg = util::format("At time = %lg CR_ON__PC_RM_HI__TES_FULL__AUX_OFF method converged to a power cycle"
						" thermal input less than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);
				}


				// Check if solver converged or a new operating mode is required
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg CR_ON__PC_RM_HI__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
						T_rec_in_exit_mode = CONVERGED;
					}				
				}

				if( T_rec_in_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_RM_HI__TES_FULL__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// If convergence was successful, finalize this timestep and get out
				// Have solved CR, TES, and PC in this operating mode, so only need to set flag to get out of Mode Iteration
				are_models_converged = true;

			}	// end 'CR_ON__PC_RM_HI__TES_FULL__AUX_OFF
				break;

			case CR_ON__PC_MIN__TES_EMPTY__AUX_OFF:
			{
				// The collector-receiver is on and returning hot HTF to the PC
				// The PC is operating at its minimum fraction
				// The CR + TES output over the initial controller timestep is less than the PC min fraction
				// so the controller calculates a new timestep such that the TES completely discharges and 
				//  ... the CR + TES output is = the PC min fraction

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Guess the time required to deplete storage while delivering thermal power requirements to PC
				double time_empty_max = mc_sim_info.m_step;		//[s]
				double time_empty_min = 0.001;					//[s]

				double time_empty_guess_ini = fmin(mc_sim_info.m_step, fmax(0.0, mc_sim_info.m_step*((q_dot_tes_dc + q_dot_cr_on)/q_pc_min) ) );
				double time_empty_guess = time_empty_guess_ini; 

				double time_empty_lower = time_empty_min;
				double y_time_empty_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_time_lower_bound = true;
				bool is_time_lower_error = false;

				double time_empty_upper = time_empty_max;
				double y_time_empty_upper = std::numeric_limits<double>::quiet_NaN();
				bool is_time_upper_bound = true;
				bool is_time_upper_error = false;

				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;
				double relaxed_tol = relaxed_tol_mult*tol;

				double diff_q_dot = 999.9*tol;		//[-]

				int iter_q_dot = 0;

				int q_pc_exit_mode = CONVERGED;
				double q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int T_cold_exit_mode = CONVERGED;
				double T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Set new local timestep
				C_csp_solver_sim_info temp_sim_info = mc_sim_info;

				// Start iteration on timestep duration to achieve PC target
				while( fabs(diff_q_dot) > tol || diff_q_dot != diff_q_dot )
				{
					iter_q_dot++;		// First iteration = 1

					// Check if distance between bounds is "too small"
					double diff_q_dot_bounds = time_empty_upper - time_empty_lower;
					if( diff_q_dot_bounds / 3600.0 < tol / 2.0 )
					{
						if( diff_q_dot != diff_q_dot )
						{	// Models aren't solving !?!?!

							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// exits while() on diff_q_dot
						}
						else if( (time_empty_max - time_empty_lower) / 3600.0 < tol / 2.0 )
						{
							q_pc_exit_mode = OVER_TARGET_PC;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// q_dot_pc should be > target (min)
						}
						else if( (time_empty_upper - time_empty_min) / 3600.0 < tol / 2.0 )
						{
							q_pc_exit_mode = UNDER_TARGET_PC;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else
						{

							q_pc_exit_mode = POOR_CONVERGENCE;
							q_pc_exit_tolerance = diff_q_dot;
							break;		// exits while() on diff_q_dot
						}
					}

					// Subsequent iterations need to re-calculate time_empty_guess
					if( iter_q_dot > 1 )
					{
						if( diff_q_dot != diff_q_dot )
						{	// Models did not solve such that a convergence error could be calculated
							// However, if upper and lower bounds are set, then we can calculate a new guess via bisection method
							// First, need to check that bounds exist
							if( !is_time_lower_bound || !is_time_upper_bound )
							{

								q_pc_exit_mode = NO_SOLUTION;
								q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;		// exits while() on diff_q_dot
							}
							time_empty_guess = 0.5*(time_empty_lower + time_empty_upper);
						}
						else if( diff_q_dot > 0.0 )			// diff_q_dot = (q_dot_calc - q_dot_pc_min)/q_dot_pc_min
						{	// Time guess is too small

							is_time_lower_bound = true;
							is_time_lower_error = true;
							time_empty_lower = time_empty_guess;
							y_time_empty_lower = diff_q_dot;

							if( is_time_upper_bound && is_time_upper_error )
							{
								time_empty_guess = y_time_empty_upper / (y_time_empty_upper - y_time_empty_lower)*(time_empty_lower - time_empty_upper) + time_empty_upper;		//[kg/hr]
							}
							else if( is_time_upper_bound )
							{
								time_empty_guess = 0.5*(time_empty_lower + time_empty_upper);
							}
							else
							{
								time_empty_guess = fmin(time_empty_guess*1.25, time_empty_max);	//[s]
							}
						}
						else
						{

							is_time_upper_bound = true;
							is_time_upper_error = true;
							time_empty_upper = time_empty_guess;
							y_time_empty_upper = diff_q_dot;

							if( is_time_lower_bound && is_time_lower_error )
							{
								time_empty_guess = y_time_empty_upper / (y_time_empty_upper - y_time_empty_lower)*(time_empty_lower - time_empty_upper) + time_empty_upper;		//[kg/hr]
							}
							else if( is_time_lower_bound )
							{
								time_empty_guess = 0.5*(time_empty_lower + time_empty_upper);
							}
							else
							{
								time_empty_guess = fmin(time_empty_guess*0.75, time_empty_min);		//[s]adfa
							}
						}
					}	// end logic to calculate new time_empty_guess
					
					temp_sim_info.m_step = time_empty_guess;					//[s]
					temp_sim_info.m_time = mc_sim_info.m_time - mc_sim_info.m_step + time_empty_guess;	//[s]

					// Now, need to iterate on the cold side temperature (CR and TES inlet)
					double T_cold_guess_ini = m_T_htf_cold_des - 273.15;	//[C], convert from K
					double T_cold_guess = T_cold_guess_ini;					//[C]

					// Lower bound could be freeze protection temperature...
					double T_cold_lower = std::numeric_limits<double>::quiet_NaN();
					double T_cold_upper = std::numeric_limits<double>::quiet_NaN();
					double y_T_cold_lower = std::numeric_limits<double>::quiet_NaN();
					double y_T_cold_upper = std::numeric_limits<double>::quiet_NaN();
					// Booleans for bounds and convergence error
					bool is_upper_bound = false;
					bool is_lower_bound = false;
					bool is_upper_error = false;
					bool is_lower_error = false;

					double tol_T_cold = 0.9*tol;

					double diff_T_cold = 999.9*tol_T_cold;			// (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

					int iter_T_cold = 0;

					T_cold_exit_mode = CONVERGED;
					T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

					// Start iteration on T_cold
					while( fabs(diff_T_cold) > tol_T_cold || diff_T_cold != diff_T_cold )
					{
						iter_T_cold++;		// First iteration = 1

						// Check if distance between bounds is "too small"
						double diff_T_bounds = T_cold_upper - T_cold_lower;
						if( diff_T_bounds / T_cold_upper < tol / 2.0 )
						{
							if( diff_T_cold != diff_T_cold )
							{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

								T_cold_exit_mode = NO_SOLUTION;
								T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;	// exit to outer iteration on timestep duration
							}
							else
							{
								T_cold_exit_mode = POOR_CONVERGENCE;
								T_cold_exit_tolerance = diff_T_cold;
								break;	// exit to outer iteration on timestep duration
							}
						}

						// Subsequent iterations need to re-calculate T_in
						if( iter_T_cold > 1 )
						{	// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

							if( diff_T_cold != diff_T_cold )
							{	// Models did not solve such that a convergence error could be calculated
								// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
								// But, check that bounds exist
								if( !is_lower_bound || !is_upper_bound )
								{
									T_cold_exit_mode = NO_SOLUTION;
									T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;	// exit to outer iteration on timestep duration
								}
								T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]						
							}
							else if( diff_T_cold > 0.0 )		// Guess receiver inlet temperature was too low
							{
								is_lower_bound = true;
								is_lower_error = true;
								T_cold_lower = T_cold_guess;		//[C]
								y_T_cold_lower = diff_T_cold;		//[-]

								if( is_upper_bound && is_upper_error )
								{
									T_cold_guess = y_T_cold_upper / (y_T_cold_upper - y_T_cold_lower)*(T_cold_lower - T_cold_upper) + T_cold_upper;		//[C]
								}
								else if( is_upper_bound )
								{
									T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]	
								}
								else
								{
									T_cold_guess += 2.5;			//[C]
								}
							}
							else
							{
								is_upper_bound = true;
								is_upper_error = true;
								T_cold_upper = T_cold_guess;		//[C] Set upper bound
								y_T_cold_upper = diff_T_cold;		//[-]

								if( is_lower_bound && is_upper_bound )
								{
									T_cold_guess = y_T_cold_upper / (y_T_cold_upper - y_T_cold_lower)*(T_cold_lower - T_cold_upper) + T_cold_upper;		//[C]
								}
								else if( is_lower_bound )
								{
									T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]
								}
								else
								{
									T_cold_guess -= 2.5;		//[C]
								}
							}
						} // end logic to determine new T_rec_in

						// Solve the collector-receiver model
						mc_cr_htf_state_in.m_temp = T_cold_guess;		//[C]
						mc_cr_inputs.m_field_control = 1.0;				//[-] no defocusing for initial simulation
						mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

						mc_collector_receiver.call(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_inputs,
							mc_cr_out_solver,
							mc_cr_out_report,
							temp_sim_info);			// **** Use TEMP sim info ****

						// Check if receiver is OFF or model didn't solve
						if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
						{
							// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
							if( iter_T_cold == 1 )
							{
								T_cold_exit_mode = NO_SOLUTION;
								T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;	// exit to outer iteration on timestep duration
							}
							else
							{	// If collector-receiver model has solved with results previously in this loop, then try to find another guess value
								// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
								// Also, assume that if both upper and lower bounds exist, then can't generate a new guess because don't know which way to move
								if( T_cold_guess < T_cold_guess_ini )
								{	// If current guess value is less than the initial guess value, then:

									// If lower bound is already set OR upper bound is not set, can't generate a new guess value
									if( is_lower_bound || !is_upper_bound )
									{
										T_cold_exit_mode = NO_SOLUTION;
										T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit to outer iteration on timestep duration
									}

									// Else, set lower bound and flags and 'continue' to start of while()
									T_cold_lower = T_cold_guess;
									is_lower_bound = true;
									is_lower_error = false;

									// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
									diff_T_cold = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
								else
								{	// If current guess value is greater than initial value, then:

									// If upper bound is already set OR lower bound is not set, can't generate a new guess value
									if( is_upper_bound || !is_lower_bound )
									{
										T_cold_exit_mode = NO_SOLUTION;
										T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit to outer iteration on timestep duration
									}

									// Else, set upper bound and flags and 'continue' to start of while()
									T_cold_upper = T_cold_guess;
									is_upper_bound = true;
									is_upper_error = false;

									// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
									diff_T_cold = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
							}	// end else on if(iter_T_rec_in == 1)
						}	// end logic to determine path if receiver is off or did not solve

						// Solve TES FULL discharge
						double T_tes_htf_hot, m_dot_tes_dc;
						mc_tes.discharge_full(temp_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_cold_guess + 273.15, T_tes_htf_hot, m_dot_tes_dc, mc_tes_outputs);
						T_tes_htf_hot -= 273.15;	//[C] convert from K
						m_dot_tes_dc *= 3600.0;		//[kg/hr] convert from kg/s

						// HTF discharging state
						mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc;			//[kg/hr]
						mc_tes_dc_htf_state.m_temp_in = T_cold_guess;		//[C]
						mc_tes_dc_htf_state.m_temp_out = T_tes_htf_hot;		//[C]

						// HTF charging state
						mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
						mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

						// Mass balance to PC
						double m_dot_pc = mc_cr_out_solver.m_m_dot_salt_tot + m_dot_tes_dc;		//[kg/hr]

						double T_pc_htf_in = (m_dot_tes_dc*T_tes_htf_hot + mc_cr_out_solver.m_m_dot_salt_tot*mc_cr_out_solver.m_T_salt_hot) / (m_dot_pc);	//[C]

						// Solve power cycle model
						mc_pc_htf_state_in.m_temp = T_pc_htf_in;		//[C]
						mc_pc_inputs.m_m_dot = m_dot_pc;				//[kg/hr]

						// Inputs
						mc_pc_inputs.m_standby_control = C_csp_power_cycle::ON;

						// Performance Call
						mc_power_cycle.call(mc_weather.ms_outputs,
							mc_pc_htf_state_in,
							mc_pc_inputs,
							mc_pc_out_solver,
							mc_pc_out_report,
							temp_sim_info);			// **** Use TEMP sim info ****

						// Check that power cycle is producing power or model didn't solve
						if( mc_pc_out_solver.m_P_cycle == 0.0 )
						{
							// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
							// Go to Receiver OFF power cycle OFF
							if( iter_T_cold == 1 )
							{
								T_cold_exit_mode = NO_SOLUTION;
								T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;	// exit to outer iteration on timestep duration
							}
							else
							{
								// Set T_rec_in_guess as either upper or lower bound, depending on which end of DESIGN temp it falls
								// Assumption here is that receiver solved at first guess temperature
								// But if both upper and lower bounds are established, then don't have a direction to try
								// So this reguess is only good for 1 PC failure on the 2nd iteration...
								if( T_cold_guess < T_cold_guess_ini )
								{
									if( is_lower_bound || !is_upper_bound )
									{
										T_cold_exit_mode = NO_SOLUTION;
										T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;
									}
									T_cold_lower = T_cold_guess;
									is_lower_bound = true;
									is_lower_error = false;
									// At this point, both and upper and lower bound should exist, so can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_in to NaN
									diff_T_cold = std::numeric_limits<double>::quiet_NaN();
								}
								else
								{
									if( is_upper_bound || !is_lower_bound )
									{
										T_cold_exit_mode = NO_SOLUTION;
										T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;
									}
									T_cold_upper = T_cold_guess;
									is_upper_bound = true;
									is_upper_error = false;
									// At this point, both and upper and lower bound should exist, so can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_in to NaN
									diff_T_cold = std::numeric_limits<double>::quiet_NaN();
								}
							}
						}	// End logic for PC off or not producing power

						// Get HTF temperature out from the power cycle and compare to guess value (T_rec_in)
						double T_cold_calc = mc_pc_out_solver.m_T_htf_cold;	//[C]

						diff_T_cold = (T_cold_calc - T_cold_guess) / T_cold_guess;		//[-]

					}	// end while() on T_rec_in

					// Handle exit modes on T_cold iteration
					if( T_cold_exit_mode != CONVERGED && T_cold_exit_mode != POOR_CONVERGENCE )
					{
						break;
					}

					diff_q_dot = (mc_pc_out_solver.m_q_dot_htf - q_pc_min) / q_pc_min;

				}	// end while() on timestep duration to achieve target to power cycle

				if( q_pc_exit_mode == OVER_TARGET_PC )
				{
					error_msg = util::format("At time = %lg CR_ON__PC_MIN__TES_EMPTY__AUX_OFF method converged to a power cycle"
						" thermal input greater than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);

					q_pc_exit_mode == CONVERGED;
				}

				if( T_cold_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_cold_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						T_cold_exit_mode = NO_SOLUTION;
					}
					else
					{	// Converge within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg CR_ON__PC_MIN__TES_EMPTY__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_cold_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_cold_exit_mode = CONVERGED;
					}
				}

				if( T_cold_exit_mode != CONVERGED || q_pc_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_MIN__TES_EMPTY__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// Reset timestep information
				is_sim_timestep_complete = false;
				mc_sim_info.m_step = temp_sim_info.m_step;
				mc_sim_info.m_time = temp_sim_info.m_time;
				
				are_models_converged = true;
				
			}	// end 'CR_ON__PC_MIN__TES_EMPTY__AUX_OFF
				break;

			case CR_DF__PC_MAX__TES_FULL__AUX_OFF:
			{
				// The PC is operating at its maximum operating fraction
				// TES is fully charged at the end of the timestep
				// The CR is still delivering too much mass flow rate and needs to be defocused


				// Set up iteration to find defocus that results in completely filled storage
				// Upper bound, error, and booleans
				double defocus_upper = 1.0;
				double y_defocus_upper = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_upper_bound = true;
				bool is_defocus_upper_error = false;
				// Lower bound, error, and booleans
				double defocus_lower = std::numeric_limits<double>::quiet_NaN();
				double y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_lower_bound = false;
				bool is_defocus_lower_error = false;

				double tol = 0.001;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				// Estimate required defocus
					// Max charging mass flow rate
				double T_htf_tes_out_est, m_dot_htf_tes_max_est;
				T_htf_tes_out_est = m_dot_htf_tes_max_est = std::numeric_limits<double>::quiet_NaN();
				mc_tes.charge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_T_htf_cold_des,
					T_htf_tes_out_est, m_dot_htf_tes_max_est, mc_tes_outputs);
				m_dot_htf_tes_max_est *= 3600.0;		//[kg/hr] convert from kg/s

					// Max PC mass flow rate
				double m_dot_pc_max_est = m_m_dot_pc_des*(q_pc_max/m_cycle_q_dot_des);

				double defocus_guess_ini = fmin(1.0, (m_dot_htf_tes_max_est+m_dot_pc_max_est) / mc_cr_out_solver.m_m_dot_salt_tot );
				double defocus_guess = defocus_guess_ini;

				// Defocus: 1 = full power, 0 = no power
				double diff_q_dot = 999.9*tol;

				int defocus_exit_mode = CONVERGED;
				double defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int iter_defocus = 0;

				// Exit information for outer loop on T_rec_in (needs to be accessible in this scope)
				int T_rec_in_exit_mode = CONVERGED;
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				// Iterate on defocus to fully charge storage
				while( fabs(diff_q_dot) > tol || diff_q_dot != diff_q_dot )
				{
					iter_defocus++;		// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_defocus_bounds = defocus_upper - defocus_lower;
					if( diff_defocus_bounds / defocus_upper < tol / 2.0 )
					{
						if( diff_q_dot != diff_q_dot )
						{	// System models not converging, so need to shut them down

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = NO_SOLUTION;
							break;		// Get out of while()					
						}
						else if( 1.0 - defocus_lower < tol / 2.0)
						{	// Full CR output is not fully charging storage and operating PC at max
						
							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = UNDER_TARGET_PC;
							break;		// Get out of while() on defocus
						}
						else if( defocus_upper < 0.05 )
						{
							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = OVER_TARGET_PC;
							break;	
						}
						else
						{	// Poor convergence between power delivered to PC and power requested

							defocus_exit_tolerance = diff_q_dot;
							defocus_exit_mode = POOR_CONVERGENCE;
							break;		// Get out of while()
						}
					}

					// Subsequent iterations need to re-calculate defocus
					if( iter_defocus > 1 )
					{
						if( diff_q_dot != diff_q_dot )		// Check if solution was found
						{	// CR-PC model did not converge, so we don't know anything about this defocus
							// However, we know that we should now have an upper or lower bound (else code would have exited from logic below)
							// But, check that bounds exist, just to be careful
							if( !is_defocus_lower_bound || !is_defocus_upper_bound )
							{

								defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							defocus_guess = 0.5*(defocus_lower + defocus_upper);
						}
						else if( diff_q_dot > 0.0 )		// q_dot was too high, decrease defocus
						{
							is_defocus_upper_bound = true;
							is_defocus_upper_error = true;
							defocus_upper = defocus_guess;		// Set upper bound
							y_defocus_upper = diff_q_dot;		// Set upper convergence error

							if( is_defocus_lower_bound && is_defocus_lower_error )	// False-position method
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_lower_bound )
							{
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmax(0.01, defocus_guess - 0.05);			// Could perhaps use last solution to make a smarter guess...
							}

						}
						else							// q_dot was too low, increase defocus 
						{
							is_defocus_lower_bound = true;
							is_defocus_lower_error = true;
							defocus_lower = defocus_guess;	// Set lower bound
							y_defocus_lower = diff_q_dot;	// Set lower convergence error

							if( is_defocus_upper_bound && is_defocus_upper_error )
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_upper_bound )
							{	// should always have upper bound, but keep this framework for consistency...
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmin(1.0, defocus_guess + 0.05);
							}
						}
					}

					// Exit information for outer loop on T_rec_in
					T_rec_in_exit_mode = CONVERGED;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

					int power_cycle_mode = C_csp_power_cycle::ON;
					double tol_solver = 0.98*tol;

					// Solver cr-pc-tes_full
					solver_cr_on__pc_float__tes_full(power_cycle_mode, defocus_guess, tol_solver,
						T_rec_in_exit_mode, T_rec_in_exit_tolerance);

					// Check T_rec_in_exit_mode
					if( T_rec_in_exit_mode != CONVERGED && T_rec_in_exit_mode != POOR_CONVERGENCE )
					{
						// Assume this means we've hit the lower bound on defocus
						is_defocus_lower_bound = true;
						is_defocus_lower_error = false;
						defocus_lower = defocus_guess;
						y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
						diff_q_dot = std::numeric_limits<double>::quiet_NaN();

						continue;
					}

					// Calculate defocus error
					diff_q_dot = (mc_pc_out_solver.m_q_dot_htf - q_pc_max) / q_pc_max;

				}	// end while() on defocus to achieve full storage charge and PC max

				// Did system solve with a defocus < 1.0?
				// Controller hierarchy doesn't allow to go back to No Defocus and PC_RM, so check that defocus is <= 1
				if( defocus_exit_mode == UNDER_TARGET_PC )
				{
					error_msg = util::format("At time = %lg CR_DF__PC_MAX__TES_FULL__AUX_OFF method converged to a power cycle"
						" thermal input less than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);

					defocus_exit_mode = CONVERGED;
				}

				// Handle exit modes
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_MAX__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(defocus_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						defocus_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_MAX__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, defocus_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						defocus_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					are_models_converged = false;
					m_is_CR_DF__PC_MAX__TES_FULL__AUX_OFF_avail = false;
					break;
				}

				// Set member defocus
				m_defocus = defocus_guess;

				are_models_converged = true;
			
			}	// end 'CR_DF__PC_MAX__TES_FULL__AUX_OFF'
				break;

			case CR_ON__PC_SB__TES_FULL__AUX_OFF:
			{
				// The collecter receiver is on and delivering hot HTF to the TES and PC
				// The power cycle is operating at standby
				// and the TES is fully charging

				// The thermal input to the power at standby can go over target standby
				// ... but it cannot go UNDER

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				int power_cycle_mode = C_csp_power_cycle::STANDBY;
				double field_control_in = 1.0;					//[-]
				double tol_C = 1.0;								//[C]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				int T_rec_in_exit_mode = -1;

				solver_cr_on__pc_float__tes_full(power_cycle_mode,
					field_control_in,
					tol,
					T_rec_in_exit_mode, T_rec_in_exit_tolerance);

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				if( mc_pc_out_solver.m_q_dot_htf < q_pc_sb )
				{
					T_rec_in_exit_mode = NO_SOLUTION;
				}

				// Check if solver converged or a new operating mode is required
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg CR_ON__PC_SB__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( T_rec_in_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_SB__TES_FULL__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				are_models_converged = true;

			}	// end 'CR_ON__PC_SB__TES_FULL__AUX_OFF'
				break;

			case CR_SU__PC_SU__TES_DC__AUX_OFF:
			{
				// Collector-receiver is starting up
				// Power cycle is starting up, with thermal power from TES
				// Code calculates the shortest timestep of: (CR SU, PC SU, initial timestep)
				//      and then recalculates the other component startup as necessary

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;


				// First, startup the collector-receiver and get the time required
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

				mc_collector_receiver.startup(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				// Check that startup happened
				if( mc_cr_out_solver.m_q_startup == 0.0 )
				{	// Collector/receiver can't produce useful energy

					m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = false;

					are_models_converged = false;
					break;
				}

				// Get startup time
				double step_cr = fmin(mc_sim_info.m_step, mc_cr_out_solver.m_time_required_su);	//[s]

				// *****************************************************************
				// Next, calculate the required power cycle startup time
				double step_tol = step_tolerance;		//[s]
				double step_pc_su = std::numeric_limits<double>::quiet_NaN();

				int exit_mode = CONVERGED;
				double T_pc_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				solver_pc_su_controlled__tes_dc(step_tol,
					step_pc_su,
					exit_mode, T_pc_in_exit_tolerance);

				// Check exit mode
				if( exit_mode != CONVERGED )
				{
					are_models_converged = false;
					m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = false;
					break;
				}

				// ******************************************************************
				// Compare the CR and PC startup times
				if( step_cr > step_pc_su )
				{	// If the time required for CR startup is longer than the time to startup the PC
					//       then rerun CR_SU with the PC startup timestep (and CR_SU will continue in the next timestep)

					// Check if shortest timestep is close to end of initial timestep
					if(step_pc_su < mc_sim_info.m_step - step_tolerance)
					{
						// Know that timestep is not complete
						is_sim_timestep_complete = false;

						// Update simulation time info
						mc_sim_info.m_step = step_pc_su;							//[s]
						mc_sim_info.m_time = time_previous + step_pc_su;			//[s]

						// Rerun CR_SU
						mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]

						mc_collector_receiver.startup(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_out_solver,
							mc_cr_out_report,
							mc_sim_info);

						// Check that startup happened
						if( mc_cr_out_solver.m_q_startup == 0.0 )
						{	// Collector/receiver can't produce useful energy

							m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = false;

							are_models_converged = false;
							break;
						}		
					}		
				}
				else if( step_pc_su > step_cr )
				{	// If the time required to discharge TES at PC MIN is longer than CR startup
					//      the rerun PC startup with the CR startup timestep (and PC_SU will continue in the next timestep)
				
					// Check if shortest timestep is close to end of initial timestep
					if(step_cr < mc_sim_info.m_step - step_tolerance)
					{
						// Know that timestep is not complete
						is_sim_timestep_complete = false;

						// Update simulation time info
						mc_sim_info.m_step = step_cr;						//[s]
						mc_sim_info.m_time = time_previous + step_cr;		//[s]

						// Rerun PC_SU
						step_tol = step_tolerance;		//[s]
						step_pc_su = std::numeric_limits<double>::quiet_NaN();

						exit_mode = CONVERGED;
						T_pc_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

						solver_pc_su_controlled__tes_dc(step_tol,
							step_pc_su,
							exit_mode, T_pc_in_exit_tolerance);

						// Check exit mode
						if( exit_mode != CONVERGED )
						{
							are_models_converged = false;
							m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = false;
							break;
						}
					}
				}
				else if( step_pc_su == step_cr )
				{
					// Check whether, improbably, both CR_SU and PC_SU are equal but less than the initial simulation

					if( step_cr < mc_sim_info.m_step - step_tolerance )
					{
						is_sim_timestep_complete = false;

						mc_sim_info.m_step = step_cr;
						mc_sim_info.m_time = time_previous + step_cr;
					}
				}
				else
				{	// Catch if 'step_pc_su' or 'step_cr' return NaN

					error_msg = util::format("At time = %lg CR_SU__PC_SU__TES_DC__AUX_OFF method calculated a NaN timestep",
						mc_sim_info.m_time / 3600.0);

					m_is_CR_SU__PC_SU__TES_DC__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				are_models_converged = true;
			
			}
				break;


			case CR_ON__PC_SU__TES_CH__AUX_OFF:
			{
				// CR in on
				// PC is starting up with its maximum thermal power for startup
				//      and is returning the startup time required
				// Excess CR output is charging TES

				// Set Solved Controller Variables Here (that won't be reset in this operating mode)
				m_defocus = 1.0;

				// Guess and iterate for the collector-receiver inlet temperature
				double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
				double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

				double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
				double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
				double y_T_rec_in_lower	= std::numeric_limits<double>::quiet_NaN();
				double y_T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();

				bool is_upper_bound = false;
				bool is_lower_bound = false;
				bool is_upper_error = false;
				bool is_lower_error = false;

				double tol_C = 1.0;								//[K]
				double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				double diff_T_rec_in = 999.9*tol;
				
				int iter_T_rec_in = 0;

				// Exit information for loop on T_rec_in
				int T_rec_in_exit_mode = CONVERGED;
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				double step_pc_su = std::numeric_limits<double>::quiet_NaN();

				// Start iteration on T_rec_in
				while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
				{
					iter_T_rec_in++;			// First iteration = 1

					// Check if distance between bounds is "too small"
					double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
					if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
					{
						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

							T_rec_in_exit_mode = NO_SOLUTION;
							T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else
						{
							T_rec_in_exit_mode = POOR_CONVERGENCE;
							T_rec_in_exit_tolerance = diff_T_rec_in;
							break;
						}
					}

					// Subsequent iterations need to re-calculate T_in
					if( iter_T_rec_in > 1 )
					{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
						if( diff_T_rec_in != diff_T_rec_in )
						{	// Models did not solve such that a convergence error could be calculated
							// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
							// But, check that bounds exist
							if( !is_lower_bound || !is_upper_bound )
							{
								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;
							}
							T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
						}
						else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
						{
							is_lower_bound = true;
							is_lower_error = true;
							T_rec_in_lower = T_rec_in_guess;		//[C]
							y_T_rec_in_lower = diff_T_rec_in;			//[-]

							if( is_upper_bound && is_upper_error )
							{
								T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
							}
							else if( is_upper_bound )
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
							}
							else
							{
								T_rec_in_guess += 2.5;			//[C]
							}
						}
						else
						{
							is_upper_bound = true;
							is_upper_error = true;
							T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
							y_T_rec_in_upper = diff_T_rec_in;			//[-]

							if( is_lower_bound && is_upper_bound )
							{
								T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
							}
							else if( is_lower_bound )
							{
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else
							{
								T_rec_in_guess -= 2.5;		//[C]
							}
						}
					}

					// Solve the receiver model with T_rec_in_guess
					// CR ON
					mc_cr_htf_state_in.m_temp = T_rec_in_guess;		//[C]
					mc_cr_inputs.m_field_control = 1.0;				//[-] no defocusing for initial simulation
					mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

					mc_collector_receiver.call(mc_weather.ms_outputs,
						mc_cr_htf_state_in,
						mc_cr_inputs,
						mc_cr_out_solver,
						mc_cr_out_report,
						mc_sim_info);

					// Check if receiver is OFF or model didn't solve
					if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
					{
						// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
						if( iter_T_rec_in == 1 )
						{
							T_rec_in_exit_mode = NO_SOLUTION;
							T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;  // exit while() on diff_T_rec_in
						}
						else
						{	// If collector-receiver model has solved with results previously, then try to find another guess value
							// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
							// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
							if( T_rec_in_guess < T_rec_in_guess_ini )
							{	// If current guess value is less than initial value, then:

								// If lower bound is already set OR upper bound is not set, can't generate new guess
								if( is_lower_bound || !is_upper_bound )
								{
									T_rec_in_exit_mode = NO_SOLUTION;
									T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;	// exit while() on diff_T_rec_in
								}

								T_rec_in_lower = T_rec_in_guess;
								is_lower_bound = true;
								is_lower_error = false;
								// At this point, both upper and lower bound should exist, so we can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}
							else
							{	// If current guess value is greater than initial value, then:

								// If upper bound is already set OR lower bound is not set, can't generate new guess
								if( is_upper_bound || !is_lower_bound )
								{
									T_rec_in_exit_mode = NO_SOLUTION;
									T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									return;	// exit while() on diff_T_rec_in
								}

								T_rec_in_upper = T_rec_in_guess;
								is_upper_bound = true;
								is_upper_error = false;
								// At this point, both upper and lower bound should exist, so we can generate new guess
								// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
								diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
								continue;
							}
						}	// end else on if(iter_T_rec_in == 1)					
					}	// end logic to determine path if receiver is off or did not solve

					// Call the power cycle in STARTUP_CONTROLLED mode
					mc_pc_inputs.m_m_dot = 0.0;
					mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
					mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP_CONTROLLED;

					mc_power_cycle.call(mc_weather.ms_outputs,
						mc_pc_htf_state_in,
						mc_pc_inputs,
						mc_pc_out_solver,
						mc_pc_out_report,
						mc_sim_info);

					// Check for new PC startup timestep, probably will find one here
					step_pc_su = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)

					// Get mass flow rate PC requires for Controlled Startup
					double m_dot_pc = mc_pc_out_solver.m_m_dot_htf;				//[kg/hr]

					// Reset mass flow rate in 'mc_pc_htf_state'
					mc_pc_inputs.m_m_dot = mc_pc_out_solver.m_m_dot_htf;		//[kg/hr]

					// Calculate mass flow remaining to charge storage
					double m_dot_tes_ch = mc_cr_out_solver.m_m_dot_salt_tot - mc_pc_inputs.m_m_dot;	//[kg/hr]

					// Check that m_dot_tes_ch is > 0.0
					// If not, I *guess* we could try increasing the receiver inlet temperature (thereby increasing the mass flow rate)
					//      IF the upper bound is not already set.
					if( m_dot_tes_ch < 0.0 )
					{
						// First check if upper bound exists
						if( is_upper_bound )
						{
							T_rec_in_exit_mode = NO_SOLUTION;
							T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;	// exits while() on T_rec_in
						}
					
						T_rec_in_lower = T_rec_in_guess;
						is_lower_bound = true;
						is_lower_error = false;

						// At this point, both upper bound should exist, so we can generate new guess
						// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
						diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
						continue;
					}

					double T_tes_cold_out = std::numeric_limits<double>::quiet_NaN();
					bool ch_solved = mc_tes.charge(step_pc_su, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_tes_ch / 3600.0, mc_cr_out_solver.m_T_salt_hot + 273.15,
							T_tes_cold_out, mc_tes_outputs);
					T_tes_cold_out -= 273.15;		//[C] convert back from K

					// Check if TES.charge method solved.
					// If not, I *guess* we could try decreasing the receiver inlet temperature (thereby decreasing the mass flow rate)
					//       IF the lower bound is not already set.
					if( !ch_solved )
					{
						// First, check if low bound exists
						if( is_lower_bound )
						{
							T_rec_in_exit_mode = NO_SOLUTION;
							T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;	// exits while() on T_rec_in
						}

						T_rec_in_upper = T_rec_in_guess;
						is_upper_bound = true;
						is_upper_error = false;
						
						// At this point, both upper bound should exist, so we can generate new guess
						// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
						diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
						continue;
					}

					// HTF Charging State
					mc_tes_ch_htf_state.m_m_dot = m_dot_tes_ch;						//[kg/hr]
					mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;		//[C]
					mc_tes_ch_htf_state.m_temp_out = T_tes_cold_out;				//[C]

					// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
					mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
					mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
					mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
					
					// Enthalpy balance to calculate T_rec_in
					double T_rec_in_calc = (m_dot_tes_ch*T_tes_cold_out + m_dot_pc*mc_pc_out_solver.m_T_htf_cold)/mc_cr_out_solver.m_m_dot_salt_tot;

					diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;		//[-]

				}	// end while() on T_rec_in

				// Check if solver converged or a new operating mode is required
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance
					
						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{	// Convergence within Relaxed Tolerance, *Report message* but assume timestep solved in this mode

						error_msg = util::format("At time = %lg CR_ON__PC_SU__TES_CH__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);
						T_rec_in_exit_mode = CONVERGED;
					}				
				}

				// But if the loop above crashes because storage is over-charged, breaking 'false' here will move to mode that doesn't charge storage at all
				//  which essentially means the power cycle is dumping extra energy it doesn't need for startup...

				if( T_rec_in_exit_mode != CONVERGED )
				{
					m_is_CR_ON__PC_SU__TES_CH__AUX_OFF_avail = false;
					are_models_converged = false;
					break;
				}

				// Check reported timestep against initial timestep
				if( step_pc_su < mc_sim_info.m_step - step_tolerance )
				{
					is_sim_timestep_complete = false;
					mc_sim_info.m_step = step_pc_su;
					mc_sim_info.m_time = time_previous + step_pc_su;
				}

				are_models_converged = true;
			
			}
				break;

			case CR_DF__PC_SU__TES_FULL__AUX_OFF:
			{
				// The PC is operating at the maximum startup thermal power
				// TES is fully charged at the end of the timestep
				// The CR is still delivering too much mass flow rate and needs to be defocused

				// Set up iteration to find defocus that results in completely filled storage
				// Upper bound, error, and booleans
				double defocus_upper = 1.0;
				double y_defocus_upper = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_upper_bound = true;
				bool is_defocus_upper_error = false;

				// Lower bound, error, and booleans
				double defocus_lower = std::numeric_limits<double>::quiet_NaN();
				double y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_lower_bound = false;
				bool is_defocus_lower_error = false;

				double tol = 0.001;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				// Estimate required defocus
					// Max charging mass flow rate
				double T_htf_tes_out_est, m_dot_htf_tes_max_est;
				T_htf_tes_out_est = m_dot_htf_tes_max_est = std::numeric_limits<double>::quiet_NaN();
				mc_tes.charge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_T_htf_cold_des,
					T_htf_tes_out_est, m_dot_htf_tes_max_est, mc_tes_outputs);
				m_dot_htf_tes_max_est *= 3600.0;		//[kg/hr] convert from kg/s

					// Max (estimate) PC mass flow rate
				double m_dot_pc_max_est = m_m_dot_pc_des*(q_dot_pc_su_max/m_cycle_q_dot_des);	//[kg/hr]

				double defocus_guess_ini = fmin(1.0, (m_dot_htf_tes_max_est + m_dot_pc_max_est) / mc_cr_out_solver.m_m_dot_salt_tot);
				double defocus_guess = defocus_guess_ini;
				
				// Defocus: 1 = full power, 0 = no power
				double diff_m_dot = 999.9*tol;

				int defocus_exit_mode = CONVERGED;
				double defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int iter_defocus = 0;

				// Exit information for outer loop on T_rec_in (needs to be accessible in this scope)
				int T_rec_in_exit_mode = CONVERGED;
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				double step_pc_su = std::numeric_limits<double>::quiet_NaN();

				double m_dot_htf_tes_out = std::numeric_limits<double>::quiet_NaN();
				double m_dot_tes_ch_balance = std::numeric_limits<double>::quiet_NaN();

				// Iterate on defocus to fully charge storage
				while( fabs(diff_m_dot) > tol || diff_m_dot != diff_m_dot )
				{
					iter_defocus++;		// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_defocus_bounds = defocus_upper - defocus_lower;
					if( diff_defocus_bounds / defocus_upper < tol / 2.0 )
					{
						if( diff_m_dot != diff_m_dot )
						{	// System models not converging, so need to shut them down

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = NO_SOLUTION;
							break;		// Get out of while()					
						}
						else if( 1.0 - defocus_lower < tol / 2.0 )
						{	// Full CR output is not fully charging storage and operating PC at max

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = UNDER_TARGET_PC;
							break;		// Get out of while() on defocus
						}
						else if( defocus_upper < 0.05 )
						{
							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = OVER_TARGET_PC;
							break;
						}
						else
						{	// Poor convergence between power delivered to PC and power requested

							defocus_exit_tolerance = diff_m_dot;
							defocus_exit_mode = POOR_CONVERGENCE;
							break;		// Get out of while()
						}
					}

					// Subsequent iterations need to re-calculate defocus
					if( iter_defocus > 1 )
					{
						if( diff_m_dot != diff_m_dot )		// Check if solution was found
						{	// CR-PC model did not converge, so we don't know anything about this defocus
							// However, we know that we should now have an upper or lower bound (else code would have exited from logic below)
							// But, check that bounds exist, just to be careful
							if( !is_defocus_lower_bound || !is_defocus_upper_bound )
							{

								defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							defocus_guess = 0.5*(defocus_lower + defocus_upper);
						}
						else if( diff_m_dot > 0.0 )		// q_dot was too high, decrease defocus
						{
							is_defocus_upper_bound = true;
							is_defocus_upper_error = true;
							defocus_upper = defocus_guess;		// Set upper bound
							y_defocus_upper = diff_m_dot;		// Set upper convergence error

							if( is_defocus_lower_bound && is_defocus_lower_error )	// False-position method
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_lower_bound )
							{
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmax(0.01, defocus_guess - 0.05);			// Could perhaps use last solution to make a smarter guess...
							}

						}
						else							// q_dot was too low, increase defocus 
						{
							is_defocus_lower_bound = true;
							is_defocus_lower_error = true;
							defocus_lower = defocus_guess;	// Set lower bound
							y_defocus_lower = diff_m_dot;	// Set lower convergence error

							if( is_defocus_upper_bound && is_defocus_upper_error )
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_upper_bound )
							{	// should always have upper bound, but keep this framework for consistency...
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmin(1.0, defocus_guess + 0.05);
							}
						}
					}

					
					// Guess and iterate for the collector-receiver inlet temperature
					double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
					double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

					// Lower bound could be freeze protection temperature...
					double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					// Booleans for bounds and convergence error
					bool is_upper_bound = false;
					bool is_lower_bound = false;
					bool is_upper_error = false;
					bool is_lower_error = false;

					double diff_T_rec_in = 999.9*tol;

					int iter_T_rec_in = 0;

					// Exit information for outer loop on T_rec_in
					T_rec_in_exit_mode = CONVERGED;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

					// Start iteration on T_rec_in
					while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
					{
						iter_T_rec_in++;		// First iteration = 1

						// Check if distance between bounds is "too small"
						double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
						if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
						{
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;
							}
							else
							{
								T_rec_in_exit_mode = POOR_CONVERGENCE;
								T_rec_in_exit_tolerance = diff_T_rec_in;
								break;
							}
						}

						// Subsequent iterations need to re-calculate T_in
						if( iter_T_rec_in > 1 )
						{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models did not solve such that a convergence error could be calculated
								// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
								// But, check that bounds exist
								if( !is_lower_bound || !is_upper_bound )
								{
									T_rec_in_exit_mode = NO_SOLUTION;
									T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;
								}
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
							{
								is_lower_bound = true;
								is_lower_error = true;
								T_rec_in_lower = T_rec_in_guess;		//[C]
								y_T_rec_in_lower = diff_T_rec_in;			//[-]

								if( is_upper_bound && is_upper_error )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_upper_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
								}
								else
								{
									T_rec_in_guess += 2.5;			//[C]
								}
							}
							else
							{
								is_upper_bound = true;
								is_upper_error = true;
								T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
								y_T_rec_in_upper = diff_T_rec_in;			//[-]

								if( is_lower_bound && is_upper_bound )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_lower_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
								}
								else
								{
									T_rec_in_guess -= 2.5;		//[C]
								}
							}
						}

						// Solve the receiver model with T_rec_in_guess and current defocus guess
						// CR ON
						mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
						mc_cr_inputs.m_field_control = defocus_guess;		//[-] no defocusing for initial simulation
						mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

						mc_collector_receiver.call(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_inputs,
							mc_cr_out_solver,
							mc_cr_out_report,
							mc_sim_info);


							// Check if receiver is OFF or model didn't solve
						if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
						{
							// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
							if( iter_T_rec_in == 1 )
							{
								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;  // exit while() on diff_T_rec_in
							}
							else
							{	// If collector-receiver model has solved with results previously, then try to find another guess value
								// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
								// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
								if( T_rec_in_guess < T_rec_in_guess_ini )
								{	// If current guess value is less than initial value, then:

									// If lower bound is already set OR upper bound is not set, can't generate new guess
									if( is_lower_bound || !is_upper_bound )
									{
										T_rec_in_exit_mode = NO_SOLUTION;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in
									}

									T_rec_in_lower = T_rec_in_guess;
									is_lower_bound = true;
									is_lower_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
								else
								{	// If current guess value is greater than initial value, then:

									// If upper bound is already set OR lower bound is not set, can't generate new guess
									if( is_upper_bound || !is_lower_bound )
									{
										T_rec_in_exit_mode = NO_SOLUTION;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in
									}

									T_rec_in_upper = T_rec_in_guess;
									is_upper_bound = true;
									is_upper_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
							}	// end else on if(iter_T_rec_in == 1)					
						}	// end logic to determine path if receiver is off or did not solve

						
						// Get receiver HTF outputs
						double m_dot_receiver = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr]
						double T_htf_rec_out = mc_cr_out_solver.m_T_salt_hot;			//[C]

						// ********************
						// Call PC STARTUP CONTROLLED
						mc_pc_inputs.m_m_dot = 0.0;
						mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
						mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP_CONTROLLED;

						mc_power_cycle.call(mc_weather.ms_outputs,
							mc_pc_htf_state_in,
							mc_pc_inputs,
							mc_pc_out_solver,
							mc_pc_out_report,
							mc_sim_info);

						// Check for new PC startup timestep, probably will find one here
						step_pc_su = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)

						// Get mass flow rate PC requires for Controlled Startup
						double m_dot_pc = mc_pc_out_solver.m_m_dot_htf;				//[kg/hr]

						// PC mass flow rate return temperature
						double T_pc_out = mc_pc_out_solver.m_T_htf_cold;		//[C]

						// Reset mass flow rate in 'mc_pc_htf_state'
						mc_pc_inputs.m_m_dot = mc_pc_out_solver.m_m_dot_htf;		//[kg/hr]

						// Calculate mass flow remaining to charge storage
						m_dot_tes_ch_balance = mc_cr_out_solver.m_m_dot_salt_tot - mc_pc_inputs.m_m_dot;	//[kg/hr]

						// Solve TES for FULL charge
						double T_htf_tes_out;
						T_htf_tes_out = m_dot_htf_tes_out = std::numeric_limits<double>::quiet_NaN();
						mc_tes.charge_full(step_pc_su, mc_weather.ms_outputs.m_tdry + 273.15, T_htf_rec_out + 273.15,
							T_htf_tes_out, m_dot_htf_tes_out, mc_tes_outputs);

						T_htf_tes_out -= 273.15;		//[C] convert from K
						m_dot_htf_tes_out *= 3600.0;	//[kg/hr] convert from kg/s

						// HTF charging state
						mc_tes_ch_htf_state.m_m_dot = m_dot_htf_tes_out;				//[kg/hr]
						mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;		//[C]
						mc_tes_ch_htf_state.m_temp_out = T_htf_tes_out;					//[C] convert from K

						// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
						mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
						mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
						mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K

						// Enthalpy balance to get T_rec_in_calc
						// Use TES outlet temperature, but *calculated* TES mass flow rate to conserve mass flow rate
						double T_rec_in_calc = (m_dot_tes_ch_balance*T_htf_tes_out + m_dot_pc*T_pc_out) / m_dot_receiver;

						// Calculate diff_T_rec_in
						diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;

					}	// end while() on T_rec_in

					// Check T_rec_in_exit_mode
					if( T_rec_in_exit_mode != CONVERGED && T_rec_in_exit_mode != POOR_CONVERGENCE )
					{
						// Assume this means we've hit the lower bound on defocus
						is_defocus_lower_bound = true;
						is_defocus_lower_error = false;
						defocus_lower = defocus_guess;
						y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
						diff_m_dot = std::numeric_limits<double>::quiet_NaN();

						continue;
					}
					
					// Calculate mass balance error
					// (+) should be too much CR m_dot
					// (-) should be not enough CR m_dot
					diff_m_dot = (m_dot_tes_ch_balance - m_dot_htf_tes_out)/m_dot_htf_tes_out;

				}	// end while() on defocus

				// Did system solve with a defocus < 1.0?
				// Controller hierarchy doesn't allow to go back to No Defocus and PC_RM, so check that defocus is <= 1
				if( defocus_exit_mode == UNDER_TARGET_PC )
				{
					error_msg = util::format("At time = %lg CR_DF__PC_SU__TES_FULL__AUX_OFF method converged to a power cycle"
						" thermal input less than the target.",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::NOTICE, error_msg);

					defocus_exit_mode = CONVERGED;
				}

				// Handle exit modes
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_SU__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(defocus_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						defocus_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_SU__TES_FULL__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, defocus_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						defocus_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					are_models_converged = false;
					m_is_CR_DF__PC_SU__TES_FULL__AUX_OFF_avail = false;
					break;
				}

				// Set member defocus
				m_defocus = defocus_guess;

				are_models_converged = true;

			}
				break;

			case CR_DF__PC_SU__TES_OFF__AUX_OFF:
			{
				// Running the CR at full power results in too much thermal power for power cycle start up
				// No available TES charging
				// So, must defocus CR during PC startup

				// This mode fails to CR_ON__PC_SU__TES_OFF__AUX_OFF

				// Get collector-receiver performance with no defocus
				mc_cr_htf_state_in.m_temp = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
				mc_cr_inputs.m_field_control = 1.0;						//[-] no defocusing for initial simulation
				mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;
				mc_collector_receiver.call(mc_weather.ms_outputs,
					mc_cr_htf_state_in,
					mc_cr_inputs,
					mc_cr_out_solver,
					mc_cr_out_report,
					mc_sim_info);

				double defocus_guess_ini = std::numeric_limits<double>::quiet_NaN();
				if( mc_cr_out_solver.m_q_thermal > 0.0 )
				{
					// Controller hierarchy doesn't allow to go back to No Defocus and PC_RM, so check that defocus is <= 1
					defocus_guess_ini = fmin(1.0, q_dot_pc_su_max / mc_cr_out_solver.m_q_thermal);
				}
				else
				{
					// CR not producing power at design inlet temperature

					// Weird that controller chose Defocus operating mode, so report message and shut down CR and PC
					error_msg = util::format("At time = %lg the controller chose CR_DF__PC_SU__TES_OFF__AUX_OFF operating mode, but the collector/receiver"
						"did not produce power with the design inlet temperature and no defocus. Trying next operating mode...",
						mc_sim_info.m_time / 3600.0);
					mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

					are_models_converged = false;

					m_is_CR_DF__PC_SU__TES_OFF__AUX_OFF_avail = false;

					break;
				}

				double defocus_guess = defocus_guess_ini;

				// Set up iteration to find defocus that results in completely filled storage
				// Upper bound, error, and booleans
				double defocus_upper = 1.0;
				double y_defocus_upper = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_upper_bound = true;
				bool is_defocus_upper_error = false;

				// Lower bound, error, and booleans
				double defocus_lower = std::numeric_limits<double>::quiet_NaN();
				double y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
				bool is_defocus_lower_bound = false;
				bool is_defocus_lower_error = false;
			
				double tol = 0.001;		//[-]

				double relaxed_tol_mult = 5.0;				//[-]
				double relaxed_tol = relaxed_tol_mult*tol;	//[-]

				// Defocus: 1 = full power, 0 = no_power
				double diff_m_dot = 999.9*tol;

				int defocus_exit_mode = CONVERGED;
				double defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				int iter_defocus = 0;

				// Exit information for outer loop on T_rec_in (needs to be accessible in this scope)
				int T_rec_in_exit_mode = CONVERGED;
				double T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

				double step_pc_su = std::numeric_limits<double>::quiet_NaN();

				// Iterate on defocus so maximum power cycle thermal power is not exceeded
				while( fabs(diff_m_dot) > tol || diff_m_dot != diff_m_dot )
				{
					iter_defocus++;			// First iteration = 1

					// Check if distance between bounds is "too small" (using 'bounds_tol' defined above)
					double diff_defocus_bounds = defocus_upper - defocus_lower;
					if( diff_defocus_bounds / defocus_upper < tol / 2.0 )
					{
						if( diff_m_dot != diff_m_dot )
						{	// System models not converging, so need to shut them down

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = NO_SOLUTION;
							break;		// Get out of while()					
						}
						else if( 1.0 - defocus_lower < tol / 2.0 )
						{	// Full CR output is not fully charging storage and operating PC at max

							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = UNDER_TARGET_PC;
							break;		// Get out of while() on defocus
						}
						else if( defocus_upper < 0.05 )
						{
							defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							defocus_exit_mode = OVER_TARGET_PC;
							break;
						}
						else
						{	// Poor convergence between power delivered to PC and power requested

							defocus_exit_tolerance = diff_m_dot;
							defocus_exit_mode = POOR_CONVERGENCE;
							break;		// Get out of while()
						}
					}

					// Subsequent iterations need to re-calculate defocus
					if( iter_defocus > 1 )
					{
						if( diff_m_dot != diff_m_dot )		// Check if solution was found
						{	// CR-PC model did not converge, so we don't know anything about this defocus
							// However, we know that we should now have an upper or lower bound (else code would have exited from logic below)
							// But, check that bounds exist, just to be careful
							if( !is_defocus_lower_bound || !is_defocus_upper_bound )
							{

								defocus_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								defocus_exit_mode = NO_SOLUTION;
								break;		// Get out of while()	
							}
							defocus_guess = 0.5*(defocus_lower + defocus_upper);
						}
						else if( diff_m_dot > 0.0 )		// q_dot was too high, decrease defocus
						{
							is_defocus_upper_bound = true;
							is_defocus_upper_error = true;
							defocus_upper = defocus_guess;		// Set upper bound
							y_defocus_upper = diff_m_dot;		// Set upper convergence error

							if( is_defocus_lower_bound && is_defocus_lower_error )	// False-position method
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_lower_bound )
							{
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmax(0.01, defocus_guess - 0.05);			// Could perhaps use last solution to make a smarter guess...
							}

						}
						else							// q_dot was too low, increase defocus 
						{
							is_defocus_lower_bound = true;
							is_defocus_lower_error = true;
							defocus_lower = defocus_guess;	// Set lower bound
							y_defocus_lower = diff_m_dot;	// Set lower convergence error

							if( is_defocus_upper_bound && is_defocus_upper_error )
							{
								defocus_guess = y_defocus_upper / (y_defocus_upper - y_defocus_lower)*(defocus_lower - defocus_upper) + defocus_upper;
							}
							else if( is_defocus_upper_bound )
							{	// should always have upper bound, but keep this framework for consistency...
								defocus_guess = 0.5*(defocus_upper + defocus_lower);
							}
							else
							{
								defocus_guess = fmin(1.0, defocus_guess + 0.05);
							}
						}
					}

					// Guess and iterate for the collector-receiver inlet temperature
					double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
					double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

					// Lower bound could be freeze protection temperature...
					double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
					double y_T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
					// Booleans for bounds and convergence error
					bool is_upper_bound = false;
					bool is_lower_bound = false;
					bool is_upper_error = false;
					bool is_lower_error = false;

					double diff_T_rec_in = 999.9*tol;

					int iter_T_rec_in = 0;

					// Exit information for outer loop on T_rec_in
					T_rec_in_exit_mode = CONVERGED;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

					// Start iteration on T_rec_in
					while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
					{
						iter_T_rec_in++;		// First iteration = 1

						// Check if distance between bounds is "too small"
						double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
						if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
						{
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;
							}
							else
							{
								T_rec_in_exit_mode = POOR_CONVERGENCE;
								T_rec_in_exit_tolerance = diff_T_rec_in;
								break;
							}
						}

						// Subsequent iterations need to re-calculate T_in
						if( iter_T_rec_in > 1 )
						{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
							if( diff_T_rec_in != diff_T_rec_in )
							{	// Models did not solve such that a convergence error could be calculated
								// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
								// But, check that bounds exist
								if( !is_lower_bound || !is_upper_bound )
								{
									T_rec_in_exit_mode = NO_SOLUTION;
									T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
									break;
								}
								T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
							}
							else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
							{
								is_lower_bound = true;
								is_lower_error = true;
								T_rec_in_lower = T_rec_in_guess;		//[C]
								y_T_rec_in_lower = diff_T_rec_in;			//[-]

								if( is_upper_bound && is_upper_error )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_upper_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
								}
								else
								{
									T_rec_in_guess += 2.5;			//[C]
								}
							}
							else
							{
								is_upper_bound = true;
								is_upper_error = true;
								T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
								y_T_rec_in_upper = diff_T_rec_in;			//[-]

								if( is_lower_bound && is_upper_bound )
								{
									T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
								}
								else if( is_lower_bound )
								{
									T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
								}
								else
								{
									T_rec_in_guess -= 2.5;		//[C]
								}
							}
						}

						// Solve the receiver model with T_rec_in_guess and current defocus guess
						// CR ON
						mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
						mc_cr_inputs.m_field_control = defocus_guess;		//[-] no defocusing for initial simulation
						mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

						mc_collector_receiver.call(mc_weather.ms_outputs,
							mc_cr_htf_state_in,
							mc_cr_inputs,
							mc_cr_out_solver,
							mc_cr_out_report,
							mc_sim_info);

							// Check if receiver is OFF or model didn't solve
						if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
						{
							// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
							if( iter_T_rec_in == 1 )
							{
								T_rec_in_exit_mode = NO_SOLUTION;
								T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
								break;  // exit while() on diff_T_rec_in
							}
							else
							{	// If collector-receiver model has solved with results previously, then try to find another guess value
								// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
								// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
								if( T_rec_in_guess < T_rec_in_guess_ini )
								{	// If current guess value is less than initial value, then:

									// If lower bound is already set OR upper bound is not set, can't generate new guess
									if( is_lower_bound || !is_upper_bound )
									{
										T_rec_in_exit_mode = NO_SOLUTION;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in
									}

									T_rec_in_lower = T_rec_in_guess;
									is_lower_bound = true;
									is_lower_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
								else
								{	// If current guess value is greater than initial value, then:

									// If upper bound is already set OR lower bound is not set, can't generate new guess
									if( is_upper_bound || !is_lower_bound )
									{
										T_rec_in_exit_mode = NO_SOLUTION;
										T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
										break;	// exit while() on diff_T_rec_in
									}

									T_rec_in_upper = T_rec_in_guess;
									is_upper_bound = true;
									is_upper_error = false;
									// At this point, both upper and lower bound should exist, so we can generate new guess
									// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
									diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
									continue;
								}
							}	// end else on if(iter_T_rec_in == 1)					
						}	// end logic to determine path if receiver is off or did not solve

						// Get receiver HTF outputs
						double m_dot_receiver = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr]
						double T_htf_rec_out = mc_cr_out_solver.m_T_salt_hot;			//[C]

						// ********************
						// Call PC STARTUP CONTROLLED
						mc_pc_inputs.m_m_dot = 0.0;
						mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
						mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP_CONTROLLED;

						mc_power_cycle.call(mc_weather.ms_outputs,
							mc_pc_htf_state_in,
							mc_pc_inputs,
							mc_pc_out_solver,
							mc_pc_out_report,
							mc_sim_info);

						// Check for new PC startup timestep, probably will find one here
						step_pc_su = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)

						// Get mass flow rate PC requires for Controlled Startup
						double m_dot_pc = mc_pc_out_solver.m_m_dot_htf;				//[kg/hr]

						// PC mass flow rate return temperature
						double T_pc_out = mc_pc_out_solver.m_T_htf_cold;		//[C]

						// Reset mass flow rate in 'mc_pc_htf_state'
						mc_pc_inputs.m_m_dot = mc_pc_out_solver.m_m_dot_htf;		//[kg/hr]

						// Calculate diff_T_rec_in
						diff_T_rec_in = (T_pc_out - T_rec_in_guess) / T_rec_in_guess;	//[-]

					}	// end while() on T_rec_in

					// Check T_rec_in_exit_mode
					if( T_rec_in_exit_mode != CONVERGED && T_rec_in_exit_mode != POOR_CONVERGENCE )
					{
						// Assume this means we've hit the lower bound on defocus
						is_defocus_lower_bound = true;
						is_defocus_lower_error = false;
						defocus_lower = defocus_guess;
						y_defocus_lower = std::numeric_limits<double>::quiet_NaN();
						diff_m_dot = std::numeric_limits<double>::quiet_NaN();

						continue;
					}

					// Calculate mass balance error
					// (+) should be too much CR m_dot
					// (-) should be not enough CR m_dot
					diff_m_dot = (mc_cr_out_solver.m_m_dot_salt_tot - mc_pc_inputs.m_m_dot) / mc_pc_inputs.m_m_dot;
				
				}	// end while() on defocus

				// Handle exit modes
				if( T_rec_in_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(T_rec_in_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						T_rec_in_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_SU__TES_OFF__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, T_rec_in_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						T_rec_in_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode == POOR_CONVERGENCE )
				{
					if( fabs(defocus_exit_tolerance) > relaxed_tol )
					{	// Did not converge within Relaxed Tolerance

						defocus_exit_mode = NO_SOLUTION;
					}
					else
					{
						error_msg = util::format("At time = %lg CR_DF__PC_SU__TES_OFF__AUX_OFF method only reached a convergence"
							"= %lg. Check that results at this timestep are not unreasonably biasing total simulation results",
							mc_sim_info.m_time / 3600.0, defocus_exit_tolerance);

						mc_csp_messages.add_message(C_csp_messages::WARNING, error_msg);

						defocus_exit_mode = CONVERGED;
					}
				}

				if( defocus_exit_mode != CONVERGED || T_rec_in_exit_mode != CONVERGED )
				{
					are_models_converged = false;
					m_is_CR_DF__PC_SU__TES_OFF__AUX_OFF_avail = false;
					break;
				}

				// Set member defocus
				m_defocus = defocus_guess;

				are_models_converged = true;

			}
				break;		// end CR_DF__PC_SU__TES_OFF__AUX_OFF

			default: 
				throw(C_csp_exception("Operation mode not recognized",""));

			}	// End switch() on receiver operating modes
		
		}	
        
        /* 
        ------------ End loop to find correct operating mode and system performance --------
        */


		// Calculate system-level parasitics: can happen after controller/solver converges
		double W_dot_fixed = ms_system_params.m_pb_fixed_par*m_cycle_W_dot_des;			//[MWe]

		double W_dot_ratio = mc_pc_out_solver.m_P_cycle / m_cycle_W_dot_des;		//[-]

		double W_dot_bop = m_cycle_W_dot_des*ms_system_params.m_bop_par*ms_system_params.m_bop_par_f *
			(ms_system_params.m_bop_par_0 + ms_system_params.m_bop_par_1*W_dot_ratio + ms_system_params.m_bop_par_2*pow(W_dot_ratio,2));
			// [MWe]

		double W_dot_net = mc_pc_out_solver.m_P_cycle - 
			mc_cr_out_solver.m_W_dot_col_tracking -
			mc_cr_out_solver.m_W_dot_htf_pump - 
			(mc_pc_out_solver.m_W_dot_htf_pump + mc_tes_outputs.m_W_dot_rhtf_pump) -
			mc_pc_out_solver.m_W_cool_par -
			mc_tes_outputs.m_q_heater - 
			W_dot_fixed -
			W_dot_bop;	//[MWe]


        // Timestep solved: run post-processing, converged()		
		mc_collector_receiver.converged();
		mc_power_cycle.converged();
		mc_tes.converged();
		
        //update the tracked field generation
        disp_qsf_last = mc_cr_out_solver.m_q_startup > 0. ? mc_cr_out_solver.m_q_thermal : 0.;    //only count if not starting up

        //Update the estimated thermal energy storage charge state
        double e_tes_disch = 0.;
        if(m_is_tes)
        {
            double mdot_disch, Tdisch;
            mc_tes.discharge_avail_est( m_T_htf_cold_des, mc_sim_info.m_step, e_tes_disch, mdot_disch, Tdisch);

            e_tes_disch *= mc_sim_info.m_step / 3600.;  //MWh
        }

		// Don't converge weather file if working with partial timesteps
		if( !is_sim_timestep_complete )
		{
			// Calculate new timestep
			step_local = time_sim_step_next - mc_sim_info.m_time;
		}
		else
		{
			// If partial timestep, use constant weather data for all partial timesteps
			mc_weather.converged();

			step_local = m_sim_step_size_baseline;

			time_sim_step_next += m_sim_step_size_baseline;
		}


		double step_hr = mc_sim_info.m_step/3600.0;
		// Save timestep outputs
		// This is after timestep convergence, so be sure convergence() methods don't unexpectedly change outputs
		
			// Simulation outputs
		mvv_outputs_temp[TIME_FINAL].push_back(mc_sim_info.m_time);				//[s] Time at end of timestep		
		mvv_outputs_temp[N_OP_MODES].push_back(0.0);                            //[-] Need to push this back, but elements aren't used
		mvv_outputs_temp[ERR_M_DOT].push_back(0.0);
		mvv_outputs_temp[ERR_Q_DOT].push_back(0.0);

		if( mvv_outputs_temp[TIME_FINAL].size() == 1 )
		{
			mvv_outputs_temp[OP_MODE_1].push_back(operating_mode);
			mvv_outputs_temp[OP_MODE_2].push_back(0.0);
			mvv_outputs_temp[OP_MODE_3].push_back(0.0);
		}
		else if( mvv_outputs_temp[TIME_FINAL].size() == 2 )
		{
			mvv_outputs_temp[OP_MODE_1].push_back(mvv_outputs_temp[OP_MODE_1][0]);
			mvv_outputs_temp[OP_MODE_2].push_back(operating_mode);
			mvv_outputs_temp[OP_MODE_3].push_back(0.0);
		}
		else if( mvv_outputs_temp[TIME_FINAL].size() >= 3 )
		{
			mvv_outputs_temp[OP_MODE_1].push_back(mvv_outputs_temp[OP_MODE_1][0]);
			mvv_outputs_temp[OP_MODE_2].push_back(mvv_outputs_temp[OP_MODE_2][1]);
			mvv_outputs_temp[OP_MODE_3].push_back(operating_mode);
		}
		
		mvv_outputs_temp[TOU_PERIOD].push_back(tou_period);         //[-] 
		mvv_outputs_temp[PRICING_MULT].push_back(pricing_mult);		//[-]
		mvv_outputs_temp[PC_Q_DOT_SB].push_back(q_pc_sb);           //[MW]
		mvv_outputs_temp[PC_Q_DOT_MIN].push_back(q_pc_min);         //[MW]
		mvv_outputs_temp[PC_Q_DOT_TARGET].push_back(q_pc_target);   //[MW]
		mvv_outputs_temp[PC_Q_DOT_MAX].push_back(q_pc_max);         //[MW]
		mvv_outputs_temp[CTRL_IS_REC_SU].push_back((int)is_rec_su_allowed);     //[-]
		mvv_outputs_temp[CTRL_IS_PC_SU].push_back((int)is_pc_su_allowed);       //[-]
		mvv_outputs_temp[CTRL_IS_PC_SB].push_back((int)is_pc_sb_allowed);       //[-]
		mvv_outputs_temp[EST_Q_DOT_CR_SU].push_back(q_dot_cr_startup);          //[MWt]
		mvv_outputs_temp[EST_Q_DOT_CR_ON].push_back(q_dot_cr_on);               //[MWt]
		mvv_outputs_temp[EST_Q_DOT_DC].push_back(q_dot_tes_dc);                 //[MWt]
		mvv_outputs_temp[EST_Q_DOT_CH].push_back(q_dot_tes_ch);                 //[MWt]

		mvv_outputs_temp[SOLZEN].push_back(mc_weather.ms_outputs.m_solzen);		//[deg] Solar zenith
		mvv_outputs_temp[BEAM].push_back(mc_weather.ms_outputs.m_beam);		    //[W/m2] DNI

			// Collector-receiver outputs
		mvv_outputs_temp[CR_Q_INC].push_back(mc_cr_out_report.m_q_dot_field_inc);	//[MWt] Field incident thermal power
		mvv_outputs_temp[CR_OPT_ETA].push_back(mc_cr_out_report.m_eta_field);	        //[-] Field efficiency (= eta_field_full * defocus)
		mvv_outputs_temp[CR_DEFOCUS].push_back(m_defocus);                          //[-] Defocus
		mvv_outputs_temp[REC_Q_DOT_INC].push_back(mc_cr_out_report.m_q_dot_rec_inc);   //[MWt] Rec. incident thermal power
		mvv_outputs_temp[REC_ETA_THERMAL].push_back(mc_cr_out_report.m_eta_thermal);   //[-] Receiver thermal efficiency    
		mvv_outputs_temp[REC_Q_DOT].push_back(mc_cr_out_solver.m_q_thermal);           //[MWt] Receiver thermal power output  
		mvv_outputs_temp[REC_M_DOT].push_back(mc_cr_out_solver.m_m_dot_salt_tot);      //[kg/hr] Receiver mass flow rate output          
		mvv_outputs_temp[REC_Q_DOT_STARTUP].push_back(mc_cr_out_solver.m_q_startup/step_hr);		//[MWt] Receiver startup thermal power, convert from MWt-hr  
		mvv_outputs_temp[REC_T_IN].push_back(mc_cr_htf_state_in.m_temp);            //[C] Receiver HTF inlet temperature           
		mvv_outputs_temp[REC_T_OUT].push_back(mc_cr_out_solver.m_T_salt_hot);          //[C] Receiver HTF outlet temperature          
		mvv_outputs_temp[CR_Q_DOT_PIPING_LOSS].push_back(mc_cr_out_report.m_q_dot_piping_loss);    //[MWt] Tower piping thermal power loss

			// Power cycle outputs
		mvv_outputs_temp[PC_ETA_THERMAL].push_back(mc_pc_out_report.m_eta);            //[-] Power cycle efficiency (gross - no parasitics outside of power block)
		mvv_outputs_temp[PC_Q_DOT].push_back(mc_pc_out_solver.m_q_dot_htf);            //[MWt] Power cycle input thermal power
		mvv_outputs_temp[PC_M_DOT].push_back(mc_pc_inputs.m_m_dot);              //[kg/hr] Mass flow rate to power cycle
		//mvv_outputs_temp[PC_Q_DOT_STARTUP].push_back(mc_pc_outputs.m_q_startup);    //[MWt-hr] Power cycle startup thermal energy
		mvv_outputs_temp[PC_Q_DOT_STARTUP].push_back(mc_pc_out_report.m_q_startup);    //[MWt] Power cycle startup thermal energy
		mvv_outputs_temp[PC_W_DOT].push_back(mc_pc_out_solver.m_P_cycle);              //[MWe] Power cycle electric gross power (only parasitics baked into regression)
		mvv_outputs_temp[PC_T_IN].push_back(mc_pc_htf_state_in.m_temp);             //[C] Power cycle HTF inlet temperature
		mvv_outputs_temp[PC_T_OUT].push_back(mc_pc_out_solver.m_T_htf_cold);           //[C] Power cycle HTF outlet temperature
		mvv_outputs_temp[PC_M_DOT_WATER].push_back(mc_pc_out_report.m_m_dot_makeup);	//[kg/s] Cycle water consumption: makeup + cooling

			// Thermal energy storage outputs
		mvv_outputs_temp[TES_Q_DOT_LOSS].push_back(mc_tes_outputs.m_q_dot_loss);       //[MWt] TES thermal power losses to environment
		mvv_outputs_temp[TES_W_DOT_HEATER].push_back(mc_tes_outputs.m_q_heater);       //[MWe] Energy into TES from heaters (hot+cold) to maintain tank temperatures
		mvv_outputs_temp[TES_T_HOT].push_back(mc_tes_outputs.m_T_hot_final-273.15);    //[C] TES hot temperature at end of timestep
		mvv_outputs_temp[TES_T_COLD].push_back(mc_tes_outputs.m_T_cold_final-273.15);  //[C] TES cold temperature at end of timestep
		mvv_outputs_temp[TES_Q_DOT_DC].push_back(mc_tes_outputs.m_q_dot_dc_to_htf);    //[MWt] TES discharge thermal power
		mvv_outputs_temp[TES_Q_DOT_CH].push_back(mc_tes_outputs.m_q_dot_ch_from_htf);  //[MWt] TES charge thermal power
        mvv_outputs_temp[TES_E_CH_STATE].push_back(e_tes_disch);                       //[MWht] TES charge state
		mvv_outputs_temp[TES_M_DOT_DC].push_back(mc_tes_dc_htf_state.m_m_dot);         //[kg/hr] TES mass flow rate discharge
		mvv_outputs_temp[TES_M_DOT_CH].push_back(mc_tes_ch_htf_state.m_m_dot);         //[kg/hr] TES mass flow rate charge

			// Parasitics outputs
		mvv_outputs_temp[COL_W_DOT_TRACK].push_back(mc_cr_out_solver.m_W_dot_col_tracking);    //[MWe] Collector tracking, startup, stow power consumption
		mvv_outputs_temp[CR_W_DOT_PUMP].push_back(mc_cr_out_solver.m_W_dot_htf_pump);          //[MWe] Receiver/tower HTF pumping power
		mvv_outputs_temp[SYS_W_DOT_PUMP].push_back((mc_pc_out_solver.m_W_dot_htf_pump + mc_tes_outputs.m_W_dot_rhtf_pump));    //[MWe] TES & PC HTF pumping power (Receiver - PC side HTF)
		mvv_outputs_temp[PC_W_DOT_COOLING].push_back(mc_pc_out_solver.m_W_cool_par);           //[MWe] Power cycle cooling power consumption (fan, pumps, etc.)
		mvv_outputs_temp[SYS_W_DOT_FIXED].push_back(W_dot_fixed);                           //[MWe] Fixed electric parasitic power load
		mvv_outputs_temp[SYS_W_DOT_BOP].push_back(W_dot_bop);                               //[MWe] Balance-of-plant electric parasitic power load

		mvv_outputs_temp[W_DOT_NET].push_back(W_dot_net);				//[MWe] Total electric power output to grid
		
            //Dispatch optimization outputs
        mvv_outputs_temp[DISPATCH_SOLVE_STATE].push_back(dispatch.outputs.solve_state);
        mvv_outputs_temp[DISPATCH_SOLVE_ITER].push_back(dispatch.outputs.solve_iter);
        mvv_outputs_temp[DISPATCH_SOLVE_OBJ].push_back(dispatch.outputs.objective);
        mvv_outputs_temp[DISPATCH_SOLVE_OBJ_RELAX].push_back(dispatch.outputs.objective_relaxed);
        
        mvv_outputs_temp[DISPATCH_QSF_EXPECT].push_back(disp_qsf_expect);
        mvv_outputs_temp[DISPATCH_QSFPROD_EXPECT].push_back(disp_qsfprod_expect);
        mvv_outputs_temp[DISPATCH_QSFSU_EXPECT].push_back(disp_qsfsu_expect);
        mvv_outputs_temp[DISPATCH_TES_EXPECT].push_back(disp_tes_expect);
        mvv_outputs_temp[DISPATCH_PCEFF_EXPECT].push_back(disp_etapb_expect);
        mvv_outputs_temp[DISPATCH_SFEFF_EXPECT].push_back(disp_etasf_expect);
        mvv_outputs_temp[DISPATCH_QPBSU_EXPECT].push_back(disp_qpbsu_expect);
        mvv_outputs_temp[DISPATCH_WPB_EXPECT].push_back(disp_wpb_expect);
        mvv_outputs_temp[DISPATCH_REV_EXPECT].push_back(disp_rev_expect);

        mvv_outputs_temp[DISPATCH_PRES_NCONSTR].push_back(dispatch.outputs.presolve_nconstr);
        mvv_outputs_temp[DISPATCH_PRES_NVAR].push_back(dispatch.outputs.presolve_nvar);
        mvv_outputs_temp[DISPATCH_SOLVE_TIME].push_back(dispatch.outputs.solve_time);

		// Report series of operating modes attempted during the timestep as a 'double' using 0s to separate the enumerations 
		// ... (10 is set as a dummy enumeration so it won't show up as a potential operating mode)
		int n_op_modes = m_op_mode_tracking.size();
		double op_mode_key = 0.0;
		for( int i = 0; i < fmin(3,n_op_modes); i++ )
		{
			double op_mode_step = m_op_mode_tracking[i];

			if( op_mode_step < 10.0 )
			{
				op_mode_key = 100.0*op_mode_key + 10.0*op_mode_step;
			}
			else
			{
				op_mode_key = 100.0*op_mode_key + op_mode_step;
			}
		}
		mvv_outputs_temp[CTRL_OP_MODE_SEQ_A].push_back(op_mode_key);				// Track the list of operating modes tried at each timestep
		//mv_operating_modes_a.push_back(op_mode_key);				// Track the list of operating modes tried at each timestep

		op_mode_key = 0.0;
		for( int i = 3; i < fmin(6,n_op_modes); i++ )
		{
			double op_mode_step = m_op_mode_tracking[i];

			if( op_mode_step < 10.0 )
			{
				op_mode_key = 100.0*op_mode_key + 10.0*op_mode_step;
			}
			else
			{
				op_mode_key = 100.0*op_mode_key + op_mode_step;
			}
		}
		mvv_outputs_temp[CTRL_OP_MODE_SEQ_B].push_back(op_mode_key);				// Track the list of operating modes tried at each timestep
		//mv_operating_modes_b.push_back(op_mode_key);				// Track the list of operating modes tried at each timestep

		op_mode_key = 0.0;
		for( int i = 6; i < n_op_modes; i++ )
		{
			double op_mode_step = m_op_mode_tracking[i];

			if( op_mode_step < 10.0 )
			{
				op_mode_key = 100.0*op_mode_key + 10.0*op_mode_step;
			}
			else
			{
				op_mode_key = 100.0*op_mode_key + op_mode_step;
			}
		}
		mvv_outputs_temp[CTRL_OP_MODE_SEQ_C].push_back(op_mode_key);				// Track the list of operating modes tried at each timestep
		//mv_operating_modes_c.push_back(op_mode_key);				// Track the list of operating modes tried at each timestep

		// ****************************************************
		//          End saving timestep outputs
		// ****************************************************

		// Now check if internal csp timestep matches reporting timestep
		do
		{			
			if(mc_sim_info.m_time >= m_report_time_end)
			{
				set_outputs_at_reporting_interval();

				// Advance time_reporting_hr index
				m_i_reporting++;
				m_report_time_start = m_report_time_end;	//[s]
				m_report_time_end += m_report_step;	//[s]			
			}
			else
			{
				break;
			}

		} while(true);

		

		// ***********************************
		//      Track time and step forward
		// ***********************************
		//is_sim_timestep_complete = true;
		time_previous = mc_sim_info.m_time;						//[s]
		mc_sim_info.m_step = step_local;						//[s]
		mc_sim_info.m_time = time_previous + step_local;		//[s]
					
		

	}	// End timestep loop

}	// End simulate() method

void C_csp_solver::set_outputs_at_reporting_interval()
{
	// Step through each uniform reporting period
	int n_report = mvv_outputs_temp[W_DOT_NET].size();

	if( n_report < 1 )
	{
		throw(C_csp_exception("No data to send to SSC", "Reporting Intervals"));
	}

	double time_prev = m_report_time_start;		//[s]

	// Save final time, and convert to hours
	mp_reporting_array[C_csp_solver::TIME_FINAL][m_i_reporting] = m_report_time_end/3600.0;		//[hr] time at end of reporting timestep, convert from s

	// Report number of csp solver operating modes (= timesteps) in reporting timestep
	mp_reporting_array[C_csp_solver::N_OP_MODES][m_i_reporting] = n_report;

	double m_dot_bal = 0.0;
	double q_dot_bal = 0.0;
	for( int i = 0; i < n_report; i++ )
	{
		m_dot_bal += 
			(mvv_outputs_temp[REC_M_DOT][i]       //[kg/hr]
			+ mvv_outputs_temp[TES_M_DOT_DC][i]   //[kg/hr]
			- mvv_outputs_temp[PC_M_DOT][i]       //[kg/hr]
			- mvv_outputs_temp[TES_M_DOT_CH][i])  //[kg/hr]
			/ (double) n_report;  
			
		q_dot_bal += 
			(mvv_outputs_temp[REC_Q_DOT][i]       //[MWt]
			+ mvv_outputs_temp[TES_Q_DOT_DC][i]   //[MWt]
			- mvv_outputs_temp[PC_Q_DOT][i]       //[MWt]
			- mvv_outputs_temp[TES_Q_DOT_CH][i])  //[MWt]
			/ (double) n_report;
	}
	mp_reporting_array[ERR_M_DOT][m_i_reporting] = m_dot_bal / m_m_dot_pc_des;
	mp_reporting_array[ERR_Q_DOT][m_i_reporting] = q_dot_bal / m_cycle_q_dot_des;

	double check_op_mode_1 = mvv_outputs_temp[OP_MODE_1][0];

	if( n_report == 1 )
	{
		mp_reporting_array[C_csp_solver::OP_MODE_1][m_i_reporting] = 
			mvv_outputs_temp[OP_MODE_1][0];
		mp_reporting_array[C_csp_solver::OP_MODE_2][m_i_reporting] = 0.0;
		mp_reporting_array[C_csp_solver::OP_MODE_3][m_i_reporting] = 0.0;
	}
	else if( mvv_outputs_temp[TIME_FINAL].size() == 2 )
	{
		mp_reporting_array[C_csp_solver::OP_MODE_1][m_i_reporting] = 
			mvv_outputs_temp[OP_MODE_1][0];
		mp_reporting_array[C_csp_solver::OP_MODE_2][m_i_reporting] =
			mvv_outputs_temp[OP_MODE_2][1];
		mp_reporting_array[C_csp_solver::OP_MODE_3][m_i_reporting] = 0.0;
	}
	else if( mvv_outputs_temp[TIME_FINAL].size() >= 3 )
	{
		mp_reporting_array[C_csp_solver::OP_MODE_1][m_i_reporting] =
			mvv_outputs_temp[OP_MODE_1][0];
		mp_reporting_array[C_csp_solver::OP_MODE_2][m_i_reporting] =
			mvv_outputs_temp[OP_MODE_2][1];
		mp_reporting_array[C_csp_solver::OP_MODE_3][m_i_reporting] = 
			mvv_outputs_temp[OP_MODE_3][2];
	}

	// ************************************************************
	// Set instantaneous outputs that are reported as the first value
	//   if multiple csp-timesteps for one reporting timestep
	// ************************************************************
	for( int j = C_csp_solver::TOU_PERIOD; j < C_csp_solver::SOLZEN; j++ )
	{
		mp_reporting_array[j][m_i_reporting] = mvv_outputs_temp[j][0];
	}

	// ***********************************************************
	//      Set outputs that are reported as weighted averages if 
	//       multiple csp-timesteps for one reporting timestep
	//    The following code assumes 'SOLZEN' is the first such output
	//    and that all names following it in 'E_reported_outputs' are weight averages
	// **************************************************************
	for( int j = C_csp_solver::SOLZEN; j < C_csp_solver::N_END; j++ )
	{
		time_prev = m_report_time_start;		//[s]
		for( int i = 0; i < n_report; i++ )
		{
			mp_reporting_array[j][m_i_reporting] += (fmin(mvv_outputs_temp[TIME_FINAL][i], m_report_time_end) - time_prev)*mvv_outputs_temp[j][i]; //[units]*[s]
			time_prev = fmin(mvv_outputs_temp[TIME_FINAL][i], m_report_time_end);
		}
		mp_reporting_array[j][m_i_reporting] /= m_report_step;
	}




	// Check if the most recent csp solver timestep aligns with the end of the reporting timestep
	bool delete_last_step = false;
	int pop_back_start = 1;

	if( mvv_outputs_temp[TIME_FINAL][n_report - 1] == m_report_time_end )
	{
		delete_last_step = true;
		pop_back_start = 0;
	}

	// If more than 1 element in temp vectors, only keep most recent value
	if( n_report > 1 || delete_last_step)
	{
		for( int j = 0; j < C_csp_solver::N_END; j++ )
		{
			if( !delete_last_step )
			{
				if( j == C_csp_solver::OP_MODE_1 )
				{
					if( n_report == 1 )
						mvv_outputs_temp[j][0] = mvv_outputs_temp[j][0];
					else if(n_report == 2)
						mvv_outputs_temp[j][0] = mvv_outputs_temp[C_csp_solver::OP_MODE_2][1];
					else
						mvv_outputs_temp[j][0] = mvv_outputs_temp[C_csp_solver::OP_MODE_3][1];

				}
				else if( j == C_csp_solver::OP_MODE_2 || j == C_csp_solver::OP_MODE_3 )
				{
					mvv_outputs_temp[j][0] = 0.0;
				}
				else
				{
					mvv_outputs_temp[j][0] = mvv_outputs_temp[j][n_report - 1];
				}				
			}

			for( int i = pop_back_start; i < n_report; i++ )
			{
				mvv_outputs_temp[j].pop_back();
			}
		}
	}

	// Populate post-processed outputs
	mp_post_proc_array[C_csp_solver::PC_Q_STARTUP][m_i_reporting] = 
		mp_reporting_array[PC_Q_DOT_STARTUP][m_i_reporting] * (m_report_step/3600.0);	//[MW]*[hr]

}

void C_csp_solver::solver_pc_su_controlled__tes_dc(double step_tol /*s*/,
	double &time_pc_su /*s*/, 
	int &exit_mode, double &T_pc_in_exit_tolerance)
{
	double T_pc_in_guess = mc_tes.get_hot_temp();

	double T_pc_in_upper = std::numeric_limits<double>::quiet_NaN();	//[K]
	double T_pc_in_lower = std::numeric_limits<double>::quiet_NaN();	//[K]

	double y_T_pc_in_upper = std::numeric_limits<double>::quiet_NaN();	//[-]
	double y_T_pc_in_lower = std::numeric_limits<double>::quiet_NaN();	//[-]

	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double tol_C = 1.0;								//[K]
	double tol = tol_C / m_cycle_T_htf_hot_des;		//[-]

	double diff_T_pc_in = 999.9*tol;

	int iter_T_pc_in = 0;

	exit_mode = NO_SOLUTION;
	T_pc_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	double T_pc_in_calc = std::numeric_limits<double>::quiet_NaN();

	while( fabs(diff_T_pc_in) > tol || diff_T_pc_in != diff_T_pc_in )
	{
		iter_T_pc_in++;			// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_pc_in_upper - T_pc_in_lower;
		if( diff_T_bounds / m_cycle_T_htf_hot_des < tol / 2.0 )
		{
			exit_mode = NO_SOLUTION;
			T_pc_in_exit_tolerance = diff_T_pc_in;
			break;
		}

		// Subsequent iterations need to re-calculate T_pc_in_guess
		if( iter_T_pc_in > 1 )
		{
			if( diff_T_pc_in != diff_T_pc_in )
			{	// Models did not solve such that a convergence error could be generated
				// If upper bound exists, then assume this is lower bound and try iterating
				// If upper bound does not exist, then we don't have information to move forward
				// ... if can't move forward, then assume not enough storage to meet *optimal* PC startup requirements
				// ... but, we can still attempt to begin PC startup during this timestep
				if( !is_upper_bound )
				{
					exit_mode = NO_SOLUTION;
					T_pc_in_exit_tolerance = diff_T_pc_in;
					break;
				}
				else
				{
					is_lower_bound = true;
					is_lower_error = false;
					T_pc_in_lower = T_pc_in_guess;
					T_pc_in_guess = 0.5*(T_pc_in_lower + T_pc_in_upper);
				}
			}
			else if( diff_T_pc_in > 0.0 )	// T_pc_in_guess was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_pc_in_lower = T_pc_in_guess;		// Set lower bound
				y_T_pc_in_lower = diff_T_pc_in;		// Set lower convergence error

				if( is_upper_bound && is_upper_error )
				{
					T_pc_in_guess = y_T_pc_in_upper / (y_T_pc_in_upper - y_T_pc_in_lower)*(T_pc_in_lower - T_pc_in_upper) + T_pc_in_upper;
				}
				else if( is_upper_bound )
				{
					T_pc_in_guess = 0.5*(T_pc_in_lower + T_pc_in_upper);
				}
				else
				{	// Initial guess is the hot side temperature at the beginning of the timestep
					// Assume that the storage won't get hotter throughout the timestep,
					// ... so if results from 1st guess suggests that a higher temperature is required, get out

					exit_mode = NO_SOLUTION;
					T_pc_in_exit_tolerance = diff_T_pc_in;
					break;
				}
			}
			else							// T_pc_in_guess was too high
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_pc_in_upper = T_pc_in_guess;		// Set upper bound
				y_T_pc_in_upper = diff_T_pc_in;		// Set upper convergence error

				if( is_lower_bound && is_lower_error )
				{
					T_pc_in_guess = y_T_pc_in_upper / (y_T_pc_in_upper - y_T_pc_in_lower)*(T_pc_in_lower - T_pc_in_upper) + T_pc_in_upper;
				}
				else if( is_lower_bound )
				{
					T_pc_in_guess = 0.5*(T_pc_in_lower + T_pc_in_upper);
				}
				else
				{
					T_pc_in_guess = T_pc_in_calc - 5.0;
				}

			}
		}


		// Call the power cycle in STARTUP_CONTROLLED mode
		mc_pc_inputs.m_m_dot = 0.0;
		mc_pc_htf_state_in.m_temp = T_pc_in_guess - 273.15;		//[C] convert from K
		mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP_CONTROLLED;

		mc_power_cycle.call(mc_weather.ms_outputs,
			mc_pc_htf_state_in,
			mc_pc_inputs,
			mc_pc_out_solver,
			mc_pc_out_report,
			mc_sim_info);

		// Check for new timestep, probably will find one here
		time_pc_su = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)

		// Use 'm_m_dot_demand' as an input to TES model
		// 'm_m_dot_htf' and 'm_m_dot_htf_ref' will be NaN, but that should be ok...

		double m_dot_pc = mc_pc_out_solver.m_m_dot_htf / 3600.0;		//[kg/s]

		// Reset mass flow rate in 'mc_pc_htf_state'
		mc_pc_inputs.m_m_dot = mc_pc_out_solver.m_m_dot_htf;			//[kg/hr]

		bool dc_solved = mc_tes.discharge(time_pc_su, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_pc, m_T_htf_cold_des, T_pc_in_calc, mc_tes_outputs);

		// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
		mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
		mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
		mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

		// Set discharge htf state
		mc_tes_dc_htf_state.m_m_dot = m_dot_pc*3600.0;							//[kg/hr]
		mc_tes_dc_htf_state.m_temp_in = m_T_htf_cold_des - 273.15;				//[C] convert from K
		mc_tes_dc_htf_state.m_temp_out = T_pc_in_calc - 273.15;					//[C] convert from K

		if( dc_solved )
		{
			diff_T_pc_in = (T_pc_in_calc - T_pc_in_guess) / T_pc_in_guess;
		}
		else
		{
			diff_T_pc_in = std::numeric_limits<double>::quiet_NaN();	// Provided discharge mass flow rate is too large for amount of storage remaining
			continue;
		}

		exit_mode = CONVERGED;
	}

	if( exit_mode == NO_SOLUTION )
	{	// Try fully discharging TES and beginning PC startup
		// Check that power cycle hasn't completely started up, as that suggests an error above (in this mode)

		time_pc_su = mc_sim_info.m_step;		//[s]

		// Get mass flow rate and temperature at a full discharge
		double m_dot_pc = std::numeric_limits<double>::quiet_NaN();
		mc_tes.discharge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_T_htf_cold_des, T_pc_in_calc, m_dot_pc, mc_tes_outputs);

		// If not actually charging (i.e. mass flow rate = 0.0), what should the temperatures be?
		mc_tes_ch_htf_state.m_m_dot = 0.0;										//[kg/hr]
		mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
		mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

		// Set discharge htf state
		mc_tes_dc_htf_state.m_m_dot = m_dot_pc*3600.0;							//[kg/hr]
		mc_tes_dc_htf_state.m_temp_in = m_T_htf_cold_des - 273.15;				//[C] convert from K
		mc_tes_dc_htf_state.m_temp_out = T_pc_in_calc - 273.15;					//[C] convert from K

		// If receiver IS producing energy, try starting up power cycle
		// Power Cycle: STARTUP
		mc_pc_htf_state_in.m_temp = T_pc_in_calc - 273.15;				//[C]
		mc_pc_inputs.m_m_dot = m_dot_pc*3600.0;						//[kg/hr] no mass flow rate to power cycle
		// Inputs
		mc_pc_inputs.m_standby_control = C_csp_power_cycle::STARTUP;
		//mc_pc_inputs.m_tou = tou_timestep;
		// Performance Call
		mc_power_cycle.call(mc_weather.ms_outputs,
			mc_pc_htf_state_in,
			mc_pc_inputs,
			mc_pc_out_solver,
			mc_pc_out_report,
			mc_sim_info);

		// Would be nice to have some check to know whether startup solved appropriately...


		// Check for new timestep
		time_pc_su = mc_pc_out_solver.m_time_required_su;		//[s] power cycle model returns MIN(time required to completely startup, full timestep duration)
		if( time_pc_su < mc_sim_info.m_step - step_tol )
		{
			exit_mode = NO_SOLUTION;

			T_pc_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
		}
		else
		{
			exit_mode = CONVERGED;

			time_pc_su = mc_sim_info.m_step;
			T_pc_in_exit_tolerance = 0.0;
		}
	}

}


void C_csp_solver::solver_cr_on__pc_float__tes_full(int power_cycle_mode,
	double field_control_in,
	double tol,
	int &T_rec_in_exit_mode, double &T_rec_in_exit_tolerance)
{
	// The collector-receiver is on and delivering hot HTF to the PC and TES
	// The PC accepts floating input
	// The TES is fully charging over the timestep

	// Guess and iterate for the collector-receiver inlet temperature
	double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
	double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

	// Lower bound could be freeze protection temperature...
	double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	double y_T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double y_T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	// Booleans for bounds and convergence error
	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double diff_T_rec_in = 999.9*tol;

	int iter_T_rec_in = 0;

	// Exit information for outer loop on T_rec_in
	T_rec_in_exit_mode = CONVERGED;
	T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Start iteration on T_rec_in
	while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
	{
		iter_T_rec_in++;		// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
		if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
		{
			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			else
			{
				T_rec_in_exit_mode = POOR_CONVERGENCE;
				T_rec_in_exit_tolerance = diff_T_rec_in;
				return;
			}
		}

		// Subsequent iterations need to re-calculate T_in
		if( iter_T_rec_in > 1 )
		{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models did not solve such that a convergence error could be calculated
				// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
				// But, check that bounds exist
				if( !is_lower_bound || !is_upper_bound )
				{
					T_rec_in_exit_mode = NO_SOLUTION;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					return;
				}
				T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
			}
			else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_rec_in_lower = T_rec_in_guess;		//[C]
				y_T_rec_in_lower = diff_T_rec_in;			//[-]

				if( is_upper_bound && is_upper_error )
				{
					T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_upper_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
				}
				else
				{
					T_rec_in_guess += 2.5;			//[C]
				}
			}
			else
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
				y_T_rec_in_upper = diff_T_rec_in;			//[-]

				if( is_lower_bound && is_upper_bound )
				{
					T_rec_in_guess = y_T_rec_in_upper / (y_T_rec_in_upper - y_T_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_lower_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
				}
				else
				{
					T_rec_in_guess -= 2.5;		//[C]
				}
			}
		}

		// Solve the receiver model with T_rec_in_guess
		// CR ON
		mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
		mc_cr_inputs.m_field_control = field_control_in;	//[-] no defocusing for initial simulation
		mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

		mc_collector_receiver.call(mc_weather.ms_outputs,
			mc_cr_htf_state_in,
			mc_cr_inputs,
			mc_cr_out_solver,
			mc_cr_out_report,
			mc_sim_info);

		// Check if receiver is OFF or model didn't solve
		if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
		{
			// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
			if( iter_T_rec_in == 1 )
			{
				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;  // exit while() on diff_T_rec_in
			}
			else
			{	// If collector-receiver model has solved with results previously, then try to find another guess value
				// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
				// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
				if( T_rec_in_guess < T_rec_in_guess_ini )
				{	// If current guess value is less than initial value, then:

					// If lower bound is already set OR upper bound is not set, can't generate new guess
					if( is_lower_bound || !is_upper_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;
					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
				else
				{	// If current guess value is greater than initial value, then:

					// If upper bound is already set OR lower bound is not set, can't generate new guess
					if( is_upper_bound || !is_lower_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;
					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
			}	// end else on if(iter_T_rec_in == 1)					
		}	// end logic to determine path if receiver is off or did not solve

		// Get receiver HTF outputs
		double m_dot_receiver = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr]
		double T_htf_rec_out = mc_cr_out_solver.m_T_salt_hot;			//[C]

		// Solve TES for FULL charge
		double T_htf_tes_out, m_dot_htf_tes_out;
		T_htf_tes_out = m_dot_htf_tes_out = std::numeric_limits<double>::quiet_NaN();
		mc_tes.charge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_htf_rec_out+273.15, 
			T_htf_tes_out, m_dot_htf_tes_out, mc_tes_outputs);

		T_htf_tes_out -= 273.15;		//[C] convert from K
		m_dot_htf_tes_out *= 3600.0;	//[kg/hr] convert from kg/s

		// HTF charging state
		mc_tes_ch_htf_state.m_m_dot = m_dot_htf_tes_out;				//[kg/hr]
		mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;		//[C]
		mc_tes_ch_htf_state.m_temp_out = T_htf_tes_out;					//[C] convert from K

		// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
		mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
		mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
		mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K

		double m_dot_pc = m_dot_receiver - m_dot_htf_tes_out;	//[kg/hr]

		// Check that m_dot_pc > 0
		if( m_dot_pc < 0.0 )
		{
			T_rec_in_lower = T_rec_in_guess;
			is_lower_bound = true;
			is_lower_error = false;
			y_T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();

			continue;
		}

		// Solve PC performance
		// Set inputs to power cycle model
		mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
		mc_pc_inputs.m_m_dot = m_dot_pc;							//[kg/hr]

		// Inputs
		mc_pc_inputs.m_standby_control = power_cycle_mode;

		// Performance Call
		mc_power_cycle.call(mc_weather.ms_outputs,
			mc_pc_htf_state_in,
			mc_pc_inputs,
			mc_pc_out_solver,
			mc_pc_out_report,
			mc_sim_info);

		// Check that power cycle is producing power or model didn't solve
		if( mc_pc_out_solver.m_P_cycle == 0.0 && mc_pc_inputs.m_standby_control == C_csp_power_cycle::ON )
		{
			// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
			if( iter_T_rec_in == 1 )
			{
				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
			}
			else
			{	// If power cycle model has solved with results previously, then try to find another guess value
				// Assumption here is that power cycle solved at the first guess mass flow rate
				// Also, assume that if both upper and lower bounds exist, then can't generate a new guess

				if( T_rec_in_guess < T_rec_in_guess_ini )
				{	// If current guess value is less than initial value, then:	

					// If lower bound is already set OR upper bound is not set, can't generate new guess
					if( is_lower_bound || !is_upper_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
					}

					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;

					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
				}
				else
				{	// If current guess value is greater than initial guess, then:

					// If upper bound is already set OR lower bound is not set, can't generate new guess
					if( is_upper_bound || !is_lower_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
					}

					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;

					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
				}
			}
		}	// end logic to handle power cycle not solving 

		// Get power cycle HTF return temperature...
		double T_pc_out = mc_pc_out_solver.m_T_htf_cold;		//[C]

		// Enthalpy balance to get T_rec_in_calc
		double T_rec_in_calc = (m_dot_htf_tes_out*T_htf_tes_out + m_dot_pc*T_pc_out) / m_dot_receiver;	//[C]

		// Calculate diff_T_rec_in
		diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;

	}	// end while() on the receiver HTF inlet temperature
}

void C_csp_solver::solver_pc_fixed__tes_dc(double q_dot_pc_fixed /*MWt*/, int power_cycle_mode,
	double tol,
	int &T_cold_exit_mode, double &T_cold_exit_tolerance,
	int &q_pc_exit_mode, double &q_pc_exit_tolerance)
{
	// Power cycle requires fixed thermal input
	// TES supplies the entire thermal input to the PC

	double T_cold_guess_ini = m_T_htf_cold_des - 273.15;	//[C], convert from K
	double T_cold_guess = T_cold_guess_ini;					//[C]

	// Lower bound could be freeze protection temperature...
	double T_cold_lower = std::numeric_limits<double>::quiet_NaN();
	double T_cold_upper = std::numeric_limits<double>::quiet_NaN();
	double y_T_cold_lower = std::numeric_limits<double>::quiet_NaN();
	double y_T_cold_upper = std::numeric_limits<double>::quiet_NaN();
	// Booleans for bounds and convergence error
	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double diff_T_cold = 999.9*tol;			// (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

	int iter_T_cold = 0;

	T_cold_exit_mode = CONVERGED;
	T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	q_pc_exit_mode = CONVERGED;
	q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Start iteration loop
	while( fabs(diff_T_cold) > tol || diff_T_cold != diff_T_cold )
	{
		iter_T_cold++;		// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_cold_upper - T_cold_lower;
		if( diff_T_bounds / T_cold_upper < tol / 2.0 )
		{
			if( diff_T_cold != diff_T_cold )
			{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

				T_cold_exit_mode = NO_SOLUTION;
				T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			else
			{
				T_cold_exit_mode = POOR_CONVERGENCE;
				T_cold_exit_tolerance = diff_T_cold;
				return;
			}
		}

		// Subsequent iterations need to re-calculate T_cold
		if( iter_T_cold > 1 )
		{	// diff_T_cold = (T_cold_calc - T_cold_guess)/T_cold_guess

			if( diff_T_cold != diff_T_cold )
			{	// Models did not solve such that a convergence error could be calculated
				// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
				// But, check that bounds exist
				if( !is_lower_bound || !is_upper_bound )
				{
					T_cold_exit_mode = NO_SOLUTION;
					T_cold_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					return;
				}
				T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]						
			}
			else if( diff_T_cold > 0.0 )		// Guess receiver inlet temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_cold_lower = T_cold_guess;		//[C]
				y_T_cold_lower = diff_T_cold;			//[-]

				if( is_upper_bound && is_upper_error )
				{
					T_cold_guess = y_T_cold_upper / (y_T_cold_upper - y_T_cold_lower)*(T_cold_lower - T_cold_upper) + T_cold_upper;		//[C]
				}
				else if( is_upper_bound )
				{
					T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]	
				}
				else
				{
					T_cold_guess += 2.5;			//[C]
				}
			}
			else
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_cold_upper = T_cold_guess;		//[C] Set upper bound
				y_T_cold_upper = diff_T_cold;			//[-]

				if( is_lower_bound && is_upper_bound )
				{
					T_cold_guess = y_T_cold_upper / (y_T_cold_upper - y_T_cold_lower)*(T_cold_lower - T_cold_upper) + T_cold_upper;		//[C]
				}
				else if( is_lower_bound )
				{
					T_cold_guess = 0.5*(T_cold_lower + T_cold_upper);		//[C]
				}
				else
				{
					T_cold_guess -= 2.5;		//[C]
				}
			}
		}


		//*********************************************
		// Could potentially add receiver solution here
		//*********************************************

		// Knowing the cold HTF temperature to TES, can calculate the maximum mass flow rate available for discharge
		double q_dot_tes_dc_local, m_dot_tes_dc_max, T_tes_hot_return;
		q_dot_tes_dc_local = m_dot_tes_dc_max = T_tes_hot_return = std::numeric_limits<double>::quiet_NaN();
		mc_tes.discharge_avail_est(T_cold_guess + 273.15, mc_sim_info.m_step, q_dot_tes_dc_local, m_dot_tes_dc_max, T_tes_hot_return);
		m_dot_tes_dc_max *= 3600.0;		//[kg/hr] convert from kg/s

		// Calculate minimum and maximum possible mass flow rates to the power cycle
		double m_dot_pc_min = 0.0;		//[kg/hr]
		// '1.2' here is an approximation with
		//		goal to not send the power cycle a ridiculously large mass flow rate
		//		which could be possible if we choose to dispatch a lot of TES
		double m_dot_pc_max = fmin(m_dot_tes_dc_max, m_m_dot_pc_des*1.2*m_cycle_max_frac);	//[kg/hr]

		// Set iteration limits for mass flow rate to PC loop
		double m_dot_pc_lower = m_dot_pc_min;		//[kg/hr]
		// Goal in setting m_dot_pc_upper is to not send the power cycle a ridiculously large mass flow rate - want return values that solver can "handle"
		double m_dot_pc_upper = m_dot_pc_max;		//[kg/hr]

		double m_dot_pc_guess = fmin(m_dot_pc_max, fmax(m_dot_pc_min, m_m_dot_pc_des*q_dot_pc_fixed / m_cycle_q_dot_des));
		double m_dot_pc_guess_ini = m_dot_pc_guess;

		double y_m_dot_pc_lower = std::numeric_limits<double>::quiet_NaN();
		double y_m_dot_pc_upper = std::numeric_limits<double>::quiet_NaN();

		bool is_m_dot_upper_bound = true;
		bool is_m_dot_lower_bound = false;
		bool is_m_dot_upper_error = false;
		bool is_m_dot_lower_error = false;

		// Iteration assumption: increasing mass flow rate to power cycle at a constant inlet temperature will increase
		// the thermal power delivered to the cycle
		double tol_q_pc = 0.9*tol;		//[-] Set inner nest tolerance smaller than outer nest
		double diff_q_pc = 999.9*tol;	//[-] (Calc - Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low
		int iter_q_pc = 0;				//[-]

		q_pc_exit_mode = CONVERGED;
		q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

		// Start loop to iteration on mass flow rate to PC that results in target q_dot to PC
		while( fabs(diff_q_pc) > tol_q_pc || diff_q_pc != diff_q_pc )
		{
			iter_q_pc++;		// First iteration = 1

			// Check if distance between bounds is "too small"
			// Could hit this first iteration if m_dot_pc_min > m_dot_pc_max
			double diff_q_pc_bounds = m_dot_pc_upper - m_dot_pc_lower;
			if( diff_q_pc_bounds / m_dot_pc_upper < tol_q_pc / 2.0 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for m_dot_pc

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else if( (m_dot_pc_max - m_dot_pc_guess) / m_dot_pc_max < tol_q_pc )
				{	// Have tried maximum mass flow rate and can't achieve target power

					q_pc_exit_mode = UNDER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else if( (m_dot_pc_guess - m_dot_pc_min) / m_dot_pc_max < tol_q_pc )
				{	// At minimum mass flow rate, we're still overshooting target power

					q_pc_exit_mode = OVER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// At minimum mass flow rate, we're still overshooting target power

					q_pc_exit_mode = POOR_CONVERGENCE;
					q_pc_exit_tolerance = diff_q_pc;
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
			}


			// Subsequent iterations need to re-calculate T_in
			if( iter_q_pc > 1 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models did not solve such that a convergence error could be calculated
					// However, if upper and lower bounds are set, then we can calculate a new guess via bisection method
					// First, need to check that bounds exist
					if( !is_m_dot_lower_bound || !is_m_dot_upper_bound )
					{

						q_pc_exit_mode = NO_SOLUTION;
						q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
					}
					m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]							
				}
				else if( diff_q_pc < 0.0 )			// Mass flow rate guess was too low
				{
					is_m_dot_lower_bound = true;
					is_m_dot_lower_error = true;
					m_dot_pc_lower = m_dot_pc_guess;	// Set lower bound
					y_m_dot_pc_lower = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_upper_bound && is_m_dot_upper_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_upper_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmin(1.35*m_dot_pc_guess, m_dot_pc_max);	//[kg/hr]
					}
				}
				else							// Mass flow rate guess was too high
				{
					is_m_dot_upper_bound = true;
					is_m_dot_upper_error = true;
					m_dot_pc_upper = m_dot_pc_guess;	// Set upper bound
					y_m_dot_pc_upper = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_lower_bound && is_m_dot_lower_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_lower_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmax(0.75*m_dot_pc_guess, m_dot_pc_min);	//[kg/hr]
					}
				}
			}


			// Solve TES discharge at calculate m_dot_dc
			double m_dot_tes_dc = m_dot_pc_guess;				//[kg/hr]
			double T_htf_hot_out = std::numeric_limits<double>::quiet_NaN();
			bool tes_success = mc_tes.discharge(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_tes_dc / 3600.0, T_cold_guess + 273.15,
				T_htf_hot_out, mc_tes_outputs);

			T_htf_hot_out -= 273.15;		//[C] convert from K

			if( !tes_success )
			{
				q_pc_exit_mode = UNDER_TARGET_PC;
				break;
			}

			// HTF discharging state
			mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc;			//[kg/hr]
			mc_tes_dc_htf_state.m_temp_in = T_cold_guess;		//[C]
			mc_tes_dc_htf_state.m_temp_out = T_htf_hot_out;		//[C]

			// HTF charging state
			mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
			mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
			mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

			// Enthalpy balance (mixer)
			// double T_pc_htf_in = (m_dot_tes_dc*T_htf_hot_out + m_dot_receiver*mc_cr_out_solver.m_T_salt_hot) / (m_dot_pc_guess);	//[C]
			double T_pc_htf_in = T_htf_hot_out;

			// Solver power cycle model
			mc_pc_htf_state_in.m_temp = T_pc_htf_in;		//[C]
			mc_pc_inputs.m_m_dot = m_dot_pc_guess;		//[kg/hr]

			// Inputs
			mc_pc_inputs.m_standby_control = power_cycle_mode;

			// Performance Call
			mc_power_cycle.call(mc_weather.ms_outputs,
				mc_pc_htf_state_in,
				mc_pc_inputs,
				mc_pc_out_solver,
				mc_pc_out_report,
				mc_sim_info);

			// Check that power cycle is producing power or model didn't solve
			if( mc_pc_out_solver.m_P_cycle == 0.0 && mc_pc_inputs.m_standby_control == C_csp_power_cycle::ON )
			{
				// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
				if( iter_q_pc == 1 )
				{

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// If power cycle model has solved with results previously, then try to find another guess 
					// Assumption here is that power cycle solved at the first guess mass flow rate
					// Also, assume that if both upper and lower bounds exist, then can't generate a new guess

					if( m_dot_pc_guess < m_dot_pc_guess_ini )
					{	// If current guess value is less than initial value, then:

						// If lower bound is already set OR upper bound is not set, then can't generate new guess
						if( is_m_dot_lower_bound || !is_m_dot_upper_bound )
						{

							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_lower = m_dot_pc_guess;		//[kg/hr]
						is_m_dot_lower_bound = true;
						is_m_dot_lower_error = false;

						// Set diff_q_pc to NaN to indicate to Guess Generator that bisection method should be used
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
					else
					{	// If current guess value is greater than initial guess, then:

						// If upper bound is already set OR lower is not set, then can't generate new guess
						if( is_m_dot_upper_bound || !is_m_dot_lower_bound )
						{

							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_upper = m_dot_pc_guess;		//[kg/hr]
						is_m_dot_upper_bound = true;
						is_m_dot_upper_error = false;

						// Set diff_q_pc to NaN to indicate to Guess Generator that bisection method should be used
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
				}
			}	// end logic to handle power cycle not producing power or failing

			// Get thermal power delivered to power cycle and calculate the difference between the calculated thermal power and target
			diff_q_pc = (mc_pc_out_solver.m_q_dot_htf - q_dot_pc_fixed) / q_dot_pc_fixed;		//[-] (Calc-Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low

		}	// end while() loop on discharge mass flow rate to hit PC thermal target


		// Check exit mode from diff_q_pc loop
		if( q_pc_exit_mode != CONVERGED && q_pc_exit_mode != POOR_CONVERGENCE )
		{
			return;
		}

		// Get HTF temperature out from the power cycle and compare to guess value (T_rec_in)
		double T_cold_calc = mc_pc_out_solver.m_T_htf_cold;	//[C]

		diff_T_cold = (T_cold_calc - T_cold_guess) / T_cold_guess;		//[-]

	}	// end while() loop on T_cold


}

void C_csp_solver::solver_pc_fixed__tes_empty(double q_dot_pc_fixed /*MWt*/,
	double tol,
	double & time_tes_dc,
	int &T_tes_in_exit_mode, double &T_tes_in_exit_tolerance,
	int &q_pc_exit_mode, double &q_pc_exit_tolerance)
{

	// CR is either off or in startup. It's flow (if applicable) is not connected to TES or PC
	// The power cycle is controlled for a fixed q_dot_in
	// TES discharges until it is depleted
	// A new, shorter timestep is calculated if solver is successful

	double T_tes_cold_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
	double T_tes_cold_guess = T_tes_cold_ini;				//[C]

	double T_tes_cold_lower = std::numeric_limits<double>::quiet_NaN();
	double y_T_tes_cold_lower = std::numeric_limits<double>::quiet_NaN();
	bool is_lower_bound = false;
	bool is_lower_error = false;

	double T_tes_cold_upper = std::numeric_limits<double>::quiet_NaN();
	double y_T_tes_cold_upper = std::numeric_limits<double>::quiet_NaN();
	bool is_upper_bound = false;
	bool is_upper_error = false;

	double diff_T_tes_cold = 999.9*tol;			//[-] (T_tes_cold_calc - T_tes_cold_guess)/T_tes_cold_guess

	int iter_T_tes_cold = 0;

	T_tes_in_exit_mode = CONVERGED;
	T_tes_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	q_pc_exit_mode = CONVERGED;
	q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Start iteration on cold PC return temperature to TES
	while( fabs(diff_T_tes_cold) > tol || diff_T_tes_cold != diff_T_tes_cold )
	{
		iter_T_tes_cold++;

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_tes_cold_upper - T_tes_cold_lower;
		if( diff_T_bounds / T_tes_cold_upper < tol / 2.0 )
		{
			if( diff_T_tes_cold != diff_T_tes_cold )
			{	// Models aren't producing power or are return errors, and it appears we've tried the solution space for T_tes_cold
			
				T_tes_in_exit_mode = NO_SOLUTION;
				T_tes_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			else
			{
				T_tes_in_exit_mode = POOR_CONVERGENCE;
				T_tes_in_exit_tolerance = diff_T_tes_cold;
				return;
			}		
		}

		// Subsequent iterations need to re-calculate T_tes_cold
		if( iter_T_tes_cold > 1 )
		{	// diff_T_tes_cold = (T_tes_cold_calc - T_tes_cold_guess)/T_tes_cold_guess

			if( diff_T_tes_cold != diff_T_tes_cold )
			{	// Models did not solve such that a convergence error could be calculated
				// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
				// But, check that bounds exist
				if( !is_lower_bound || !is_upper_bound )
				{

					T_tes_in_exit_mode = NO_SOLUTION;
					T_tes_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					return;
				}
				T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
			}
			else if( diff_T_tes_cold > 0.0 )		// Guess cold temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_tes_cold_lower = T_tes_cold_guess;	//[C]
				y_T_tes_cold_lower = diff_T_tes_cold;	//[-]

				if( is_upper_bound && is_upper_error )
				{
					T_tes_cold_guess = y_T_tes_cold_upper / (y_T_tes_cold_upper - y_T_tes_cold_lower)*(T_tes_cold_lower - T_tes_cold_upper) + T_tes_cold_upper;		//[C]
				}
				else if( is_upper_bound )
				{
					T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
				}
				else
				{
					T_tes_cold_guess += 2.5;			//[C]
				}
			}
			else
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_tes_cold_upper = T_tes_cold_guess;	//[C]
				y_T_tes_cold_upper = diff_T_tes_cold;	//[-]

				if( is_lower_bound && is_lower_error )
				{
					T_tes_cold_guess = y_T_tes_cold_upper / (y_T_tes_cold_upper - y_T_tes_cold_lower)*(T_tes_cold_lower - T_tes_cold_upper) + T_tes_cold_upper;		//[C]
				}
				else if( is_lower_bound )
				{
					T_tes_cold_guess = 0.5*(T_tes_cold_lower + T_tes_cold_upper);
				}
				else
				{
					T_tes_cold_guess -= 2.5;		//[C]
				}
			}
		}	// end logic to determine new T_tes_cold
	
		// First, get the maximum possible max flow rate from TES discharge using the guess value for the TES cold inlet temperature
		double T_htf_hot_out, m_dot_htf_max;
		T_htf_hot_out = m_dot_htf_max = std::numeric_limits<double>::quiet_NaN();
		mc_tes.discharge_full(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, T_tes_cold_guess + 273.15, T_htf_hot_out, m_dot_htf_max, mc_tes_outputs);

		// Calculate max TES discharge MASS
		double mass_tes_max = m_dot_htf_max*mc_sim_info.m_step;		//[kg]

		// Guess time required to deplete storage while delivering thermal power requirements to PC
		double time_empty_ini = mc_sim_info.m_step*(mc_tes_outputs.m_q_dot_dc_to_htf / q_dot_pc_fixed);		//[s]
		time_tes_dc = time_empty_ini;		//[s]

		double time_empty_lower = 0.0;
		double y_time_empty_lower = std::numeric_limits<double>::quiet_NaN();
		bool is_time_lower_bound = true;
		bool is_time_lower_error = false;

		double time_empty_upper = mc_sim_info.m_step;
		double y_time_empty_upper = std::numeric_limits<double>::quiet_NaN();
		bool is_time_upper_bound = true;
		bool is_time_upper_error = false;

		double tol_q_dot = 0.9*tol;		//[-] tolerance of inner loop on q_dot_pc_min

		double diff_q_dot = 999.9*tol;	//[-] (q_dot_calc - q_dot_pc_min)/q_dot_pc_min

		int iter_q_dot = 0;

		q_pc_exit_mode = CONVERGED;
		q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

		// Need access to these variables in this scope, but they're defined in following inner nest
		double T_htf_tes_hot, m_dot_tes_dc;
		T_htf_tes_hot = m_dot_tes_dc = std::numeric_limits<double>::quiet_NaN();

		while( fabs(diff_q_dot) > tol_q_dot || diff_q_dot != diff_q_dot )
		{
			iter_q_dot++;		// First iteration = 1

			// Check if distance between bounds is "too small"
			double diff_q_dot_bounds = time_empty_upper - time_empty_lower;
			if( diff_q_dot_bounds / 3600.0 < tol_q_dot / 2.0 )
			{
				if( diff_q_dot != diff_q_dot )
				{	// Models aren't solving !?!?!

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;		// exits while() on diff_q_dot and sends control to while() on diff_T_tes_cold
				}
				else
				{

					q_pc_exit_mode = POOR_CONVERGENCE;
					q_pc_exit_tolerance = diff_q_dot;
					break;		// exits while() on diff_q_dot and sends control to while() on diff_T_tes_cold
				}
			}

			// Subsequent iterations need to re-calculate time_empty_guess
			if( iter_q_dot > 1 )
			{
				if( diff_q_dot != diff_q_dot )
				{	// Models did not solve such that a convergence error could be calculated
					// However, if upper and lower bounds are set, then we can calculate a new guess via bisection method
					// First, need to check that bounds exist
					if( !is_time_lower_bound || !is_time_upper_bound )
					{

						q_pc_exit_mode = NO_SOLUTION;
						q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;		// exits while() on diff_q_dot and sends control to while() on diff_T_tes_cold
					}
					time_tes_dc = 0.5*(time_empty_lower + time_empty_upper);
				}
				else if( diff_q_dot > 0.0 )			// diff_q_dot = (q_dot_calc - q_dot_pc_min)/q_dot_pc_min
				{	// Time guess is too small

					is_time_lower_bound = true;
					is_time_lower_error = true;
					time_empty_lower = time_tes_dc;
					y_time_empty_lower = diff_q_dot;

					if( is_time_upper_bound && is_time_upper_error )
					{
						time_tes_dc = y_time_empty_upper / (y_time_empty_upper - y_time_empty_lower)*(time_empty_lower - time_empty_upper) + time_empty_upper;		//[kg/hr]
					}
					else if( is_time_upper_bound )
					{
						time_tes_dc = 0.5*(time_empty_lower + time_empty_upper);
					}
					else
					{
						time_tes_dc = fmin(time_tes_dc*1.25, mc_sim_info.m_step);	//[s]
					}
				}
				else
				{

					is_time_upper_bound = true;
					is_time_upper_error = true;
					time_empty_upper = time_tes_dc;
					y_time_empty_upper = diff_q_dot;

					if( is_time_lower_bound && is_time_lower_error )
					{
						time_tes_dc = y_time_empty_upper / (y_time_empty_upper - y_time_empty_lower)*(time_empty_lower - time_empty_upper) + time_empty_upper;		//[kg/hr]
					}
					else if( is_time_lower_bound )
					{
						time_tes_dc = 0.5*(time_empty_lower + time_empty_upper);
					}
					else
					{
						time_tes_dc = fmin(time_tes_dc*0.75, 0.001);	//[s]
					}
				}
			}	// end logic to calculate new time_empty_guess

			// Calculate full discharge at new timestep
			T_htf_tes_hot = m_dot_tes_dc = std::numeric_limits<double>::quiet_NaN();
			mc_tes.discharge_full(time_tes_dc, mc_weather.ms_outputs.m_tdry + 273.15, T_tes_cold_guess + 273.15, T_htf_tes_hot, m_dot_tes_dc, mc_tes_outputs);

			// Set TES HTF states (this needs to be less bulky...)
			// HTF discharging state
			mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc*3600.0;		//[kg/hr]
			mc_tes_dc_htf_state.m_temp_in = T_tes_cold_guess;		//[C]
			mc_tes_dc_htf_state.m_temp_out = T_htf_tes_hot - 273.15;	//[C]

			// HTF charging state
			mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
			mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
			mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

			// Calculate diff_q_dot
			diff_q_dot = (mc_tes_outputs.m_q_dot_dc_to_htf - q_dot_pc_fixed) / q_dot_pc_fixed;

		}	// end while() loop on diff_q_dot to find time required to empty storage while meeting power cycle requirements

		// Check q_dot_exit_mode
		if( q_pc_exit_mode != CONVERGED && q_pc_exit_mode != POOR_CONVERGENCE )
		{
			return;		// exits while() on diff_T_rec_in
		}

		// Solve PC model
		mc_pc_htf_state_in.m_temp = T_htf_tes_hot - 273.15;		//[C]
		mc_pc_inputs.m_m_dot = m_dot_tes_dc*3600.0;			//[kg/hr]

		// Inputs
		mc_pc_inputs.m_standby_control = C_csp_power_cycle::ON;

		// Set new local timestep
		C_csp_solver_sim_info temp_sim_info = mc_sim_info;
		temp_sim_info.m_step = time_tes_dc;					//[s]
		temp_sim_info.m_time = mc_sim_info.m_time - mc_sim_info.m_step + time_tes_dc;	//[s]

		// Performance Call
		mc_power_cycle.call(mc_weather.ms_outputs,
			mc_pc_htf_state_in,
			mc_pc_inputs,
			mc_pc_out_solver,
			mc_pc_out_report,
			temp_sim_info);			// **** Note using 'temp_sim_info' here ****

		diff_T_tes_cold = (mc_pc_out_solver.m_T_htf_cold - T_tes_cold_guess) / T_tes_cold_guess;

	}	// end while() on TES inlet temperature

}

void C_csp_solver::solver_cr_on__pc_fixed__tes_dc(double q_dot_pc_fixed /*MWt*/, int power_cycle_mode,
	double field_control_in,
	double tol,
	int &T_rec_in_exit_mode, double &T_rec_in_exit_tolerance,
	int &q_pc_exit_mode, double &q_pc_exit_tolerance)
{
	// CR in on
	// PC is controlled for a fixed q_dot_in
	// TES supplements CR output to PC

	double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
	double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

	// Lower bound could be freeze protection temperature...
	double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	// Booleans for bounds and convergence error
	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double diff_T_rec_in = 999.9*tol;			// (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

	int iter_T_rec_in = 0;

	T_rec_in_exit_mode = CONVERGED;
	T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	q_pc_exit_mode = CONVERGED;
	q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Start iteration loop
	while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
	{
		iter_T_rec_in++;		// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
		if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
		{
			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			else
			{
				T_rec_in_exit_mode = POOR_CONVERGENCE;
				T_rec_in_exit_tolerance = diff_T_rec_in;
				return;
			}
		}


		// Subsequent iterations need to re-calculate T_in
		if( iter_T_rec_in > 1 )
		{	// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess

			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models did not solve such that a convergence error could be calculated
				// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
				// But, check that bounds exist
				if( !is_lower_bound || !is_upper_bound )
				{
					T_rec_in_exit_mode = NO_SOLUTION;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					return;
				}
				T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]						
			}
			else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_rec_in_lower = T_rec_in_guess;		//[C]
				y_rec_in_lower = diff_T_rec_in;			//[-]

				if( is_upper_bound && is_upper_error )
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_upper_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
				}
				else
				{
					T_rec_in_guess += 2.5;			//[C]
				}
			}
			else
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
				y_rec_in_upper = diff_T_rec_in;			//[-]

				if( is_lower_bound && is_upper_bound )
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_lower_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
				}
				else
				{
					T_rec_in_guess -= 2.5;		//[C]
				}
			}
		}

		// Solve the receiver model
		mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
		mc_cr_inputs.m_field_control = field_control_in;	//[-] no defocusing for initial simulation
		mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

		mc_collector_receiver.call(mc_weather.ms_outputs,
			mc_cr_htf_state_in,
			mc_cr_inputs,
			mc_cr_out_solver,
			mc_cr_out_report,
			mc_sim_info);

		// Check if receiver is OFF or model didn't solve
		if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
		{
			// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
			if( iter_T_rec_in == 1 )
			{
				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;	// exit while() on diff_T_rec_in
			}
			else
			{	// If collector-receiver model has solved with results previously in this loop, then try to find another guess value
				// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
				// Also, assume that if both upper and lower bounds exist, then can't generate a new guess because don't know which way to move
				if( T_rec_in_guess < T_rec_in_guess_ini )
				{	// If current guess value is less than the initial guess value, then:

					// If lower bound is already set OR upper bound is not set, can't generate a new guess value
					if( is_lower_bound || !is_upper_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					// Else, set lower bound and flags and 'continue' to start of while()
					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;

					// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
				else
				{	// If current guess value is greater than initial value, then:

					// If upper bound is already set OR lower bound is not set, can't generate a new guess value
					if( is_upper_bound || !is_lower_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					// Else, set upper bound and flags and 'continue' to start of while()
					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;

					// Set diff_T_rec_in to NaN to indicate to Guess Generator that bisection method should be used
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
			}	// end else on if(iter_T_rec_in == 1)
		}	// end logic to determine path if receiver is off or did not solve

		// Now need to iterate on mass flow rate to send to the Power Cycle
		double m_dot_receiver = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr]

		// Knowing the cold HTF temperature to TES, can calculate the maximum mass flow rate available for discharge
		double q_dot_tes_dc_local, m_dot_tes_dc_max, T_tes_hot_return;
		q_dot_tes_dc_local = m_dot_tes_dc_max = T_tes_hot_return = std::numeric_limits<double>::quiet_NaN();
		mc_tes.discharge_avail_est(T_rec_in_guess + 273.15, mc_sim_info.m_step, q_dot_tes_dc_local, m_dot_tes_dc_max, T_tes_hot_return);
		m_dot_tes_dc_max *= 3600.0;		//[kg/hr] convert from kg/s

		// Calculate minimum and maximum possible mass flow rates to the power cycle
		double m_dot_pc_min = fmax(0.0, m_dot_receiver);		//[kg/hr]
		// '1.2' here is an approximation with
		//		goal to not send the power cycle a ridiculously large mass flow rate
		//		which could be possible if we choose to dispatch a lot of TES
		double m_dot_pc_max = fmin(m_dot_receiver + m_dot_tes_dc_max, m_m_dot_pc_des*1.2*m_cycle_max_frac);	//[kg/hr]

		// Set iteration limits for mass flow rate to PC loop
		double m_dot_pc_lower = m_dot_pc_min;		//[kg/hr]
		// Goal in setting m_dot_pc_upper is to not send the power cycle a ridiculously large mass flow rate - want return values that solver can "handle"
		double m_dot_pc_upper = m_dot_pc_max;		//[kg/hr]

		double m_dot_pc_guess = fmin(m_dot_pc_max, fmax(m_dot_pc_min, m_m_dot_pc_des*q_dot_pc_fixed / m_cycle_q_dot_des));
		double m_dot_pc_guess_ini = m_dot_pc_guess;

		double y_m_dot_pc_lower = std::numeric_limits<double>::quiet_NaN();
		double y_m_dot_pc_upper = std::numeric_limits<double>::quiet_NaN();

		bool is_m_dot_upper_bound = true;
		bool is_m_dot_lower_bound = false;
		bool is_m_dot_upper_error = false;
		bool is_m_dot_lower_error = false;

		// Iteration assumption: increasing mass flow rate to power cycle at a constant inlet temperature will increase
		// the thermal power delivered to the cycle
		double tol_q_pc = 0.9*tol;		//[-] Set inner nest tolerance smaller than outer nest
		double diff_q_pc = 999.9*tol;	//[-] (Calc - Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low
		int iter_q_pc = 0;				//[-]

		q_pc_exit_mode = CONVERGED;
		q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

		// Start loop to iteration on mass flow rate to PC that results in target q_dot to PC
		while( fabs(diff_q_pc) > tol_q_pc || diff_q_pc != diff_q_pc )
		{
			iter_q_pc++;		// First iteration = 1

			// Check if distance between bounds is "too small"
			// Could hit this first iteration if m_dot_pc_min > m_dot_pc_max
			double diff_q_pc_bounds = m_dot_pc_upper - m_dot_pc_lower;
			if( diff_q_pc_bounds / m_dot_pc_upper < tol_q_pc / 2.0 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for m_dot_pc

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else if( (m_dot_pc_max - m_dot_pc_guess) / m_dot_receiver < tol_q_pc )
				{	// Have tried maximum mass flow rate and can't achieve target power

					q_pc_exit_mode = UNDER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else if( (m_dot_pc_guess - m_dot_pc_min) / m_dot_receiver < tol_q_pc )
				{	// At minimum mass flow rate, we're still overshooting target power

					q_pc_exit_mode = OVER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// At minimum mass flow rate, we're still overshooting target power

					q_pc_exit_mode = POOR_CONVERGENCE;
					q_pc_exit_tolerance = diff_q_pc;
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
			}


			// Subsequent iterations need to re-calculate T_in
			if( iter_q_pc > 1 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models did not solve such that a convergence error could be calculated
					// However, if upper and lower bounds are set, then we can calculate a new guess via bisection method
					// First, need to check that bounds exist
					if( !is_m_dot_lower_bound || !is_m_dot_upper_bound )
					{

						q_pc_exit_mode = NO_SOLUTION;
						q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
					}
					m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]							
				}
				else if( diff_q_pc < 0.0 )			// Mass flow rate guess was too low
				{
					is_m_dot_lower_bound = true;
					is_m_dot_lower_error = true;
					m_dot_pc_lower = m_dot_pc_guess;	// Set lower bound
					y_m_dot_pc_lower = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_upper_bound && is_m_dot_upper_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_upper_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmin(1.35*m_dot_pc_guess, m_dot_pc_max);	//[kg/hr]
					}
				}
				else							// Mass flow rate guess was too high
				{
					is_m_dot_upper_bound = true;
					is_m_dot_upper_error = true;
					m_dot_pc_upper = m_dot_pc_guess;	// Set upper bound
					y_m_dot_pc_upper = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_lower_bound && is_m_dot_lower_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_lower_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmax(0.75*m_dot_pc_guess, m_dot_pc_min);	//[kg/hr]
					}
				}
			}

			// Solve TES discharge at calculate m_dot_dc
			double m_dot_tes_dc = m_dot_pc_guess - m_dot_receiver;				//[kg/hr]
			double T_htf_hot_out = std::numeric_limits<double>::quiet_NaN();
			bool tes_success = mc_tes.discharge(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_tes_dc / 3600.0, T_rec_in_guess + 273.15,
				T_htf_hot_out, mc_tes_outputs);

			T_htf_hot_out -= 273.15;		//[C] convert from K

			if( !tes_success )
			{
				q_pc_exit_mode = UNDER_TARGET_PC;
				break;
			}

			// HTF discharging state
			mc_tes_dc_htf_state.m_m_dot = m_dot_tes_dc;			//[kg/hr]
			mc_tes_dc_htf_state.m_temp_in = T_rec_in_guess;		//[C]
			mc_tes_dc_htf_state.m_temp_out = T_htf_hot_out;		//[C]

			// HTF charging state
			mc_tes_ch_htf_state.m_m_dot = 0.0;									//[kg/hr]
			mc_tes_ch_htf_state.m_temp_in = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K
			mc_tes_ch_htf_state.m_temp_out = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K

			// Enthalpy balance (mixer)
			double T_pc_htf_in = (m_dot_tes_dc*T_htf_hot_out + m_dot_receiver*mc_cr_out_solver.m_T_salt_hot) / (m_dot_pc_guess);	//[C]

			// Solver power cycle model
			mc_pc_htf_state_in.m_temp = T_pc_htf_in;		//[C]
			mc_pc_inputs.m_m_dot = m_dot_pc_guess;		//[kg/hr]

			// Inputs
			mc_pc_inputs.m_standby_control = power_cycle_mode;

			// Performance Call
			mc_power_cycle.call(mc_weather.ms_outputs,
				mc_pc_htf_state_in,
				mc_pc_inputs,
				mc_pc_out_solver,
				mc_pc_out_report,
				mc_sim_info);


			// Check that power cycle is producing power or model didn't solve
			if( mc_pc_out_solver.m_P_cycle == 0.0 && mc_pc_inputs.m_standby_control == C_csp_power_cycle::ON )
			{
				// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
				if( iter_q_pc == 1 )
				{

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// If power cycle model has solved with results previously, then try to find another guess 
					// Assumption here is that power cycle solved at the first guess mass flow rate
					// Also, assume that if both upper and lower bounds exist, then can't generate a new guess

					if( m_dot_pc_guess < m_dot_pc_guess_ini )
					{	// If current guess value is less than initial value, then:

						// If lower bound is already set OR upper bound is not set, then can't generate new guess
						if( is_m_dot_lower_bound || !is_m_dot_upper_bound )
						{

							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_lower = m_dot_pc_guess;		//[kg/hr]
						is_m_dot_lower_bound = true;
						is_m_dot_lower_error = false;

						// Set diff_q_pc to NaN to indicate to Guess Generator that bisection method should be used
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
					else
					{	// If current guess value is greater than initial guess, then:

						// If upper bound is already set OR lower is not set, then can't generate new guess
						if( is_m_dot_upper_bound || !is_m_dot_lower_bound )
						{

							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_upper = m_dot_pc_guess;		//[kg/hr]
						is_m_dot_upper_bound = true;
						is_m_dot_upper_error = false;

						// Set diff_q_pc to NaN to indicate to Guess Generator that bisection method should be used
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
				}
			}	// end logic to handle power cycle not producing power or failing

			// Get thermal power delivered to power cycle and calculate the difference between the calculated thermal power and target
			diff_q_pc = (mc_pc_out_solver.m_q_dot_htf - q_dot_pc_fixed) / q_dot_pc_fixed;		//[-] (Calc-Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low

		}	// end while() loop on the mass flow rate to the power cycle to hit the thermal input requirements

		// Check exit mode from diff_q_pc loop
		if( q_pc_exit_mode == NO_SOLUTION )
		{
			return;		
		}

		// Get HTF temperature out from the power cycle and compare to guess value (T_rec_in)
		double T_rec_in_calc = mc_pc_out_solver.m_T_htf_cold;	//[C]

		diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;		//[-]

	}	// end while() loop on the receiver and TES inlet temperature

	return;
}

void C_csp_solver::solver_cr_on__pc_fixed__tes_ch(double q_dot_pc_fixed /*MWt*/, int power_cycle_mode,
	double field_control_in, 
	double tol, 
	int &T_rec_in_exit_mode, double &T_rec_in_exit_tolerance,
	int &q_pc_exit_mode, double &q_pc_exit_tolerance)
{
	// CR in on
	// PC is controlled for a fixed q_dot_in
	// excess CR output is charging TES

	// Guess and iterate for the collector-receiver inlet temperature
	double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from K
	double T_rec_in_guess = T_rec_in_guess_ini;					//[C]

	// Lower bound could be freeze protection temperature...
	double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	// Booleans for bounds and convergence error
	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double diff_T_rec_in = 999.9*tol;

	int iter_T_rec_in = 0;

	// Exit information for outer loop on T_rec_in
	T_rec_in_exit_mode = CONVERGED;
	T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Exit information for inner loop on q_dot_pc
	q_pc_exit_mode = CONVERGED;
	q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

	// Start iteration on T_rec_in
	while( fabs(diff_T_rec_in) > tol || diff_T_rec_in != diff_T_rec_in )
	{
		iter_T_rec_in++;		// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
		if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
		{
			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in

				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			else
			{
				T_rec_in_exit_mode = POOR_CONVERGENCE;
				T_rec_in_exit_tolerance = diff_T_rec_in;
				return;
			}
		}

		// Subsequent iterations need to re-calculate T_in
		if( iter_T_rec_in > 1 )
		{			// diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess)/T_rec_in_guess;
			if( diff_T_rec_in != diff_T_rec_in )
			{	// Models did not solve such that a convergence error could be calculated
				// However, we can check whether upper and lower bounds are set, and may be able to calculate a new guess via bisection method
				// But, check that bounds exist
				if( !is_lower_bound || !is_upper_bound )
				{
					T_rec_in_exit_mode = NO_SOLUTION;
					T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					return;
				}
				T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
			}
			else if( diff_T_rec_in > 0.0 )		// Guess receiver inlet temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_rec_in_lower = T_rec_in_guess;		//[C]
				y_rec_in_lower = diff_T_rec_in;			//[-]

				if( is_upper_bound && is_upper_error )
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_upper_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]	
				}
				else
				{
					T_rec_in_guess += 2.5;			//[C]
				}
			}
			else
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_rec_in_upper = T_rec_in_guess;		//[C] Set upper bound
				y_rec_in_upper = diff_T_rec_in;			//[-]

				if( is_lower_bound && is_upper_bound )
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;		//[C]
				}
				else if( is_lower_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
				}
				else
				{
					T_rec_in_guess -= 2.5;		//[C]
				}
			}
		}

		// Solve the receiver model with T_rec_in_guess
		// CR ON
		mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C]
		mc_cr_inputs.m_field_control = field_control_in;	//[-] no defocusing for initial simulation
		mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

		mc_collector_receiver.call(mc_weather.ms_outputs,
			mc_cr_htf_state_in,
			mc_cr_inputs,
			mc_cr_out_solver,
			mc_cr_out_report,
			mc_sim_info);

		// Check if receiver is OFF or model didn't solve
		if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
		{
			// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
			if( iter_T_rec_in == 1 )
			{
				T_rec_in_exit_mode = NO_SOLUTION;
				T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
				return;  // exit while() on diff_T_rec_in
			}
			else
			{	// If collector-receiver model has solved with results previously, then try to find another guess value
				// Assumption here is that the receiver solved at the first guess temperature: 'T_rec_in_guess_ini'
				// Also, assume that if both upper and lower bounds exist, then can't generate a new guess
				if( T_rec_in_guess < T_rec_in_guess_ini )
				{	// If current guess value is less than initial value, then:

					// If lower bound is already set OR upper bound is not set, can't generate new guess
					if( is_lower_bound || !is_upper_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;
					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
				else
				{	// If current guess value is greater than initial value, then:

					// If upper bound is already set OR lower bound is not set, can't generate new guess
					if( is_upper_bound || !is_lower_bound )
					{
						T_rec_in_exit_mode = NO_SOLUTION;
						T_rec_in_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						return;	// exit while() on diff_T_rec_in
					}

					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;
					// At this point, both upper and lower bound should exist, so we can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
					diff_T_rec_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
			}	// end else on if(iter_T_rec_in == 1)					
		}	// end logic to determine path if receiver is off or did not solve
	
		// Now need to iterate on receiver mass flow rate output to send to PC
		double m_dot_receiver = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr]


		// Set up iteration variables
		// Calculate the max and min possible mass flow rates to the power cycle
		// If there is no solution space between them, then need to guess new receiver inlet temperature or get out of iteration

		// Knowing the receiver outlet temperature, can calculate the maximum mass flow rate available for charging
		double q_dot_tes_ch_local, m_dot_tes_ch_max, T_tes_cold_return;
		q_dot_tes_ch_local = m_dot_tes_ch_max = T_tes_cold_return = std::numeric_limits<double>::quiet_NaN();
		mc_tes.charge_avail_est(mc_cr_out_solver.m_T_salt_hot + 273.15, mc_sim_info.m_step, q_dot_tes_ch_local, m_dot_tes_ch_max, T_tes_cold_return);
		m_dot_tes_ch_max *= 3600.0;		//[kg/hr] convert from kg/s

		double m_dot_pc_min = fmax(0.0, m_dot_receiver - m_dot_tes_ch_max);
			// '1.2' here is an approximation with
			//		goal to not send the power cycle a ridiculously large mass flow rate
			//		which could be possible in large solar multiple cases
		double m_dot_pc_max = fmin(m_dot_receiver, m_m_dot_pc_des*1.2*m_cycle_max_frac);

		double m_dot_pc_lower = m_dot_pc_min;
		double m_dot_pc_upper = m_dot_pc_max;

		double m_dot_pc_guess = fmin(m_dot_pc_max, fmin(m_dot_receiver, m_m_dot_pc_des*q_dot_pc_fixed / m_cycle_q_dot_des));
		double m_dot_pc_guess_ini = m_dot_pc_guess;

		double y_m_dot_pc_lower = std::numeric_limits<double>::quiet_NaN();
		double y_m_dot_pc_upper = std::numeric_limits<double>::quiet_NaN();

		bool is_m_dot_upper_bound = true;
		bool is_m_dot_lower_bound = false;
		bool is_m_dot_upper_error = false;
		bool is_m_dot_lower_error = false;

		// Iteration assumption: increasing mass flow rate to power cycle at a constant inlet temperature will increase
		// the thermal power delivered to the cycle

		double tol_q_pc = 0.9*tol;		//[-] Set inner nest tolerance smaller than outer nest
		double diff_q_pc = 999.9*tol;	//[-] (Calc - Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low
		int iter_q_pc = 0;				//[-]

		q_pc_exit_mode = CONVERGED;
		q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();

		// Start iteration loop
		while( fabs(diff_q_pc) > tol_q_pc || diff_q_pc != diff_q_pc )
		{
			iter_q_pc++;		// First iteration = 1

			// Check if distance between bounds is "too small"
			double diff_q_pc_bounds = m_dot_pc_upper - m_dot_pc_lower;
			if( diff_q_pc_bounds / m_dot_pc_upper < tol_q_pc / 2.0 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for m_dot_pc

					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in 
				}
				else if( (m_dot_pc_max - m_dot_pc_guess) / m_dot_receiver < tol_q_pc )
				{	// Have tried maximum mass flow rate and can't achieve target power

					q_pc_exit_mode = UNDER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else if( (m_dot_pc_guess - m_dot_pc_min) / m_dot_receiver < tol_q_pc )
				{	// At minimum mass flow rate, we're still overshooting target power

					q_pc_exit_mode = OVER_TARGET_PC;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// Models are producing power, but convergence errors are not within Tolerance

					q_pc_exit_mode = POOR_CONVERGENCE;
					q_pc_exit_tolerance = diff_q_pc;
					break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
			}

			// Subsequent iterations need to re-calculate T_in
			if( iter_q_pc > 1 )
			{
				if( diff_q_pc != diff_q_pc )
				{	// Models did not solve such that a convergence error could be calculated
					// However, if upper and lower bounds are set, then we can calculate a new guess via bisection method
					// First, need to check that bounds exist
					if( !is_m_dot_lower_bound || !is_m_dot_upper_bound )
					{
						q_pc_exit_mode = NO_SOLUTION;
						q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
						break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
					}
					m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]							
				}
				else if( diff_q_pc < 0.0 )		// Mass flow rate guess was too low
				{
					is_m_dot_lower_bound = true;
					is_m_dot_lower_error = true;
					m_dot_pc_lower = m_dot_pc_guess;	// Set lower bound
					y_m_dot_pc_lower = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_upper_bound && is_m_dot_upper_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_upper_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmin(1.35*m_dot_pc_guess, m_dot_pc_max);	//[kg/hr]
					}
				}
				else							// Mass flow rate guess was too high
				{
					is_m_dot_upper_bound = true;
					is_m_dot_upper_error = true;
					m_dot_pc_upper = m_dot_pc_guess;	// Set upper bound
					y_m_dot_pc_upper = diff_q_pc;		// Set lower convergence error

					if( is_m_dot_lower_bound && is_m_dot_lower_error )	// False-position method
					{
						m_dot_pc_guess = y_m_dot_pc_upper / (y_m_dot_pc_upper - y_m_dot_pc_lower)*(m_dot_pc_lower - m_dot_pc_upper) + m_dot_pc_upper;	//[kg/hr]
					}
					else if( is_m_dot_lower_bound )
					{
						m_dot_pc_guess = 0.5*(m_dot_pc_lower + m_dot_pc_upper);		//[kg/hr]
					}
					else
					{
						m_dot_pc_guess = fmax(0.75*m_dot_pc_guess, m_dot_pc_min);
					}
				}
			}	// End calculation of m_dot_pc_guess
		
			// Set inputs to power cycle model
			// Power Cycle: ON
			mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
			mc_pc_inputs.m_m_dot = m_dot_pc_guess;					//[kg/hr]

			// Inputs
			mc_pc_inputs.m_standby_control = power_cycle_mode;

			// Performance Call
			mc_power_cycle.call(mc_weather.ms_outputs,
				mc_pc_htf_state_in,
				mc_pc_inputs,
				mc_pc_out_solver,
				mc_pc_out_report,
				mc_sim_info);

			// Check that power cycle is producing power or model didn't solve
			// Assumes that standby mode always solves
			if( mc_pc_out_solver.m_P_cycle == 0.0 && mc_pc_inputs.m_standby_control == C_csp_power_cycle::ON )
			{
				// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
				if( iter_q_pc == 1 )
				{
					q_pc_exit_mode = NO_SOLUTION;
					q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
					break;		// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
				}
				else
				{	// If power cycle model has solved with results previously, then try to find another guess value
					// Assumption here is that power cycle solved at the first guess mass flow rate
					// Also, assume that if both upper and lower bounds exist, then can't generate a new guess

					if( m_dot_pc_guess < m_dot_pc_guess_ini )
					{	// If current guess value is less than initial value, then:	

						// If lower bound is already set OR upper bound is not set, can't generate new guess
						if( is_m_dot_lower_bound || !is_m_dot_upper_bound )
						{
							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_lower = m_dot_pc_guess;
						is_m_dot_lower_bound = true;
						is_m_dot_lower_error = false;

						// At this point, both upper and lower bound should exist, so we can generate new guess
						// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
					else
					{	// If current guess value is greater than initial guess, then:

						// If upper bound is already set OR lower bound is not set, can't generate new guess
						if( is_m_dot_upper_bound || !is_m_dot_lower_bound )
						{
							q_pc_exit_mode = NO_SOLUTION;
							q_pc_exit_tolerance = std::numeric_limits<double>::quiet_NaN();
							break;	// exits while() on diff_q_pc and sends control to while() on diff_T_rec_in
						}

						m_dot_pc_upper = m_dot_pc_guess;
						is_m_dot_upper_bound = true;
						is_m_dot_upper_error = false;

						// At this point, both upper and lower bound should exist, so we can generate new guess
						// And communicate this to Guess-Generator by setting diff_T_rec_in to NaN
						diff_q_pc = std::numeric_limits<double>::quiet_NaN();
					}
				}
			}	// end logic to handle power cycle not solving 

			// Calculate thermal power delivered to power cycle
			// Calculate difference between calculated thermal power to cycle and target: diff_q_pc
			diff_q_pc = (mc_pc_out_solver.m_q_dot_htf - q_dot_pc_fixed) / q_dot_pc_fixed; 	//[-] (Calc - Target)/Target: (+) Mass flow rate guess too high, (-) Mass flow rate guess too low

		}	// end while() diff_q_pc

		// Check exit modes
		if( q_pc_exit_mode != CONVERGED && q_pc_exit_mode != POOR_CONVERGENCE )
		{
			return;		// exits while() on diff_T_rec_in
		}

		// Get power cycle HTF return temperature...
		double T_pc_out = mc_pc_out_solver.m_T_htf_cold + 273.15;	//[K]

		// Charge storage
		double m_dot_tes = m_dot_receiver - m_dot_pc_guess;					//[kg/hr]
		double T_tes_cold_out = std::numeric_limits<double>::quiet_NaN();
		bool tes_success = mc_tes.charge(mc_sim_info.m_step, mc_weather.ms_outputs.m_tdry + 273.15, m_dot_tes / 3600.0, mc_cr_out_solver.m_T_salt_hot + 273.15,
			T_tes_cold_out, mc_tes_outputs);

		if( !tes_success )
		{
			T_rec_in_exit_mode = OVER_TARGET_PC;
			break;
		}

		// HTF charging state
		mc_tes_ch_htf_state.m_m_dot = m_dot_tes;								//[kg/hr]
		mc_tes_ch_htf_state.m_temp_in = mc_cr_out_solver.m_T_salt_hot;				//[C]
		mc_tes_ch_htf_state.m_temp_out = T_tes_cold_out - 273.15;				//[C] convert from K

		// If not actually discharging (i.e. mass flow rate = 0.0), what should the temperatures be?
		mc_tes_dc_htf_state.m_m_dot = 0.0;										//[kg/hr]
		mc_tes_dc_htf_state.m_temp_in = mc_tes_outputs.m_T_cold_ave - 273.15;	//[C] convert from K
		mc_tes_dc_htf_state.m_temp_out = mc_tes_outputs.m_T_hot_ave - 273.15;	//[C] convert from K

		// Enthalpy balancer (mixer)
		double T_rec_in_calc = (m_dot_tes*T_tes_cold_out + m_dot_pc_guess*T_pc_out) / m_dot_receiver - 273.15;		//[C]

		// Calculate diff_T_rec_in
		diff_T_rec_in = (T_rec_in_calc - T_rec_in_guess) / T_rec_in_guess;		//[-]

	} // end while() on diff_T_rec_in

	return;
}

void C_csp_solver::solver_cr_to_pc_to_cr(double field_control_in, double tol, int &exit_mode, double &exit_tolerance)
{
	// Method to solve scenario where the CR is on (under some fixed operating conditions, i.e. defocus)
	// and the PC is on. No TES or AUX, so the output of the CR connects directly to the PC

	// Ouputs:
	// int exit_mode: E_solver_outcomes 
	
	// Solution procedure
	// 1) Guess the receiver inlet temperature
	// Use design temperature for now, but this is an area where "smart" guesses could be applied
	double T_rec_in_guess_ini = m_T_htf_cold_des - 273.15;		//[C], convert from [K]
	double T_rec_in_guess = T_rec_in_guess_ini;
	// Set lower and upper bounds, or find through iteration?
	// Lower bound could be freeze protection temperature...
	double T_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double T_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_lower = std::numeric_limits<double>::quiet_NaN();
	double y_rec_in_upper = std::numeric_limits<double>::quiet_NaN();
	// Booleans for bounds and convergence error
	bool is_upper_bound = false;
	bool is_lower_bound = false;
	bool is_upper_error = false;
	bool is_lower_error = false;

	double diff_T_in = 999.9*tol;		// (Calc - Guess)/Guess: (+) Guess was too low, (-) Guess was too high

	int iter_T_in = 0;

	// Start iteration loop
	while( fabs(diff_T_in) > tol || diff_T_in != diff_T_in )
	{
		iter_T_in++;			// First iteration = 1

		// Check if distance between bounds is "too small"
		double diff_T_bounds = T_rec_in_upper - T_rec_in_lower;
		if( diff_T_bounds / T_rec_in_upper < tol / 2.0 )
		{
			if( diff_T_in != diff_T_in )
			{	// Models aren't producing power or are returning errors, and it appears we've tried the solution space for T_rec_in
				
				exit_mode = NO_SOLUTION;
				exit_tolerance = diff_T_in;
				return;
			}
			else
			{	// Models are producing power, but convergence errors are not within Tolerance

				exit_mode = POOR_CONVERGENCE;
				exit_tolerance = diff_T_in;
				return;
			}
		}


		// Subsequent iterations need to re-calculate T_in
		if( iter_T_in > 1 )
		{
			if( diff_T_in != diff_T_in )
			{	// Models did not solve such that a convergence error could be generated
				// However, we know that upper and lower bounds are set, so we can calculate a new guess via bisection method
				// but check that bounds exist, to be careful
				if( !is_lower_bound || !is_upper_bound )
				{
					exit_mode = NO_SOLUTION;
					exit_tolerance = diff_T_in;
					return;
				}
				T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
			}
			else if( diff_T_in > 0.0 )		// Guess receiver inlet temperature was too low
			{
				is_lower_bound = true;
				is_lower_error = true;
				T_rec_in_lower = T_rec_in_guess;		// Set lower bound
				y_rec_in_lower = diff_T_in;				// Set lower convergence error

				if( is_upper_bound && is_upper_error )		// False-position method
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;	//[C]
				}
				else if( is_upper_bound )						// Bisection method
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
				}
				else				// Constant adjustment
				{
					T_rec_in_guess += 2.5;			//[C]
				}
			}
			else							// Guess receiver inlet temperature was too high
			{
				is_upper_bound = true;
				is_upper_error = true;
				T_rec_in_upper = T_rec_in_guess;		// Set upper bound
				y_rec_in_upper = diff_T_in;				// Set upper convergence error

				if( is_lower_bound && is_lower_error )		// False-position method
				{
					T_rec_in_guess = y_rec_in_upper / (y_rec_in_upper - y_rec_in_lower)*(T_rec_in_lower - T_rec_in_upper) + T_rec_in_upper;	//[C]
				}
				else if( is_lower_bound )
				{
					T_rec_in_guess = 0.5*(T_rec_in_lower + T_rec_in_upper);		//[C]
				}
				else
				{
					T_rec_in_guess -= 2.5;			//[C] 
				}
			}
		}

		// 2) Solve the receiver model

		// CR: ON
		mc_cr_htf_state_in.m_temp = T_rec_in_guess;			//[C], convert from [K]
		mc_cr_inputs.m_field_control = field_control_in;	//[-] apply defocus from method input 
		mc_cr_inputs.m_input_operation_mode = C_csp_collector_receiver::ON;

		mc_collector_receiver.call(mc_weather.ms_outputs,
			mc_cr_htf_state_in,
			mc_cr_inputs,
			mc_cr_out_solver,
			mc_cr_out_report,
			mc_sim_info);

		// Check if receiver is OFF or model didn't solve
		if( mc_cr_out_solver.m_m_dot_salt_tot == 0.0 || mc_cr_out_solver.m_q_thermal == 0.0 )
		{
			// If first iteration, don't know enough about why collector/receiver is not producing power to advance iteration
			if( iter_T_in == 1 )
			{	
				exit_mode = NO_SOLUTION;
				exit_tolerance = diff_T_in;
				return;
			}
			else
			{	// Set this T_rec_in_guess as either upper or lower bound, depending on which end of DESIGN temp it falls
				// Assumption here is that receiver solved at first guess temperature
				// and that the failure wouldn't occur between established bounds
				if( T_rec_in_guess < T_rec_in_guess_ini )
				{
					if( is_lower_bound || !is_upper_bound )
					{
						exit_mode = NO_SOLUTION;
						exit_tolerance = diff_T_in;
						return;
					}
					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;
					// At this point, both and upper and lower bound should exist, so can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_in to NaN
					diff_T_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
				else
				{
					if( is_upper_bound || !is_lower_bound )
					{
						exit_mode = NO_SOLUTION;
						exit_tolerance = diff_T_in;
						return;
					}
					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;
					// At this point, both and upper and lower bound should exist, so can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_in to NaN
					diff_T_in = std::numeric_limits<double>::quiet_NaN();
					continue;
				}
			}
		}	// End Collector/Receiver OFF decisions

		// 3) Solve the power cycle model using receiver outputs
		// Power Cycle: ON
		mc_pc_htf_state_in.m_temp = mc_cr_out_solver.m_T_salt_hot;		//[C]
		mc_pc_inputs.m_m_dot = mc_cr_out_solver.m_m_dot_salt_tot;		//[kg/hr] no mass flow rate to power cycle
		// Inputs
		mc_pc_inputs.m_standby_control = C_csp_power_cycle::ON;
			//mc_pc_inputs.m_tou = tou_timestep;
		// Performance Call
		mc_power_cycle.call(mc_weather.ms_outputs,
			mc_pc_htf_state_in,
			mc_pc_inputs,
			mc_pc_out_solver,
			mc_pc_out_report,
			mc_sim_info);

		// Check that power cycle is producing power or model didn't solve
		if( mc_pc_out_solver.m_P_cycle == 0.0 )
		{
			// If first iteration, don't know enough about why power cycle is not producing power to advance iteration
			// Go to Receiver OFF power cycle OFF
			if( iter_T_in == 1 )
			{
				exit_mode = NO_SOLUTION;
				exit_tolerance = diff_T_in;
				return;
			}
			else
			{	// Set this T_rec_in_guess as either upper or lower bound, depending on which end of DESIGN temp it falls
				// Assumption here is that receiver solved at first guess temperature
				// and that the failure wouldn't occur between established bounds
				if( T_rec_in_guess < T_rec_in_guess_ini )
				{
					if( is_lower_bound || !is_upper_bound )
					{
						exit_mode = NO_SOLUTION;
						exit_tolerance = diff_T_in;
						return;
					}
					T_rec_in_lower = T_rec_in_guess;
					is_lower_bound = true;
					is_lower_error = false;
					// At this point, both and upper and lower bound should exist, so can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_in to NaN
					diff_T_in = std::numeric_limits<double>::quiet_NaN();
				}
				else
				{
					if( is_upper_bound || !is_lower_bound )
					{
						exit_mode = NO_SOLUTION;
						exit_tolerance = diff_T_in;
						return;
					}
					T_rec_in_upper = T_rec_in_guess;
					is_upper_bound = true;
					is_upper_error = false;
					// At this point, both and upper and lower bound should exist, so can generate new guess
					// And communicate this to Guess-Generator by setting diff_T_in to NaN
					diff_T_in = std::numeric_limits<double>::quiet_NaN();
				}
			}
		}	// end Power Cycle OFF decisions
		else
		{
			diff_T_in = (mc_pc_out_solver.m_T_htf_cold - T_rec_in_guess) / T_rec_in_guess;
		}

	}	// end iteration on T_rec_in

	exit_mode = CONVERGED;
	exit_tolerance = diff_T_in;

	return;
}

void C_csp_tou::init_parent()
{
	// Check that dispatch logic is reasonable
	if( !(mc_dispatch_params.m_dispatch_optimize || mc_dispatch_params.m_is_block_dispatch) )
	{
		throw(C_csp_exception("Must select a plant control strategy", "TOU initialization"));
	}

	if( mc_dispatch_params.m_dispatch_optimize && mc_dispatch_params.m_is_block_dispatch )
	{
		throw(C_csp_exception("Both plant control strategies were selected. Please select one.", "TOU initialization"));
	}

	if( mc_dispatch_params.m_is_block_dispatch )
	{
		if( mc_dispatch_params.m_use_rule_1 )
		{
			if( mc_dispatch_params.m_standby_off_buffer < 0.0 )
			{
				throw(C_csp_exception("Block Dispatch Rule 1 was selected, but the time entered was invalid."
					" Please select a time >= 0", "TOU initialization"));
			}
		}

		if( mc_dispatch_params.m_use_rule_2 )
		{
			if( mc_dispatch_params.m_f_q_dot_pc_overwrite <= 0.0 || 
				mc_dispatch_params.m_q_dot_rec_des_mult <= 0.0 )
			{
				throw(C_csp_exception("Block Dispatch Rule 2 was selected, but the parameters entered were invalid."
					" Both values must be greater than 0", "TOU initialization"));
			}
		}
	}
}