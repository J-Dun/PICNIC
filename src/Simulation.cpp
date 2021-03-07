#include "Simulation.H"
#include <limits>

#include "LoadBalance.H"

#include "NamespaceHeader.H"

Simulation::Simulation( ParmParse& a_pp )
   :   m_verbosity(0),
       m_cur_step(0),
       m_max_step(0),
       m_cur_time(0.0),
       m_max_time(0.0),
       m_cur_dt(-1.0),
       m_fixed_dt(-1.0),
       m_max_dt_grow(1.1),
       m_cfl(1.0),
       m_adapt_dt(true),
       m_checkpoint_interval(0),
       m_last_checkpoint(0),
       m_checkpoint_prefix( "chk" ),
       m_plot_interval(0),
       m_plot_time_interval(0.0),
       m_plot_time(0.0),
       m_last_plot(0),
       m_history(false),
       m_history_interval(1),
       m_last_history(0),
       m_plot_prefix( "plt" ),
       m_system( NULL )
#ifdef CH_USE_TIMER
,
       m_all_timer( NULL ),
       m_setup_timer( NULL ),
       m_solve_timer( NULL ),
       m_shutdown_timer( NULL )
#endif
{
   m_main_start = clock();
#ifdef CH_USE_TIMER
   initializeTimers();
   m_all_timer->start();
   m_setup_timer->start();
#endif

#ifdef USE_ARRAYVIEW
   trapfpe();
#endif

   // parse simulation specific parameters from input file
   //
   ParmParse ppsim( "simulation" );
   parseParameters( ppsim );

   // initialize the entire system 
   // (domain, mesh, species, fields, operators, ibcs...)
   //
   m_system = new System( a_pp );
   m_system->initialize(m_cur_step, m_cur_time);

   // write t=0 values to plot files
   //
   if ( m_plot_interval>=0 || m_plot_time_interval>=0.0) {
      writePlotFile(); // plot at t=0
      m_last_plot = m_cur_step;
      if ( m_plot_time_interval>=0.0 ) m_plot_time = m_plot_time_interval;
   }
   //writeHistFile(m_history); // "true" forces a write on start-up
   //m_last_history = m_cur_step;

#ifdef CH_USE_TIMER
   setup_timer->stop();
#endif

#ifdef CH_USE_TIMER
   solve_timer->start();
#endif
   m_solve_start = clock();

}

Simulation::~Simulation()
{
   delete m_system;
#ifdef CH_USE_TIMER
   delete m_all_timer;
   delete m_setup_timer;
   delete m_solve_timer;
   delete m_shutdown_timer;
#endif
}

const Real
Simulation::s_DT_EPS = std::numeric_limits<Real>::epsilon();

void Simulation::printParameters()
{
   pout() << "maximum step = " << m_max_step << endl;
   pout() << "maximum time = " << m_max_time << endl;
   pout() << "checkpoint interval = " << m_checkpoint_interval << endl;
   pout() << "plot interval = " << m_plot_interval << endl;
   if(m_plot_time_interval>0.0) pout() << "plot time interval = " << m_plot_time_interval << endl;
   if(m_history)                pout() << "hist step interval = " << m_history_interval << endl;
}


void Simulation::loadRestartFile( ParmParse& a_ppsim )
{
   std::string restartFile;
   a_ppsim.query( "restart_file", restartFile );
#ifdef CH_USE_HDF5
   HDF5Handle handle( restartFile, HDF5Handle::OPEN_RDONLY );
   //m_system->readCheckpointFile( handle, m_cur_step, m_cur_time, m_cur_dt );
   handle.close();
#else
   MayDay::Error("restart only defined with hdf5");
#endif
}

//static int plottedonce = 0;

void Simulation::writePlotFile()
{
   if (!procID() && m_verbosity) {
      cout << "Simulation::writePlotFile" << endl;
   }

#ifdef CH_USE_HDF5
   m_system->writePlotFile(m_cur_step, m_cur_time);
#else
   MayDay::Error( "plotting only defined with hdf5" );
#endif

}

inline void Simulation::writeHistFile(bool startup_flag)
{
   // the startup_flag is used to force a write on start-up
   m_system->writeHistFile(m_cur_step, m_cur_time, startup_flag);

}

void Simulation::writeCheckpointFile()
{
   if (m_verbosity>=3) {
      pout() << "Simulation::writeCheckpointFile" << endl;
   }

#ifdef CH_USE_HDF5
   char iter_str[100];

   sprintf( iter_str, "%s%04d.%dd.hdf5",
            m_checkpoint_prefix.c_str(), m_cur_step, SpaceDim );

   if (m_verbosity>=2) {
      pout() << "checkpoint file name = " << iter_str << endl;
   }

   HDF5Handle handle( iter_str, HDF5Handle::CREATE );
   //m_system->writeCheckpointFile( handle, m_cur_step, m_cur_time, m_cur_dt );
   handle.close();
#else
   MayDay::Error( "restart only defined with hdf5" );
#endif
}

void Simulation::initializeTimers()
{
#ifdef CH_USE_TIMER
   m_all_timer      = new Timer("All",0);
   m_setup_timer    = new Timer("Setup",*m_all_timer);
   m_solve_timer    = new Timer("Solve",*m_all_timer,1);
   m_shutdown_timer = new Timer("Shutdown",*n_all_timer);
   Timer::TimerInit( 0 );
#endif
}

bool Simulation::notDone()
{
   return ( (m_cur_step<m_max_step) && (m_cur_time<m_max_time) );
}


void Simulation::advance()
{
   CH_TIMERS("Simulation::advance()");
   CH_TIMER("print_diagnostics",t_print_diagnostcs);

   preTimeStep();
   if (m_verbosity >= 1) {
      pout() << endl << "Step " << m_cur_step << endl;
      if (procID()==0) {
         cout << endl << "Step " << m_cur_step+1 
              << ", dt = " << m_cur_dt << endl;
      }
   }
   m_system->advance( m_cur_time, m_cur_dt, m_cur_step );
   //postTimeStep();

   clock_t m_now = clock();
   double walltime, walltime_g;
   walltime = ((double) (m_now - m_solve_start)) / CLOCKS_PER_SEC;
#ifdef CH_MPI
   MPI_Allreduce(&walltime,&walltime_g,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
#else
   walltime_g = walltime;
#endif

   CH_START(t_print_diagnostcs);
   if (m_verbosity >= 1) {
      //m_system->printDiagnostics();
      pout()<< "Step " << m_cur_step << " completed, simulation time is "
            << m_cur_time << ", solver wall time is " << walltime_g << " seconds"
            << endl << "----" << endl;
      if(procID()==0) {
         cout << "Step " << m_cur_step << " completed, simulation time is "
              << m_cur_time << ", solver wall time is " << walltime_g << " seconds"
              << endl << "----" << endl;
      }
   }
   CH_STOP(t_print_diagnostcs);

   if ( m_plot_time_interval>0.0 ) {
      if ( m_cur_time>=m_plot_time ) {
         writePlotFile();
         m_last_plot = m_cur_step;
         m_plot_time = m_plot_time + m_plot_time_interval;
      } 
   } 
   else {
      if ( (m_cur_step % m_plot_interval)==0 ) {
         writePlotFile();
         m_last_plot = m_cur_step;
      }
   }

   //writeHistFile(false);

   if ( (m_cur_step % m_checkpoint_interval)==0 ) {
      if (!procID()) {
         cout << "----\n";
      }
      //m_system->printFunctionCounts();
      if (!procID()) {
         cout << "----\n";
      }
      //m_system->printTimeIntegratorCounts();
      if (!procID()) {
         cout << "----\n";
      }
      //writeCheckpointFile();
      //m_last_checkpoint = m_cur_step;
   }
}


void Simulation::finalize()
{
  m_solve_end = clock();
#ifdef CH_USE_TIMER
   solve_timer->stop();
#endif
#ifdef CH_USE_TIMER
   shutdown_timer->start() ;
#endif

   if ( (m_plot_interval >= 0 || m_plot_time_interval >= 0.0) && (m_last_plot!=m_cur_step) ) {
      writePlotFile();
   }

   if ( m_last_checkpoint!=m_cur_step ) {
      if (!procID()) {
         cout << "----\n";
      }
      //m_system->printFunctionCounts();
      if (!procID()) {
        cout << "----\n";
      }
      //m_system->printTimeIntegratorCounts();
      if (!procID()) {
        cout << "----\n";
      }
      //writeCheckpointFile();
   }

   m_main_end = clock();

   double main_walltime = ((double) (m_main_end - m_main_start)) / CLOCKS_PER_SEC;
   double solve_walltime = ((double) (m_solve_end - m_solve_start)) / CLOCKS_PER_SEC;
   double main_walltime_g, solve_walltime_g;
#ifdef CH_MPI
   MPI_Allreduce(&main_walltime,&main_walltime_g,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
   MPI_Allreduce(&solve_walltime,&solve_walltime_g,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
#else
   main_walltime_g = main_walltime;
   solve_walltime_g = solve_walltime;
#endif
   if (!procID()) {
     cout << "Solve wall time (in seconds): " << solve_walltime_g << "\n";
     cout << "Total wall time (in seconds): " << main_walltime_g << "\n";
   }

#ifdef CH_USE_TIMER
   m_shutdown_timer->stop() ;
   m_all_timer->stop() ;
   Timer::TimerSummary();
#endif
}


void Simulation::parseParameters( ParmParse& a_ppsim )
{
   // This determines the amount of diagnositic output generated
   a_ppsim.query( "verbosity", m_verbosity );
   CH_assert( m_verbosity >= 0 );

   // Stop after this number of steps
   a_ppsim.query( "max_step", m_max_step );
   CH_assert( m_max_step >= 0 );

   // Stop when the simulation time get here
   a_ppsim.query( "max_time", m_max_time );
   CH_assert( m_max_time >= 0.0 );

   // If set, use as the fixed time step size
   if ( a_ppsim.query( "fixed_dt", m_fixed_dt ) ) {
      CH_assert( m_fixed_dt>0.0 );
      m_adapt_dt = false;
   }

   // Multiply by which to increase dt each step
   if ( a_ppsim.query( "max_dt_grow", m_max_dt_grow ) ) {
      CH_assert( m_max_dt_grow>1.0 );
      if (!m_adapt_dt) {
         MayDay::Error( "fixed_dt and max_dt_grow are mutually exclusive!" );
      }
   }

   // set cfl number for the case of dynamic timestep selection
   if ( a_ppsim.query( "cfl_number", m_cfl ) ) {
      CH_assert( m_cfl>0.0 && m_cfl<=2.0 );
      if (!m_adapt_dt) {
         MayDay::Error( "fixed_dt and cfl are mutually exclusive!" );
      }
   }

   // Set up checkpointing
   a_ppsim.query( "checkpoint_interval", m_checkpoint_interval );
   a_ppsim.query( "checkpoint_prefix", m_checkpoint_prefix );

   // Set up plot file writing
   a_ppsim.query( "plot_interval", m_plot_interval );
   a_ppsim.query( "plot_time_interval", m_plot_time_interval );
   if( m_plot_time_interval>0.0 ) m_plot_interval = 0;
   a_ppsim.query( "plot_prefix", m_plot_prefix );

   // get History parameter
   a_ppsim.query("history", m_history);
   a_ppsim.query("history_interval", m_history_interval );
   if(m_history) CH_assert( m_history_interval>0 );

   if (m_verbosity) {
      printParameters();
   }

}


void Simulation::setFixedTimeStep( const Real& a_dt_stable )
{
   m_cur_dt = m_fixed_dt; 
}


inline void Simulation::postTimeStep()
{
   if(!procID()) cout << "Simulation::postTimeStep()" << endl;
   //m_system->postTimeStep( m_cur_step, m_cur_dt, m_cur_time );
}

void Simulation::preTimeStep()
{
   //m_system->preTimeStep( m_cur_step, m_cur_time );
   Real dt_stable = m_system->stableDt( m_cur_step )*m_cfl;
   CH_assert( dt_stable > 1.0e-16 );

   if ( m_adapt_dt ) { 
      m_cur_dt = std::min( dt_stable, m_max_dt_grow * m_cur_dt );
   } 
   else {
      setFixedTimeStep( dt_stable );
   }
   
   // If less than a time step from the final time, adjust time step
   // to end just over the final time.
   Real timeRemaining = m_max_time - m_cur_time;
   if ( m_cur_dt > timeRemaining ) {
      m_cur_dt = timeRemaining + m_max_time * s_DT_EPS;
   }

}

#include "NamespaceFooter.H"
